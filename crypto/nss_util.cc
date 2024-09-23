// Copyright 2012 The Chromium Authors
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
#include "base/containers/flat_map.h"
#include "base/containers/heap_array.h"
#include "base/debug/alias.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "crypto/nss_crypto_module_delegate.h"
#include "crypto/nss_util_internal.h"

namespace crypto {

namespace {

#if !(BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS))
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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)

// On non-Chrome OS platforms, return the default config directory. On Chrome
// OS return a empty path which will result in NSS being initialized without a
// persistent database.
base::FilePath GetInitialConfigDirectory() {
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  return base::FilePath();
#else
  return GetDefaultConfigDirectory();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

// This callback for NSS forwards all requests to a caller-specified
// CryptoModuleBlockingPasswordDelegate object.
char* PKCS11PasswordFunc(PK11SlotInfo* slot, PRBool retry, void* arg) {
  crypto::CryptoModuleBlockingPasswordDelegate* delegate =
      reinterpret_cast<crypto::CryptoModuleBlockingPasswordDelegate*>(arg);
  if (delegate) {
    bool cancelled = false;
    std::string password = delegate->RequestPassword(
        PK11_GetTokenName(slot), retry != PR_FALSE, &cancelled);
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

  NSPRInitSingleton() { PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0); }

  // NOTE(willchan): We don't actually cleanup on destruction since we leak NSS
  // to prevent non-joinable threads from using NSS after it's already been
  // shut down.
  ~NSPRInitSingleton() = delete;
};

base::LazyInstance<NSPRInitSingleton>::Leaky g_nspr_singleton =
    LAZY_INSTANCE_INITIALIZER;

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
 public:
  // NOTE(willchan): We don't actually cleanup on destruction since we leak NSS
  // to prevent non-joinable threads from using NSS after it's already been
  // shut down.
  ~NSSInitSingleton() = delete;

  ScopedPK11Slot OpenSoftwareNSSDB(const base::FilePath& path,
                                   const std::string& description) {
    base::AutoLock lock(slot_map_lock_);

    auto slot_map_iter = slot_map_.find(path);
    if (slot_map_iter != slot_map_.end()) {
      // PK11_ReferenceSlot returns a new PK11Slot instance which refers
      // to the same slot.
      return ScopedPK11Slot(PK11_ReferenceSlot(slot_map_iter->second.get()));
    }

    const std::string modspec =
        base::StringPrintf("configDir='sql:%s' tokenDescription='%s'",
                           path.value().c_str(), description.c_str());

    // TODO(crbug.com/1163303): Presumably there's a race condition with
    // session_manager around creating/opening the software NSS database. The
    // retry loop is a temporary workaround that should at least reduce the
    // amount of failures until a proper fix is implemented.
    PK11SlotInfo* db_slot_info = nullptr;
    int attempts_counter = 0;
    for (; !db_slot_info && (attempts_counter < 10); ++attempts_counter) {
      db_slot_info = SECMOD_OpenUserDB(modspec.c_str());
    }
    if (db_slot_info && (attempts_counter > 1)) {
      LOG(ERROR) << "Opening persistent database failed "
                 << attempts_counter - 1 << " times before succeeding";
    }

    if (db_slot_info) {
      if (PK11_NeedUserInit(db_slot_info))
        PK11_InitPin(db_slot_info, nullptr, nullptr);
      slot_map_[path] = ScopedPK11Slot(PK11_ReferenceSlot(db_slot_info));
    } else {
      LOG(ERROR) << "Error opening persistent database (" << modspec
                 << "): " << GetNSSErrorMessage();
#if BUILDFLAG(IS_CHROMEOS_ASH)
      DiagnosePublicSlotAndCrash(path);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    }

    return ScopedPK11Slot(db_slot_info);
  }

  SECStatus CloseSoftwareNSSDB(PK11SlotInfo* slot) {
    if (!slot) {
      return SECFailure;
    }

    base::AutoLock lock(slot_map_lock_);
    CK_SLOT_ID slot_id = PK11_GetSlotID(slot);
    for (auto const& [stored_path, stored_slot] : slot_map_) {
      if (PK11_GetSlotID(stored_slot.get()) == slot_id) {
        slot_map_.erase(stored_path);
        return SECMOD_CloseUserDB(slot);
      }
    }
    return SECFailure;
  }

 private:
  friend struct base::LazyInstanceTraitsBase<NSSInitSingleton>;

  NSSInitSingleton() {
    // Initializing NSS causes us to do blocking IO.
    // Temporarily allow it until we fix
    //   http://code.google.com/p/chromium/issues/detail?id=59847
    ScopedAllowBlockingForNSS allow_blocking;

    EnsureNSPRInit();

    // We *must* have NSS >= 3.35 at compile time.
    static_assert((NSS_VMAJOR == 3 && NSS_VMINOR >= 35) || (NSS_VMAJOR > 3),
                  "nss version check failed");
    // Also check the run-time NSS version.
    // NSS_VersionCheck is a >= check, not strict equality.
    if (!NSS_VersionCheck("3.35")) {
      LOG(FATAL) << "NSS_VersionCheck(\"3.35\") failed. NSS >= 3.35 is "
                    "required. Please upgrade to the latest NSS, and if you "
                    "still get this error, contact your distribution "
                    "maintainer.";
    }

    SECStatus status = SECFailure;
    base::FilePath database_dir = GetInitialConfigDirectory();
    // In MSAN, all loaded libraries needs to be instrumented. But the user
    // config may reference an uninstrumented module, so load NSS without cert
    // DBs instead. Tests should ideally be run under
    // testing/run_with_dummy_home.py to eliminate dependencies on user
    // configuration, but the bots are not currently configured to do so. This
    // workaround may be removed if/when the bots use run_with_dummy_home.py.
#if !defined(MEMORY_SANITIZER)
    if (!database_dir.empty()) {
      // Initialize with a persistent database (likely, ~/.pki/nssdb).
      // Use "sql:" which can be shared by multiple processes safely.
      std::string nss_config_dir =
          base::StringPrintf("sql:%s", database_dir.value().c_str());
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
      status = NSS_Init(nss_config_dir.c_str());
#else
      status = NSS_InitReadWrite(nss_config_dir.c_str());
#endif
      if (status != SECSuccess) {
        LOG(ERROR) << "Error initializing NSS with a persistent "
                      "database ("
                   << nss_config_dir << "): " << GetNSSErrorMessage();
      }
    }
#endif  // !defined(MEMORY_SANITIZER)
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
    NSS_SetAlgorithmPolicy(SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION, 0,
                           NSS_USE_ALG_IN_CERT_SIGNATURE);
  }

  // Stores opened software NSS databases.
  base::flat_map<base::FilePath, /*slot=*/ScopedPK11Slot> slot_map_
      GUARDED_BY(slot_map_lock_);
  // Ensures thread-safety for the methods that modify slot_map_.
  // Performance considerations:
  // Opening/closing a database is a rare operation in Chrome. Actually opening
  // a database is a blocking I/O operation. Chrome doesn't open a lot of
  // different databases in parallel. So, waiting for another thread to finish
  // opening a database and (almost certainly) reusing the result is comparable
  // to opening the same database twice in parallel (but the latter is not
  // supported by NSS).
  base::Lock slot_map_lock_;
};

base::LazyInstance<NSSInitSingleton>::Leaky g_nss_singleton =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

ScopedPK11Slot OpenSoftwareNSSDB(const base::FilePath& path,
                                 const std::string& description) {
  return g_nss_singleton.Get().OpenSoftwareNSSDB(path, description);
}

SECStatus CloseSoftwareNSSDB(PK11SlotInfo* slot) {
  return g_nss_singleton.Get().CloseSoftwareNSSDB(slot);
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
    auto error_text =
        base::HeapArray<char>::Uninit(PR_GetErrorTextLength() + 1);
    PRInt32 copied = PR_GetErrorText(error_text.data());
    result = std::string(error_text.data(), copied);
  } else {
    result = base::StringPrintf("NSS error code: %d", PR_GetError());
  }
  return result;
}

}  // namespace crypto
