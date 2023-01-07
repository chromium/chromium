// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_PASSWORDS_WEB_VIEW_PASSWORD_CHANGE_SUCCESS_TRACKER_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_PASSWORDS_WEB_VIEW_PASSWORD_CHANGE_SUCCESS_TRACKER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

enum class ServiceAccessType;

namespace password_manager {
class PasswordChangeSuccessTracker;
}  // namespace password_manager

namespace ios_web_view {

class WebViewBrowserState;

// Singleton that owns all PasswordChangeSuccessTracker and associates them with
// WebViewBrowserStates.
class WebViewPasswordChangeSuccessTrackerFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static password_manager::PasswordChangeSuccessTracker* GetForBrowserState(
      WebViewBrowserState* browser_state,
      ServiceAccessType access_type);

  static WebViewPasswordChangeSuccessTrackerFactory* GetInstance();

 private:
  friend class base::NoDestructor<WebViewPasswordChangeSuccessTrackerFactory>;

  WebViewPasswordChangeSuccessTrackerFactory();
  ~WebViewPasswordChangeSuccessTrackerFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;

  WebViewPasswordChangeSuccessTrackerFactory(
      const WebViewPasswordChangeSuccessTrackerFactory&) = delete;
  WebViewPasswordChangeSuccessTrackerFactory& operator=(
      const WebViewPasswordChangeSuccessTrackerFactory&) = delete;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_PASSWORDS_WEB_VIEW_PASSWORD_CHANGE_SUCCESS_TRACKER_FACTORY_H_
