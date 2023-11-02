// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_CAPI_UTIL_H_
#define CRYPTO_CAPI_UTIL_H_

#include <windows.h>
#include <stddef.h>

#include "crypto/crypto_export.h"

namespace crypto {

// Wrappers of malloc and free for CryptoAPI routines that need memory
// allocators, such as in CRYPT_DECODE_PARA. Such routines require WINAPI
// calling conventions.
CRYPTO_EXPORT void* WINAPI CryptAlloc(size_t size);
CRYPTO_EXPORT void WINAPI CryptFree(void* p);

}  // namespace crypto

#endif  // CRYPTO_CAPI_UTIL_H_
