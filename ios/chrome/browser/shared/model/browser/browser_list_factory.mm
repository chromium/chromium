// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/main/browser_list_impl.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
BrowserList* BrowserListFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<BrowserList*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
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
  return std::make_unique<BrowserListImpl>();
}

web::BrowserState* BrowserListFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  // Incognito browser states use same service as regular browser states.
  return GetBrowserStateRedirectedInIncognito(context);
}
