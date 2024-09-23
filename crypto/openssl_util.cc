// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/openssl_util.h"

#include <stddef.h>
#include <stdint.h>

#include <string_view>

#include "base/logging.h"
#include "third_party/boringssl/src/include/openssl/err.h"

namespace crypto {

namespace {

// Callback routine for OpenSSL to print error messages. |str| is a
// NULL-terminated string of length |len| containing diagnostic information
// such as the library, function and reason for the error, the file and line
// where the error originated, plus potentially any context-specific
// information about the error. |context| contains a pointer to user-supplied
// data, which is currently unused.
// If this callback returns a value <= 0, OpenSSL will stop processing the
// error queue and return, otherwise it will continue calling this function
// until all errors have been removed from the queue.
int OpenSSLErrorCallback(const char* str, size_t len, void* context) {
  DVLOG(1) << "\t" << std::string_view(str, len);
  return 1;
}

}  // namespace

void ClearOpenSSLERRStack(const base::Location& location) {
  if (DCHECK_IS_ON() && VLOG_IS_ON(1)) {
    uint32_t error_num = ERR_peek_error();
    if (error_num == 0)
      return;

    DVLOG(1) << "OpenSSL ERR_get_error stack from " << location.ToString();
    ERR_print_errors_cb(&OpenSSLErrorCallback, NULL);
  } else {
    ERR_clear_error();
  }
}

}  // namespace crypto
