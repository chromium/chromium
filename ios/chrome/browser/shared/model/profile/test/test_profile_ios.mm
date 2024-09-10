// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"

#import <tuple>

#import "base/base_paths.h"
#import "base/files/file_util.h"
#import "base/functional/callback_helpers.h"
#import "base/location.h"
#import "base/logging.h"
#import "base/memory/ptr_util.h"
#import "base/path_service.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/single_thread_task_runner.h"
#import "base/task/thread_pool.h"
#import "base/test/test_file_util.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#import "components/profile_metrics/browser_profile_type.h"
#import "components/supervised_user/core/browser/supervised_user_settings_service.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "components/user_prefs/user_prefs.h"
#import "ios/chrome/browser/prefs/model/ios_chrome_pref_service_factory.h"
#import "ios/chrome/browser/profile/model/keyed_service_factories.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_settings_service_factory.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "net/url_request/url_request_test_util.h"

namespace {

// Assigns `testing_factories` to `browser_state`.
void AssignTestingFactories(
    TestChromeBrowserState* browser_state,
    TestChromeBrowserState::TestingFactories testing_factories) {
  for (auto& item : testing_factories) {
    absl::visit(
        [browser_state](auto& p) {
          p.first->SetTestingFactory(browser_state, std::move(p.second));
        },
        item.service_factory_and_testing_factory);
  }
}

}  // namespace

TestChromeBrowserState::TestingFactory::TestingFactory(
    BrowserStateKeyedServiceFactory* service_factory,
    BrowserStateKeyedServiceFactory::TestingFactory testing_factory)
    : service_factory_and_testing_factory(
          std::make_pair(service_factory, std::move(testing_factory))) {}

TestChromeBrowserState::TestingFactory::TestingFactory(
    RefcountedBrowserStateKeyedServiceFactory* service_factory,
    RefcountedBrowserStateKeyedServiceFactory::TestingFactory testing_factory)
    : service_factory_and_testing_factory(
          std::make_pair(service_factory, std::move(testing_factory))) {}

TestChromeBrowserState::TestingFactory::TestingFactory(TestingFactory&&) =
    default;

TestChromeBrowserState::TestingFactory&
TestChromeBrowserState::TestingFactory::operator=(TestingFactory&&) = default;

TestChromeBrowserState::TestingFactory::~TestingFactory() = default;

TestChromeBrowserState::TestingFactories::TestingFactories() = default;

TestChromeBrowserState::TestingFactories::TestingFactories(TestingFactories&&) =
    default;

TestChromeBrowserState::TestingFactories&
TestChromeBrowserState::TestingFactories::operator=(TestingFactories&&) =
    default;

TestChromeBrowserState::TestingFactories::~TestingFactories() = default;

TestChromeBrowserState::TestChromeBrowserState(
    const base::FilePath& state_path,
    TestChromeBrowserState* original_browser_state,
    TestingFactories testing_factories)
    : ChromeBrowserState(state_path,
                         /*browser_state_name=*/std::string(),
                         original_browser_state->GetIOTaskRunner()),
      testing_prefs_(nullptr),
      otr_browser_state_(nullptr),
      original_browser_state_(original_browser_state) {
  DCHECK(original_browser_state_);

  BrowserStateDependencyManager::GetInstance()->MarkBrowserStateLive(this);

  AssignTestingFactories(this, std::move(testing_factories));
  profile_metrics::SetBrowserProfileType(
      this, profile_metrics::BrowserProfileType::kIncognito);

  // Not calling Init() here as the bi-directional link between original and
  // off-the-record TestChromeBrowserState must be established before this
  // method can be called.
}

