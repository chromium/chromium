// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_keyed_service_factory.h"

#import <memory>
#import <utility>

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_keyed_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"

TTCKeyedServiceFactory::TTCKeyedServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key), "TTCKeyedService") {
  DependsOn(BrowserListFactory::GetInstance());
  DependsOn(OptimizationGuideServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService> TTCKeyedServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  if (!IsTTCEnabled()) {
    return nullptr;
  }
  return std::make_unique<TTCKeyedService>(
      OptimizationGuideServiceFactory::GetForProfile(profile),
      BrowserListFactory::GetForProfile(profile));
}
