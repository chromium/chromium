// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/java_script_console/java_script_console_feature_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/web/java_script_console/java_script_console_feature.h"
#include "ios/web/public/browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
JavaScriptConsoleFeature* JavaScriptConsoleFeatureFactory::GetForBrowserState(
    web::BrowserState* browser_state) {
  return static_cast<JavaScriptConsoleFeature*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
JavaScriptConsoleFeatureFactory*
JavaScriptConsoleFeatureFactory::GetInstance() {
  static base::NoDestructor<JavaScriptConsoleFeatureFactory> instance;
  return instance.get();
}

JavaScriptConsoleFeatureFactory::JavaScriptConsoleFeatureFactory()
    : BrowserStateKeyedServiceFactory(
          "JavaScriptConsoleFeature",
          BrowserStateDependencyManager::GetInstance()) {}

JavaScriptConsoleFeatureFactory::~JavaScriptConsoleFeatureFactory() = default;

std::unique_ptr<KeyedService>
JavaScriptConsoleFeatureFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return std::make_unique<JavaScriptConsoleFeature>();
}

web::BrowserState* JavaScriptConsoleFeatureFactory::GetBrowserStateToUse(
    web::BrowserState* browser_state) const {
  return GetBrowserStateOwnInstanceInIncognito(browser_state);
}
