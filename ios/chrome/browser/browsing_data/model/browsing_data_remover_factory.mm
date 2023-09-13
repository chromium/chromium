// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browsing_data/model/browsing_data_remover_factory.h"

#import <utility>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remover_impl.h"
#import "ios/chrome/browser/sessions/session_service_ios.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

// To get access to UseSessionSerializationOptimizations().
// TODO(crbug.com/1383087): remove once the feature is fully launched.
#import "ios/web/common/features.h"

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
  SessionServiceIOS* sessionServiceIOS = nil;
  if (!web::features::UseSessionSerializationOptimizations()) {
    sessionServiceIOS = [SessionServiceIOS sharedService];
  }
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<BrowsingDataRemoverImpl>(browser_state,
                                                   sessionServiceIOS);
}

web::BrowserState* BrowsingDataRemoverFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateOwnInstanceInIncognito(context);
}
