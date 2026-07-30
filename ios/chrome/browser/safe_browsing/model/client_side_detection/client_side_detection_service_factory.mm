// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/client_side_detection/client_side_detection_service_factory.h"

#import "components/safe_browsing/core/common/features.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/safe_browsing/model/client_side_detection/client_side_detection_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

// static
safe_browsing::ClientSideDetectionService*
ClientSideDetectionServiceFactory::GetForProfile(ProfileIOS* profile) {
  if (!base::FeatureList::IsEnabled(
          safe_browsing::kClientSideDetectionEnabledIos)) {
    return nullptr;
  }
  return GetInstance()
      ->GetServiceForProfileAs<safe_browsing::ClientSideDetectionService>(
          profile, true);
}

// static
ClientSideDetectionServiceFactory*
ClientSideDetectionServiceFactory::GetInstance() {
  static base::NoDestructor<ClientSideDetectionServiceFactory> instance;
  return instance.get();
}

ClientSideDetectionServiceFactory::ClientSideDetectionServiceFactory()
    : ProfileKeyedServiceFactoryIOS("ClientSideDetectionService",
                                    ProfileSelection::kNoInstanceInIncognito) {
  DependsOn(OptimizationGuideServiceFactory::GetInstance());
}

ClientSideDetectionServiceFactory::~ClientSideDetectionServiceFactory() =
    default;

std::unique_ptr<KeyedService>
ClientSideDetectionServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  if (!base::FeatureList::IsEnabled(
          safe_browsing::kClientSideDetectionEnabledIos)) {
    return nullptr;
  }

  OptimizationGuideService* opt_guide =
      OptimizationGuideServiceFactory::GetForProfile(profile);
  PrefService* prefs = profile->GetPrefs();
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      profile->GetSharedURLLoaderFactory();

  if (!opt_guide || !prefs || !url_loader_factory) {
    return nullptr;
  }

  return std::make_unique<safe_browsing::ClientSideDetectionService>(
      prefs, std::move(url_loader_factory), opt_guide);
}
