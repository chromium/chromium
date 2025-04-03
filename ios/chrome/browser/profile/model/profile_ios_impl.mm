// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/profile/model/profile_ios_impl.h"

#import <Foundation/Foundation.h>

#import <utility>

#import "base/apple/backup_util.h"
#import "base/check.h"
#import "base/feature_list.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/threading/thread_restrictions.h"
#import "base/uuid.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#import "components/policy/core/common/configuration_policy_provider.h"
#import "components/policy/core/common/schema_registry.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/json_pref_store.h"
#import "components/prefs/pref_service.h"
#import "components/profile_metrics/browser_profile_type.h"
#import "components/proxy_config/ios/proxy_service_factory.h"
#import "components/proxy_config/pref_proxy_config_tracker.h"
#import "components/supervised_user/core/browser/supervised_user_content_settings_provider.h"
#import "components/supervised_user/core/browser/supervised_user_pref_store.h"
#import "components/supervised_user/core/browser/supervised_user_settings_service.h"
#import "components/supervised_user/core/common/features.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "components/user_prefs/user_prefs.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/model/profile_policy_connector.h"
#import "ios/chrome/browser/policy/model/profile_policy_connector_factory.h"
#import "ios/chrome/browser/policy/model/schema_registry_factory.h"
#import "ios/chrome/browser/prefs/model/ios_chrome_pref_service_factory.h"
#import "ios/chrome/browser/profile/model/constants.h"
#import "ios/chrome/browser/profile/model/ios_chrome_url_request_context_getter.h"
#import "ios/chrome/browser/profile/model/off_the_record_profile_ios_impl.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/paths/paths_internal.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_settings_service_factory.h"
#import "ios/web/public/thread/web_thread.h"

// TODO(crbug.com/369296278): Remove when MaybeMigrateSyncingUserToSignedIn()
// is no longer used (i.e. ~one year after kForceMigrateSyncingUserToSignedIn
// is fully launched).
#import "base/task/bind_post_task.h"
#import "components/browser_sync/sync_to_signin_migration.h"

namespace {

// Determine the WebKit storage UUID for profile.
//
// There are three possibilities depending on the age of the profile.
// First for really old profiles (before the introduction of separate
// data store in iOS 17.0) they store the data in the global unnamed
// store, and thus use an empty UUID to represent this. For profiles
// created between M-128 and M-133, a random UUID is generated and is
// stored in the PrefService to use as the identifier for WebKit data.
// Profile created in M-133+ have their name be an UUID and the name
// can use used as the storage identifier.
//
// To determine which value to use for the storage identifier, proceed
// in steps. If the profile name can be parsed as an UUID, then use it
// as the storage identifier (this is a profile created in M-133+). If
// not, then use the value stored in kBrowserStateStorageIdentifier if
// it is set, otherwise do not use an UUID (the profile is legacy and
// will use the global storage).
//
// Assert that the profile name is an UUID if it has just been created
// to ensure the algorithm above correctly cover all possible cases.
//
// There is no WebKit API to change the storage nor to copy data from
// one storage location to another. Until an API is provided, it will
// not be possible to migrate users. For this reason the state is not
// recorded as an histogram (it could be years before we can drop the
// support for "default" storage or for storing the UUID in prefs).
base::Uuid GetStorageUUID(const std::string& name,
                          PrefService* prefs,
                          bool is_new_profile) {
  base::Uuid uuid = base::Uuid::ParseLowercase(name);
  if (uuid.is_valid()) {
    // M-133+ profile whose name is an UUID already. In that case use
    // the profile name as the storage identifier.
    return uuid;
  }

  const std::string& uuid_string =
      prefs->GetString(prefs::kBrowserStateStorageIdentifier);
  if (uuid_string.empty() && !is_new_profile) {
    // Pre M-128 profile, there is no UUID stored in the prefs and the
    // profile use the default WebKit storage, which is represented as
    // an invalid UUID.
    return base::Uuid();
  }

  // M-128+ profile by default.
  //
  // Note: `uuid_string` should be a valid UUID and `is_new_profile`
  // should be false, but the code will still generate a random UUID
  // if either of those assumption are incorrect.
  //
  // They could happen if the storage for a profile was tampered with
  // or somehow got corrupted (e.g. the profile directory was deleted
  // or the json data on disk modified). Another possibility is that
  // the name was reserved in ProfileAttributesStorageIOS but for some
  // reason the profile never created.
  uuid = base::Uuid::ParseCaseInsensitive(uuid_string);
  if (!uuid.is_valid()) {
    uuid = base::Uuid::GenerateRandomV4();
    prefs->SetString(prefs::kBrowserStateStorageIdentifier,
                     uuid.AsLowercaseString());
  }

  return uuid;
}

}  // namespace

