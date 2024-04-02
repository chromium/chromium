// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_UNEXPORTABLE_KEY_WIN_H_
#define CRYPTO_UNEXPORTABLE_KEY_WIN_H_

#include <windows.h>

#include <ncrypt.h>

#include "base/containers/span.h"
#include "crypto/crypto_export.h"
#include "crypto/scoped_cng_types.h"

namespace crypto {

// Attempts to load a TPM-backed CNG key from the given `wrapped` value. Will
// assign the out `provider` and `key` values respectively. Returns true if all
// operations were successful.
CRYPTO_EXPORT bool LoadWrappedTPMKey(base::span<const uint8_t> wrapped,
                                     ScopedNCryptProvider& provider,
                                     ScopedNCryptKey& key);

}  // namespace crypto

#endif  // CRYPTO_UNEXPORTABLE_KEY_WIN_H_
