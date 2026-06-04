// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_PAGE_STABILITY_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_PAGE_STABILITY_JAVA_SCRIPT_FEATURE_H_

#import <Foundation/Foundation.h>

#import <optional>
#import <string>

#import "base/functional/callback.h"
#import "base/memory/weak_ptr.h"
#import "base/no_destructor.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
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

  // Waits for page stability in `target_frame` before running `callback`.
  //
  // This checks for stability by counting DOM mutations over an interval and
  // comparing it to a threshold. Both the mutation threshold and waiting
  // interval are configured by Finch params. This will also time out based on
  // another Finch param if the page doesn't become stable.
  //
  // Returns via `callback` whether the page successfully stabilizes within the
  // timeout.
  void WaitForStability(base::WeakPtr<web::WebFrame> target_frame,
                        base::OnceCallback<void(ToolExecutionResult)> callback);

  // JavaScriptFeature:
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(web::WebState* web_state,
                             const web::ScriptMessage& message) override;

 protected:
  PageStabilityJavaScriptFeature();
  ~PageStabilityJavaScriptFeature() override;

 private:
  friend class base::NoDestructor<PageStabilityJavaScriptFeature>;

  void OnStabilityResult(base::WeakPtr<web::WebFrame> target_frame,
                         base::OnceCallback<void(ToolExecutionResult)> callback,
                         const base::Value* result,
                         NSError* error);
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_PAGE_STABILITY_JAVA_SCRIPT_FEATURE_H_
