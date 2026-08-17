// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ATTEMPT_FORM_FILLING_TOOL_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ATTEMPT_FORM_FILLING_TOOL_JAVA_SCRIPT_FEATURE_H_

#import "base/functional/callback.h"
#import "base/memory/weak_ptr.h"
#import "base/no_destructor.h"
#import "base/types/expected.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/action_target.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {
class WebFrame;
}  // namespace web

namespace actor {

// LINT.IfChange(AttemptFormFillingToolResultCode)
enum class AttemptFormFillingToolResultCode {
  // The lookup was successful and the renderer ID was resolved.
  kOk = 0,
  // The provided target is invalid.
  kInvalidTarget = 1,
  // The provided coordinates are out of bounds or did not match any element on
  // the page.
  kCoordinatesOutOfBounds = 2,
  // The provided DOM node ID could not be found or did not resolve to an
  // element.
  kInvalidDomNodeId = 3,
  // The provided target is not an autofillable element.
  kTargetNotAutofillElement = 4,
};
// LINT.ThenChange(//ios/chrome/browser/intelligence/actor/tools/model/resources/attempt_form_filling_tool.ts:AttemptFormFillingToolResultCode)

class AttemptFormFillingToolJavaScriptFeature : public web::JavaScriptFeature {
 public:
  using GetAutofillRendererIdsCallback = base::OnceCallback<void(
      base::expected<std::vector<uint32_t>, ToolExecutionResult> result)>;

  static AttemptFormFillingToolJavaScriptFeature* GetInstance();

  // Queries the Autofill renderer IDs for the given `targets` in the given
  // `target_frame`.
  void GetAutofillRendererIds(base::WeakPtr<web::WebFrame> target_frame,
                              const std::vector<ActionTarget>& targets,
                              GetAutofillRendererIdsCallback callback);

 protected:
  AttemptFormFillingToolJavaScriptFeature();
  ~AttemptFormFillingToolJavaScriptFeature() override;

 private:
  friend class base::NoDestructor<AttemptFormFillingToolJavaScriptFeature>;
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ATTEMPT_FORM_FILLING_TOOL_JAVA_SCRIPT_FEATURE_H_
