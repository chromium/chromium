// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_UNEXPORTABLE_KEY_WIN_H_
#define CRYPTO_UNEXPORTABLE_KEY_WIN_H_

#include <memory>

#include "base/feature_list.h"
#include "crypto/crypto_export.h"
#include "crypto/scoped_cng_types.h"
#include "crypto/unexportable_key.h"

namespace crypto::internal {
enum class WinKeyHardwareSupport;
}  // namespace crypto::internal

namespace crypto {

// Attempts to reload a CNG key's handle from the given `key`. Returns the key
// on success or an invalid handle on error.
CRYPTO_EXPORT ScopedNCryptKey
DuplicatePlatformKeyHandle(const UnexportableSigningKey& key);

// Kill switch for returning no TPM provider on machines where TBS reports no
// TPM. When disabled, `GetUnexportableKeyProviderWin()` always returns a
// provider and doesn't load tbs.dll.
CRYPTO_EXPORT BASE_DECLARE_FEATURE(kUnexportableKeyProviderWinRequiresTpm);

// Returns an `UnexportableKeyProvider` that is backed by the Windows TPM, or
// nullptr if the machine has no TPM. The first call loads tbs.dll to ask TBS
// for the TPM, and the result is cached for the lifetime of the process.
std::unique_ptr<UnexportableKeyProvider> GetUnexportableKeyProviderWin();

// Returns the TPM version that TBS reports, or why it reports none. Unlike
// `GetUnexportableKeyProviderWin()`, asks TBS again on every call, so it may
// block.
internal::WinKeyHardwareSupport GetKeyHardwareSupportWin();

// Returns an `UnexportableKeyProvider` that is backed by the Microsoft Software
// Key Storage Provider.
std::unique_ptr<UnexportableKeyProvider>
GetMicrosoftSoftwareUnexportableKeyProviderWin();

}  // namespace crypto

#endif  // CRYPTO_UNEXPORTABLE_KEY_WIN_H_
