/* librepo - A library providing (libcURL like) API to downloading repository
 * Copyright (C) 2012  Tomas Mlcoch
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <gpgme.h>

#include "rcodes.h"
#include "setup.h"
#include "util.h"
#include "gpg.h"

int
lr_gpg_check_signature(const char *signature_fn,
                       const char *data_fn,
                       const char *home_dir)
{
    int signature_fd, data_fd;
    gpgme_error_t err;
    gpgme_ctx_t context;
    gpgme_data_t signature_data;
    gpgme_data_t data_data;
    gpgme_verify_result_t result;
    gpgme_signature_t sig;

    // Initialization
    gpgme_check_version(NULL);
    err = gpgme_engine_check_version(GPGME_PROTOCOL_OpenPGP);
    if (err != GPG_ERR_NO_ERROR) {
        DPRINTF("%s: gpgme_engine_check_version: %s\n",
                 __func__, gpgme_strerror(err));
        return LRE_GPGNOTSUPPORTED;
    }

    err = gpgme_new(&context);
    if (err != GPG_ERR_NO_ERROR) {
        DPRINTF("%s: gpgme_new: %s\n", __func__, gpgme_strerror(err));
        return LRE_GPGERROR;
    }

    err = gpgme_set_protocol(context, GPGME_PROTOCOL_OpenPGP);
    if (err != GPG_ERR_NO_ERROR) {
        DPRINTF("%s: gpgme_set_protocol: %s\n", __func__, gpgme_strerror(err));
        return LRE_GPGERROR;
    }

    if (home_dir) {
        err = gpgme_ctx_set_engine_info(context, GPGME_PROTOCOL_OpenPGP,
                                        NULL, home_dir);
        if (err != GPG_ERR_NO_ERROR) {
            DPRINTF("%s: gpgme_ctx_set_engine_info: %s\n", __func__, gpgme_strerror(err));
            return LRE_GPGERROR;
        }
    }

    gpgme_set_armor(context, 1);

    signature_fd = open(signature_fn, O_RDONLY);
    if (signature_fd == -1) {
        DPRINTF("%s: Opening signature: %s\n", __func__, strerror(errno));
        return LRE_IO;
    }

    data_fd = open(data_fn, O_RDONLY);
    if (signature_fd == -1) {
        DPRINTF("%s: Opening data: %s\n", __func__, strerror(errno));
        return LRE_IO;
    }

    err = gpgme_data_new_from_fd(&signature_data, signature_fd);
    if (err != GPG_ERR_NO_ERROR) {
        DPRINTF("%s: gpgme_data_new_from_fd: %s\n",
                 __func__, gpgme_strerror(err));
        return LRE_GPGERROR;
    }

    err = gpgme_data_new_from_fd(&data_data, data_fd);
    if (err != GPG_ERR_NO_ERROR) {
        DPRINTF("%s: gpgme_data_new_from_fd: %s\n",
                 __func__, gpgme_strerror(err));
        return LRE_GPGERROR;
    }

    // Verify
    err = gpgme_op_verify(context, signature_data, data_data, NULL);
    if (err != GPG_ERR_NO_ERROR) {
        DPRINTF("%s: gpgme_op_verify: %s\n", __func__, gpgme_strerror(err));
        return LRE_GPGERROR;
    }

    result = gpgme_op_verify_result(context);
    if (!result) {
        DPRINTF("%s: gpgme_op_verify_result: error\n", __func__);
        return LRE_GPGERROR;
    }

    // Check result of verification
    sig = result->signatures;
    if(!sig) {
        DPRINTF("%s: signature verify error (no signatures)\n", __func__);
        return LRE_BADGPG;
    }

    // Example of signature usage could be found in gpgme git repository
    // in the gpgme/tests/run-verify.c
    for (; sig; sig = sig->next) {
        if ((sig->summary & GPGME_SIGSUM_VALID) ||  // Valid
            (sig->summary & GPGME_SIGSUM_GREEN) ||  // Valid
            (sig->summary == 0 && sig->status == GPG_ERR_NO_ERROR)) // Valid but key is not certified with a trusted signature
        {
            return LRE_OK;
        }
    }

    return LRE_BADGPG;
}

int
lr_gpg_import_key(const char *key_fn, const char *home_dir)
{
    gpgme_error_t err;
    int key_fd;
    gpgme_ctx_t context;
    gpgme_data_t key_data;

    // Initialization
    gpgme_check_version(NULL);
    err = gpgme_engine_check_version(GPGME_PROTOCOL_OpenPGP);
    if (err != GPG_ERR_NO_ERROR) {
        DPRINTF("%s: gpgme_engine_check_version: %s\n",
                 __func__, gpgme_strerror(err));
        return LRE_GPGNOTSUPPORTED;
    }

    err = gpgme_new(&context);
    if (err != GPG_ERR_NO_ERROR) {
        DPRINTF("%s: gpgme_new: %s\n", __func__, gpgme_strerror(err));
        return LRE_GPGERROR;
    }

    err = gpgme_set_protocol(context, GPGME_PROTOCOL_OpenPGP);
    if (err != GPG_ERR_NO_ERROR) {
        DPRINTF("%s: gpgme_set_protocol: %s\n", __func__, gpgme_strerror(err));
        return LRE_GPGERROR;
    }

    if (home_dir) {
        err = gpgme_ctx_set_engine_info(context, GPGME_PROTOCOL_OpenPGP,
                                        NULL, home_dir);
        if (err != GPG_ERR_NO_ERROR) {
            DPRINTF("%s: gpgme_ctx_set_engine_info: %s\n", __func__, gpgme_strerror(err));
            return LRE_GPGERROR;
        }
    }

    gpgme_set_armor(context, 1);

    // Key import

    key_fd = open(key_fn, O_RDONLY);
    if (key_fd == -1) {
        DPRINTF("%s: Opening key: %s\n", __func__, strerror(errno));
        return LRE_IO;
    }

    err = gpgme_data_new_from_fd(&key_data, key_fd);
    if (err != GPG_ERR_NO_ERROR) {
        DPRINTF("%s: gpgme_data_new_from_fd: %s\n",
                 __func__, gpgme_strerror(err));
        return LRE_GPGERROR;
    }

    err = gpgme_op_import(context, key_data);
    if (err != GPG_ERR_NO_ERROR) {
        DPRINTF("%s: gpgme_op_import: %s\n", __func__, gpgme_strerror(err));
        return LRE_GPGERROR;
    }

    return LRE_OK;
}
