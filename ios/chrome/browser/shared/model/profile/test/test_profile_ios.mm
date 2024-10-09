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

// Assigns `testing_factories` to `profile`.
void AssignTestingFactories(
    TestProfileIOS* profile,
    TestProfileIOS::TestingFactories testing_factories) {
  for (auto& item : testing_factories) {
    absl::visit(
        [profile](auto& p) {
          p.first->SetTestingFactory(profile, std::move(p.second));
        },
        item.service_factory_and_testing_factory);
  }
}

}  // namespace

TestProfileIOS::TestingFactory::TestingFactory(
    BrowserStateKeyedServiceFactory* service_factory,
    BrowserStateKeyedServiceFactory::TestingFactory testing_factory)
    : service_factory_and_testing_factory(
          std::make_pair(service_factory, std::move(testing_factory))) {}

TestProfileIOS::TestingFactory::TestingFactory(
    RefcountedBrowserStateKeyedServiceFactory* service_factory,
    RefcountedBrowserStateKeyedServiceFactory::TestingFactory testing_factory)
    : service_factory_and_testing_factory(
          std::make_pair(service_factory, std::move(testing_factory))) {}

TestProfileIOS::TestingFactory::TestingFactory(TestingFactory&&) = default;

TestProfileIOS::TestingFactory& TestProfileIOS::TestingFactory::operator=(
    TestingFactory&&) = default;

TestProfileIOS::TestingFactory::~TestingFactory() = default;

TestProfileIOS::TestingFactories::TestingFactories() = default;

TestProfileIOS::TestingFactories::TestingFactories(TestingFactories&&) =
    default;

TestProfileIOS::TestingFactories& TestProfileIOS::TestingFactories::operator=(
    TestingFactories&&) = default;

TestProfileIOS::TestingFactories::~TestingFactories() = default;

TestProfileIOS::TestProfileIOS(const base::FilePath& state_path,
                               TestProfileIOS* original_profile,
                               TestingFactories testing_factories)
    : ProfileIOS(state_path,
                 /*profile_name=*/std::string(),
                 original_profile->GetIOTaskRunner()),
      testing_prefs_(nullptr),
      otr_profile_(nullptr),
      original_profile_(original_profile) {
  DCHECK(original_profile_);

  BrowserStateDependencyManager::GetInstance()->MarkBrowserStateLive(this);

  AssignTestingFactories(this, std::move(testing_factories));
  profile_metrics::SetBrowserProfileType(
      this, profile_metrics::BrowserProfileType::kIncognito);

  // Not calling Init() here as the bi-directional link between original and
  // off-the-record TestProfileIOS must be established before this
  // method can be called.
}

