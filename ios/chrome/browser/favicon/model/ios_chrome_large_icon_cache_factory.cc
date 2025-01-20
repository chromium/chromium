// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/favicon/model/ios_chrome_large_icon_cache_factory.h"

#include "base/no_destructor.h"
#include "ios/chrome/browser/favicon/model/large_icon_cache.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
LargeIconCache* IOSChromeLargeIconCacheFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<LargeIconCache>(profile,
                                                               /*create=*/true);
}

// static
IOSChromeLargeIconCacheFactory* IOSChromeLargeIconCacheFactory::GetInstance() {
  static base::NoDestructor<IOSChromeLargeIconCacheFactory> instance;
  return instance.get();
}

IOSChromeLargeIconCacheFactory::IOSChromeLargeIconCacheFactory()
    : ProfileKeyedServiceFactoryIOS("LargeIconCache",
                                    ProfileSelection::kOwnInstanceInIncognito) {
}

IOSChromeLargeIconCacheFactory::~IOSChromeLargeIconCacheFactory() = default;

std::unique_ptr<KeyedService>
IOSChromeLargeIconCacheFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return std::make_unique<LargeIconCache>();
}
