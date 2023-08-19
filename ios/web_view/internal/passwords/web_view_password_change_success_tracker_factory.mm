// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/passwords/web_view_password_change_success_tracker_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/password_manager/core/browser/password_change_success_tracker_impl.h"
#include "ios/web_view/internal/web_view_browser_state.h"

namespace ios_web_view {

// static
password_manager::PasswordChangeSuccessTracker*
WebViewPasswordChangeSuccessTrackerFactory::GetForBrowserState(
    WebViewBrowserState* browser_state,
    ServiceAccessType access_type) {
  return static_cast<password_manager::PasswordChangeSuccessTracker*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
WebViewPasswordChangeSuccessTrackerFactory*
WebViewPasswordChangeSuccessTrackerFactory::GetInstance() {
  static base::NoDestructor<WebViewPasswordChangeSuccessTrackerFactory>
      instance;
  return instance.get();
}

WebViewPasswordChangeSuccessTrackerFactory::
    WebViewPasswordChangeSuccessTrackerFactory()
    : BrowserStateKeyedServiceFactory(
          "PasswordChangeSuccessTrackerFactory",
          BrowserStateDependencyManager::GetInstance()) {}

WebViewPasswordChangeSuccessTrackerFactory::
    ~WebViewPasswordChangeSuccessTrackerFactory() {}

std::unique_ptr<KeyedService>
WebViewPasswordChangeSuccessTrackerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return std::make_unique<password_manager::PasswordChangeSuccessTrackerImpl>(
      WebViewBrowserState::FromBrowserState(context)->GetPrefs());
}

}  // namespace ios_web_view
