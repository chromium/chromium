// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/client_side_detection/client_side_detection_intelligent_scan_delegate_factory.h"

#import <memory>
#import <utility>

#import "base/check.h"
#import "base/feature_list.h"
#import "components/prefs/pref_service.h"
#import "components/safe_browsing/core/browser/intelligent_scan_delegate.h"
#import "components/safe_browsing/core/common/features.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/safe_browsing/model/client_side_detection/client_side_detection_intelligent_scan_delegate_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// The service is created lazily. There is no on-device model to observe on
// iOS, so nothing needs to happen before the first intelligent scan request.
ClientSideDetectionIntelligentScanDelegateFactory::
    ClientSideDetectionIntelligentScanDelegateFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "ClientSideDetectionIntelligentScanDelegate",
          ProfileSelection::kNoInstanceInIncognito) {
  DependsOn(OptimizationGuideServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
ClientSideDetectionIntelligentScanDelegateFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  if (!base::FeatureList::IsEnabled(
          safe_browsing::kClientSideDetectionEnabledIos)) {
    return nullptr;
  }

  OptimizationGuideService* opt_guide =
      OptimizationGuideServiceFactory::GetForProfile(profile);
  // The server-side model is reachable only through the Optimization
  // Guide `RemoteModelExecutor`, and iOS has no on-device fallback, so a
  // delegate without it could never run a scan.
  if (!opt_guide) {
    return nullptr;
  }

  PrefService* prefs = profile->GetPrefs();
  CHECK(prefs);
  return std::make_unique<
      safe_browsing::ClientSideDetectionIntelligentScanDelegateIOS>(*prefs,
                                                                    opt_guide);
}
