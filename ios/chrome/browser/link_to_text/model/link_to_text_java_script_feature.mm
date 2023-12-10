// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/link_to_text/model/link_to_text_java_script_feature.h"

#import "base/barrier_callback.h"
#import "base/no_destructor.h"
#import "base/ranges/algorithm.h"
#import "base/timer/elapsed_timer.h"
#import "components/shared_highlighting/core/common/disabled_sites.h"
#import "components/shared_highlighting/core/common/shared_highlighting_features.h"
#import "ios/chrome/browser/link_to_text/model/link_to_text_constants.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

namespace {
const char kScriptName[] = "link_to_text";
const char kGetLinkToTextFunction[] = "linkToText.getLinkToText";

bool IsKnownAmpCache(web::WebFrame* frame) {
  GURL origin = frame->GetSecurityOrigin();

  // Source:
  // https://github.com/ampproject/amphtml/blob/main/build-system/global-configs/caches.json
  return origin.DomainIs("ampproject.org") || origin.DomainIs("bing-amp.com");
}

LinkToTextResponse* ParseResponse(base::WeakPtr<web::WebState> web_state,
                                  const base::ElapsedTimer& timer,
                                  const base::Value* value) {
  return [LinkToTextResponse linkToTextResponseWithValue:value
                                                webState:web_state.get()
                                                 latency:timer.Elapsed()];
}

}  // namespace

LinkToTextJavaScriptFeature::LinkToTextJavaScriptFeature()
    : JavaScriptFeature(
          web::ContentWorld::kIsolatedWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kAllFrames,
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
    base::OnceCallback<void(LinkToTextResponse*)> callback) {
  base::ElapsedTimer link_generation_timer;

  RunGenerationJS(
      web_state->GetPageWorldWebFramesManager()->GetMainWebFrame(),
      base::BindOnce(&LinkToTextJavaScriptFeature::HandleResponse,
                     weak_ptr_factory_.GetWeakPtr(), web_state->GetWeakPtr(),
                     std::move(link_generation_timer), std::move(callback)));
}

void LinkToTextJavaScriptFeature::RunGenerationJS(
    web::WebFrame* frame,
    base::OnceCallback<void(const base::Value*)> callback) {
  CallJavaScriptFunction(frame, kGetLinkToTextFunction, /* parameters= */ {},
                         std::move(callback),
                         link_to_text::kLinkGenerationTimeout);
}

// static
bool LinkToTextJavaScriptFeature::ShouldAttemptIframeGeneration(
    std::optional<shared_highlighting::LinkGenerationError> error,
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
    base::WeakPtr<web::WebState> web_state,
    base::ElapsedTimer link_generation_timer,
    base::OnceCallback<void(LinkToTextResponse*)> final_callback,
    const base::Value* response) {
  LinkToTextResponse* parsed_response = [LinkToTextResponse
      linkToTextResponseWithValue:response
                         webState:web_state.get()
                          latency:link_generation_timer.Elapsed()];
  std::optional<shared_highlighting::LinkGenerationError> error =
      [parsed_response error];

  std::vector<web::WebFrame*> amp_frames;
  if (web_state &&
      ShouldAttemptIframeGeneration(error, web_state->GetLastCommittedURL())) {
    base::ranges::copy_if(
        web_state->GetPageWorldWebFramesManager()->GetAllWebFrames(),
        std::back_inserter(amp_frames), IsKnownAmpCache);
  }

  // Empty indicates we're not attempting AMP generation (e.g., succeeded or
  // conclusively failed on main frame, feature is disabled, no AMP frames
  // found, etc.) so run the callback immediately.
  if (amp_frames.empty()) {
    std::move(final_callback).Run(parsed_response);
    return;
  }

  // The response will be parsed immediately after the call finishes, and the
  // result will be held on to by the BarrierCallback until all the calls have
  // returned. Because LinkToTextResponse* is managed by ARC, this is OK even
  // though the original base::Value* will go out of scope.
  const auto parse_value = base::BindRepeating(
      &ParseResponse, web_state, std::move(link_generation_timer));
  const auto accumulate_subframe_results =
      base::BarrierCallback<LinkToTextResponse*>(
          amp_frames.size(),
          base::BindOnce(
              &LinkToTextJavaScriptFeature::HandleResponseFromSubframe,
              weak_ptr_factory_.GetWeakPtr(), std::move(final_callback)));

  for (auto* frame : amp_frames) {
    RunGenerationJS(frame, parse_value.Then(accumulate_subframe_results));
  }
}

void LinkToTextJavaScriptFeature::HandleResponseFromSubframe(
    base::OnceCallback<void(LinkToTextResponse*)> final_callback,
    std::vector<LinkToTextResponse*> parsed_responses) {
  DCHECK(!parsed_responses.empty());

  // First, see if we succeeded in any frame.
  auto success_response = base::ranges::find_if_not(
      parsed_responses,
      [](LinkToTextResponse* response) { return response.payload == nil; });
  if (success_response != parsed_responses.end()) {
    std::move(final_callback).Run(*success_response);
    return;
  }

  // If not, look for a frame where we failed with an error other than Incorrect
  // Selector. There should be at most one of these (since every frame with no
  // user selection should return Incorrect Selector).
  auto error_response = base::ranges::find_if_not(
      parsed_responses, [](LinkToTextResponse* response) {
        return [response error].value() ==
               shared_highlighting::LinkGenerationError::kIncorrectSelector;
      });
  if (error_response != parsed_responses.end()) {
    std::move(final_callback).Run(*error_response);
    return;
  }

  // All the frames have Incorrect Selector, so just use the first one.
  std::move(final_callback).Run(parsed_responses[0]);
}
