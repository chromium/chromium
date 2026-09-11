// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_DRAG_AND_RELEASE_TOOL_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_DRAG_AND_RELEASE_TOOL_JAVA_SCRIPT_FEATURE_H_

#import "base/memory/weak_ptr.h"
#import "base/no_destructor.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/action_target.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {
class WebFrame;
}  // namespace web

namespace actor {

// LINT.IfChange(DragAndReleaseToolResultCode)
enum class DragAndReleaseToolResultCode {
  // The function call was successful.
  kOk = 0,
  // The coordinates provided for the start target were not in the viewport.
  kFromCoordinatesOutOfBounds = 1,
  // The coordinates provided for the end target were not in the viewport.
  kToCoordinatesOutOfBounds = 2,
  // The DOM node ID provided for the start target was not found or not an
  // Element.
  kFromInvalidDomNodeId = 3,
  // The DOM node ID provided for the end target was not found or not an
  // Element.
  kToInvalidDomNodeId = 4,
  // The drag event was suppressed.
  kDragSuppressed = 5,
  // The source element is disabled.
  kFromElementDisabled = 6,
  // The destination element is disabled.
  kToElementDisabled = 7,
};
// LINT.ThenChange(//ios/chrome/browser/intelligence/actor/tools/model/resources/drag_and_release_tool.ts:DragAndReleaseToolResultCode)

// Feature that interacts with the JavaScript drag and release implementation.
class DragAndReleaseToolJavaScriptFeature : public web::JavaScriptFeature {
 public:
  static DragAndReleaseToolJavaScriptFeature* GetInstance();

  DragAndReleaseToolJavaScriptFeature(
      const DragAndReleaseToolJavaScriptFeature&) = delete;
  DragAndReleaseToolJavaScriptFeature& operator=(
      const DragAndReleaseToolJavaScriptFeature&) = delete;

  // Executes a drag and release action from `from_target` to `to_target`
  // in the given `target_frame`.
  void DragAndRelease(base::WeakPtr<web::WebFrame> target_frame,
                      const ActionTarget& from_target,
                      const ActionTarget& to_target,
                      ToolExecutionCallback callback);

 protected:
  DragAndReleaseToolJavaScriptFeature();
  ~DragAndReleaseToolJavaScriptFeature() override;

 private:
  friend class base::NoDestructor<DragAndReleaseToolJavaScriptFeature>;
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_DRAG_AND_RELEASE_TOOL_JAVA_SCRIPT_FEATURE_H_
