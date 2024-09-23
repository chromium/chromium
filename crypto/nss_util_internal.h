// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_NSS_UTIL_INTERNAL_H_
#define CRYPTO_NSS_UTIL_INTERNAL_H_

#include <secmodt.h>

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "components/nacl/common/buildflags.h"
#include "crypto/crypto_export.h"
#include "crypto/scoped_nss_types.h"

namespace base {
class FilePath;
}

// These functions return a type defined in an NSS header, and so cannot be
// declared in nss_util.h.  Hence, they are declared here.

namespace crypto {

// Opens an NSS software database in folder `path`, with the (potentially)
// user-visible description `description`. Returns the slot for the opened
// database, or nullptr if the database could not be opened. Can be called
// multiple times for the same `path`, thread-safe.
CRYPTO_EXPORT ScopedPK11Slot OpenSoftwareNSSDB(const base::FilePath& path,
                                               const std::string& description);

// Closes the underlying database for the `slot`. All remaining slots
// referencing the same database will remain valid objects, but won't be able to
// successfully retrieve certificates, etc. Should be used for all databases
// that were opened with `OpenSoftwareNSSDB` (instead of `SECMOD_CloseUserDB`).
// Can be called multiple times. Returns `SECSuccess` if the database was
// successfully closed, returns `SECFailure` if it was never opened, was already
// closed by an earlier call, or failed to close. Thread-safe.
CRYPTO_EXPORT SECStatus CloseSoftwareNSSDB(PK11SlotInfo* slot);

// A helper class that acquires the SECMOD list read lock while the
// AutoSECMODListReadLock is in scope.
class CRYPTO_EXPORT AutoSECMODListReadLock {
 public:
  AutoSECMODListReadLock();

  AutoSECMODListReadLock(const AutoSECMODListReadLock&) = delete;
  AutoSECMODListReadLock& operator=(const AutoSECMODListReadLock&) = delete;

  ~AutoSECMODListReadLock();

 private:
  raw_ptr<SECMODListLock> lock_;
};

#if BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_MINIMAL_TOOLCHAIN)
// Returns path to the NSS database file in the provided profile
// directory.
CRYPTO_EXPORT base::FilePath GetSoftwareNSSDBPath(
    const base::FilePath& profile_directory_path);

// Returns a reference to the system-wide TPM slot (or nullptr if it will never
// be loaded).
CRYPTO_EXPORT void GetSystemNSSKeySlot(
    base::OnceCallback<void(ScopedPK11Slot)> callback);

// Injects the given |slot| as a system slot set by the future
// |InitializeTPMTokenAndSystemSlot| call.
CRYPTO_EXPORT void PrepareSystemSlotForTesting(ScopedPK11Slot slot);

// Attempt to unset the testing system slot.
// Note: After this method is called, the system is in an undefined state; it is
// NOT possible to call `PrepareSystemSlotForTesting()` and have it return to a
// known-good state. The primary purpose is to attempt to release system
// resources, such as file handles, to allow the cleanup of files on disk, but
// because of the process-wide effect, it's not possible to unwind any/all
// initialization that depended on this previously-configured system slot.
CRYPTO_EXPORT void ResetSystemSlotForTesting();

// Reset the global ChromeOSTokenManager. This is used between tests, so
// tests that run in the same process won't hit DCHECKS because they have
// different BrowserIO threads.
CRYPTO_EXPORT void ResetTokenManagerForTesting();

// Prepare per-user NSS slot mapping. It is safe to call this function multiple
// times. Returns true if the user was added, or false if it already existed.
// Loads the database from `path` to use as a public slot.
CRYPTO_EXPORT bool InitializeNSSForChromeOSUser(
    const std::string& username_hash,
    const base::FilePath& path);

// Prepare per-user NSS slot mapping. It is safe to call this function multiple
// times. Returns true if the user was added, or false if it already existed.
CRYPTO_EXPORT bool InitializeNSSForChromeOSUserWithSlot(
    const std::string& username_hash,
    ScopedPK11Slot public_slot);

// Returns whether TPM for ChromeOS user still needs initialization. If
// true is returned, the caller can proceed to initialize TPM slot for the
// user, but should call |WillInitializeTPMForChromeOSUser| first.
// |InitializeNSSForChromeOSUser| must have been called first.
[[nodiscard]] CRYPTO_EXPORT bool ShouldInitializeTPMForChromeOSUser(
    const std::string& username_hash);

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
[[nodiscard]] CRYPTO_EXPORT ScopedPK11Slot
GetPublicSlotForChromeOSUser(const std::string& username_hash);

// Returns the private slot for |username_hash| if it is loaded. If it is not
// loaded and |callback| is non-null, the |callback| will be run once the slot
// is loaded.
[[nodiscard]] CRYPTO_EXPORT ScopedPK11Slot GetPrivateSlotForChromeOSUser(
    const std::string& username_hash,
    base::OnceCallback<void(ScopedPK11Slot)> callback);

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

#endif  // BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_MINIMAL_TOOLCHAIN)

// Loads the given module for this NSS session.
SECMODModule* LoadNSSModule(const char* name,
                            const char* library_path,
                            const char* params);

// Returns the current NSS error message.
std::string GetNSSErrorMessage();

}  // namespace crypto

#endif  // CRYPTO_NSS_UTIL_INTERNAL_H_
