// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/nss_util.h"

#include <dlfcn.h>
#include <nss.h>
#include <pk11pub.h>
#include <plarena.h>
#include <prerror.h>
#include <prinit.h>
#include <prtime.h>
#include <secmod.h>

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/debug/stack_trace.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "crypto/nss_util_internal.h"

namespace crypto {

namespace {

const char kUserNSSDatabaseName[] = "UserNSSDB";

// Constants for loading the Chrome OS TPM-backed PKCS #11 library.
const char kChapsModuleName[] = "Chaps";
const char kChapsPath[] = "libchaps.so";

class ChromeOSUserData {
 public:
  explicit ChromeOSUserData(ScopedPK11Slot public_slot)
      : public_slot_(std::move(public_slot)),
        private_slot_initialization_started_(false) {}
  ~ChromeOSUserData() {
    if (public_slot_) {
      SECStatus status = SECMOD_CloseUserDB(public_slot_.get());
      if (status != SECSuccess)
        PLOG(ERROR) << "SECMOD_CloseUserDB failed: " << PORT_GetError();
    }
  }

  ScopedPK11Slot GetPublicSlot() {
    return ScopedPK11Slot(public_slot_ ? PK11_ReferenceSlot(public_slot_.get())
                                       : nullptr);
  }

  ScopedPK11Slot GetPrivateSlot(
      base::OnceCallback<void(ScopedPK11Slot)> callback) {
    if (private_slot_)
      return ScopedPK11Slot(PK11_ReferenceSlot(private_slot_.get()));
    if (!callback.is_null())
      tpm_ready_callback_list_.push_back(std::move(callback));
    return ScopedPK11Slot();
  }

  void SetPrivateSlot(ScopedPK11Slot private_slot) {
    DCHECK(!private_slot_);
    private_slot_ = std::move(private_slot);

    SlotReadyCallbackList callback_list;
    callback_list.swap(tpm_ready_callback_list_);
    for (SlotReadyCallbackList::iterator i = callback_list.begin();
         i != callback_list.end(); ++i) {
      std::move(*i).Run(
          ScopedPK11Slot(PK11_ReferenceSlot(private_slot_.get())));
    }
  }

  bool private_slot_initialization_started() const {
    return private_slot_initialization_started_;
  }

  void set_private_slot_initialization_started() {
    private_slot_initialization_started_ = true;
  }

 private:
  ScopedPK11Slot public_slot_;
  ScopedPK11Slot private_slot_;

  bool private_slot_initialization_started_;

  typedef std::vector<base::OnceCallback<void(ScopedPK11Slot)>>
      SlotReadyCallbackList;
  SlotReadyCallbackList tpm_ready_callback_list_;
};

class ScopedChapsLoadFixup {
 public:
  ScopedChapsLoadFixup();
  ~ScopedChapsLoadFixup();

 private:
#if defined(COMPONENT_BUILD)
  void* chaps_handle_;
#endif
};

#if defined(COMPONENT_BUILD)

ScopedChapsLoadFixup::ScopedChapsLoadFixup() {
  // HACK: libchaps links the system protobuf and there are symbol conflicts
  // with the bundled copy. Load chaps with RTLD_DEEPBIND to workaround.
  chaps_handle_ = dlopen(kChapsPath, RTLD_LOCAL | RTLD_NOW | RTLD_DEEPBIND);
}

ScopedChapsLoadFixup::~ScopedChapsLoadFixup() {
  // LoadNSSModule() will have taken a 2nd reference.
  if (chaps_handle_)
    dlclose(chaps_handle_);
}

#else

ScopedChapsLoadFixup::ScopedChapsLoadFixup() {}
ScopedChapsLoadFixup::~ScopedChapsLoadFixup() {}

#endif  // defined(COMPONENT_BUILD)

class ChromeOSTokenManager {
 public:
  // Used with PostTaskAndReply to pass handles to worker thread and back.
  struct TPMModuleAndSlot {
    explicit TPMModuleAndSlot(SECMODModule* init_chaps_module)
        : chaps_module(init_chaps_module) {}
    SECMODModule* chaps_module;
    crypto::ScopedPK11Slot tpm_slot;
  };

  ScopedPK11Slot OpenPersistentNSSDBForPath(const std::string& db_name,
                                            const base::FilePath& path) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    // NSS is allowed to do IO on the current thread since dispatching
    // to a dedicated thread would still have the affect of blocking
    // the current thread, due to NSS's internal locking requirements
    base::ThreadRestrictions::ScopedAllowIO allow_io;

