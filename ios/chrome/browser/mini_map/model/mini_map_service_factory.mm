// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/mini_map/model/mini_map_service_factory.h"

#import <utility>

#import "base/check.h"
#import "ios/chrome/browser/mini_map/model/mini_map_service.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"

MiniMapServiceFactory::MiniMapServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "MiniMapService",
          ProfileSelection::kRedirectedInIncognito) {
  DependsOn(ios::TemplateURLServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService> MiniMapServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  CHECK(IsMiniMapUniversalLinkEnabled());

  return std::make_unique<MiniMapService>(
      profile->GetPrefs(),
      ios::TemplateURLServiceFactory::GetForProfile(profile));
}
