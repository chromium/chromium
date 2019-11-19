// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_NSS_UTIL_INTERNAL_H_
#define CRYPTO_NSS_UTIL_INTERNAL_H_

#include <secmodt.h>

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "crypto/crypto_export.h"
#include "crypto/scoped_nss_types.h"

namespace base {
class FilePath;
}

// These functions return a type defined in an NSS header, and so cannot be
// declared in nss_util.h.  Hence, they are declared here.

namespace crypto {

// Opens an NSS software database in folder |path|, with the (potentially)
// user-visible description |description|. Returns the slot for the opened
// database, or nullptr if the database could not be opened.
CRYPTO_EXPORT ScopedPK11Slot OpenSoftwareNSSDB(const base::FilePath& path,
                                               const std::string& description);

// A helper class that acquires the SECMOD list read lock while the
// AutoSECMODListReadLock is in scope.
class CRYPTO_EXPORT AutoSECMODListReadLock {
 public:
  AutoSECMODListReadLock();
  ~AutoSECMODListReadLock();

 private:
  SECMODListLock* lock_;
  DISALLOW_COPY_AND_ASSIGN(AutoSECMODListReadLock);
};

#if defined(OS_CHROMEOS)
// Returns a reference to the system-wide TPM slot if it is loaded. If it is not
// loaded and |callback| is non-null, the |callback| will be run once the slot
// is loaded.
CRYPTO_EXPORT ScopedPK11Slot GetSystemNSSKeySlot(
    base::OnceCallback<void(ScopedPK11Slot)> callback) WARN_UNUSED_RESULT;

// Sets the test system slot to |slot|, which means that |slot| will be exposed
// through |GetSystemNSSKeySlot| and |IsTPMTokenReady| will return true.
// |InitializeTPMTokenAndSystemSlot|, which triggers the TPM initialization,
// does not have to be called if the test system slot is set.
// This must must not be called consecutively with a |slot| != nullptr. If
// |slot| is nullptr, the test system slot is unset.
CRYPTO_EXPORT void SetSystemKeySlotForTesting(ScopedPK11Slot slot);

// Injects the given |slot| as a system slot set by the future
// |InitializeTPMTokenAndSystemSlot| call.
// This must must not be called consecutively with a |slot| != nullptr. If
// |slot| is nullptr and the system slot is already initialized to the
// previously passed test value, the system slot is unset.
CRYPTO_EXPORT void SetSystemKeySlotWithoutInitializingTPMForTesting(
    ScopedPK11Slot slot);

// Prepare per-user NSS slot mapping. It is safe to call this function multiple
// times. Returns true if the user was added, or false if it already existed.
CRYPTO_EXPORT bool InitializeNSSForChromeOSUser(
    const std::string& username_hash,
    const base::FilePath& path);

// Returns whether TPM for ChromeOS user still needs initialization. If
// true is returned, the caller can proceed to initialize TPM slot for the
// user, but should call |WillInitializeTPMForChromeOSUser| first.
// |InitializeNSSForChromeOSUser| must have been called first.
CRYPTO_EXPORT bool ShouldInitializeTPMForChromeOSUser(
    const std::string& username_hash) WARN_UNUSED_RESULT;

// Makes |ShouldInitializeTPMForChromeOSUser| start returning false.
// Should be called before starting TPM initialization for the user.
// Assumes |InitializeNSSForChromeOSUser| had already been called.
CRYPTO_EXPORT void WillInitializeTPMForChromeOSUser(
    const std::string& username_hash);

// Use TPM slot |slot_id| for user.  InitializeNSSForChromeOSUser must have been
// called first.
CRYPTO_EXPORT void InitializeTPMForChromeOSUser(
    const std::string& username_hash,
    CK_SLOT_ID slot_id);

// Use the software slot as the private slot for user.
// InitializeNSSForChromeOSUser must have been called first.
CRYPTO_EXPORT void InitializePrivateSoftwareSlotForChromeOSUser(
    const std::string& username_hash);

// Returns a reference to the public slot for user.
CRYPTO_EXPORT ScopedPK11Slot GetPublicSlotForChromeOSUser(
    const std::string& username_hash) WARN_UNUSED_RESULT;

// Returns the private slot for |username_hash| if it is loaded. If it is not
// loaded and |callback| is non-null, the |callback| will be run once the slot
// is loaded.
CRYPTO_EXPORT ScopedPK11Slot GetPrivateSlotForChromeOSUser(
    const std::string& username_hash,
    base::OnceCallback<void(ScopedPK11Slot)> callback) WARN_UNUSED_RESULT;

// Closes the NSS DB for |username_hash| that was previously opened by the
// *Initialize*ForChromeOSUser functions.
CRYPTO_EXPORT void CloseChromeOSUserForTesting(
    const std::string& username_hash);

// Sets the slot which should be used as private slot for the next
// |InitializePrivateSoftwareSlotForChromeOSUser| called. This is intended for
// simulating a separate private slot in Chrome OS browser tests.
// As a sanity check, it is recommended to check that the private slot of the
// profile's certificate database is set to |slot| when the profile is
// available, because |slot| will be used as private slot for whichever profile
// is initialized next.
CRYPTO_EXPORT void SetPrivateSoftwareSlotForChromeOSUserForTesting(
    ScopedPK11Slot slot);

#endif  // defined(OS_CHROMEOS)

// Loads the given module for this NSS session.
SECMODModule* LoadNSSModule(const char* name,
                            const char* library_path,
                            const char* params);

// Returns the current NSS error message.
std::string GetNSSErrorMessage();

}  // namespace crypto

#endif  // CRYPTO_NSS_UTIL_INTERNAL_H_
