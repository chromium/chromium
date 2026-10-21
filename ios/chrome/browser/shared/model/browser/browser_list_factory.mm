// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"

#import <memory>
#import <utility>

#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

BrowserListFactory::BrowserListFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "BrowserList",
          ProfileSelection::kRedirectedInIncognito) {}

std::unique_ptr<KeyedService> BrowserListFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<BrowserList>();
}
