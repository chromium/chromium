// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/commerce/shopping_service_factory.h"

#include "components/commerce/core/shopping_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/optimization_guide/optimization_guide_service.h"
#include "ios/chrome/browser/optimization_guide/optimization_guide_service_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace commerce {

// static
ShoppingServiceFactory* ShoppingServiceFactory::GetInstance() {
  static base::NoDestructor<ShoppingServiceFactory> instance;
  return instance.get();
}

// static
ShoppingService* ShoppingServiceFactory::GetForBrowserState(
    web::BrowserState* state) {
  return static_cast<ShoppingService*>(
      GetInstance()->GetServiceForBrowserState(state, true));
}

// static
ShoppingService* ShoppingServiceFactory::GetForBrowserStateIfExists(
    web::BrowserState* state) {
  return static_cast<ShoppingService*>(
      GetInstance()->GetServiceForBrowserState(state, false));
}

ShoppingServiceFactory::ShoppingServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "ShoppingService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::BookmarkModelFactory::GetInstance());
  DependsOn(OptimizationGuideServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService> ShoppingServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* state) const {
  ChromeBrowserState* chrome_state =
      ChromeBrowserState::FromBrowserState(state);
  PrefService* pref_service = chrome_state ? chrome_state->GetPrefs() : nullptr;
  return std::make_unique<ShoppingService>(
      ios::BookmarkModelFactory::GetInstance()->GetForBrowserState(
          chrome_state),
      OptimizationGuideServiceFactory::GetForBrowserState(chrome_state),
      pref_service);
}

web::BrowserState* ShoppingServiceFactory::GetBrowserStateToUse(
    web::BrowserState* state) const {
  return GetBrowserStateRedirectedInIncognito(state);
}

bool ShoppingServiceFactory::ServiceIsCreatedWithBrowserState() const {
  return true;
}

bool ShoppingServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace commerce