TestChromeBrowserState::TestChromeBrowserState(
    const base::FilePath& state_path,
    std::string_view browser_state_name,
    std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs,
    TestingFactories testing_factories,
    std::unique_ptr<BrowserStatePolicyConnector> policy_connector,
    std::unique_ptr<policy::UserCloudPolicyManager> user_cloud_policy_manager)
    : ChromeBrowserState(
          state_path,
          browser_state_name,
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      prefs_(std::move(prefs)),
      testing_prefs_(nullptr),
      user_cloud_policy_manager_(std::move(user_cloud_policy_manager)),
      policy_connector_(std::move(policy_connector)),
      otr_browser_state_(nullptr),
      original_browser_state_(nullptr) {
  DCHECK(!browser_state_name.empty());

  BrowserStateDependencyManager::GetInstance()->MarkBrowserStateLive(this);

  AssignTestingFactories(this, std::move(testing_factories));
  profile_metrics::SetBrowserProfileType(
      this, profile_metrics::BrowserProfileType::kRegular);

  Init();
}

TestChromeBrowserState::~TestChromeBrowserState() {
  // Allows blocking in this scope for testing.
  base::ScopedAllowBlockingForTesting allow_bocking;

  // If this TestChromeBrowserState owns an incognito TestChromeBrowserState,
  // tear it down first.
  otr_browser_state_.reset();

  // Here, (1) the browser state services may
  // depend on `policy_connector_` and `user_cloud_policy_manager_`, and (2)
  // `policy_connector_` depends on `user_cloud_policy_manager_`. The
  // dependencies have to be shut down backward.
  policy_connector_->Shutdown();
  if (user_cloud_policy_manager_) {
    user_cloud_policy_manager_->Shutdown();
  }

  BrowserStateDependencyManager::GetInstance()->DestroyBrowserStateServices(
      this);
}

void TestChromeBrowserState::Init() {
  // Allows blocking in this scope so directory manipulation can happen in this
  // scope for testing.
  base::ScopedAllowBlockingForTesting allow_bocking;

  // If threads have been initialized, we should be on the UI thread.
  DCHECK(!web::WebThread::IsThreadInitialized(web::WebThread::UI) ||
         web::WebThread::CurrentlyOn(web::WebThread::UI));

  const base::FilePath state_path = GetStatePath();
  if (!base::PathExists(state_path)) {
    base::CreateDirectory(state_path);
  }

  // Normally this would happen during browser startup, but for tests we need to
  // trigger creation of Profile-related services.
  EnsureProfileKeyedServiceFactoriesBuilt();

  if (prefs_) {
    // If user passed a custom PrefServiceSyncable, then leave `testing_prefs_`
    // unset as it is not possible to determine its type.
  } else if (IsOffTheRecord()) {
    // This leaves `testing_prefs_` unset as CreateIncognitoBrowserStatePrefs()
    // does not return a TestingPrefServiceSyncable.
    DCHECK(original_browser_state_);
    prefs_ =
        CreateIncognitoBrowserStatePrefs(original_browser_state_->prefs_.get());
  } else {
    testing_prefs_ = new sync_preferences::TestingPrefServiceSyncable();
    RegisterBrowserStatePrefs(testing_prefs_->registry());
    prefs_.reset(testing_prefs_);
  }
  user_prefs::UserPrefs::Set(this, prefs_.get());

  // Prefs for incognito TestChromeBrowserState are set in
  // CreateIncognitoBrowserStatePrefs().
  if (!IsOffTheRecord()) {
    user_prefs::PrefRegistrySyncable* pref_registry =
        static_cast<user_prefs::PrefRegistrySyncable*>(
            prefs_->DeprecatedGetPrefRegistry());
    BrowserStateDependencyManager::GetInstance()
        ->RegisterBrowserStatePrefsForServices(pref_registry);
  }

  BrowserStateDependencyManager::GetInstance()
      ->CreateBrowserStateServicesForTest(this);
  // `SupervisedUserSettingsService` needs to be initialized for SyncService.
  SupervisedUserSettingsServiceFactory::GetForBrowserState(this)->Init(
      GetStatePath(), GetIOTaskRunner().get(),
      /*load_synchronously=*/true);
}

bool TestChromeBrowserState::IsOffTheRecord() const {
  return original_browser_state_ != nullptr;
}

scoped_refptr<base::SequencedTaskRunner>
TestChromeBrowserState::GetIOTaskRunner() {
  return base::SingleThreadTaskRunner::GetCurrentDefault();
}

ChromeBrowserState* TestChromeBrowserState::GetOriginalChromeBrowserState() {
  return GetOriginalProfile();
}

bool TestChromeBrowserState::HasOffTheRecordChromeBrowserState() const {
  return HasOffTheRecordProfile();
}

ChromeBrowserState*
TestChromeBrowserState::GetOffTheRecordChromeBrowserState() {
  return GetOffTheRecordProfile();
}

void TestChromeBrowserState::DestroyOffTheRecordChromeBrowserState() {
  return DestroyOffTheRecordProfile();
}

TestChromeBrowserState*
TestChromeBrowserState::CreateOffTheRecordBrowserStateWithTestingFactories(
    TestingFactories testing_factories) {
  return CreateOffTheRecordProfileWithTestingFactories(
      std::move(testing_factories));
}

ProfileIOS* TestProfileIOS::GetOriginalProfile() {
  if (IsOffTheRecord()) {
    return original_browser_state_;
  }
  return this;
}

bool TestProfileIOS::HasOffTheRecordProfile() const {
  return otr_browser_state_ != nullptr;
}

ProfileIOS* TestProfileIOS::GetOffTheRecordProfile() {
  if (IsOffTheRecord()) {
    return this;
  }

  if (otr_browser_state_) {
    return otr_browser_state_.get();
  }

  return CreateOffTheRecordBrowserStateWithTestingFactories();
}

void TestProfileIOS::DestroyOffTheRecordProfile() {
  DCHECK(!IsOffTheRecord());
  otr_browser_state_.reset();
}

TestProfileIOS* TestProfileIOS::CreateOffTheRecordProfileWithTestingFactories(
    TestingFactories testing_factories) {
  DCHECK(!IsOffTheRecord());
  DCHECK(!otr_browser_state_);
  otr_browser_state_.reset(new TestChromeBrowserState(
      GetOffTheRecordStatePath(), this, std::move(testing_factories)));
  otr_browser_state_->Init();
  return otr_browser_state_.get();
}

PrefProxyConfigTracker* TestChromeBrowserState::GetProxyConfigTracker() {
  return nullptr;
}

BrowserStatePolicyConnector* TestChromeBrowserState::GetPolicyConnector() {
  return policy_connector_.get();
}

sync_preferences::PrefServiceSyncable*
TestChromeBrowserState::GetSyncablePrefs() {
  return prefs_.get();
}

ProfileIOSIOData* TestChromeBrowserState::GetIOData() {
  return nullptr;
}

void TestChromeBrowserState::ClearNetworkingHistorySince(
    base::Time time,
    base::OnceClosure completion) {
  if (!completion.is_null()) {
    std::move(completion).Run();
  }
}

net::URLRequestContextGetter* TestChromeBrowserState::CreateRequestContext(
    ProtocolHandlerMap* protocol_handlers) {
  return new net::TestURLRequestContextGetter(web::GetIOThreadTaskRunner({}));
}

base::WeakPtr<ChromeBrowserState> TestChromeBrowserState::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

sync_preferences::TestingPrefServiceSyncable*
TestChromeBrowserState::GetTestingPrefService() {
  DCHECK(prefs_);
  DCHECK(testing_prefs_);
  return testing_prefs_;
}

policy::UserCloudPolicyManager*
TestChromeBrowserState::GetUserCloudPolicyManager() {
  return user_cloud_policy_manager_.get();
}

scoped_refptr<network::SharedURLLoaderFactory>
TestChromeBrowserState::GetSharedURLLoaderFactory() {
  return test_shared_url_loader_factory_
             ? test_shared_url_loader_factory_
             : BrowserState::GetSharedURLLoaderFactory();
}

void TestChromeBrowserState::SetSharedURLLoaderFactory(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory) {
  test_shared_url_loader_factory_ = std::move(shared_url_loader_factory);
}

TestChromeBrowserState::Builder::Builder() = default;

TestChromeBrowserState::Builder::Builder(Builder&&) = default;

TestChromeBrowserState::Builder& TestChromeBrowserState::Builder::operator=(
    Builder&&) = default;

TestChromeBrowserState::Builder::~Builder() = default;

TestChromeBrowserState::Builder&
TestChromeBrowserState::Builder::AddTestingFactory(
    BrowserStateKeyedServiceFactory* service_factory,
    BrowserStateKeyedServiceFactory::TestingFactory testing_factory) {
  testing_factories_.emplace_back(service_factory, std::move(testing_factory));
  return *this;
}

TestChromeBrowserState::Builder&
TestChromeBrowserState::Builder::AddTestingFactory(
    RefcountedBrowserStateKeyedServiceFactory* service_factory,
    RefcountedBrowserStateKeyedServiceFactory::TestingFactory testing_factory) {
  testing_factories_.emplace_back(service_factory, std::move(testing_factory));
  return *this;
}

TestChromeBrowserState::Builder&
TestChromeBrowserState::Builder::AddTestingFactories(
    TestingFactories testing_factories) {
  for (auto& item : testing_factories) {
    testing_factories.emplace_back(std::move(item));
  }
  return *this;
}

TestChromeBrowserState::Builder& TestChromeBrowserState::Builder::SetName(
    const std::string& name) {
  browser_state_name_ = name;
  return *this;
}

TestChromeBrowserState::Builder&
TestChromeBrowserState::Builder::SetPrefService(
    std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs) {
  pref_service_ = std::move(prefs);
  return *this;
}

TestChromeBrowserState::Builder&
TestChromeBrowserState::Builder::SetPolicyConnector(
    std::unique_ptr<BrowserStatePolicyConnector> policy_connector) {
  policy_connector_ = std::move(policy_connector);
  return *this;
}

TestChromeBrowserState::Builder&
TestChromeBrowserState::Builder::SetUserCloudPolicyManager(
    std::unique_ptr<policy::UserCloudPolicyManager> user_cloud_policy_manager) {
  user_cloud_policy_manager_ = std::move(user_cloud_policy_manager);
  return *this;
}

std::unique_ptr<TestChromeBrowserState>
TestChromeBrowserState::Builder::Build() && {
  return std::move(*this).Build(base::CreateUniqueTempDirectoryScopedToTest());
}

std::unique_ptr<TestChromeBrowserState> TestChromeBrowserState::Builder::Build(
    const base::FilePath& data_dir) && {
  CHECK(!data_dir.empty());

  // Ensure that the name is not empty.
  if (browser_state_name_.empty()) {
    browser_state_name_ = "Test";
  }

  return base::WrapUnique(new TestChromeBrowserState(
      data_dir.Append(browser_state_name_), browser_state_name_,
      std::move(pref_service_), std::move(testing_factories_),
      std::move(policy_connector_), std::move(user_cloud_policy_manager_)));
}
