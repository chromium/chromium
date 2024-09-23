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

#include <map>
#include <memory>
#include <utility>

#include "base/callback_list.h"
#include "base/containers/contains.h"
#include "base/debug/stack_trace.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/not_fatal_until.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "crypto/chaps_support.h"
#include "crypto/nss_util_internal.h"

namespace crypto {

namespace {

const char kUserNSSDatabaseName[] = "UserNSSDB";

class ChromeOSUserData {
 public:
  using SlotReadyCallback = base::OnceCallback<void(ScopedPK11Slot)>;

  explicit ChromeOSUserData(ScopedPK11Slot public_slot)
      : public_slot_(std::move(public_slot)) {}

  ~ChromeOSUserData() {
    if (public_slot_) {
      SECStatus status = CloseSoftwareNSSDB(public_slot_.get());
      if (status != SECSuccess)
        PLOG(ERROR) << "CloseSoftwareNSSDB failed: " << PORT_GetError();
    }
  }

  ScopedPK11Slot GetPublicSlot() {
    return ScopedPK11Slot(public_slot_ ? PK11_ReferenceSlot(public_slot_.get())
                                       : nullptr);
  }

  ScopedPK11Slot GetPrivateSlot(SlotReadyCallback callback) {
    if (private_slot_)
      return ScopedPK11Slot(PK11_ReferenceSlot(private_slot_.get()));
    if (!callback.is_null()) {
      // Callback lists cannot hold callbacks that take move-only args, since
      // Notify()ing such a list would move the arg into the first callback,
      // leaving it null or unspecified for remaining callbacks.  Instead, adapt
      // the provided callbacks to accept a raw pointer, which can be copied,
      // and then wrap in a separate scoping object for each callback.
      tpm_ready_callback_list_.AddUnsafe(base::BindOnce(
          [](SlotReadyCallback callback, PK11SlotInfo* info) {
            std::move(callback).Run(ScopedPK11Slot(PK11_ReferenceSlot(info)));
          },
          std::move(callback)));
    }
    return ScopedPK11Slot();
  }

  void SetPrivateSlot(ScopedPK11Slot private_slot) {
    DCHECK(!private_slot_);
    private_slot_ = std::move(private_slot);
    tpm_ready_callback_list_.Notify(private_slot_.get());
  }

  bool private_slot_initialization_started() const {
    return private_slot_initialization_started_;
  }

  void set_private_slot_initialization_started() {
    private_slot_initialization_started_ = true;
  }

 private:
  using SlotReadyCallbackList = base::OnceCallbackList<void(PK11SlotInfo*)>;

  ScopedPK11Slot public_slot_;
  ScopedPK11Slot private_slot_;

  bool private_slot_initialization_started_ = false;

  SlotReadyCallbackList tpm_ready_callback_list_;
};

// Contains state used for the ChromeOSTokenManager. Unlike the
// ChromeOSTokenManager, which is thread-checked, this object may live
// and be accessed on multiple threads. While this is normally dangerous,
// this is done to support callers initializing early in process startup,
// where the threads using the objects may not be created yet, and the
// thread startup may depend on these objects.
// Put differently: They may be written to from any thread, if, and only
// if, the thread they will be read from has not yet been created;
// otherwise, this should be treated as thread-affine/thread-hostile.
struct ChromeOSTokenManagerDataForTesting {
  static ChromeOSTokenManagerDataForTesting& GetInstance() {
    static base::NoDestructor<ChromeOSTokenManagerDataForTesting> instance;
    return *instance;
  }

  // System slot that will be used for the system slot initialization.
  ScopedPK11Slot test_system_slot;
};

class ChromeOSTokenManager {
 public:
  enum class State {
    // Initial state.
    kInitializationNotStarted,
    // Initialization of the TPM token was started.
    kInitializationStarted,
    // TPM token was successfully initialized, but not available to the class'
    // users yet.
    kTpmTokenInitialized,
    // TPM token was successfully enabled. It is a final state.
    kTpmTokenEnabled,
    // TPM token will never be enabled. It is a final state.
    kTpmTokenDisabled,
  };

