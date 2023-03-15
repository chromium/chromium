// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_state/chrome_browser_state_impl.h"

#import <utility>

#import "base/check.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/mac/backup_util.h"
#import "base/task/sequenced_task_runner.h"
#import "base/threading/thread_restrictions.h"
#import "components/bookmarks/browser/bookmark_model.h"
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
#import "components/sync_preferences/pref_service_syncable.h"
#import "components/user_prefs/user_prefs.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/bookmarks/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/browser_state/bookmark_model_loaded_observer.h"
#import "ios/chrome/browser/browser_state/constants.h"
#import "ios/chrome/browser/browser_state/off_the_record_chrome_browser_state_impl.h"
#import "ios/chrome/browser/net/ios_chrome_url_request_context_getter.h"
#import "ios/chrome/browser/paths/paths_internal.h"
#import "ios/chrome/browser/policy/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/browser_state_policy_connector.h"
#import "ios/chrome/browser/policy/browser_state_policy_connector_factory.h"
#import "ios/chrome/browser/policy/schema_registry_factory.h"
#import "ios/chrome/browser/prefs/browser_prefs.h"
#import "ios/chrome/browser/prefs/ios_chrome_pref_service_factory.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/web/public/thread/web_thread.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Returns a bool indicating whether the necessary directories were able to be
// created (or already existed).
bool EnsureBrowserStateDirectoriesCreated(const base::FilePath& path,
                                          const base::FilePath& otr_path,
                                          const base::FilePath& cache_path) {
  // Create the browser state directory synchronously otherwise we would need to
  // sequence every otherwise independent I/O operation inside the browser state
  // directory with this operation. base::CreateDirectory() should be a
  // lightweight I/O operation and avoiding the headache of sequencing all
  // otherwise unrelated I/O after this one justifies running it on the main
  // thread.
  base::ScopedAllowBlocking allow_blocking_to_create_directory;

  if (!base::PathExists(path) && !base::CreateDirectory(path))
    return false;
  // Create the directory for the OTR stash state now, even though it won't
  // necessarily be needed: the OTR browser state itself is created
  // synchronously on an as-needed basis on the UI thread, so creation of its
  // stash state directory cannot easily be done at that point.
  if (!base::PathExists(otr_path) && !base::CreateDirectory(otr_path))
    return false;
  base::mac::SetBackupExclusion(otr_path);
  if (!base::PathExists(cache_path) && !base::CreateDirectory(cache_path))
    return false;
  return true;
}

namespace {

base::FilePath GetCachePath(const base::FilePath& base) {
  return base.Append(kIOSChromeCacheDirname);
}

}  // namespace

ChromeBrowserStateImpl::ChromeBrowserStateImpl(
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    const base::FilePath& path)
    : ChromeBrowserState(std::move(io_task_runner)),
      state_path_(path),
      pref_registry_(new user_prefs::PrefRegistrySyncable),
      io_data_(new ChromeBrowserStateImplIOData::Handle(this)) {
  otr_state_path_ = state_path_.Append(FILE_PATH_LITERAL("OTR"));

  profile_metrics::SetBrowserProfileType(
      this, profile_metrics::BrowserProfileType::kRegular);

  // It would be nice to use PathService for fetching this directory, but
  // the cache directory depends on the browser state stash directory, which
  // isn't available to PathService.
  base::FilePath base_cache_path;
  ios::GetUserCacheDirectory(state_path_, &base_cache_path);

  bool directories_created = EnsureBrowserStateDirectoriesCreated(
      state_path_, otr_state_path_, base_cache_path);
  DCHECK(directories_created);

  // Bring up the policy system before creating `prefs_`.
  BrowserPolicyConnectorIOS* connector =
      GetApplicationContext()->GetBrowserPolicyConnector();
  DCHECK(connector);
  policy_schema_registry_ = BuildSchemaRegistryForBrowserState(
      this, connector->GetChromeSchema(), connector->GetSchemaRegistry());

  // Create the UserCloudPolicyManager and force it to load immediately since
  // BrowserState is loaded synchronously.
  user_cloud_policy_manager_ = policy::UserCloudPolicyManager::Create(
      GetStatePath(), policy_schema_registry_.get(),
      /*force_immediate_load=*/true, GetIOTaskRunner(),
      base::BindRepeating(&ApplicationContext::GetNetworkConnectionTracker,
                          base::Unretained(GetApplicationContext())));

  policy_connector_ =
      BuildBrowserStatePolicyConnector(policy_schema_registry_.get(), connector,
                                       user_cloud_policy_manager_.get());

  RegisterBrowserStatePrefs(pref_registry_.get());
  BrowserStateDependencyManager::GetInstance()
      ->RegisterBrowserStatePrefsForServices(pref_registry_.get());

  prefs_ = CreateBrowserStatePrefs(
      state_path_, GetIOTaskRunner().get(), pref_registry_,
      policy_connector_ ? policy_connector_->GetPolicyService() : nullptr,
      GetApplicationContext()->GetBrowserPolicyConnector());
  // Register on BrowserState.
  user_prefs::UserPrefs::Set(this, prefs_.get());

  // Migrate obsolete prefs.
  PrefService* local_state = GetApplicationContext()->GetLocalState();
  MigrateObsoleteLocalStatePrefs(local_state);
  MigrateObsoleteBrowserStatePrefs(prefs_.get());

  BrowserStateDependencyManager::GetInstance()->CreateBrowserStateServices(
      this);

  base::FilePath cookie_path = state_path_.Append(kIOSChromeCookieFilename);
  base::FilePath cache_path = GetCachePath(base_cache_path);
  int cache_max_size = 0;

  // Make sure we initialize the io_data_ after everything else has been
  // initialized that we might be reading from the IO thread.
  io_data_->Init(cookie_path, cache_path, cache_max_size, state_path_);

  // Listen for bookmark model load, to bootstrap the sync service.
  bookmarks::BookmarkModel* model =
      ios::LocalOrSyncableBookmarkModelFactory::GetForBrowserState(this);
  model->AddObserver(new BookmarkModelLoadedObserver(this));
}