// Struct storing informations that is needed for prefs initialisation.
struct ProfileIOSImpl::InitInfo {
  CreationMode creation_mode;
  bool is_new_profile;
  base::FilePath cache_path;
  scoped_refptr<SupervisedUserPrefStore> supervised_user_prefs;
};

// Helper class to create the Profile's directory.
//
// This is a separate class to limit how much code can be allowed to block
// the main sequence. It is required to block the sequence because we need
// to synchronously create the directories used to store the Profile data.
class BrowserStateDirectoryBuilder {
 public:
  // Stores the result of creating the directories.
  struct [[nodiscard]] Result {
    static Result Success(bool is_new_profile, base::FilePath cache) {
      return Result{
          .success = true,
          .created = is_new_profile,
          .cache_path = std::move(cache),
      };
    }

    static Result Failure() {
      return Result{
          .success = false,
          .created = false,
      };
    }

    const bool success;
    const bool created;
    const base::FilePath cache_path;
  };

  // Creates the directories if possible.
  static Result CreateDirectories(const base::FilePath& state_path,
                                  const base::FilePath& otr_path);
};

// static
BrowserStateDirectoryBuilder::Result
BrowserStateDirectoryBuilder::CreateDirectories(
    const base::FilePath& state_path,
    const base::FilePath& otr_path) {
  // Create the profile directory synchronously otherwise we would need to
  // sequence every otherwise independent I/O operation inside the profile
  // directory with this operation. base::CreateDirectory() should be a
  // lightweight I/O operation and avoiding the headache of sequencing all
  // otherwise unrelated I/O after this one justifies running it on the main
  // thread.
  base::ScopedAllowBlocking allow_blocking_to_create_directory;

  bool created = false;
  if (!base::PathExists(state_path)) {
    if (!base::CreateDirectory(state_path)) {
      return Result::Failure();
    }
    created = true;
  }

  // Create the directory for the OTR stash state now, even though it won't
  // necessarily be needed: the OTR profile itself is created
  // synchronously on an as-needed basis on the UI thread, so creation of its
  // stash state directory cannot easily be done at that point.
  if (!base::PathExists(otr_path)) {
    if (!base::CreateDirectory(otr_path)) {
      return Result::Failure();
    }
  }
  base::apple::SetBackupExclusion(otr_path);

  base::FilePath base_cache_path;
  ios::GetUserCacheDirectory(state_path, &base_cache_path);
  base::FilePath cache_path = base_cache_path.Append(kIOSChromeCacheDirname);

  if (!base::PathExists(cache_path)) {
    if (!base::CreateDirectory(cache_path)) {
      return Result::Failure();
    }
  }

  return Result::Success(created, cache_path);
}

// static
std::unique_ptr<ProfileIOS> ProfileIOS::CreateProfile(
    const base::FilePath& path,
    std::string_view profile_name,
    CreationMode creation_mode,
    Delegate* delegate) {
  // Get sequenced task runner for making sure that file operations of
  // this profile are executed in expected order (what was previously assured by
  // the FILE thread).
  scoped_refptr<base::SequencedTaskRunner> io_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskShutdownBehavior::BLOCK_SHUTDOWN, base::MayBlock()});

  return base::WrapUnique(new ProfileIOSImpl(path, profile_name, io_task_runner,
                                             creation_mode, delegate));
}