  // Used with PostTaskAndReply to pass handles to worker thread and back.
  struct TPMModuleAndSlot {
    explicit TPMModuleAndSlot(SECMODModule* init_chaps_module)
        : chaps_module(init_chaps_module) {}

    raw_ptr<SECMODModule> chaps_module;
    ScopedPK11Slot tpm_slot;
  };

  ScopedPK11Slot OpenPersistentNSSDBForPath(const std::string& db_name,
                                            const base::FilePath& path) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    // NSS is allowed to do IO on the current thread since dispatching
    // to a dedicated thread would still have the affect of blocking
    // the current thread, due to NSS's internal locking requirements
    ScopedAllowBlockingForNSS allow_blocking;

    base::FilePath nssdb_path = GetSoftwareNSSDBPath(path);
    if (!base::CreateDirectory(nssdb_path)) {
      LOG(ERROR) << "Failed to create " << nssdb_path.value() << " directory.";
      return ScopedPK11Slot();
    }
    return OpenSoftwareNSSDB(nssdb_path, db_name);
  }

  void InitializeTPMTokenAndSystemSlot(
      int system_slot_id,
      base::OnceCallback<void(bool)> callback) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK_EQ(state_, State::kInitializationNotStarted);
    state_ = State::kInitializationStarted;

    // Note that a reference is not taken to chaps_module_. This is safe since
    // ChromeOSTokenManager is Leaky, so the reference it holds is never
    // released.
    std::unique_ptr<TPMModuleAndSlot> tpm_args(
        new TPMModuleAndSlot(chaps_module_));
    TPMModuleAndSlot* tpm_args_ptr = tpm_args.get();
    base::ThreadPool::PostTaskAndReply(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(&ChromeOSTokenManager::InitializeTPMTokenInThreadPool,
                       system_slot_id, tpm_args_ptr),
        base::BindOnce(
            &ChromeOSTokenManager::OnInitializedTPMTokenAndSystemSlot,
            base::Unretained(this),  // ChromeOSTokenManager is leaky
            std::move(callback), std::move(tpm_args)));
  }

  void FinishInitializingTPMTokenAndSystemSlot() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(!IsInitializationFinished());

    // If `OnInitializedTPMTokenAndSystemSlot` was not called, but a test system
    // slot is prepared, start using it now. Can happen in tests that don't fake
    // enable TPM.
    if (!system_slot_ &&
        ChromeOSTokenManagerDataForTesting::GetInstance().test_system_slot) {
      system_slot_ = ScopedPK11Slot(
          PK11_ReferenceSlot(ChromeOSTokenManagerDataForTesting::GetInstance()
                                 .test_system_slot.get()));
    }

    state_ = (state_ == State::kTpmTokenInitialized) ? State::kTpmTokenEnabled
                                                     : State::kTpmTokenDisabled;

