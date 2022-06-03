// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search/search_service_factory.h"

#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/search/search_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

SearchService* SearchServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<SearchService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

SearchServiceFactory* SearchServiceFactory::GetInstance() {
  static base::NoDestructor<SearchServiceFactory> instance;
  return instance.get();
}

SearchServiceFactory::SearchServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "SearchService",
          BrowserStateDependencyManager::GetInstance()) {}

SearchServiceFactory::~SearchServiceFactory() = default;

std::unique_ptr<KeyedService> SearchServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return std::make_unique<SearchService>();
}

web::BrowserState* SearchServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateOwnInstanceInIncognito(context);
}
