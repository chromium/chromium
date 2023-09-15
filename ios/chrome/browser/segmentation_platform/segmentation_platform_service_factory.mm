// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"

#import "base/feature_list.h"
#import "base/scoped_observation.h"
#import "base/supports_user_data.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "base/time/default_clock.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
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
#import "ios/chrome/browser/history/history_service_factory.h"
#import "ios/chrome/browser/optimization_guide/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/optimization_guide_service_factory.h"
#import "ios/chrome/browser/segmentation_platform/otr_web_state_observer.h"
#import "ios/chrome/browser/segmentation_platform/segmentation_platform_config.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/sync/device_info_sync_service_factory.h"
#import "ios/chrome/browser/sync/session_sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"

namespace segmentation_platform {
namespace {

const base::FilePath::CharType kSegmentationPlatformStorageDirName[] =
    FILE_PATH_LITERAL("Segmentation Platform");

const char kSegmentationPlatformProfileObserverKey[] =
    "segmentation_platform_profile_observer";
const char kSegmentationDeviceSwitcherUserDataKey[] =
    "segmentation_device_switcher_data";
const char kSegmentationTabRankDispatcherUserDataKey[] =
    "segmentation_tab_rank_dispatcher_data";

UkmDataManager* GetUkmDataManager() {
  static base::NoDestructor<DummyUkmDataManager> instance;
  return instance.get();
}

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

// Observes existance of Incognito tabs in the application.
class IncognitoObserver : public OTRWebStateObserver::ObserverClient,
                          public base::SupportsUserData::Data {
 public:
  IncognitoObserver(SegmentationPlatformService* service,
                    OTRWebStateObserver* otr_observer)
      : service_(service) {
    observation_.Observe(otr_observer);
  }

  // OTRWebStateObserver::ObserverClient:
  void OnOTRWebStateCountChanged(bool otr_state_exists) override {
    const bool enable_metrics = !otr_state_exists;
    service_->EnableMetrics(enable_metrics);
  }

 private:
  base::ScopedObservation<OTRWebStateObserver,
                          OTRWebStateObserver::ObserverClient>
      observation_{this};
  const raw_ptr<SegmentationPlatformService> service_;
};

std::unique_ptr<KeyedService> BuildSegmentationPlatformService(
    web::BrowserState* context) {
  if (!base::FeatureList::IsEnabled(features::kSegmentationPlatformFeature)) {
    return nullptr;
  }

  if (!context || context->IsOffTheRecord()) {
    return nullptr;
  }

  ChromeBrowserState* chrome_browser_state =
      ChromeBrowserState::FromBrowserState(context);
  DCHECK(chrome_browser_state);
  const base::FilePath profile_path = chrome_browser_state->GetStatePath();
  auto* optimization_guide =
      OptimizationGuideServiceFactory::GetForBrowserState(chrome_browser_state);

  auto* protodb_provider = chrome_browser_state->GetProtoDatabaseProvider();
  if (!protodb_provider) {
    return nullptr;
  }
  sync_sessions::SessionSyncService* session_sync_service =
      SessionSyncServiceFactory::GetForBrowserState(chrome_browser_state);
  auto tab_fetcher = std::make_unique<TabFetcher>(session_sync_service);

  auto params = std::make_unique<SegmentationPlatformServiceImpl::InitParams>();

  params->history_service = ios::HistoryServiceFactory::GetForBrowserState(
      chrome_browser_state, ServiceAccessType::IMPLICIT_ACCESS);
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

  params->ukm_data_manager = GetUkmDataManager();
  params->profile_prefs = chrome_browser_state->GetPrefs();

  params->configs = GetSegmentationPlatformConfig();
  params->model_provider = std::make_unique<ModelProviderFactoryImpl>(
      optimization_guide, params->configs, params->task_runner);
  params->field_trial_register = std::make_unique<IOSFieldTrialRegisterImpl>();
  auto* field_trial_register = params->field_trial_register.get();
  // Guaranteed to outlive the SegmentationPlatformService, which depends on the
  // DeviceInfoSynceService.
  params->device_info_tracker =
      DeviceInfoSyncServiceFactory::GetForBrowserState(chrome_browser_state)
          ->GetDeviceInfoTracker();
  params->input_delegate_holder = SetUpInputDelegates(
      params->configs, session_sync_service, tab_fetcher.get());
  auto service =
      std::make_unique<SegmentationPlatformServiceImpl>(std::move(params));

  auto* otr_observer =
      GetApplicationContext()->GetSegmentationOTRWebStateObserver();
  // Can be null in tests.
  if (otr_observer) {
    service->SetUserData(
        kSegmentationPlatformProfileObserverKey,
        std::make_unique<IncognitoObserver>(service.get(), otr_observer));
  }
  service->SetUserData(
      kSegmentationDeviceSwitcherUserDataKey,
      std::make_unique<DeviceSwitcherResultDispatcher>(
          service.get(),
          DeviceInfoSyncServiceFactory::GetForBrowserState(chrome_browser_state)
              ->GetDeviceInfoTracker(),
          chrome_browser_state->GetPrefs(), field_trial_register));
  service->SetUserData(
      kSegmentationTabRankDispatcherUserDataKey,
      std::make_unique<TabRankDispatcher>(service.get(), session_sync_service,
                                          std::move(tab_fetcher)));
  return service;
}

}  // namespace

// static
SegmentationPlatformService*
SegmentationPlatformServiceFactory::GetForBrowserState(
    ChromeBrowserState* context) {
  if (!base::FeatureList::IsEnabled(features::kSegmentationPlatformFeature)) {
    return nullptr;
  }
  return static_cast<SegmentationPlatformService*>(
      GetInstance()->GetServiceForBrowserState(context, /*create=*/true));
}

// static
SegmentationPlatformServiceFactory*
SegmentationPlatformServiceFactory::GetInstance() {
  static base::NoDestructor<SegmentationPlatformServiceFactory> instance;
  return instance.get();
}

SegmentationPlatformServiceFactory::SegmentationPlatformServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "SegmentationPlatformService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(OptimizationGuideServiceFactory::GetInstance());
  DependsOn(ios::HistoryServiceFactory::GetInstance());
  DependsOn(DeviceInfoSyncServiceFactory::GetInstance());
  DependsOn(SessionSyncServiceFactory::GetInstance());
}

SegmentationPlatformServiceFactory::~SegmentationPlatformServiceFactory() =
    default;

// static
DeviceSwitcherResultDispatcher*
SegmentationPlatformServiceFactory::GetDispatcherForBrowserState(
    ChromeBrowserState* context) {
  CHECK(!context->IsOffTheRecord());
  SegmentationPlatformService* service = GetForBrowserState(context);
  if (!service) {
    return nullptr;
  }
  return static_cast<segmentation_platform::DeviceSwitcherResultDispatcher*>(
      service->GetUserData(kSegmentationDeviceSwitcherUserDataKey));
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

bool SegmentationPlatformServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace segmentation_platform