ProfileIOSImpl::ProfileIOSImpl(
    const base::FilePath& state_path,
    std::string_view profile_name,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    CreationMode creation_mode,
    Delegate* delegate)
    : ProfileIOS(state_path, profile_name, std::move(io_task_runner)),
      delegate_(delegate),
      pref_registry_(new user_prefs::PrefRegistrySyncable),
      io_data_(new ProfileIOSImplIOData::Handle(this)) {
  DCHECK(!profile_name.empty());
  BrowserStateDependencyManager::GetInstance()->MarkBrowserStateLive(this);

  profile_metrics::SetBrowserProfileType(
      this, profile_metrics::BrowserProfileType::kRegular);

  if (delegate_) {
    delegate_->OnProfileCreationStarted(this, creation_mode);
  }

  const BrowserStateDirectoryBuilder::Result directories_creation_result =
      BrowserStateDirectoryBuilder::CreateDirectories(
          state_path, GetOffTheRecordStatePath());
  DCHECK(directories_creation_result.success);

  // Bring up the policy system before creating `prefs_`.
  BrowserPolicyConnectorIOS* connector =
      GetApplicationContext()->GetBrowserPolicyConnector();
  DCHECK(connector);
  policy_schema_registry_ = BuildSchemaRegistryForProfile(
      this, connector->GetChromeSchema(), connector->GetSchemaRegistry());

  // Create the UserCloudPolicyManager and start it immediately if the
  // Profile is loaded synchronously.
  user_cloud_policy_manager_ = policy::UserCloudPolicyManager::Create(
      state_path, policy_schema_registry_.get(),
      creation_mode == CreationMode::kSynchronous, GetIOTaskRunner(),
      base::BindRepeating(&ApplicationContext::GetNetworkConnectionTracker,
                          base::Unretained(GetApplicationContext())));

  policy_connector_ = BuildProfilePolicyConnector(
      policy_schema_registry_.get(), connector,
      user_cloud_policy_manager_.get(),
      user_cloud_policy_manager_ && user_cloud_policy_manager_->core()
          ? user_cloud_policy_manager_->core()->store()
          : nullptr);

  // Register Profile preferences.
  RegisterProfilePrefs(pref_registry_.get());
  BrowserStateDependencyManager::GetInstance()
      ->RegisterBrowserStatePrefsForServices(pref_registry_.get());

  // Create a SupervisedUserPrefStore and initialize it with empty data.
  // The pref store will load SupervisedUserSettingsService disk data after
  // the creation of PrefService.
  scoped_refptr<SupervisedUserPrefStore> supervised_user_prefs =
      base::MakeRefCounted<SupervisedUserPrefStore>();
  supervised_user_prefs->OnNewSettingsAvailable(base::Value::Dict());
  DCHECK(supervised_user_prefs->IsInitializationComplete());

  prefs_ = CreateProfilePrefs(
      state_path, GetIOTaskRunner().get(), pref_registry_,
      policy_connector_ ? policy_connector_->GetPolicyService() : nullptr,
      GetApplicationContext()->GetBrowserPolicyConnector(),
      supervised_user_prefs, creation_mode == CreationMode::kAsynchronous);

  const InitInfo init_info{
      .creation_mode = creation_mode,
      .is_new_profile = directories_creation_result.created,
      .cache_path = directories_creation_result.cache_path,
      .supervised_user_prefs = supervised_user_prefs,
  };

  if (init_info.creation_mode == CreationMode::kAsynchronous) {
    // It is safe to use base::Unretained(...) here since `this` owns the
    // PrefService and the callback will not be invoked after destruction
    // of the PrefService.
    prefs_->AddPrefInitObserver(base::BindOnce(
        &ProfileIOSImpl::PrefsInitStage1, base::Unretained(this), init_info));

    return;
  }

  // Either the operation was synchronous or unnecessary, move to the
  // next stage of the profile initialisation.
  PrefsInitStage1(init_info, true);
}

ProfileIOSImpl::~ProfileIOSImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BrowserStateDependencyManager::GetInstance()->DestroyBrowserStateServices(
      this);
  // Warning: the order for shutting down the BrowserState objects is important
  // because of interdependencies. Ideally the order for shutting down the
  // objects should be backward of their declaration in class attributes.

  if (pref_proxy_config_tracker_) {
    pref_proxy_config_tracker_->DetachFromPrefService();
  }

  // Here, (1) the profile services may depend on `policy_connector_` and
  // `user_cloud_policy_manager_`, and (2) `policy_connector_` depends on
  // `user_cloud_policy_manager_`. The dependencies have to be shut down
  // backward.
  policy_connector_->Shutdown();
  if (user_cloud_policy_manager_) {
    user_cloud_policy_manager_->Shutdown();
  }

  DestroyOffTheRecordProfile();
}

