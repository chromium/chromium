// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/favicon/model/ios_chrome_large_icon_cache_factory.h"

#include <utility>

#include "ios/chrome/browser/favicon/model/large_icon_cache.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

IOSChromeLargeIconCacheFactory::IOSChromeLargeIconCacheFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "LargeIconCache",
          ProfileSelection::kOwnInstanceInIncognito) {}

std::unique_ptr<KeyedService>
IOSChromeLargeIconCacheFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<LargeIconCache>();
}
