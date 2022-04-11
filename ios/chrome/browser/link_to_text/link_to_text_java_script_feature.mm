// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/link_to_text/link_to_text_java_script_feature.h"

#import "base/no_destructor.h"
#import "base/timer/elapsed_timer.h"
#import "components/shared_highlighting/core/common/disabled_sites.h"
#import "components/shared_highlighting/core/common/shared_highlighting_features.h"
#import "ios/chrome/browser/link_to_text/link_to_text_constants.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const char kScriptName[] = "link_to_text";
const char kGetLinkToTextFunction[] = "linkToText.getLinkToText";

LinkToTextJavaScriptFeature::LinkToTextJavaScriptFeature()
    : JavaScriptFeature(
          ContentWorld::kAnyContentWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kMainFrame,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)}),
      weak_ptr_factory_(this) {}

LinkToTextJavaScriptFeature::~LinkToTextJavaScriptFeature() = default;

// static
LinkToTextJavaScriptFeature* LinkToTextJavaScriptFeature::GetInstance() {
  static base::NoDestructor<LinkToTextJavaScriptFeature> instance;
  return instance.get();
}

void LinkToTextJavaScriptFeature::GetLinkToText(
    web::WebState* web_state,
    web::WebFrame* frame,
    base::OnceCallback<void(LinkToTextResponse*)> callback) {
  base::ElapsedTimer link_generation_timer;

  CallJavaScriptFunction(
      frame, kGetLinkToTextFunction, /* parameters= */ {},
      base::BindOnce(&LinkToTextJavaScriptFeature::HandleResponse,
                     weak_ptr_factory_.GetWeakPtr(), web_state,
                     std::move(link_generation_timer), std::move(callback)),
      base::Milliseconds(link_to_text::kLinkGenerationTimeoutInMs));
}

// static
bool LinkToTextJavaScriptFeature::ShouldAttemptIframeGeneration(
    absl::optional<shared_highlighting::LinkGenerationError> error,
    const GURL& main_frame_url) {
  if (!base::FeatureList::IsEnabled(
          shared_highlighting::kSharedHighlightingAmp)) {
    return false;
  }
  if (!error ||
      error.value() !=
          shared_highlighting::LinkGenerationError::kIncorrectSelector) {
    return false;
  }
  return shared_highlighting::SupportsLinkGenerationInIframe(main_frame_url);
}

void LinkToTextJavaScriptFeature::HandleResponse(
    web::WebState* web_state,
    base::ElapsedTimer link_generation_timer,
    base::OnceCallback<void(LinkToTextResponse*)> callback,
    const base::Value* response) {
  auto* parsed_response = [LinkToTextResponse
      linkToTextResponseWithValue:response
                         webState:web_state
                          latency:link_generation_timer.Elapsed()];
  auto error = [parsed_response error];

  if (ShouldAttemptIframeGeneration(error, web_state->GetLastCommittedURL())) {
    // TODO(crbug.com/1313162): Try to find an AMP frame and reinvoke
    // generation.
  }

  std::move(callback).Run(parsed_response);
}