    base::FilePath nssdb_path = path.AppendASCII(".pki").AppendASCII("nssdb");
    if (!base::CreateDirectory(nssdb_path)) {
      LOG(ERROR) << "Failed to create " << nssdb_path.value() << " directory.";
      return ScopedPK11Slot();
    }
    return OpenSoftwareNSSDB(nssdb_path, db_name);
  }

  void EnableTPMTokenForNSS() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    // If this gets set, then we'll use the TPM for certs with
    // private keys, otherwise we'll fall back to the software
    // implementation.
    tpm_token_enabled_for_nss_ = true;
  }

  bool IsTPMTokenEnabledForNSS() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return tpm_token_enabled_for_nss_;
  }

  void InitializeTPMTokenAndSystemSlot(
      int system_slot_id,
      base::OnceCallback<void(bool)> callback) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    // Should not be called while there is already an initialization in
    // progress.
    DCHECK(!initializing_tpm_token_);
    // If EnableTPMTokenForNSS hasn't been called, return false.
    if (!tpm_token_enabled_for_nss_) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), false));
      return;
    }

    // If everything is already initialized, then return true.
    // Note that only |tpm_slot_| is checked, since |chaps_module_| could be
    // nullptr in tests while |tpm_slot_| has been set to the test DB.
    if (tpm_slot_) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), true));
      return;
    }

    // Note that a reference is not taken to chaps_module_. This is safe since
    // ChromeOSTokenManager is Leaky, so the reference it holds is never
    // released.
    std::unique_ptr<TPMModuleAndSlot> tpm_args(
        new TPMModuleAndSlot(chaps_module_));
    TPMModuleAndSlot* tpm_args_ptr = tpm_args.get();
    base::PostTaskAndReply(
        FROM_HERE,
        {base::ThreadPool(), base::MayBlock(),
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(&ChromeOSTokenManager::InitializeTPMTokenInThreadPool,
                       system_slot_id, tpm_args_ptr),
        base::BindOnce(
            &ChromeOSTokenManager::OnInitializedTPMTokenAndSystemSlot,
            base::Unretained(this),  // ChromeOSTokenManager is leaky
            std::move(callback), std::move(tpm_args)));
    initializing_tpm_token_ = true;
  }

  static void InitializeTPMTokenInThreadPool(CK_SLOT_ID token_slot_id,
                                             TPMModuleAndSlot* tpm_args) {
    // NSS functions may reenter //net via extension hooks. If the reentered
    // code needs to synchronously wait for a task to run but the thread pool in
    // which that task must run doesn't have enough threads to schedule it, a
    // deadlock occurs. To prevent that, the base::ScopedBlockingCall below
    // increments the thread pool capacity for the duration of the TPM
    // initialization.
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::WILL_BLOCK);

    if (!tpm_args->chaps_module) {
      ScopedChapsLoadFixup chaps_loader;

      DVLOG(3) << "Loading chaps...";
      tpm_args->chaps_module = LoadNSSModule(
          kChapsModuleName, kChapsPath,
          // For more details on these parameters, see:
          // https://developer.mozilla.org/en/PKCS11_Module_Specs
          // slotFlags=[PublicCerts] -- Certificates and public keys can be
          //   read from this slot without requiring a call to C_Login.
          // askpw=only -- Only authenticate to the token when necessary.
          "NSS=\"slotParams=(0={slotFlags=[PublicCerts] askpw=only})\"");
    }
    if (tpm_args->chaps_module) {
      tpm_args->tpm_slot =
          GetTPMSlotForIdInThreadPool(tpm_args->chaps_module, token_slot_id);
    }
  }

  void OnInitializedTPMTokenAndSystemSlot(
      base::OnceCallback<void(bool)> callback,
      std::unique_ptr<TPMModuleAndSlot> tpm_args) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DVLOG(2) << "Loaded chaps: " << !!tpm_args->chaps_module
             << ", got tpm slot: " << !!tpm_args->tpm_slot;

    chaps_module_ = tpm_args->chaps_module;
    tpm_slot_ = std::move(tpm_args->tpm_slot);
    if (!chaps_module_ && test_system_slot_) {
      // chromeos_unittests try to test the TPM initialization process. If we
      // have a test DB open, pretend that it is the TPM slot.
      tpm_slot_.reset(PK11_ReferenceSlot(test_system_slot_.get()));
    }
    initializing_tpm_token_ = false;

    if (tpm_slot_)
      RunAndClearTPMReadyCallbackList();

    std::move(callback).Run(!!tpm_slot_);
  }

  void RunAndClearTPMReadyCallbackList() {
    TPMReadyCallbackList callback_list;
    callback_list.swap(tpm_ready_callback_list_);
    for (TPMReadyCallbackList::iterator i = callback_list.begin();
         i != callback_list.end(); ++i) {
      std::move(*i).Run();
    }
  }

  bool IsTPMTokenReady(base::OnceClosure callback) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    if (tpm_slot_)
      return true;

    if (!callback.is_null())
      tpm_ready_callback_list_.push_back(std::move(callback));

    return false;
  }

  // Note that CK_SLOT_ID is an unsigned long, but cryptohome gives us the slot
  // id as an int. This should be safe since this is only used with chaps, which
  // we also control.
  static crypto::ScopedPK11Slot GetTPMSlotForIdInThreadPool(
      SECMODModule* chaps_module,
      CK_SLOT_ID slot_id) {
    DCHECK(chaps_module);

    DVLOG(3) << "Poking chaps module.";
    SECStatus rv = SECMOD_UpdateSlotList(chaps_module);
    if (rv != SECSuccess)
      PLOG(ERROR) << "SECMOD_UpdateSlotList failed: " << PORT_GetError();

    PK11SlotInfo* slot = SECMOD_LookupSlot(chaps_module->moduleID, slot_id);
    if (!slot)
      LOG(ERROR) << "TPM slot " << slot_id << " not found.";
    return crypto::ScopedPK11Slot(slot);
  }

  bool InitializeNSSForChromeOSUser(const std::string& username_hash,
                                    const base::FilePath& path) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    if (chromeos_user_map_.find(username_hash) != chromeos_user_map_.end()) {
      // This user already exists in our mapping.
      DVLOG(2) << username_hash << " already initialized.";
      return false;
    }

    DVLOG(2) << "Opening NSS DB " << path.value();
    std::string db_name = base::StringPrintf("%s %s", kUserNSSDatabaseName,
                                             username_hash.c_str());
    ScopedPK11Slot public_slot(OpenPersistentNSSDBForPath(db_name, path));
    chromeos_user_map_[username_hash] =
        std::make_unique<ChromeOSUserData>(std::move(public_slot));
    return true;
  }

  bool ShouldInitializeTPMForChromeOSUser(const std::string& username_hash) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(chromeos_user_map_.find(username_hash) != chromeos_user_map_.end());

    return !chromeos_user_map_[username_hash]
                ->private_slot_initialization_started();
  }

  void WillInitializeTPMForChromeOSUser(const std::string& username_hash) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(chromeos_user_map_.find(username_hash) != chromeos_user_map_.end());

    chromeos_user_map_[username_hash]
        ->set_private_slot_initialization_started();
  }

  void InitializeTPMForChromeOSUser(const std::string& username_hash,
                                    CK_SLOT_ID slot_id) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(chromeos_user_map_.find(username_hash) != chromeos_user_map_.end());
    DCHECK(chromeos_user_map_[username_hash]
               ->private_slot_initialization_started());

    if (!chaps_module_)
      return;

    // Note that a reference is not taken to chaps_module_. This is safe since
    // ChromeOSTokenManager is Leaky, so the reference it holds is never
    // released.
    std::unique_ptr<TPMModuleAndSlot> tpm_args(
        new TPMModuleAndSlot(chaps_module_));
    TPMModuleAndSlot* tpm_args_ptr = tpm_args.get();
    base::PostTaskAndReply(
        FROM_HERE,
        {base::ThreadPool(), base::MayBlock(),
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(&ChromeOSTokenManager::InitializeTPMTokenInThreadPool,
                       slot_id, tpm_args_ptr),
        base::BindOnce(&ChromeOSTokenManager::OnInitializedTPMForChromeOSUser,
                       base::Unretained(this),  // ChromeOSTokenManager is leaky
                       username_hash, std::move(tpm_args)));
  }

  void OnInitializedTPMForChromeOSUser(
      const std::string& username_hash,
      std::unique_ptr<TPMModuleAndSlot> tpm_args) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DVLOG(2) << "Got tpm slot for " << username_hash << " "
             << !!tpm_args->tpm_slot;
    chromeos_user_map_[username_hash]->SetPrivateSlot(
        std::move(tpm_args->tpm_slot));
  }

  void InitializePrivateSoftwareSlotForChromeOSUser(
      const std::string& username_hash) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    VLOG(1) << "using software private slot for " << username_hash;
    DCHECK(chromeos_user_map_.find(username_hash) != chromeos_user_map_.end());
    DCHECK(chromeos_user_map_[username_hash]
               ->private_slot_initialization_started());

    if (prepared_test_private_slot_) {
      chromeos_user_map_[username_hash]->SetPrivateSlot(
          std::move(prepared_test_private_slot_));
      return;
    }

    chromeos_user_map_[username_hash]->SetPrivateSlot(
        chromeos_user_map_[username_hash]->GetPublicSlot());
  }

  ScopedPK11Slot GetPublicSlotForChromeOSUser(
      const std::string& username_hash) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    if (username_hash.empty()) {
      DVLOG(2) << "empty username_hash";
      return ScopedPK11Slot();
    }

    if (chromeos_user_map_.find(username_hash) == chromeos_user_map_.end()) {
      LOG(ERROR) << username_hash << " not initialized.";
      return ScopedPK11Slot();
    }
    return chromeos_user_map_[username_hash]->GetPublicSlot();
  }

  ScopedPK11Slot GetPrivateSlotForChromeOSUser(
      const std::string& username_hash,
      base::OnceCallback<void(ScopedPK11Slot)> callback) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    if (username_hash.empty()) {
      DVLOG(2) << "empty username_hash";
      if (!callback.is_null()) {
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), ScopedPK11Slot()));
      }
      return ScopedPK11Slot();
    }

    DCHECK(chromeos_user_map_.find(username_hash) != chromeos_user_map_.end());

    return chromeos_user_map_[username_hash]->GetPrivateSlot(
        std::move(callback));
  }

  void CloseChromeOSUserForTesting(const std::string& username_hash) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    auto i = chromeos_user_map_.find(username_hash);
    DCHECK(i != chromeos_user_map_.end());
    chromeos_user_map_.erase(i);
  }

  void SetSystemKeySlotForTesting(ScopedPK11Slot slot) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    // Ensure that a previous value of test_system_slot_ is not overwritten.
    // Unsetting, i.e. setting a nullptr, however is allowed.
    DCHECK(!slot || !test_system_slot_);
    test_system_slot_ = std::move(slot);
    if (test_system_slot_) {
      tpm_slot_.reset(PK11_ReferenceSlot(test_system_slot_.get()));
      RunAndClearTPMReadyCallbackList();
    } else {
      tpm_slot_.reset();
    }
  }

  void SetSystemKeySlotWithoutInitializingTPMForTesting(ScopedPK11Slot slot) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    // Ensure that a previous value of test_system_slot_ is not overwritten.
    // Unsetting, i.e. setting a nullptr, however is allowed.
    DCHECK(!slot || !test_system_slot_);
    if (tpm_slot_ && tpm_slot_ == test_system_slot_) {
      // Unset |tpm_slot_| if it was initialized from |test_system_slot_|.
      tpm_slot_.reset();
    }
    test_system_slot_ = std::move(slot);
  }

  void SetPrivateSoftwareSlotForChromeOSUserForTesting(ScopedPK11Slot slot) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    // Ensure that a previous value of prepared_test_private_slot_ is not
    // overwritten. Unsetting, i.e. setting a nullptr, however is allowed.
    DCHECK(!slot || !prepared_test_private_slot_);
    prepared_test_private_slot_ = std::move(slot);
  }

  void GetSystemNSSKeySlotCallback(
      base::OnceCallback<void(ScopedPK11Slot)> callback) {
    std::move(callback).Run(
        ScopedPK11Slot(PK11_ReferenceSlot(tpm_slot_.get())));
  }

  ScopedPK11Slot GetSystemNSSKeySlot(
      base::OnceCallback<void(ScopedPK11Slot)> callback) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    // TODO(mattm): chromeos::TPMTokenloader always calls
    // InitializeTPMTokenAndSystemSlot with slot 0.  If the system slot is
    // disabled, tpm_slot_ will be the first user's slot instead. Can that be
    // detected and return nullptr instead?

    base::OnceClosure wrapped_callback;
    if (!callback.is_null()) {
      wrapped_callback = base::BindOnce(
          &ChromeOSTokenManager::GetSystemNSSKeySlotCallback,
          base::Unretained(this) /* singleton is leaky */, std::move(callback));
    }
    if (IsTPMTokenReady(std::move(wrapped_callback)))
      return ScopedPK11Slot(PK11_ReferenceSlot(tpm_slot_.get()));
    return ScopedPK11Slot();
  }

 private:
  friend struct base::LazyInstanceTraitsBase<ChromeOSTokenManager>;

  ChromeOSTokenManager() { EnsureNSSInit(); }

  // NOTE(willchan): We don't actually cleanup on destruction since we leak NSS
  // to prevent non-joinable threads from using NSS after it's already been
  // shut down.
  ~ChromeOSTokenManager() = delete;

  bool tpm_token_enabled_for_nss_ = false;
  bool initializing_tpm_token_ = false;
  using TPMReadyCallbackList = std::vector<base::OnceClosure>;
  TPMReadyCallbackList tpm_ready_callback_list_;
  SECMODModule* chaps_module_ = nullptr;
  crypto::ScopedPK11Slot tpm_slot_;
  std::map<std::string, std::unique_ptr<ChromeOSUserData>> chromeos_user_map_;
  ScopedPK11Slot test_system_slot_;
  ScopedPK11Slot prepared_test_private_slot_;

  THREAD_CHECKER(thread_checker_);
};

