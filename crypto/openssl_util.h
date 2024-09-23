// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_OPENSSL_UTIL_H_
#define CRYPTO_OPENSSL_UTIL_H_

#include <stddef.h>
#include <string.h>

#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "crypto/crypto_export.h"

namespace crypto {

// Provides a buffer of at least MIN_SIZE bytes, for use when calling OpenSSL's
// SHA256, HMAC, etc functions, adapting the buffer sizing rules to meet those
// of our base wrapper APIs.
// This allows the library to write directly to the caller's buffer if it is of
// sufficient size, but if not it will write to temporary |min_sized_buffer_|
// of required size and then its content is automatically copied out on
// destruction, with truncation as appropriate.
template<int MIN_SIZE>
class ScopedOpenSSLSafeSizeBuffer {
 public:
  ScopedOpenSSLSafeSizeBuffer(unsigned char* output, size_t output_len)
      : output_(output),
        output_len_(output_len) {
  }

  ScopedOpenSSLSafeSizeBuffer(const ScopedOpenSSLSafeSizeBuffer&) = delete;
  ScopedOpenSSLSafeSizeBuffer& operator=(const ScopedOpenSSLSafeSizeBuffer&) =
      delete;

  ~ScopedOpenSSLSafeSizeBuffer() {
    if (output_len_ < MIN_SIZE) {
      // Copy the temporary buffer out, truncating as needed.
      memcpy(output_, min_sized_buffer_, output_len_);
    }
    // else... any writing already happened directly into |output_|.
  }

  unsigned char* safe_buffer() {
    return output_len_ < MIN_SIZE ? min_sized_buffer_ : output_.get();
  }

 private:
  // Pointer to the caller's data area and its associated size, where data
  // written via safe_buffer() will [eventually] end up.
  raw_ptr<unsigned char> output_;
  size_t output_len_;

  // Temporary buffer writen into in the case where the caller's
  // buffer is not of sufficient size.
  unsigned char min_sized_buffer_[MIN_SIZE];
};

// Drains the OpenSSL ERR_get_error stack. On a debug build the error codes
// are send to VLOG(1), on a release build they are disregarded. In most
// cases you should pass FROM_HERE as the |location|.
CRYPTO_EXPORT void ClearOpenSSLERRStack(const base::Location& location);

// Place an instance of this class on the call stack to automatically clear
// the OpenSSL error stack on function exit.
class OpenSSLErrStackTracer {
 public:
  OpenSSLErrStackTracer() = delete;

  // Pass FROM_HERE as |location|, to help track the source of OpenSSL error
  // messages. Note any diagnostic emitted will be tagged with the location of
  // the constructor call as it's not possible to trace a destructor's callsite.
  explicit OpenSSLErrStackTracer(const base::Location& location)
      : location_(location) {}

  OpenSSLErrStackTracer(const OpenSSLErrStackTracer&) = delete;
  OpenSSLErrStackTracer& operator=(const OpenSSLErrStackTracer&) = delete;

  ~OpenSSLErrStackTracer() {
    ClearOpenSSLERRStack(location_);
  }

 private:
  const base::Location location_;
};

}  // namespace crypto

#endif  // CRYPTO_OPENSSL_UTIL_H_
