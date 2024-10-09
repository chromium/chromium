// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
BrowserList* BrowserListFactory::GetForBrowserState(ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
BrowserList* BrowserListFactory::GetForProfile(ProfileIOS* profile) {
  return static_cast<BrowserList*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
BrowserListFactory* BrowserListFactory::GetInstance() {
  static base::NoDestructor<BrowserListFactory> instance;
  return instance.get();
}

BrowserListFactory::BrowserListFactory()
    : BrowserStateKeyedServiceFactory(
          "BrowserList",
          BrowserStateDependencyManager::GetInstance()) {}

std::unique_ptr<KeyedService> BrowserListFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return std::make_unique<BrowserList>();
}

web::BrowserState* BrowserListFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  // Incognito profiles use same service as regular profiles.
  return GetBrowserStateRedirectedInIncognito(context);
}