ProfileIOS* ProfileIOSImpl::GetOriginalProfile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return this;
}

ProfileIOS* ProfileIOSImpl::GetOffTheRecordProfile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!otr_state_) {
    otr_state_.reset(new OffTheRecordProfileIOSImpl(
        GetIOTaskRunner(), this, GetOffTheRecordStatePath()));
  }

  return otr_state_.get();
}

bool ProfileIOSImpl::HasOffTheRecordProfile() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !!otr_state_;
}

void ProfileIOSImpl::DestroyOffTheRecordProfile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Tear down OTR Profile with which it is associated.
  otr_state_.reset();
}

ProfilePolicyConnector* ProfileIOSImpl::GetPolicyConnector() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return policy_connector_.get();
}

policy::UserCloudPolicyManager* ProfileIOSImpl::GetUserCloudPolicyManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return user_cloud_policy_manager_.get();
}

sync_preferences::PrefServiceSyncable* ProfileIOSImpl::GetSyncablePrefs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(prefs_);  // Should explicitly be initialized.
  return prefs_.get();
}

const sync_preferences::PrefServiceSyncable* ProfileIOSImpl::GetSyncablePrefs()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(prefs_);  // Should explicitly be initialized.
  return prefs_.get();
}

bool ProfileIOSImpl::IsOffTheRecord() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return false;
}

const base::Uuid& ProfileIOSImpl::GetWebKitStorageID() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return storage_uuid_;
}

void ProfileIOSImpl::SetOffTheRecordProfileIOS(
    std::unique_ptr<ProfileIOS> otr_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!otr_state_);
  otr_state_ = std::move(otr_state);
}

void ProfileIOSImpl::PrefsInitStage1(InitInfo init_info, bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Early return in case of failure to load the preferences.
  if (!success) {
    if (delegate_) {
      delegate_->OnProfileCreationFinished(this, init_info.creation_mode,
                                           init_info.is_new_profile,
                                           /*success=*/false);
    }
    return;
  }

  // Register on Profile.
  user_prefs::UserPrefs::Set(this, prefs_.get());

  // The HostContentSettingsMap constructor expects the prefs to have
  // already been loaded from disk before it is instantiated. One the
  // other hand, the SupervisedUserPrefStore is needed as part of the
  // PrefService creation (thus before the prefs are loaded from disk)
  // and must be registered with the HostContentSettingsMap.
  //
  // When loading the pref synchronously, the read happens as part of
  // the CreateProfilePrefs(...) function call, but when loading them
  // asynchronously, it happened on a background thread. This caused
  // the HostContentSettingsMap to be created before the data was read
  // from disk, and resulted in invalid values.
  //
  // To ensure that the construction of the objects happens in the
  // same relative order compared to reading the data from disk when
  // for both synchronous and asynchronous initialisation, move all
  // the logic for initialising SupervisedUserPrefStore in the method
  // invoked when the data has been read from disk (PrefsInitStage1).

  // Initialize the settings service and have the pref store subscribe to it.
  supervised_user::SupervisedUserSettingsService* supervised_user_settings =
      SupervisedUserSettingsServiceFactory::GetForProfile(this);

  const base::FilePath& state_path = GetStatePath();
  supervised_user_settings->Init(
      state_path, GetIOTaskRunner(),
      init_info.creation_mode == CreationMode::kSynchronous);

  init_info.supervised_user_prefs->Init(supervised_user_settings);

  auto supervised_provider =
      std::make_unique<supervised_user::SupervisedUserContentSettingsProvider>(
          supervised_user_settings);

  ios::HostContentSettingsMapFactory::GetForProfile(this)->RegisterProvider(
      content_settings::ProviderType::kSupervisedProvider,
      std::move(supervised_provider));

  const int cache_max_size = 0;
  base::FilePath cookie_path = state_path.Append(kIOSChromeCookieFilename);

  // Make sure we initialize the io_data_ after everything else has been
  // initialized that we might be reading from the IO thread.
  io_data_->Init(cookie_path, init_info.cache_path, cache_max_size, state_path);

  // If the initialisation is asynchronous, then we also need to wait for
  // the SupervisedUserSettingsService to complete its initialisation, if
  // is not yet complete.
  if (init_info.creation_mode == CreationMode::kAsynchronous) {
    if (!supervised_user_settings->IsReady()) {
      // It is safe to use base::Unretained(...) here since `this` owns the
      // SupervisedUserSettingsService and the callback will not be invoked
      // after destruction of the SupervisedUserSettingsService.
      supervised_user_settings->WaitUntilReadyToSync(
          base::BindOnce(&ProfileIOSImpl::PrefsInitStage2,
                         base::Unretained(this), init_info, success));
      return;
    }
  }

  // Either the operation was synchronous or unnecessary, move to the
  // next stage of the profile initialisation.
  PrefsInitStage2(init_info, success);
}