base::LazyInstance<ChromeOSTokenManager>::Leaky g_token_manager =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

ScopedPK11Slot GetSystemNSSKeySlot(
    base::OnceCallback<void(ScopedPK11Slot)> callback) {
  return g_token_manager.Get().GetSystemNSSKeySlot(std::move(callback));
}

void SetSystemKeySlotForTesting(ScopedPK11Slot slot) {
  g_token_manager.Get().SetSystemKeySlotForTesting(std::move(slot));
}

void SetSystemKeySlotWithoutInitializingTPMForTesting(ScopedPK11Slot slot) {
  g_token_manager.Get().SetSystemKeySlotWithoutInitializingTPMForTesting(
      std::move(slot));
}

void EnableTPMTokenForNSS() {
  g_token_manager.Get().EnableTPMTokenForNSS();
}

bool IsTPMTokenEnabledForNSS() {
  return g_token_manager.Get().IsTPMTokenEnabledForNSS();
}

bool IsTPMTokenReady(base::OnceClosure callback) {
  return g_token_manager.Get().IsTPMTokenReady(std::move(callback));
}

void InitializeTPMTokenAndSystemSlot(int token_slot_id,
                                     base::OnceCallback<void(bool)> callback) {
  g_token_manager.Get().InitializeTPMTokenAndSystemSlot(token_slot_id,
                                                        std::move(callback));
}

