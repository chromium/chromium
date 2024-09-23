// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"

#import "base/feature_list.h"
#import "base/hash/hash.h"
#import "base/scoped_observation.h"
#import "base/supports_user_data.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "base/time/default_clock.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#import "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"
#import "components/segmentation_platform/embedder/input_delegate/shopping_service_input_delegate.h"
#import "components/segmentation_platform/embedder/input_delegate/tab_rank_dispatcher.h"
#import "components/segmentation_platform/embedder/input_delegate/tab_session_source.h"
#import "components/segmentation_platform/embedder/model_provider_factory_impl.h"
#import "components/segmentation_platform/embedder/tab_fetcher.h"
#import "components/segmentation_platform/internal/dummy_ukm_data_manager.h"
#import "components/segmentation_platform/internal/segmentation_platform_service_impl.h"
#import "components/segmentation_platform/internal/ukm_data_manager.h"
#import "components/segmentation_platform/public/config.h"
#import "components/segmentation_platform/public/features.h"
#import "components/sync_device_info/device_info_sync_service.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_config.h"
#import "ios/chrome/browser/segmentation_platform/model/ukm_database_client.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/incognito_session_tracker.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/sync/model/device_info_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/session_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"

namespace segmentation_platform {
namespace {

const base::FilePath::CharType kSegmentationPlatformStorageDirName[] =
    FILE_PATH_LITERAL("Segmentation Platform");

const char kSegmentationPlatformServiceSubscription[] =
    "segmentation_platform_incognito_tracker_subscription";
const char kSegmentationDeviceSwitcherUserDataKey[] =
    "segmentation_device_switcher_data";
const char kSegmentationTabRankDispatcherUserDataKey[] =
    "segmentation_tab_rank_dispatcher_data";
const char kSegmentationHomeModulesCardRegistryDataKey[] =
    "segmentation_home_modules_card_registry";

std::unique_ptr<processing::InputDelegateHolder> SetUpInputDelegates(
    std::vector<std::unique_ptr<Config>>& configs,
    sync_sessions::SessionSyncService* session_sync_service,
    TabFetcher* tab_fetcher) {
  auto input_delegate_holder =
      std::make_unique<processing::InputDelegateHolder>();
  for (auto& config : configs) {
    for (auto& id : config->input_delegates) {
      input_delegate_holder->SetDelegate(id.first, std::move(id.second));
    }
  }

  input_delegate_holder->SetDelegate(
      proto::CustomInput::FILL_TAB_METRICS,
      std::make_unique<segmentation_platform::processing::TabSessionSource>(
          session_sync_service, tab_fetcher));

  // Add shareable input delegates here.

  return input_delegate_holder;
}

// Allows attaching a base::CallbackListSubscription as to a
// SegmentationPlatformService instance.
class SegmentationPlatformServiceSubscription
    : public base::SupportsUserData::Data {
 public:
  SegmentationPlatformServiceSubscription(
      base::CallbackListSubscription subscription)
      : subscription_(std::move(subscription)) {}

 private:
  base::CallbackListSubscription subscription_;
};

// Returns the ShoppingService for `weak_profile`.
ShoppingService* GetShoppingService(base::WeakPtr<ProfileIOS> weak_profile) {
  if (ProfileIOS* profile = weak_profile.get()) {
    return commerce::ShoppingServiceFactory::GetForProfile(profile);
  }

  return nullptr;
}

std::unique_ptr<KeyedService> BuildSegmentationPlatformService(
    web::BrowserState* context) {
  DCHECK(context);
  DCHECK(!context->IsOffTheRecord());
  if (!base::FeatureList::IsEnabled(features::kSegmentationPlatformFeature)) {
    return nullptr;
  }

  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  DCHECK(profile);
  const base::FilePath profile_path = profile->GetStatePath();
  auto* optimization_guide =
      OptimizationGuideServiceFactory::GetForProfile(profile);

  auto* protodb_provider = profile->GetProtoDatabaseProvider();
  if (!protodb_provider) {
    return nullptr;
  }
  sync_sessions::SessionSyncService* session_sync_service =
      SessionSyncServiceFactory::GetForProfile(profile);
  auto tab_fetcher = std::make_unique<TabFetcher>(session_sync_service);

  auto params = std::make_unique<SegmentationPlatformServiceImpl::InitParams>();
  params->profile_id = params->profile_id =
      base::NumberToString(base::PersistentHash(profile_path.value()));
  params->history_service = ios::HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::IMPLICIT_ACCESS);
  base::TaskPriority priority = base::TaskPriority::BEST_EFFORT;
  if (base::FeatureList::IsEnabled(features::kSegmentationPlatformUserVisibleTaskRunner)) {
    priority = base::TaskPriority::USER_VISIBLE;
  }
  params->task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), priority});
  params->storage_dir =
      profile_path.Append(kSegmentationPlatformStorageDirName);
  params->db_provider = protodb_provider;
  params->clock = base::DefaultClock::GetInstance();

  params->ukm_data_manager =
      UkmDatabaseClientHolder::GetClientInstance(profile).GetUkmDataManager();
  params->profile_prefs = profile->GetPrefs();

  auto home_modules_card_registry =
      std::make_unique<home_modules::HomeModulesCardRegistry>(
          profile->GetPrefs());
  params->configs =
      GetSegmentationPlatformConfig(home_modules_card_registry.get());
  params->model_provider = std::make_unique<ModelProviderFactoryImpl>(
      optimization_guide, params->configs, params->task_runner);
  params->field_trial_register = std::make_unique<IOSFieldTrialRegisterImpl>();
  auto* field_trial_register = params->field_trial_register.get();
  // Guaranteed to outlive the SegmentationPlatformService, which depends on the
  // DeviceInfoSynceService.
  params->device_info_tracker =
      DeviceInfoSyncServiceFactory::GetForProfile(profile)
          ->GetDeviceInfoTracker();
  params->input_delegate_holder = SetUpInputDelegates(
      params->configs, session_sync_service, tab_fetcher.get());

  // Set up Shopping Service input delegate.
  auto shopping_service_callback =
      base::BindRepeating(&GetShoppingService, profile->AsWeakPtr());

  params->input_delegate_holder->SetDelegate(
      proto::CustomInput::FILL_FROM_SHOPPING_SERVICE,
      std::make_unique<ShoppingServiceInputDelegate>(
          shopping_service_callback));

  auto service =
      std::make_unique<SegmentationPlatformServiceImpl>(std::move(params));

  // The IncognitoSessionTracker can be null during tests.
  if (IncognitoSessionTracker* tracker =
          GetApplicationContext()->GetIncognitoSessionTracker()) {
    // Usage of base::Unretained(...) is safe since the callback subscription
    // is owned by the SegmentationPlatformServiceSubscription attached to the
    // SegmentationPlatformService and deleted before the object itself.
    service->SetUserData(
        kSegmentationPlatformServiceSubscription,
        std::make_unique<SegmentationPlatformServiceSubscription>(
            tracker->RegisterCallback(base::BindRepeating(
                [](SegmentationPlatformService* service, bool has_otr_tabs) {
                  service->EnableMetrics(!has_otr_tabs);
                },
                base::Unretained(service.get())))));
  }

  service->SetUserData(kSegmentationDeviceSwitcherUserDataKey,
                       std::make_unique<DeviceSwitcherResultDispatcher>(
                           service.get(),
                           DeviceInfoSyncServiceFactory::GetForProfile(profile)
                               ->GetDeviceInfoTracker(),
                           profile->GetPrefs(), field_trial_register));
  service->SetUserData(
      kSegmentationTabRankDispatcherUserDataKey,
      std::make_unique<TabRankDispatcher>(service.get(), session_sync_service,
                                          std::move(tab_fetcher)));
  service->SetUserData(kSegmentationHomeModulesCardRegistryDataKey,
                       std::move(home_modules_card_registry));
  return service;
}

}  // namespace

