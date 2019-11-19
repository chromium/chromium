// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/nss_util.h"

#include <nss.h>
#include <pk11pub.h>
#include <plarena.h>
#include <prerror.h>
#include <prinit.h>
#include <prtime.h>
#include <secmod.h>

#include <memory>
#include <utility>

#include "base/base_paths.h"
#include "base/debug/alias.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "crypto/nss_crypto_module_delegate.h"
#include "crypto/nss_util_internal.h"

namespace crypto {

namespace {

#if defined(OS_CHROMEOS)
// Fake certificate authority database used for testing.
static const base::FilePath::CharType kReadOnlyCertDB[] =
    FILE_PATH_LITERAL("/etc/fake_root_ca/nssdb");
#endif  // defined(OS_CHROMEOS)

#if !defined(OS_CHROMEOS)
base::FilePath GetDefaultConfigDirectory() {
  base::FilePath dir;
  base::PathService::Get(base::DIR_HOME, &dir);
  if (dir.empty()) {
    LOG(ERROR) << "Failed to get home directory.";
    return dir;
  }
  dir = dir.AppendASCII(".pki").AppendASCII("nssdb");
  if (!base::CreateDirectory(dir)) {
    LOG(ERROR) << "Failed to create " << dir.value() << " directory.";
    dir.clear();
  }
  DVLOG(2) << "DefaultConfigDirectory: " << dir.value();
  return dir;
}
#endif  // !defined(OS_CHROMEOS)

// On non-Chrome OS platforms, return the default config directory. On Chrome OS
// test images, return a read-only directory with fake root CA certs (which are
// used by the local Google Accounts server mock we use when testing our login
// code). On Chrome OS non-test images (where the read-only directory doesn't
// exist), return an empty path.
base::FilePath GetInitialConfigDirectory() {
#if defined(OS_CHROMEOS)
  base::FilePath database_dir = base::FilePath(kReadOnlyCertDB);
  if (!base::PathExists(database_dir))
    database_dir.clear();
  return database_dir;
#else
  return GetDefaultConfigDirectory();
#endif  // defined(OS_CHROMEOS)
}

// This callback for NSS forwards all requests to a caller-specified
// CryptoModuleBlockingPasswordDelegate object.
char* PKCS11PasswordFunc(PK11SlotInfo* slot, PRBool retry, void* arg) {
  crypto::CryptoModuleBlockingPasswordDelegate* delegate =
      reinterpret_cast<crypto::CryptoModuleBlockingPasswordDelegate*>(arg);
  if (delegate) {
    bool cancelled = false;
    std::string password = delegate->RequestPassword(PK11_GetTokenName(slot),
                                                     retry != PR_FALSE,
                                                     &cancelled);
    if (cancelled)
      return nullptr;
    char* result = PORT_Strdup(password.c_str());
    password.replace(0, password.size(), password.size(), 0);
    return result;
  }
  DLOG(ERROR) << "PK11 password requested with nullptr arg";
  return nullptr;
}

// A singleton to initialize/deinitialize NSPR.
// Separate from the NSS singleton because we initialize NSPR on the UI thread.
// Now that we're leaking the singleton, we could merge back with the NSS
// singleton.
class NSPRInitSingleton {
 private:
  friend struct base::LazyInstanceTraitsBase<NSPRInitSingleton>;

  NSPRInitSingleton() {
    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
  }

  // NOTE(willchan): We don't actually cleanup on destruction since we leak NSS
  // to prevent non-joinable threads from using NSS after it's already been
  // shut down.
  ~NSPRInitSingleton() = delete;
};

base::LazyInstance<NSPRInitSingleton>::Leaky
    g_nspr_singleton = LAZY_INSTANCE_INITIALIZER;

// Force a crash with error info on NSS_NoDB_Init failure.
void CrashOnNSSInitFailure() {
  int nss_error = PR_GetError();
  int os_error = PR_GetOSError();
  base::debug::Alias(&nss_error);
  base::debug::Alias(&os_error);
  LOG(ERROR) << "Error initializing NSS without a persistent database: "
             << GetNSSErrorMessage();
  LOG(FATAL) << "nss_error=" << nss_error << ", os_error=" << os_error;
}

class NSSInitSingleton {
 private:
  friend struct base::LazyInstanceTraitsBase<NSSInitSingleton>;

