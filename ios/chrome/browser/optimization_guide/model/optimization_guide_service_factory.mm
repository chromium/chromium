// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"

#import "base/feature_list.h"
#import "base/path_service.h"
#import "components/optimization_guide/core/optimization_guide_constants.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "components/optimization_guide/core/optimization_guide_store.h"
#import "components/optimization_guide/core/prediction_manager.h"
#import "ios/chrome/browser/optimization_guide/model/ios_chrome_hints_manager.h"
#import "ios/chrome/browser/optimization_guide/model/ios_chrome_prediction_model_store.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/paths/paths.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

// Returns the BackgroundDownloadService for `weak_profile`.
download::BackgroundDownloadService* GetBackgroundDownloadService(
    base::WeakPtr<ProfileIOS> weak_profile) {
  if (ProfileIOS* profile = weak_profile.get()) {
    return BackgroundDownloadServiceFactory::GetForProfile(profile);
  }

  return nullptr;
}

std::unique_ptr<KeyedService> BuildOptimizationGuideService(
    web::BrowserState* context) {
  if (!optimization_guide::features::IsOptimizationHintsEnabled()) {
    return nullptr;
  }

  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  ProfileIOS* original_profile = profile->GetOriginalProfile();

  // Regardless of whether the profile is off the record or not, initialize the
  // Optimization Guide with the database associated with the original profile.
  auto* proto_db_provider = original_profile->GetProtoDatabaseProvider();
  base::FilePath profile_path = original_profile->GetStatePath();

  base::WeakPtr<optimization_guide::OptimizationGuideStore> hint_store;
  if (profile->IsOffTheRecord()) {
    OptimizationGuideService* original_ogs =
        OptimizationGuideServiceFactory::GetForProfile(original_profile);
    DCHECK(original_ogs);
    hint_store = original_ogs->GetHintsManager()->hint_store();
  }

  auto service = std::make_unique<OptimizationGuideService>(
      proto_db_provider, profile_path, profile->IsOffTheRecord(),
      GetApplicationContext()->GetApplicationLocale(), hint_store,
      profile->GetPrefs(), BrowserListFactory::GetForProfile(profile),
      profile->GetSharedURLLoaderFactory(),
      base::BindOnce(&GetBackgroundDownloadService, profile->AsWeakPtr()),
      IdentityManagerFactory::GetForProfile(profile));

  service->DoFinalInit(
      BackgroundDownloadServiceFactory::GetForProfile(profile));
  return service;
}

}  // namespace

// static
OptimizationGuideService* OptimizationGuideServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<OptimizationGuideService>(
      profile, /*create=*/true);
}

// static
OptimizationGuideServiceFactory*
OptimizationGuideServiceFactory::GetInstance() {
  static base::NoDestructor<OptimizationGuideServiceFactory> instance;
  return instance.get();
}

// static
void OptimizationGuideServiceFactory::InitializePredictionModelStore() {
  base::FilePath model_downloads_dir;
  base::PathService::Get(ios::DIR_USER_DATA, &model_downloads_dir);
  model_downloads_dir = model_downloads_dir.Append(
      optimization_guide::kOptimizationGuideModelStoreDirPrefix);
  optimization_guide::IOSChromePredictionModelStore::GetInstance()->Initialize(
      model_downloads_dir);
}

OptimizationGuideServiceFactory::OptimizationGuideServiceFactory()
    : ProfileKeyedServiceFactoryIOS("OptimizationGuideService",
                                    ServiceCreation::kCreateWithProfile,
                                    TestingCreation::kNoServiceForTests,
                                    ProfileSelection::kOwnInstanceInIncognito) {
  DependsOn(BackgroundDownloadServiceFactory::GetInstance());
  DependsOn(BrowserListFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

OptimizationGuideServiceFactory::~OptimizationGuideServiceFactory() = default;

// static
BrowserStateKeyedServiceFactory::TestingFactory
OptimizationGuideServiceFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildOptimizationGuideService);
}

std::unique_ptr<KeyedService>
OptimizationGuideServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildOptimizationGuideService(context);
}
