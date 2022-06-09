// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"

#include "base/feature_list.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/segmentation_platform/internal/dummy_ukm_data_manager.h"
#include "components/segmentation_platform/internal/segmentation_platform_service_impl.h"
#include "components/segmentation_platform/internal/ukm_data_manager.h"
#include "components/segmentation_platform/public/features.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/optimization_guide/optimization_guide_service.h"
#include "ios/chrome/browser/optimization_guide/optimization_guide_service_factory.h"
#include "ios/chrome/browser/segmentation_platform/model_provider_factory_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace segmentation_platform {
namespace {

const base::FilePath::CharType kSegmentationPlatformStorageDirName[] =
    FILE_PATH_LITERAL("Segmentation Platform");

UkmDataManager* GetUkmDataManager() {
  static base::NoDestructor<DummyUkmDataManager> instance;
  return instance.get();
}

std::unique_ptr<KeyedService> BuildSegmentationPlatformService(
    web::BrowserState* context) {
  DCHECK(!context->IsOffTheRecord());

  ChromeBrowserState* chrome_browser_state =
      ChromeBrowserState::FromBrowserState(context);
  DCHECK(chrome_browser_state);
  const base::FilePath profile_path = chrome_browser_state->GetStatePath();
  auto* optimization_guide =
      OptimizationGuideServiceFactory::GetForBrowserState(chrome_browser_state);

  auto params = std::make_unique<SegmentationPlatformServiceImpl::InitParams>();

  params->history_service = ios::HistoryServiceFactory::GetForBrowserState(
      chrome_browser_state, ServiceAccessType::IMPLICIT_ACCESS);
  params->task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  params->storage_dir =
      profile_path.Append(kSegmentationPlatformStorageDirName);
  params->db_provider = chrome_browser_state->GetProtoDatabaseProvider();
  params->clock = base::DefaultClock::GetInstance();

  params->model_provider = std::make_unique<ModelProviderFactoryImpl>(
      optimization_guide, params->task_runner);
  params->ukm_data_manager = GetUkmDataManager();
  params->profile_prefs = chrome_browser_state->GetPrefs();
  // TODO(crbug.com/1333641): params->configs should be initialized.
  // TODO(crbug.com/1333641): params->field_trial_register should be
  // initialized.

  // TODO(crbug.com/1333641): The factory should call EnableMetrics() based on
  // incognito profile creation.
  return std::make_unique<SegmentationPlatformServiceImpl>(std::move(params));
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

bool SegmentationPlatformServiceFactory::ServiceIsCreatedWithBrowserState()
    const {
  return base::FeatureList::IsEnabled(features::kSegmentationPlatformFeature);
}

bool SegmentationPlatformServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace segmentation_platform