void ProfileIOSImpl::PrefsInitStage2(InitInfo init_info, bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(success);

  // Migrate the preferences, unless the profile has just been created.
  if (!init_info.is_new_profile) {
    MigrateObsoleteProfilePrefs(prefs_.get());

    // TODO(crbug.com/369296278): Remove on 12/2025.
    //
    // MaybeMigrateSyncingUserToSignedIn(...) may perform disk IO which is
    // not permitted if the Profile is loaded asynchronously.
    if (init_info.creation_mode == CreationMode::kAsynchronous) {
      browser_sync::MaybeMigrateSyncingUserToSignedInAsync(
          GetStatePath(), GetPrefs(),
          base::BindOnce(&ProfileIOSImpl::PrefsInitStage3,
                         weak_ptr_factory_.GetWeakPtr(), init_info, success));
      return;
    }

    browser_sync::MaybeMigrateSyncingUserToSignedIn(GetStatePath(), GetPrefs());
  }

  // Either the operation was synchronous or unnecessary, move to the
  // next stage of the profile initialisation.
  PrefsInitStage3(init_info, success);
}

void ProfileIOSImpl::PrefsInitStage3(InitInfo init_info, bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(success);

  // Initialize the WebKit storage identifier.
  const bool is_new_profile = init_info.is_new_profile;
  storage_uuid_ = GetStorageUUID(GetProfileName(), GetPrefs(), is_new_profile);

  // DO NOT ADD ANY INITIALISATION AFTER THIS LINE.

  // The initialisation of the ProfileIOS is now complete and the services
  // can be safely created.
  BrowserStateDependencyManager::GetInstance()->CreateBrowserStateServices(
      this);

  if (delegate_) {
    delegate_->OnProfileCreationFinished(this, init_info.creation_mode,
                                         init_info.is_new_profile,
                                         /*success=*/true);
  }
}

ProfileIOSIOData* ProfileIOSImpl::GetIOData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return io_data_->io_data();
}

net::URLRequestContextGetter* ProfileIOSImpl::CreateRequestContext(
    ProtocolHandlerMap* protocol_handlers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ApplicationContext* application_context = GetApplicationContext();
  return io_data_
      ->CreateMainRequestContextGetter(
          protocol_handlers, application_context->GetLocalState(),
          application_context->GetIOSChromeIOThread())
      .get();
}

base::WeakPtr<ProfileIOS> ProfileIOSImpl::AsWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_.GetWeakPtr();
}

void ProfileIOSImpl::ClearNetworkingHistorySince(base::Time time,
                                                 base::OnceClosure completion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  io_data_->ClearNetworkingHistorySince(time, std::move(completion));
}

PrefProxyConfigTracker* ProfileIOSImpl::GetProxyConfigTracker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pref_proxy_config_tracker_) {
    pref_proxy_config_tracker_ =
        ProxyServiceFactory::CreatePrefProxyConfigTrackerOfProfile(
            GetPrefs(), GetApplicationContext()->GetLocalState());
  }
  return pref_proxy_config_tracker_.get();
}