ChromeBrowserStateImpl::~ChromeBrowserStateImpl() {
  BrowserStateDependencyManager::GetInstance()->DestroyBrowserStateServices(
      this);
  // Warning: the order for shutting down the BrowserState objects is important
  // because of interdependencies. Ideally the order for shutting down the
  // objects should be backward of their declaration in class attributes.

  if (pref_proxy_config_tracker_)
    pref_proxy_config_tracker_->DetachFromPrefService();

  // Here, (1) the browser state services may
  // depend on `policy_connector_` and `user_cloud_policy_manager_`, and (2)
  // `policy_connector_` depends on `user_cloud_policy_manager_`. The
  // dependencies have to be shut down backward.
  policy_connector_->Shutdown();
  if (user_cloud_policy_manager_)
    user_cloud_policy_manager_->Shutdown();

  DestroyOffTheRecordChromeBrowserState();
}

ChromeBrowserState* ChromeBrowserStateImpl::GetOriginalChromeBrowserState() {
  return this;
}

ChromeBrowserState*
ChromeBrowserStateImpl::GetOffTheRecordChromeBrowserState() {
  if (!otr_state_) {
    otr_state_.reset(new OffTheRecordChromeBrowserStateImpl(
        GetIOTaskRunner(), this, otr_state_path_));
  }

  return otr_state_.get();
}

bool ChromeBrowserStateImpl::HasOffTheRecordChromeBrowserState() const {
  return !!otr_state_;
}

void ChromeBrowserStateImpl::DestroyOffTheRecordChromeBrowserState() {
  // Tear down both the OTR ChromeBrowserState and the OTR Profile with which
  // it is associated.
  otr_state_.reset();
}

BrowserStatePolicyConnector* ChromeBrowserStateImpl::GetPolicyConnector() {
  return policy_connector_.get();
}

policy::UserCloudPolicyManager*
ChromeBrowserStateImpl::GetUserCloudPolicyManager() {
  return user_cloud_policy_manager_.get();
}

sync_preferences::PrefServiceSyncable*
ChromeBrowserStateImpl::GetSyncablePrefs() {
  DCHECK(prefs_);  // Should explicitly be initialized.
  return prefs_.get();
}

bool ChromeBrowserStateImpl::IsOffTheRecord() const {
  return false;
}

base::FilePath ChromeBrowserStateImpl::GetStatePath() const {
  return state_path_;
}

void ChromeBrowserStateImpl::SetOffTheRecordChromeBrowserState(
    std::unique_ptr<ChromeBrowserState> otr_state) {
  DCHECK(!otr_state_);
  otr_state_ = std::move(otr_state);
}

ChromeBrowserStateIOData* ChromeBrowserStateImpl::GetIOData() {
  return io_data_->io_data();
}

net::URLRequestContextGetter* ChromeBrowserStateImpl::CreateRequestContext(
    ProtocolHandlerMap* protocol_handlers) {
  ApplicationContext* application_context = GetApplicationContext();
  return io_data_
      ->CreateMainRequestContextGetter(
          protocol_handlers, application_context->GetLocalState(),
          application_context->GetIOSChromeIOThread())
      .get();
}

base::WeakPtr<ChromeBrowserState> ChromeBrowserStateImpl::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ChromeBrowserStateImpl::ClearNetworkingHistorySince(
    base::Time time,
    base::OnceClosure completion) {
  io_data_->ClearNetworkingHistorySince(time, std::move(completion));
}

PrefProxyConfigTracker* ChromeBrowserStateImpl::GetProxyConfigTracker() {
  if (!pref_proxy_config_tracker_) {
    pref_proxy_config_tracker_ =
        ProxyServiceFactory::CreatePrefProxyConfigTrackerOfProfile(
            GetPrefs(), GetApplicationContext()->GetLocalState());
  }
  return pref_proxy_config_tracker_.get();
}
