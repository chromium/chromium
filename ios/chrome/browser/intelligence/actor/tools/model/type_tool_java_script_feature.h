// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_TYPE_TOOL_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_TYPE_TOOL_JAVA_SCRIPT_FEATURE_H_

#import "base/functional/callback_forward.h"
#import "base/memory/weak_ptr.h"
#import "base/no_destructor.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/action_target.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {
class WebFrame;
}  // namespace web

namespace actor {

// LINT.IfChange(TypeToolResultCode)
enum class TypeToolResultCode {
  // The function call was successful.
  kOk = 0,
  // The coordinates provided to target the element were not in the viewport.
  kCoordinatesOutOfBounds = 1,
  // The DOM node id provided to target the element was not in the viewport.
  kInvalidDomNodeId = 2,
  // The target provided exists but is not an Element.
  kTypeTargetNotElement = 3,
  // The target element is not focusable.
  kTypeTargetNotFocusable = 4,
  // A general invalid arguments error.
  kInvalidArguments = 5,
  // The target element was disabled.
  kElementDisabled = 6,
};
// LINT.ThenChange(//ios/chrome/browser/intelligence/actor/tools/model/resources/type_tool.ts:TypeToolResultCode)

// A feature that provides methods to execute a type action in the web page.
class TypeToolJavaScriptFeature : public web::JavaScriptFeature {
 public:
  static TypeToolJavaScriptFeature* GetInstance();

  // Executes a type action on the given `target_frame`.
  void Type(base::WeakPtr<web::WebFrame> target_frame,
            const ActionTarget& target,
            const std::string& text,
            optimization_guide::proto::TypeAction_TypeMode mode,
            bool follow_by_enter,
            ToolExecutionCallback callback);

 protected:
  TypeToolJavaScriptFeature();
  ~TypeToolJavaScriptFeature() override;

 private:
  friend class base::NoDestructor<TypeToolJavaScriptFeature>;
  friend class TypeToolJavaScriptFeatureTest;

  // Callback invoked when `Type()` completes, receiving the execution `result`
  // and the resolved `field_id` of the typed leaf element (or a null
  // `autofill::FieldRendererId` if typing did not succeed on a valid leaf
  // element).
  using TypeExecutionCallback =
      base::OnceCallback<void(ToolExecutionResult result,
                              autofill::FieldRendererId field_id)>;

  // Executes a type action on the given `target_frame` and passes both the
  // execution result and the resolved `autofill::FieldRendererId` to
  // `callback`.
  void Type(base::WeakPtr<web::WebFrame> target_frame,
            const ActionTarget& target,
            const std::string& text,
            optimization_guide::proto::TypeAction_TypeMode mode,
            TypeExecutionCallback callback);

  // Handles completion of the `Type()` JavaScript call and triggers
  // `SubmitWithEnterFallback()` if `follow_by_enter` is requested.
  void OnTypeCompleted(base::WeakPtr<web::WebFrame> target_frame,
                       bool follow_by_enter,
                       ToolExecutionCallback callback,
                       ToolExecutionResult result,
                       autofill::FieldRendererId field_id);

  // Dispatches a fallback synthetic `Enter` event sequence and implicit form
  // submission on the element identified by `field_id` in `target_frame`.
  void SubmitWithEnterFallback(base::WeakPtr<web::WebFrame> target_frame,
                               autofill::FieldRendererId field_id,
                               base::OnceClosure callback);
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_TYPE_TOOL_JAVA_SCRIPT_FEATURE_H_
