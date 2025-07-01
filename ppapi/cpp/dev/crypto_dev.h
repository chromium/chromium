// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_CRYPTO_DEV_H_
#define PPAPI_CPP_DEV_CRYPTO_DEV_H_

#include "ppapi/c/pp_stdint.h"

/// @file
/// This file defines APIs related to cryptography.

namespace pp {

/// APIs related to cryptography.
class Crypto_Dev {
 public:
  Crypto_Dev() {}

  /// A function that fills the buffer with random bytes. This may be slow, so
  /// avoid getting more bytes than needed.
  ///
  /// @param[out] buffer The buffer to receive the random bytes.
  /// @param[in] num_bytes A number of random bytes to produce.
  /// @return True on success, false on failure.
  bool GetRandomBytes(char* buffer, uint32_t num_bytes);
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_CRYPTO_DEV_H_
