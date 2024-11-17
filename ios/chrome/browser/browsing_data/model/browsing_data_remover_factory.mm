// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browsing_data/model/browsing_data_remover_factory.h"

#import <utility>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remover_impl.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
BrowsingDataRemover* BrowsingDataRemoverFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<BrowsingDataRemover*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
BrowsingDataRemover* BrowsingDataRemoverFactory::GetForProfileIfExists(
    ProfileIOS* profile) {
  return static_cast<BrowsingDataRemover*>(
      GetInstance()->GetServiceForBrowserState(profile, false));
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
  // TODO(crbug.com/40940855): the factory should declare the services
  // used by BrowsingDataRemoverImpl and inject them in the constructor.
  return std::make_unique<BrowsingDataRemoverImpl>(
      ProfileIOS::FromBrowserState(context));
}

web::BrowserState* BrowsingDataRemoverFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateOwnInstanceInIncognito(context);
}