    tpm_ready_callback_list_->Notify();
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
      tpm_args->chaps_module = LoadChaps();
    }
    if (tpm_args->chaps_module) {
      tpm_args->tpm_slot = GetChapsSlot(tpm_args->chaps_module, token_slot_id);
    }
  }

  void OnInitializedTPMTokenAndSystemSlot(
      base::OnceCallback<void(bool)> callback,
      std::unique_ptr<TPMModuleAndSlot> tpm_args) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DVLOG(2) << "Loaded chaps: " << !!tpm_args->chaps_module
             << ", got tpm slot: " << !!tpm_args->tpm_slot;

    chaps_module_ = tpm_args->chaps_module;

    if (ChromeOSTokenManagerDataForTesting::GetInstance().test_system_slot) {
      // chromeos_unittests try to test the TPM initialization process. If we
      // have a test DB open, pretend that it is the system slot.
      system_slot_ = ScopedPK11Slot(
          PK11_ReferenceSlot(ChromeOSTokenManagerDataForTesting::GetInstance()
                                 .test_system_slot.get()));
    } else {
      system_slot_ = std::move(tpm_args->tpm_slot);
    }

    if (system_slot_) {
      state_ = State::kTpmTokenInitialized;
    }

    std::move(callback).Run(!!system_slot_);
  }

  void IsTPMTokenEnabled(base::OnceCallback<void(bool)> callback) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(!callback.is_null());

    if (!IsInitializationFinished()) {
      // Call back to this method when initialization is finished.
      tpm_ready_callback_list_->AddUnsafe(
          base::BindOnce(&ChromeOSTokenManager::IsTPMTokenEnabled,
                         base::Unretained(this) /* singleton is leaky */,
                         std::move(callback)));
      return;
    }

    DCHECK(base::SequencedTaskRunner::HasCurrentDefault());
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       /*is_tpm_enabled=*/(state_ == State::kTpmTokenEnabled)));
  }

  bool InitializeNSSForChromeOSUser(const std::string& username_hash,
                                    const base::FilePath& path) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    if (base::Contains(chromeos_user_map_, username_hash)) {
      // This user already exists in our mapping.
      DVLOG(2) << username_hash << " already initialized.";
      return false;
    }

    DVLOG(2) << "Opening NSS DB " << path.value();
    std::string db_name = base::StringPrintf("%s %s", kUserNSSDatabaseName,
                                             username_hash.c_str());
    ScopedPK11Slot public_slot(OpenPersistentNSSDBForPath(db_name, path));

    return InitializeNSSForChromeOSUserWithSlot(username_hash,
                                                std::move(public_slot));
  }

  bool InitializeNSSForChromeOSUserWithSlot(const std::string& username_hash,
                                            ScopedPK11Slot public_slot) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    if (base::Contains(chromeos_user_map_, username_hash)) {
      // This user already exists in our mapping.
      DVLOG(2) << username_hash << " already initialized.";
      return false;
    }

    chromeos_user_map_[username_hash] =
        std::make_unique<ChromeOSUserData>(std::move(public_slot));
    return true;
  }

  bool ShouldInitializeTPMForChromeOSUser(const std::string& username_hash) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(base::Contains(chromeos_user_map_, username_hash));

    return !chromeos_user_map_[username_hash]
                ->private_slot_initialization_started();
  }

  void WillInitializeTPMForChromeOSUser(const std::string& username_hash) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(base::Contains(chromeos_user_map_, username_hash));

    chromeos_user_map_[username_hash]
        ->set_private_slot_initialization_started();
  }

  void InitializeTPMForChromeOSUser(const std::string& username_hash,
                                    CK_SLOT_ID slot_id) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(base::Contains(chromeos_user_map_, username_hash));
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
    base::ThreadPool::PostTaskAndReply(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
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
    DCHECK(base::Contains(chromeos_user_map_, username_hash));
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

    if (!base::Contains(chromeos_user_map_, username_hash)) {
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
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), ScopedPK11Slot()));
      }
      return ScopedPK11Slot();
    }

    DCHECK(base::Contains(chromeos_user_map_, username_hash));

    return chromeos_user_map_[username_hash]->GetPrivateSlot(
        std::move(callback));
  }

  void CloseChromeOSUserForTesting(const std::string& username_hash) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    auto i = chromeos_user_map_.find(username_hash);
    CHECK(i != chromeos_user_map_.end(), base::NotFatalUntil::M130);
    chromeos_user_map_.erase(i);
  }

  void GetSystemNSSKeySlot(base::OnceCallback<void(ScopedPK11Slot)> callback) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    if (!IsInitializationFinished()) {
      // Call back to this method when initialization is finished.
      tpm_ready_callback_list_->AddUnsafe(
          base::BindOnce(&ChromeOSTokenManager::GetSystemNSSKeySlot,
                         base::Unretained(this) /* singleton is leaky */,
                         std::move(callback)));
      return;
    }

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       /*system_slot=*/ScopedPK11Slot(
                           system_slot_ ? PK11_ReferenceSlot(system_slot_.get())
                                        : nullptr)));
  }

  void ResetSystemSlotForTesting() { system_slot_.reset(); }

  void ResetTokenManagerForTesting() {
    // Prevent test failures when two tests in the same process use the same
    // ChromeOSTokenManager from different threads.
    DETACH_FROM_THREAD(thread_checker_);
    state_ = State::kInitializationNotStarted;

    // Configuring chaps_module_ here is not supported yet.
    CHECK(!chaps_module_);

    // Make sure there are no outstanding callbacks between tests.
    // OnceClosureList doesn't provide a way to clear the callback list.
    tpm_ready_callback_list_ = std::make_unique<base::OnceClosureList>();

    chromeos_user_map_.clear();
    ResetSystemSlotForTesting();  // IN-TEST
    prepared_test_private_slot_.reset();
  }

  void SetPrivateSoftwareSlotForChromeOSUserForTesting(ScopedPK11Slot slot) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    // Ensure that a previous value of prepared_test_private_slot_ is not
    // overwritten. Unsetting, i.e. setting a nullptr, however is allowed.
    DCHECK(!slot || !prepared_test_private_slot_);
    prepared_test_private_slot_ = std::move(slot);
  }

  bool IsInitializationStarted() {
    return (state_ != State::kInitializationNotStarted);
  }

 private:
  friend struct base::LazyInstanceTraitsBase<ChromeOSTokenManager>;

  ChromeOSTokenManager() { EnsureNSSInit(); }

  // NOTE(willchan): We don't actually cleanup on destruction since we leak NSS
  // to prevent non-joinable threads from using NSS after it's already been
  // shut down.
  ~ChromeOSTokenManager() = delete;

  bool IsInitializationFinished() {
    switch (state_) {
      case State::kTpmTokenEnabled:
      case State::kTpmTokenDisabled:
        return true;
      case State::kInitializationNotStarted:
      case State::kInitializationStarted:
      case State::kTpmTokenInitialized:
        return false;
    }
  }

  State state_ = State::kInitializationNotStarted;
  std::unique_ptr<base::OnceClosureList> tpm_ready_callback_list_ =
      std::make_unique<base::OnceClosureList>();

  raw_ptr<SECMODModule> chaps_module_ = nullptr;
  ScopedPK11Slot system_slot_;
  std::map<std::string, std::unique_ptr<ChromeOSUserData>> chromeos_user_map_;
  ScopedPK11Slot prepared_test_private_slot_;

  THREAD_CHECKER(thread_checker_);
};

