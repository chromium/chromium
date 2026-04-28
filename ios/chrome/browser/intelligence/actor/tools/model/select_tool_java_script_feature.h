// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_SELECT_TOOL_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_SELECT_TOOL_JAVA_SCRIPT_FEATURE_H_

#import <optional>

#import "base/memory/weak_ptr.h"
#import "base/no_destructor.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {
class WebFrame;
}  // namespace web

namespace optimization_guide::proto {
class SelectAction;
}  // namespace optimization_guide::proto

namespace actor {

// LINT.IfChange(SelectToolResultCode)
enum class SelectToolResultCode {
  // The function call was successful.
  kOk = 0,
  // The coordinates provided to target the element were not in the viewport.
  kCoordinatesOutOfBounds = 1,
  // The DOM node id provided to target the element was not in the viewport.
  kInvalidDomNodeId = 2,
  // The targeted element was not a <select>.
  kSelectInvalidElement = 3,
  // The targeted element was disabled.
  kElementDisabled = 4,
  // The targeted <option> in the <select> was disabled.
  kSelectOptionDisabled = 5,
  // There isn't an <option> in the <select> matching the desired value.
  kSelectNoSuchOption = 6,
};
// LINT.ThenChange(//ios/chrome/browser/intelligence/actor/tools/model/resources/select_tool.ts:SelectToolResultCode)

class SelectToolJavaScriptFeature : public web::JavaScriptFeature {
 public:
  static SelectToolJavaScriptFeature* GetInstance();

  // Executes a select action on the given WebFrame.
  void Select(base::WeakPtr<web::WebFrame> target_frame,
              const optimization_guide::proto::SelectAction& action,
              ToolExecutionCallback callback);

 protected:
  SelectToolJavaScriptFeature();
  ~SelectToolJavaScriptFeature() override;

 private:
  friend class base::NoDestructor<SelectToolJavaScriptFeature>;
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_SELECT_TOOL_JAVA_SCRIPT_FEATURE_H_
