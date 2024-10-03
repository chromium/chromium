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
#import "ios/chrome/browser/policy/model/browser_state_policy_connector.h"
#import "ios/chrome/browser/policy/model/browser_state_policy_connector_factory.h"
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

  policy_connector_ =
      BuildBrowserStatePolicyConnector(policy_schema_registry_.get(), connector,
                                       user_cloud_policy_manager_.get());

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
  // Register on Profile.
  user_prefs::UserPrefs::Set(this, prefs_.get());

  // In //chrome/browser, SupervisedUserSettingsService is a SimpleKeyedService
  // and can be created to initialize SupervisedUserPrefStore.
  // In //ios/chrome/browser, SupervisedUserSettingsService is a
  // BrowserStateKeyedService and is only available after the creation of
  // SupervisedUserPrefStore.
  supervised_user::SupervisedUserSettingsService* supervised_user_settings =
      SupervisedUserSettingsServiceFactory::GetForProfile(this);

  // Initialize the settings service and have the pref store subscribe to it.
  supervised_user_settings->Init(state_path, GetIOTaskRunner(),
                                 creation_mode == CreationMode::kSynchronous);

  supervised_user_prefs->Init(supervised_user_settings);

  auto supervised_provider =
      std::make_unique<supervised_user::SupervisedUserContentSettingsProvider>(
          supervised_user_settings);

  ios::HostContentSettingsMapFactory::GetForBrowserState(this)
      ->RegisterProvider(content_settings::ProviderType::kSupervisedProvider,
                         std::move(supervised_provider));

  base::FilePath cookie_path = state_path.Append(kIOSChromeCookieFilename);
  base::FilePath cache_path = directories_creation_result.cache_path;
  int cache_max_size = 0;

  // Make sure we initialize the io_data_ after everything else has been
  // initialized that we might be reading from the IO thread.
  io_data_->Init(cookie_path, cache_path, cache_max_size, state_path);

  const bool is_new_profile = directories_creation_result.created;
  if (creation_mode == CreationMode::kAsynchronous) {
    // It is safe to use base::Unretained(...) here since `this` owns the
    // PrefService and the callback will not be invoked after destruction
    // of the PrefService.
    prefs_->AddPrefInitObserver(base::BindOnce(&ProfileIOSImpl::OnPrefsLoaded,
                                               base::Unretained(this),
                                               creation_mode, is_new_profile));
  } else {
    // Prefs were loaded synchronously so we can continue immediately.
    OnPrefsLoaded(creation_mode, is_new_profile, true);
  }
}