TestProfileIOS::TestProfileIOS(
    const base::FilePath& state_path,
    std::string_view profile_name,
    std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs,
    TestingFactories testing_factories,
    std::unique_ptr<BrowserStatePolicyConnector> policy_connector,
    std::unique_ptr<policy::UserCloudPolicyManager> user_cloud_policy_manager)
    : ProfileIOS(
          state_path,
          profile_name,
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      prefs_(std::move(prefs)),
      testing_prefs_(nullptr),
      user_cloud_policy_manager_(std::move(user_cloud_policy_manager)),
      policy_connector_(std::move(policy_connector)),
      otr_profile_(nullptr),
      original_profile_(nullptr) {
  DCHECK(!profile_name.empty());

  BrowserStateDependencyManager::GetInstance()->MarkBrowserStateLive(this);

  AssignTestingFactories(this, std::move(testing_factories));
  profile_metrics::SetBrowserProfileType(
      this, profile_metrics::BrowserProfileType::kRegular);

  Init();
}

TestProfileIOS::~TestProfileIOS() {
  // Allows blocking in this scope for testing.
  base::ScopedAllowBlockingForTesting allow_bocking;

  // If this TestProfileIOS owns an incognito TestProfileIOS,
  // tear it down first.
  otr_profile_.reset();

  // Here, (1) the profile services may
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

void TestProfileIOS::Init() {
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
    // This leaves `testing_prefs_` unset as CreateIncognitoProfilePrefs()
    // does not return a TestingPrefServiceSyncable.
    DCHECK(original_profile_);
    prefs_ = CreateIncognitoProfilePrefs(original_profile_->prefs_.get());
  } else {
    testing_prefs_ = new sync_preferences::TestingPrefServiceSyncable();
    RegisterProfilePrefs(testing_prefs_->registry());
    prefs_.reset(testing_prefs_);
  }
  user_prefs::UserPrefs::Set(this, prefs_.get());

  // Prefs for incognito TestProfileIOS are set in
  // CreateIncognitoProfilePrefs().
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
  SupervisedUserSettingsServiceFactory::GetForProfile(this)->Init(
      GetStatePath(), GetIOTaskRunner().get(),
      /*load_synchronously=*/true);
}

bool TestProfileIOS::IsOffTheRecord() const {
  return original_profile_ != nullptr;
}

scoped_refptr<base::SequencedTaskRunner> TestProfileIOS::GetIOTaskRunner() {
  return base::SingleThreadTaskRunner::GetCurrentDefault();
}

ProfileIOS* TestProfileIOS::GetOriginalChromeBrowserState() {
  return GetOriginalProfile();
}

bool TestProfileIOS::HasOffTheRecordChromeBrowserState() const {
  return HasOffTheRecordProfile();
}

ProfileIOS* TestProfileIOS::GetOffTheRecordChromeBrowserState() {
  return GetOffTheRecordProfile();
}

void TestProfileIOS::DestroyOffTheRecordChromeBrowserState() {
  return DestroyOffTheRecordProfile();
}

TestProfileIOS*
TestProfileIOS::CreateOffTheRecordBrowserStateWithTestingFactories(
    TestingFactories testing_factories) {
  return CreateOffTheRecordProfileWithTestingFactories(
      std::move(testing_factories));
}

ProfileIOS* TestProfileIOS::GetOriginalProfile() {
  if (IsOffTheRecord()) {
    return original_profile_;
  }
  return this;
}

bool TestProfileIOS::HasOffTheRecordProfile() const {
  return otr_profile_ != nullptr;
}

ProfileIOS* TestProfileIOS::GetOffTheRecordProfile() {
  if (IsOffTheRecord()) {
    return this;
  }

  if (otr_profile_) {
    return otr_profile_.get();
  }

  return CreateOffTheRecordBrowserStateWithTestingFactories();
}

void TestProfileIOS::DestroyOffTheRecordProfile() {
  DCHECK(!IsOffTheRecord());
  otr_profile_.reset();
}

TestProfileIOS* TestProfileIOS::CreateOffTheRecordProfileWithTestingFactories(
    TestingFactories testing_factories) {
  DCHECK(!IsOffTheRecord());
  DCHECK(!otr_profile_);
  otr_profile_.reset(new TestProfileIOS(GetOffTheRecordStatePath(), this,
                                        std::move(testing_factories)));
  otr_profile_->Init();
  return otr_profile_.get();
}

PrefProxyConfigTracker* TestProfileIOS::GetProxyConfigTracker() {
  return nullptr;
}

BrowserStatePolicyConnector* TestProfileIOS::GetPolicyConnector() {
  return policy_connector_.get();
}

sync_preferences::PrefServiceSyncable* TestProfileIOS::GetSyncablePrefs() {
  return prefs_.get();
}

const sync_preferences::PrefServiceSyncable* TestProfileIOS::GetSyncablePrefs()
    const {
  return prefs_.get();
}

ProfileIOSIOData* TestProfileIOS::GetIOData() {
  return nullptr;
}

void TestProfileIOS::ClearNetworkingHistorySince(base::Time time,
                                                 base::OnceClosure completion) {
  if (!completion.is_null()) {
    std::move(completion).Run();
  }
}

net::URLRequestContextGetter* TestProfileIOS::CreateRequestContext(
    ProtocolHandlerMap* protocol_handlers) {
  return new net::TestURLRequestContextGetter(web::GetIOThreadTaskRunner({}));
}

base::WeakPtr<ProfileIOS> TestProfileIOS::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

sync_preferences::TestingPrefServiceSyncable*
TestProfileIOS::GetTestingPrefService() {
  DCHECK(prefs_);
  DCHECK(testing_prefs_);
  return testing_prefs_;
}

policy::UserCloudPolicyManager* TestProfileIOS::GetUserCloudPolicyManager() {
  return user_cloud_policy_manager_.get();
}

scoped_refptr<network::SharedURLLoaderFactory>
TestProfileIOS::GetSharedURLLoaderFactory() {
  return test_shared_url_loader_factory_
             ? test_shared_url_loader_factory_
             : BrowserState::GetSharedURLLoaderFactory();
}

void TestProfileIOS::SetSharedURLLoaderFactory(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory) {
  test_shared_url_loader_factory_ = std::move(shared_url_loader_factory);
}

TestProfileIOS::Builder::Builder() = default;

TestProfileIOS::Builder::Builder(Builder&&) = default;

TestProfileIOS::Builder& TestProfileIOS::Builder::operator=(Builder&&) =
    default;

TestProfileIOS::Builder::~Builder() = default;

TestProfileIOS::Builder& TestProfileIOS::Builder::AddTestingFactory(
    BrowserStateKeyedServiceFactory* service_factory,
    BrowserStateKeyedServiceFactory::TestingFactory testing_factory) {
  testing_factories_.emplace_back(service_factory, std::move(testing_factory));
  return *this;
}

TestProfileIOS::Builder& TestProfileIOS::Builder::AddTestingFactory(
    RefcountedBrowserStateKeyedServiceFactory* service_factory,
    RefcountedBrowserStateKeyedServiceFactory::TestingFactory testing_factory) {
  testing_factories_.emplace_back(service_factory, std::move(testing_factory));
  return *this;
}

TestProfileIOS::Builder& TestProfileIOS::Builder::AddTestingFactories(
    TestingFactories testing_factories) {
  for (auto& item : testing_factories) {
    testing_factories.emplace_back(std::move(item));
  }
  return *this;
}

TestProfileIOS::Builder& TestProfileIOS::Builder::SetName(
    const std::string& name) {
  profile_name_ = name;
  return *this;
}

TestProfileIOS::Builder& TestProfileIOS::Builder::SetPrefService(
    std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs) {
  pref_service_ = std::move(prefs);
  return *this;
}

TestProfileIOS::Builder& TestProfileIOS::Builder::SetPolicyConnector(
    std::unique_ptr<BrowserStatePolicyConnector> policy_connector) {
  policy_connector_ = std::move(policy_connector);
  return *this;
}

TestProfileIOS::Builder& TestProfileIOS::Builder::SetUserCloudPolicyManager(
    std::unique_ptr<policy::UserCloudPolicyManager> user_cloud_policy_manager) {
  user_cloud_policy_manager_ = std::move(user_cloud_policy_manager);
  return *this;
}

std::unique_ptr<TestProfileIOS> TestProfileIOS::Builder::Build() && {
  return std::move(*this).Build(base::CreateUniqueTempDirectoryScopedToTest());
}

std::unique_ptr<TestProfileIOS> TestProfileIOS::Builder::Build(
    const base::FilePath& data_dir) && {
  CHECK(!data_dir.empty());

  // Ensure that the name is not empty.
  if (profile_name_.empty()) {
    profile_name_ = "Test";
  }

  return base::WrapUnique(new TestProfileIOS(
      data_dir.Append(profile_name_), profile_name_, std::move(pref_service_),
      std::move(testing_factories_), std::move(policy_connector_),
      std::move(user_cloud_policy_manager_)));
}