// static
SegmentationPlatformService* SegmentationPlatformServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  if (!base::FeatureList::IsEnabled(features::kSegmentationPlatformFeature)) {
    return nullptr;
  }
  return GetInstance()->GetServiceForProfileAs<SegmentationPlatformService>(
      profile, /*create=*/true);
}

// static
SegmentationPlatformServiceFactory*
SegmentationPlatformServiceFactory::GetInstance() {
  static base::NoDestructor<SegmentationPlatformServiceFactory> instance;
  return instance.get();
}

SegmentationPlatformServiceFactory::SegmentationPlatformServiceFactory()
    : ProfileKeyedServiceFactoryIOS("SegmentationPlatformService",
                                    ServiceCreation::kCreateWithProfile,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(OptimizationGuideServiceFactory::GetInstance());
  DependsOn(ios::HistoryServiceFactory::GetInstance());
  DependsOn(DeviceInfoSyncServiceFactory::GetInstance());
  DependsOn(SessionSyncServiceFactory::GetInstance());
}

SegmentationPlatformServiceFactory::~SegmentationPlatformServiceFactory() =
    default;

// static
DeviceSwitcherResultDispatcher*
SegmentationPlatformServiceFactory::GetDispatcherForProfile(
    ProfileIOS* profile) {
  CHECK(!profile->IsOffTheRecord());
  SegmentationPlatformService* service = GetForProfile(profile);
  if (!service) {
    return nullptr;
  }
  return static_cast<segmentation_platform::DeviceSwitcherResultDispatcher*>(
      service->GetUserData(kSegmentationDeviceSwitcherUserDataKey));
}

// static
home_modules::HomeModulesCardRegistry*
SegmentationPlatformServiceFactory::GetHomeCardRegistryForProfile(
    ProfileIOS* profile) {
  CHECK(!profile->IsOffTheRecord());
  SegmentationPlatformService* service = GetForProfile(profile);
  if (!service) {
    return nullptr;
  }
  return static_cast<home_modules::HomeModulesCardRegistry*>(
      service->GetUserData(kSegmentationHomeModulesCardRegistryDataKey));
}

// static
BrowserStateKeyedServiceFactory::TestingFactory
SegmentationPlatformServiceFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildSegmentationPlatformService);
}

std::unique_ptr<KeyedService>
SegmentationPlatformServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildSegmentationPlatformService(context);
}

void SegmentationPlatformServiceFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  home_modules::HomeModulesCardRegistry::RegisterProfilePrefs(registry);
}

}  // namespace segmentation_platform
