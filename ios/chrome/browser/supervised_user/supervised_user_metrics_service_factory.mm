// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/supervised_user_metrics_service_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/supervised_user/core/browser/supervised_user_metrics_service.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/supervised_user/supervised_user_service_factory.h"

// static
supervised_user::SupervisedUserMetricsService*
SupervisedUserMetricsServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<supervised_user::SupervisedUserMetricsService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, /*create=*/true));
}

// static
SupervisedUserMetricsServiceFactory*
SupervisedUserMetricsServiceFactory::GetInstance() {
  static base::NoDestructor<SupervisedUserMetricsServiceFactory> instance;
  return instance.get();
}

SupervisedUserMetricsServiceFactory::SupervisedUserMetricsServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "SupervisedUserMetricsService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(SupervisedUserServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
SupervisedUserMetricsServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<supervised_user::SupervisedUserMetricsService>(
      browser_state->GetPrefs(),
      SupervisedUserServiceFactory::GetForBrowserState(browser_state)
          ->GetURLFilter());
}
