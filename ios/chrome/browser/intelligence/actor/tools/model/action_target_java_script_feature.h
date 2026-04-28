// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTION_TARGET_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTION_TARGET_JAVA_SCRIPT_FEATURE_H_

#import "base/functional/callback.h"
#import "base/memory/weak_ptr.h"
#import "base/no_destructor.h"
#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace base {
class Value;
}

namespace web {
class WebFrame;
class WebState;
}  // namespace web

namespace actor {

// LINT.IfChange(ActionTargetResultCode)
enum class ActionTargetResultCode {
  // The function call was successful.
  kOk = 0,
  // The coordinates provided to the function were not in the viewport.
  kCoordinatesOutOfBounds = 1,
};
// LINT.ThenChange(//ios/chrome/browser/intelligence/actor/tools/model/resources/action_target.ts:ActionTargetResultCode)

// A JS feature to help find the target elements for web actuation.
class ActionTargetJavaScriptFeature : public web::JavaScriptFeature {
 public:
  static constexpr int kMaxTargetIframeDepth = 10;

  struct TargetFrameResult {
    web::WebFrame* frame;
    optimization_guide::proto::ActionTarget target;
  };

  using TargetFrameCallback = base::OnceCallback<void(
      base::expected<TargetFrameResult, ToolExecutionResult> result)>;

  static ActionTargetJavaScriptFeature* GetInstance();

  // Finds the WebFrame for the given `target` and calls `callback`
  // with it and the translated ActionTarget.
  void GetTargetFrame(web::WebState* web_state,
                      web::WebFrame* web_frame,
                      const optimization_guide::proto::ActionTarget& target,
                      TargetFrameCallback callback,
                      int depth = 0);

 protected:
  ActionTargetJavaScriptFeature();
  ~ActionTargetJavaScriptFeature() override;

 private:
  friend class base::NoDestructor<ActionTargetJavaScriptFeature>;

  void GetTargetFrameByDocumentIdentifier(
      web::WebState* web_state,
      const optimization_guide::proto::ActionTarget& target,
      TargetFrameCallback callback);

  void GetTargetFrameByCoordinate(
      web::WebState* web_state,
      web::WebFrame* web_frame,
      const optimization_guide::proto::ActionTarget& target,
      TargetFrameCallback callback,
      int depth);

  void OnTargetIframeResolved(optimization_guide::proto::ActionTarget target,
                              base::WeakPtr<web::WebState> web_state,
                              base::WeakPtr<web::WebFrame> current_frame,
                              TargetFrameCallback callback,
                              int depth,
                              const base::Value* result);

  base::expected<web::WebFrame*, ToolExecutionResult>
  GetWebFrameByRemoteFrameToken(web::WebState* web_state,
                                const std::string& remote_frame_token);
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTION_TARGET_JAVA_SCRIPT_FEATURE_H_
