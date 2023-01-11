// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_NSS_UTIL_H_
#define CRYPTO_NSS_UTIL_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/threading/thread_restrictions.h"
#include "build/chromeos_buildflags.h"
#include "components/nacl/common/buildflags.h"
#include "crypto/crypto_export.h"

namespace base {
class Time;
}  // namespace base

// This file specifically doesn't depend on any NSS or NSPR headers because it
// is included by various (non-crypto) parts of chrome to call the
// initialization functions.
namespace crypto {

class ScopedAllowBlockingForNSS : public base::ScopedAllowBlocking {};

// Initialize NRPR if it isn't already initialized.  This function is
// thread-safe, and NSPR will only ever be initialized once.
CRYPTO_EXPORT void EnsureNSPRInit();

// Initialize NSS if it isn't already initialized.  This must be called before
// any other NSS functions.  This function is thread-safe, and NSS will only
// ever be initialized once.
CRYPTO_EXPORT void EnsureNSSInit();

// Check if the current NSS version is greater than or equals to |version|.
// A sample version string is "3.12.3".
bool CheckNSSVersion(const char* version);

#if BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_MINIMAL_TOOLCHAIN)

// Returns true once the TPM is owned and PKCS#11 initialized with the
// user and security officer PINs, and Chaps has been successfully loaded into
// NSS. Returns false if the TPM will never be loaded.
CRYPTO_EXPORT void IsTPMTokenEnabled(base::OnceCallback<void(bool)> callback);

// Initialize the TPM token and system slot. The |callback| will run on the same
// thread with true if the token and slot were successfully loaded or were
// already initialized. |callback| will be passed false if loading failed.
// Should be called only once.
CRYPTO_EXPORT void InitializeTPMTokenAndSystemSlot(
    int system_slot_id,
    base::OnceCallback<void(bool)> callback);

// Notifies clients that the TPM has finished initialization (i.e. notify
// the callbacks of `IsTPMTokenEnabled()` or `GetSystemNSSKeySlot()`).
// If `InitializeTPMTokenAndSystemSlot()` has been called before this method,
// this signals that the TPM is enabled, and should use the slot configured by
// those methods. If neither of those methods have been called, this signals
// that no TPM system slot will be available.
CRYPTO_EXPORT void FinishInitializingTPMTokenAndSystemSlot();

// TODO(crbug.com/1163303) Remove when the bug is fixed.
// Can be used to collect additional information when public slot fails to open.
// Mainly checks the access permissions on the files and tries to read them.
// Crashes Chrome because it will crash anyway when it tries to instantiate
// NSSCertDatabase with a nullptr public slot, crashing early can provide better
// logs/stacktraces for diagnosing.
// Takes `nss_path` where NSS is supposed to be (or created). Will attempt
// creating the path if it doesn't exist (to check that it can be done).
// Theoretically the path should already exist because it's created when Chrome
// tries to open the public slot.
CRYPTO_EXPORT void DiagnosePublicSlotAndCrash(const base::FilePath& nss_path);

#endif  // BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_MINIMAL_TOOLCHAIN)

// Convert a NSS PRTime value into a base::Time object.
// We use a int64_t instead of PRTime here to avoid depending on NSPR headers.
CRYPTO_EXPORT base::Time PRTimeToBaseTime(int64_t prtime);

// Convert a base::Time object into a PRTime value.
// We use a int64_t instead of PRTime here to avoid depending on NSPR headers.
CRYPTO_EXPORT int64_t BaseTimeToPRTime(base::Time time);

}  // namespace crypto

#endif  // CRYPTO_NSS_UTIL_H_