base::LazyInstance<ChromeOSTokenManager>::Leaky g_token_manager =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

base::FilePath GetSoftwareNSSDBPath(
    const base::FilePath& profile_directory_path) {
  return profile_directory_path.AppendASCII(".pki").AppendASCII("nssdb");
}

void GetSystemNSSKeySlot(base::OnceCallback<void(ScopedPK11Slot)> callback) {
  g_token_manager.Get().GetSystemNSSKeySlot(std::move(callback));
}

void PrepareSystemSlotForTesting(ScopedPK11Slot slot) {
  DCHECK(!ChromeOSTokenManagerDataForTesting::GetInstance().test_system_slot);
  DCHECK(!g_token_manager.IsCreated() ||
         !g_token_manager.Get().IsInitializationStarted())
      << "PrepareSystemSlotForTesting is called after initialization started";

  ChromeOSTokenManagerDataForTesting::GetInstance().test_system_slot =
      std::move(slot);
}

void ResetSystemSlotForTesting() {
  if (g_token_manager.IsCreated()) {
    g_token_manager.Get().ResetSystemSlotForTesting();  // IN-TEST
  }
  ChromeOSTokenManagerDataForTesting::GetInstance().test_system_slot.reset();
}

void ResetTokenManagerForTesting() {
  if (g_token_manager.IsCreated()) {
    g_token_manager.Get().ResetTokenManagerForTesting();  // IN-TEST
  }
  ResetSystemSlotForTesting();  // IN-TEST
}

void IsTPMTokenEnabled(base::OnceCallback<void(bool)> callback) {
  g_token_manager.Get().IsTPMTokenEnabled(std::move(callback));
}

void InitializeTPMTokenAndSystemSlot(int token_slot_id,
                                     base::OnceCallback<void(bool)> callback) {
  g_token_manager.Get().InitializeTPMTokenAndSystemSlot(token_slot_id,
                                                        std::move(callback));
}

