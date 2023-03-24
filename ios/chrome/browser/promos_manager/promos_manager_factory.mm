// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/promos_manager/promos_manager_factory.h"

#import "base/no_destructor.h"
#import "base/time/default_clock.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/promos_manager/promos_manager.h"
#import "ios/chrome/browser/promos_manager/promos_manager_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
PromosManager* PromosManagerFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<PromosManager*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
PromosManagerFactory* PromosManagerFactory::GetInstance() {
  static base::NoDestructor<PromosManagerFactory> instance;
  return instance.get();
}

PromosManagerFactory::PromosManagerFactory()
    : BrowserStateKeyedServiceFactory(
          "PromosManagerFactory",
          BrowserStateDependencyManager::GetInstance()) {}

PromosManagerFactory::~PromosManagerFactory() = default;

std::unique_ptr<KeyedService> PromosManagerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  auto promos_manager = std::make_unique<PromosManagerImpl>(
      GetApplicationContext()->GetLocalState(),
      base::DefaultClock::GetInstance());
  promos_manager->Init();
  return promos_manager;
}

web::BrowserState* PromosManagerFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}
