// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/model/contextual_panel_model_service_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_model_service.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

// static
ContextualPanelModelService*
ContextualPanelModelServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<ContextualPanelModelService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, /*create=*/true));
}

// static
ContextualPanelModelServiceFactory*
ContextualPanelModelServiceFactory::GetInstance() {
  static base::NoDestructor<ContextualPanelModelServiceFactory> instance;
  return instance.get();
}

ContextualPanelModelServiceFactory::ContextualPanelModelServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "ContextualPanelModelService",
          BrowserStateDependencyManager::GetInstance()) {}

ContextualPanelModelServiceFactory::~ContextualPanelModelServiceFactory() {}

std::unique_ptr<KeyedService>
ContextualPanelModelServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return std::make_unique<ContextualPanelModelService>();
}
