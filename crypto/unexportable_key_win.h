// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_UNEXPORTABLE_KEY_WIN_H_
#define CRYPTO_UNEXPORTABLE_KEY_WIN_H_

#include "base/containers/span.h"
#include "base/win/wincrypt_shim.h"
#include "crypto/crypto_export.h"
#include "crypto/scoped_cng_types.h"
#include "crypto/unexportable_key.h"

namespace crypto {

// Attempts to reload a CNG key's handle from the given `key`. Returns the key
// on success or an invalid handle on error.
CRYPTO_EXPORT ScopedNCryptKey
DuplicatePlatformKeyHandle(const UnexportableSigningKey& key);

}  // namespace crypto

#endif  // CRYPTO_UNEXPORTABLE_KEY_WIN_H_