void FinishInitializingTPMTokenAndSystemSlot() {
  g_token_manager.Get().FinishInitializingTPMTokenAndSystemSlot();
}

bool InitializeNSSForChromeOSUser(const std::string& username_hash,
                                  const base::FilePath& path) {
  return g_token_manager.Get().InitializeNSSForChromeOSUser(username_hash,
                                                            path);
}

bool InitializeNSSForChromeOSUserWithSlot(const std::string& username_hash,
                                          ScopedPK11Slot public_slot) {
  return g_token_manager.Get().InitializeNSSForChromeOSUserWithSlot(
      username_hash, std::move(public_slot));
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

namespace {
void PrintDirectoryInfo(const base::FilePath& path) {
  base::stat_wrapper_t file_stat;

  if (base::File::Stat(path, &file_stat) == -1) {
    base::File::Error error_code = base::File::OSErrorToFileError(errno);
    LOG(ERROR) << "Failed to collect directory info, error: " << error_code;
  }

  LOG(ERROR) << path << ", " << std::oct << file_stat.st_mode << std::dec
             << ", "
             << "uid " << file_stat.st_uid << ", "
             << "gid " << file_stat.st_gid << std::endl;
}
}  // namespace

// TODO(crbug.com/1163303): Remove when the bug is fixed.
void DiagnosePublicSlotAndCrash(const base::FilePath& nss_path) {
  LOG(ERROR) << "Public slot is invalid. Start collecting stats.";
  // Should be something like /home/chronos/u-<hash>/.pki/nssdb .
  LOG(ERROR) << "NSS path: " << nss_path;

  {
    // NSS files like pkcs11.txt, cert9.db, key4.db .
    base::FileEnumerator files(
        nss_path, /*recursive=*/false,
        /*file_type=*/base::FileEnumerator::FILES,
        /*pattern=*/base::FilePath::StringType(),
        base::FileEnumerator::FolderSearchPolicy::MATCH_ONLY,
        base::FileEnumerator::ErrorPolicy::STOP_ENUMERATION);
    LOG(ERROR) << "Public slot database files:";
    for (base::FilePath path = files.Next(); !path.empty();
         path = files.Next()) {
      base::FileEnumerator::FileInfo file_info = files.GetInfo();

      char buf[16];
      int read_result = base::ReadFile(path, buf, sizeof(buf) - 1);

      LOG(ERROR) << file_info.GetName() << ", " << std::oct
                 << file_info.stat().st_mode << std::dec << ", "
                 << "uid " << file_info.stat().st_uid << ", "
                 << "gid " << file_info.stat().st_gid << ", "
                 << file_info.stat().st_size << " bytes, "
                 << ((read_result > 0) ? "readable" : "not readable");
    }
    LOG(ERROR) << "Enumerate error code: " << files.GetError();
  }

  // NSS directory might not be created yet, also check parent directories.
  // Use u-hash directory as a comparison point for user and group ids and
  // access permissions.

  base::FilePath nssdb_path = nss_path.Append(base::FilePath::kParentDirectory);
  PrintDirectoryInfo(nssdb_path);

  base::FilePath pki_path = nssdb_path.Append(base::FilePath::kParentDirectory);
  PrintDirectoryInfo(pki_path);

  base::FilePath u_hash_path =
      pki_path.Append(base::FilePath::kParentDirectory);
  PrintDirectoryInfo(u_hash_path);

  {
    // Check whether the NSS path exists, and if not, check whether it's
    // possible to create it.
    if (base::DirectoryExists(nss_path)) {
      LOG(ERROR) << "NSS path exists (as expected).";
    } else {
      base::File::Error error = base::File::Error::FILE_OK;
      if (base::CreateDirectoryAndGetError(nss_path, &error)) {
        LOG(ERROR) << "NSS path didn't exist. Created successfully.";
      } else {
        LOG(ERROR) << "NSS path didn't exist. Failed to create, error: "
                   << error;
      }
    }
  }

  CHECK(false) << "Public slot is invalid.";
}

}  // namespace crypto
