// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_PAGE_STABILITY_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_PAGE_STABILITY_JAVA_SCRIPT_FEATURE_H_

#import <optional>
#import <string>

#import "base/no_destructor.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {
class WebState;
class ScriptMessage;
}  // namespace web

namespace actor {

// A feature that injects scripts to monitor and act on page stability signals.
class PageStabilityJavaScriptFeature : public web::JavaScriptFeature {
 public:
  static PageStabilityJavaScriptFeature* GetInstance();

  // JavaScriptFeature:
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(web::WebState* web_state,
                             const web::ScriptMessage& message) override;

 protected:
  PageStabilityJavaScriptFeature();
  ~PageStabilityJavaScriptFeature() override;

 private:
  friend class base::NoDestructor<PageStabilityJavaScriptFeature>;
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_PAGE_STABILITY_JAVA_SCRIPT_FEATURE_H_