ProfileIOSImpl::~ProfileIOSImpl() {
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

ProfileIOS* ProfileIOSImpl::GetOriginalChromeBrowserState() {
  return GetOriginalProfile();
}

ProfileIOS* ProfileIOSImpl::GetOriginalProfile() {
  return this;
}

ProfileIOS* ProfileIOSImpl::GetOffTheRecordChromeBrowserState() {
  return GetOffTheRecordProfile();
}

ProfileIOS* ProfileIOSImpl::GetOffTheRecordProfile() {
  if (!otr_state_) {
    otr_state_.reset(new OffTheRecordProfileIOSImpl(
        GetIOTaskRunner(), this, GetOffTheRecordStatePath()));
  }

  return otr_state_.get();
}

bool ProfileIOSImpl::HasOffTheRecordChromeBrowserState() const {
  return HasOffTheRecordProfile();
}

bool ProfileIOSImpl::HasOffTheRecordProfile() const {
  return !!otr_state_;
}

void ProfileIOSImpl::DestroyOffTheRecordChromeBrowserState() {
  return DestroyOffTheRecordProfile();
}

void ProfileIOSImpl::DestroyOffTheRecordProfile() {
  // Tear down OTR Profile with which it is associated.
  otr_state_.reset();
}

BrowserStatePolicyConnector* ProfileIOSImpl::GetPolicyConnector() {
  return policy_connector_.get();
}

policy::UserCloudPolicyManager* ProfileIOSImpl::GetUserCloudPolicyManager() {
  return user_cloud_policy_manager_.get();
}

sync_preferences::PrefServiceSyncable* ProfileIOSImpl::GetSyncablePrefs() {
  DCHECK(prefs_);  // Should explicitly be initialized.
  return prefs_.get();
}

const sync_preferences::PrefServiceSyncable* ProfileIOSImpl::GetSyncablePrefs()
    const {
  DCHECK(prefs_);  // Should explicitly be initialized.
  return prefs_.get();
}

bool ProfileIOSImpl::IsOffTheRecord() const {
  return false;
}

const std::string& ProfileIOSImpl::GetWebKitStorageID() const {
  return storage_uuid_;
}

void ProfileIOSImpl::SetOffTheRecordProfileIOS(
    std::unique_ptr<ProfileIOS> otr_state) {
  DCHECK(!otr_state_);
  otr_state_ = std::move(otr_state);
}

void ProfileIOSImpl::OnPrefsLoaded(CreationMode creation_mode,
                                   bool is_new_profile,
                                   bool success) {
  // Early return in case of failure to load the preferences.
  if (!success) {
    if (delegate_) {
      delegate_->OnProfileCreationFinished(this, creation_mode, is_new_profile,
                                           /*success=*/false);
    }
    return;
  }

  // If the initialisation is asynchronous, then we also need to wait for
  // the SupervisedUserSettingsService to complete its initialisation, if
  // is not yet complete.
  if (creation_mode == CreationMode::kAsynchronous) {
    supervised_user::SupervisedUserSettingsService* supervised_user_settings =
        SupervisedUserSettingsServiceFactory::GetForProfile(this);
    if (!supervised_user_settings->IsReady()) {
      // It is safe to use base::Unretained(...) here since `this` owns the
      // SupervisedUserSettingsService and the callback will not be invoked
      // after destruction of the SupervisedUserSettingsService.
      supervised_user_settings->WaitUntilReadyToSync(
          base::BindOnce(&ProfileIOSImpl::OnPrefsLoaded, base::Unretained(this),
                         creation_mode, is_new_profile, success));
      return;
    }
  }

  // Migrate obsolete prefs.
  MigrateObsoleteProfilePrefs(GetStatePath(), prefs_.get());

  // Initialize `storage_uuid_` from the prefs. In case of a new Profile,
  // generate a new value (this avoid losing data when migrating from an old
  // Profile).
  //
  // TODO(crbug.com/346754380): Remove when all Profile use a non-default
  // storage (since there is no automatic migration, this could take years).
  storage_uuid_ = GetPrefs()->GetString(prefs::kBrowserStateStorageIdentifier);
  if (storage_uuid_.empty() && is_new_profile) {
    storage_uuid_ = base::SysNSStringToUTF8([NSUUID UUID].UUIDString);
    GetPrefs()->SetString(prefs::kBrowserStateStorageIdentifier, storage_uuid_);
  }

  // DO NOT ADD ANY INITIALISATION AFTER THIS LINE.

  // The initialisation of the ProfileIOS is now complete and the services
  // can be safely created.
  BrowserStateDependencyManager::GetInstance()->CreateBrowserStateServices(
      this);

  if (delegate_) {
    delegate_->OnProfileCreationFinished(this, creation_mode, is_new_profile,
                                         success);
  }
}

ProfileIOSIOData* ProfileIOSImpl::GetIOData() {
  return io_data_->io_data();
}

net::URLRequestContextGetter* ProfileIOSImpl::CreateRequestContext(
    ProtocolHandlerMap* protocol_handlers) {
  ApplicationContext* application_context = GetApplicationContext();
  return io_data_
      ->CreateMainRequestContextGetter(
          protocol_handlers, application_context->GetLocalState(),
          application_context->GetIOSChromeIOThread())
      .get();
}

base::WeakPtr<ProfileIOS> ProfileIOSImpl::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ProfileIOSImpl::ClearNetworkingHistorySince(base::Time time,
                                                 base::OnceClosure completion) {
  io_data_->ClearNetworkingHistorySince(time, std::move(completion));
}

PrefProxyConfigTracker* ProfileIOSImpl::GetProxyConfigTracker() {
  if (!pref_proxy_config_tracker_) {
    pref_proxy_config_tracker_ =
        ProxyServiceFactory::CreatePrefProxyConfigTrackerOfProfile(
            GetPrefs(), GetApplicationContext()->GetLocalState());
  }
  return pref_proxy_config_tracker_.get();
}
