// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/attempt_form_filling_tool_java_script_feature.h"

#import "base/check.h"
#import "base/types/expected.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/action_target.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/web/public/js_messaging/web_frame.h"

namespace actor {

// static
AttemptFormFillingToolJavaScriptFeature*
AttemptFormFillingToolJavaScriptFeature::GetInstance() {
  static base::NoDestructor<AttemptFormFillingToolJavaScriptFeature> instance;
  return instance.get();
}

void AttemptFormFillingToolJavaScriptFeature::GetAutofillRendererIds(
    base::WeakPtr<web::WebFrame> target_frame,
    const std::vector<ActionTarget>& targets,
    GetAutofillRendererIdsCallback callback) {
  // Caller should have verified the existence of `target_frame`.
  CHECK(target_frame);
  CHECK(!targets.empty());
  std::vector<uint32_t> renderer_ids;
  // TODO(crbug.com/472287741): Temporary implementation.
  for (size_t i = 0; i < targets.size(); ++i) {
    renderer_ids.push_back(i + 1);
  }
  std::move(callback).Run(std::move(renderer_ids));
}

AttemptFormFillingToolJavaScriptFeature::
    AttemptFormFillingToolJavaScriptFeature()
    : web::JavaScriptFeature(web::ContentWorld::kIsolatedWorld, {}, {}) {}

AttemptFormFillingToolJavaScriptFeature::
    ~AttemptFormFillingToolJavaScriptFeature() = default;

}  // namespace actor