  NSSInitSingleton() {
    // Initializing NSS causes us to do blocking IO.
    // Temporarily allow it until we fix
    //   http://code.google.com/p/chromium/issues/detail?id=59847
    base::ThreadRestrictions::ScopedAllowIO allow_io;

    EnsureNSPRInit();

    // We *must* have NSS >= 3.26 at compile time.
    static_assert((NSS_VMAJOR == 3 && NSS_VMINOR >= 26) || (NSS_VMAJOR > 3),
                  "nss version check failed");
    // Also check the run-time NSS version.
    // NSS_VersionCheck is a >= check, not strict equality.
    if (!NSS_VersionCheck("3.26")) {
      LOG(FATAL) << "NSS_VersionCheck(\"3.26\") failed. NSS >= 3.26 is "
                    "required. Please upgrade to the latest NSS, and if you "
                    "still get this error, contact your distribution "
                    "maintainer.";
    }

    SECStatus status = SECFailure;
    base::FilePath database_dir = GetInitialConfigDirectory();
    if (!database_dir.empty()) {
      // Initialize with a persistent database (likely, ~/.pki/nssdb).
      // Use "sql:" which can be shared by multiple processes safely.
      std::string nss_config_dir =
          base::StringPrintf("sql:%s", database_dir.value().c_str());
#if defined(OS_CHROMEOS)
      status = NSS_Init(nss_config_dir.c_str());
#else
      status = NSS_InitReadWrite(nss_config_dir.c_str());
#endif
      if (status != SECSuccess) {
        LOG(ERROR) << "Error initializing NSS with a persistent "
                      "database (" << nss_config_dir
                   << "): " << GetNSSErrorMessage();
      }
    }
    if (status != SECSuccess) {
      VLOG(1) << "Initializing NSS without a persistent database.";
      status = NSS_NoDB_Init(nullptr);
      if (status != SECSuccess) {
        CrashOnNSSInitFailure();
        return;
      }
    }

    PK11_SetPasswordFunc(PKCS11PasswordFunc);

    // If we haven't initialized the password for the NSS databases,
    // initialize an empty-string password so that we don't need to
    // log in.
    PK11SlotInfo* slot = PK11_GetInternalKeySlot();
    if (slot) {
      // PK11_InitPin may write to the keyDB, but no other thread can use NSS
      // yet, so we don't need to lock.
      if (PK11_NeedUserInit(slot))
        PK11_InitPin(slot, nullptr, nullptr);
      PK11_FreeSlot(slot);
    }

    // Load nss's built-in root certs.
    //
    // TODO(mattm): DCHECK this succeeded when crbug.com/310972 is fixed.
    // Failing to load root certs will it hard to talk to anybody via https.
    LoadNSSModule("Root Certs", "libnssckbi.so", nullptr);

    // Disable MD5 certificate signatures. (They are disabled by default in
    // NSS 3.14.)
    NSS_SetAlgorithmPolicy(SEC_OID_MD5, 0, NSS_USE_ALG_IN_CERT_SIGNATURE);
    NSS_SetAlgorithmPolicy(SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION,
                           0, NSS_USE_ALG_IN_CERT_SIGNATURE);
  }

  // NOTE(willchan): We don't actually cleanup on destruction since we leak NSS
  // to prevent non-joinable threads from using NSS after it's already been
  // shut down.
  ~NSSInitSingleton() = delete;
};

base::LazyInstance<NSSInitSingleton>::Leaky
    g_nss_singleton = LAZY_INSTANCE_INITIALIZER;
}  // namespace

ScopedPK11Slot OpenSoftwareNSSDB(const base::FilePath& path,
                                 const std::string& description) {
  const std::string modspec =
      base::StringPrintf("configDir='sql:%s' tokenDescription='%s'",
                         path.value().c_str(),
                         description.c_str());
  PK11SlotInfo* db_slot = SECMOD_OpenUserDB(modspec.c_str());
  if (db_slot) {
    if (PK11_NeedUserInit(db_slot))
      PK11_InitPin(db_slot, nullptr, nullptr);
  } else {
    LOG(ERROR) << "Error opening persistent database (" << modspec
               << "): " << GetNSSErrorMessage();
  }
  return ScopedPK11Slot(db_slot);
}

void EnsureNSPRInit() {
  g_nspr_singleton.Get();
}

void EnsureNSSInit() {
  g_nss_singleton.Get();
}

bool CheckNSSVersion(const char* version) {
  return !!NSS_VersionCheck(version);
}

AutoSECMODListReadLock::AutoSECMODListReadLock()
      : lock_(SECMOD_GetDefaultModuleListLock()) {
    SECMOD_GetReadLock(lock_);
  }

AutoSECMODListReadLock::~AutoSECMODListReadLock() {
  SECMOD_ReleaseReadLock(lock_);
}

base::Time PRTimeToBaseTime(PRTime prtime) {
  return base::Time::FromInternalValue(
      prtime + base::Time::UnixEpoch().ToInternalValue());
}

PRTime BaseTimeToPRTime(base::Time time) {
  return time.ToInternalValue() - base::Time::UnixEpoch().ToInternalValue();
}

SECMODModule* LoadNSSModule(const char* name,
                            const char* library_path,
                            const char* params) {
  std::string modparams =
      base::StringPrintf("name=\"%s\" library=\"%s\" %s", name, library_path,
                         params ? params : "");

  // Shouldn't need to const_cast here, but SECMOD doesn't properly declare
  // input string arguments as const.  Bug
  // https://bugzilla.mozilla.org/show_bug.cgi?id=642546 was filed on NSS
  // codebase to address this.
  SECMODModule* module = SECMOD_LoadUserModule(
      const_cast<char*>(modparams.c_str()), nullptr, PR_FALSE);
  if (!module) {
    LOG(ERROR) << "Error loading " << name
               << " module into NSS: " << GetNSSErrorMessage();
    return nullptr;
  }
  if (!module->loaded) {
    LOG(ERROR) << "After loading " << name
               << ", loaded==false: " << GetNSSErrorMessage();
    SECMOD_DestroyModule(module);
    return nullptr;
  }
  return module;
}

std::string GetNSSErrorMessage() {
  std::string result;
  if (PR_GetErrorTextLength()) {
    std::unique_ptr<char[]> error_text(new char[PR_GetErrorTextLength() + 1]);
    PRInt32 copied = PR_GetErrorText(error_text.get());
    result = std::string(error_text.get(), copied);
  } else {
    result = base::StringPrintf("NSS error code: %d", PR_GetError());
  }
  return result;
}

}  // namespace crypto
