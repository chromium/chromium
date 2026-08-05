// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_CLICK_TOOL_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_CLICK_TOOL_JAVA_SCRIPT_FEATURE_H_

#import "base/memory/weak_ptr.h"
#import "base/no_destructor.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/action_target.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {
class WebFrame;
}  // namespace web

namespace actor {

// LINT.IfChange(ClickToolResultCode)
enum class ClickToolResultCode {
  // The function call was successful.
  kOk = 0,
  // The coordinates provided to the function were not in the viewport.
  kCoordinatesOutOfBounds = 1,
  // The DOM node ID is invalid or did not resolve to a clickable element.
  kInvalidDomNodeId = 2,
  // The targeted element is disabled.
  kElementDisabled = 3,
  // The click event was not able to be dispatched.
  kClickSuppressed = 4,
};
// LINT.ThenChange(//ios/chrome/browser/intelligence/actor/tools/model/resources/click_tool.ts:ClickToolResultCode)

// A feature that provides methods to execute a click action in the web page.
class ClickToolJavaScriptFeature : public web::JavaScriptFeature {
 public:
  static ClickToolJavaScriptFeature* GetInstance();

  // Executes a click action on the given WebFrame.
  void Click(base::WeakPtr<web::WebFrame> target_frame,
             const ActionTarget& target,
             optimization_guide::proto::ClickAction_ClickType click_type,
             int click_count,
             ToolExecutionCallback callback);

 protected:
  ClickToolJavaScriptFeature();
  ~ClickToolJavaScriptFeature() override;

 private:
  friend class base::NoDestructor<ClickToolJavaScriptFeature>;
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_CLICK_TOOL_JAVA_SCRIPT_FEATURE_H_
