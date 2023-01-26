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
#import "components/segmentation_platform/embedder/model_provider_factory_impl.h"
#import "components/segmentation_platform/internal/dummy_ukm_data_manager.h"
#import "components/segmentation_platform/internal/segmentation_platform_service_impl.h"
#import "components/segmentation_platform/internal/ukm_data_manager.h"
#import "components/segmentation_platform/public/config.h"
#import "components/segmentation_platform/public/features.h"
#import "components/sync_device_info/device_info_sync_service.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/history/history_service_factory.h"
#import "ios/chrome/browser/optimization_guide/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/optimization_guide_service_factory.h"
#import "ios/chrome/browser/segmentation_platform/otr_web_state_observer.h"
#import "ios/chrome/browser/segmentation_platform/segmentation_platform_config.h"
#import "ios/chrome/browser/sync/device_info_sync_service_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace segmentation_platform {
namespace {

const base::FilePath::CharType kSegmentationPlatformStorageDirName[] =
    FILE_PATH_LITERAL("Segmentation Platform");

const char kSegmentationPlatformProfileObserverKey[] =
    "segmentation_platform_profile_observer";

UkmDataManager* GetUkmDataManager() {
  static base::NoDestructor<DummyUkmDataManager> instance;
  return instance.get();
}

std::unique_ptr<processing::InputDelegateHolder> SetUpInputDelegates(
    std::vector<std::unique_ptr<Config>>& configs) {
  auto input_delegate_holder =
      std::make_unique<processing::InputDelegateHolder>();
  for (auto& config : configs) {
    for (auto& id : config->input_delegates) {
      input_delegate_holder->SetDelegate(id.first, std::move(id.second));
    }
  }

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

  auto params = std::make_unique<SegmentationPlatformServiceImpl::InitParams>();

  params->history_service = ios::HistoryServiceFactory::GetForBrowserState(
      chrome_browser_state, ServiceAccessType::IMPLICIT_ACCESS);
  params->task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
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
  // Guaranteed to outlive the SegmentationPlatformService, which depends on the
  // DeviceInfoSynceService.
  params->device_info_tracker =
      DeviceInfoSyncServiceFactory::GetForBrowserState(chrome_browser_state)
          ->GetDeviceInfoTracker();
  params->input_delegate_holder = SetUpInputDelegates(params->configs);
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
}

SegmentationPlatformServiceFactory::~SegmentationPlatformServiceFactory() =
    default;

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
