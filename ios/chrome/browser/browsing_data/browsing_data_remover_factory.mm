// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/browsing_data/browsing_data_remover_factory.h"

#include <utility>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remover_impl.h"
#import "ios/chrome/browser/sessions/session_service_ios.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
BrowsingDataRemover* BrowsingDataRemoverFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<BrowsingDataRemover*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
BrowsingDataRemover* BrowsingDataRemoverFactory::GetForBrowserStateIfExists(
    ChromeBrowserState* browser_state) {
  return static_cast<BrowsingDataRemover*>(
      GetInstance()->GetServiceForBrowserState(browser_state, false));
}

// static
BrowsingDataRemoverFactory* BrowsingDataRemoverFactory::GetInstance() {
  static base::NoDestructor<BrowsingDataRemoverFactory> instance;
  return instance.get();
}

BrowsingDataRemoverFactory::BrowsingDataRemoverFactory()
    : BrowserStateKeyedServiceFactory(
          "BrowsingDataRemover",
          BrowserStateDependencyManager::GetInstance()) {}

BrowsingDataRemoverFactory::~BrowsingDataRemoverFactory() = default;

std::unique_ptr<KeyedService>
BrowsingDataRemoverFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<BrowsingDataRemoverImpl>(
      browser_state, [SessionServiceIOS sharedService]);
}

web::BrowserState* BrowsingDataRemoverFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateOwnInstanceInIncognito(context);
}