bool InitializeNSSForChromeOSUser(const std::string& username_hash,
                                  const base::FilePath& path) {
  return g_token_manager.Get().InitializeNSSForChromeOSUser(username_hash,
                                                            path);
}

bool ShouldInitializeTPMForChromeOSUser(const std::string& username_hash) {
  return g_token_manager.Get().ShouldInitializeTPMForChromeOSUser(
      username_hash);
}

void WillInitializeTPMForChromeOSUser(const std::string& username_hash) {
  g_token_manager.Get().WillInitializeTPMForChromeOSUser(username_hash);
}

void InitializeTPMForChromeOSUser(const std::string& username_hash,
                                  CK_SLOT_ID slot_id) {
  g_token_manager.Get().InitializeTPMForChromeOSUser(username_hash, slot_id);
}

void InitializePrivateSoftwareSlotForChromeOSUser(
    const std::string& username_hash) {
  g_token_manager.Get().InitializePrivateSoftwareSlotForChromeOSUser(
      username_hash);
}

ScopedPK11Slot GetPublicSlotForChromeOSUser(const std::string& username_hash) {
  return g_token_manager.Get().GetPublicSlotForChromeOSUser(username_hash);
}

ScopedPK11Slot GetPrivateSlotForChromeOSUser(
    const std::string& username_hash,
    base::OnceCallback<void(ScopedPK11Slot)> callback) {
  return g_token_manager.Get().GetPrivateSlotForChromeOSUser(
      username_hash, std::move(callback));
}

void CloseChromeOSUserForTesting(const std::string& username_hash) {
  g_token_manager.Get().CloseChromeOSUserForTesting(username_hash);
}

void SetPrivateSoftwareSlotForChromeOSUserForTesting(ScopedPK11Slot slot) {
  g_token_manager.Get().SetPrivateSoftwareSlotForChromeOSUserForTesting(
      std::move(slot));
}

}  // namespace crypto
