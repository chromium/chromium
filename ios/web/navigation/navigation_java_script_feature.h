// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_NAVIGATION_JAVA_SCRIPT_FEATURE_H_
#define IOS_WEB_NAVIGATION_NAVIGATION_JAVA_SCRIPT_FEATURE_H_

#include <optional>

#include "base/no_destructor.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {

// A feature which receives messages about the main frame page navigation.
class NavigationJavaScriptFeature : public web::JavaScriptFeature {
 public:
  // This feature holds no state, so only a single static instance is ever
  // needed.
  static NavigationJavaScriptFeature* GetInstance();

 private:
  friend class base::NoDestructor<NavigationJavaScriptFeature>;

  NavigationJavaScriptFeature();
  ~NavigationJavaScriptFeature() override;

  NavigationJavaScriptFeature(const NavigationJavaScriptFeature&) = delete;
  NavigationJavaScriptFeature& operator=(const NavigationJavaScriptFeature&) =
      delete;

  // JavaScriptFeature:
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(web::WebState* web_state,
                             const web::ScriptMessage& message) override;
};

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_NAVIGATION_JAVA_SCRIPT_FEATURE_H_
