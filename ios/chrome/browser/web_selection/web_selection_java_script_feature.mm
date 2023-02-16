// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_selection/web_selection_java_script_feature.h"

#import "base/barrier_callback.h"
#import "base/no_destructor.h"
#import "base/ranges/algorithm.h"
#import "ios/chrome/browser/web_selection/web_selection_response.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kScriptName[] = "web_selection";
const char kWebSelectionFunctionName[] = "webSelection.getSelectedText";

constexpr base::TimeDelta kScriptTimeout = base::Milliseconds(500);

WebSelectionResponse* ParseResponse(base::WeakPtr<web::WebState> weak_web_state,
                                    const base::Value* value) {
  web::WebState* web_state = weak_web_state.get();
  if (!web_state || !value) {
    return [WebSelectionResponse invalidResponse];
  }
  return [WebSelectionResponse selectionResponseWithValue:*value
                                                 webState:web_state];
}

bool IsSubFrame(web::WebFrame* frame) {
  return !frame->IsMainFrame();
}

}  // namespace

// TODO(crbug.com/1416459): migrate to kIsolatedWorld.
WebSelectionJavaScriptFeature::WebSelectionJavaScriptFeature()
    : JavaScriptFeature(
          web::ContentWorld::kPageContentWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kAllFrames,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)}),
      weak_ptr_factory_(this) {}

WebSelectionJavaScriptFeature::~WebSelectionJavaScriptFeature() = default;

// static
WebSelectionJavaScriptFeature* WebSelectionJavaScriptFeature::GetInstance() {
  static base::NoDestructor<WebSelectionJavaScriptFeature> instance;
  return instance.get();
}

void WebSelectionJavaScriptFeature::GetSelectedText(
    web::WebState* web_state,
    base::OnceCallback<void(WebSelectionResponse*)> callback) {
  RunGetSelectionFunction(
      web_state->GetPageWorldWebFramesManager()->GetMainWebFrame(),
      base::BindOnce(&WebSelectionJavaScriptFeature::HandleResponse,
                     weak_ptr_factory_.GetWeakPtr(), web_state->GetWeakPtr(),
                     std::move(callback)));
}

void WebSelectionJavaScriptFeature::HandleResponse(
    base::WeakPtr<web::WebState> weak_web_state,
    base::OnceCallback<void(WebSelectionResponse*)> final_callback,
    const base::Value* response) {
  web::WebState* web_state = weak_web_state.get();
  if (!response || !web_state) {
    // If the JS timed out, no need to try to find the selection in the sub
    // frames.
    std::move(final_callback).Run([WebSelectionResponse invalidResponse]);
    return;
  }
  WebSelectionResponse* parsed_response =
      [WebSelectionResponse selectionResponseWithValue:*response
                                              webState:web_state];
  if (parsed_response.selectedText.length) {
    std::move(final_callback).Run(parsed_response);
    return;
  }

  // Try on all web frames except main
  std::vector<web::WebFrame*> sub_frames;
  base::ranges::copy_if(
      web_state->GetPageWorldWebFramesManager()->GetAllWebFrames(),
      std::back_inserter(sub_frames), IsSubFrame);

  // Empty indicates there is no subframe to test. Pass the selection from main
  // frame even if empty.
  if (sub_frames.empty()) {
    std::move(final_callback).Run(parsed_response);
    return;
  }

  // The response will be parsed immediately after the call finishes, and the
  // result will be held on to by the BarrierCallback until all the calls have
  // returned. No copy of the result value pointer is kept and the values are
  // kept in a WebSelectionResponse ObjC object that is correctly reference
  // counted.
  const auto parse_value = base::BindRepeating(&ParseResponse, weak_web_state);
  const auto accumulate_subframe_results =
      base::BarrierCallback<WebSelectionResponse*>(
          sub_frames.size(),
          base::BindOnce(
              &WebSelectionJavaScriptFeature::ProcessResponseFromSubframes,
              weak_ptr_factory_.GetWeakPtr(), std::move(final_callback)));

  for (auto* frame : sub_frames) {
    RunGetSelectionFunction(frame,
                            parse_value.Then(accumulate_subframe_results));
  }
}

void WebSelectionJavaScriptFeature::RunGetSelectionFunction(
    web::WebFrame* frame,
    base::OnceCallback<void(const base::Value*)> callback) {
  if (!frame || !frame->CanCallJavaScriptFunction()) {
    std::move(callback).Run(nullptr);
    return;
  }
  CallJavaScriptFunction(frame, kWebSelectionFunctionName, /* parameters= */ {},
                         std::move(callback), kScriptTimeout);
}

void WebSelectionJavaScriptFeature::ProcessResponseFromSubframes(
    base::OnceCallback<void(WebSelectionResponse*)> final_callback,
    std::vector<WebSelectionResponse*> parsed_responses) {
  DCHECK(!parsed_responses.empty());

  // Check if a successful response was returned by any frame.
  auto success_response = base::ranges::find_if_not(
      parsed_responses, [](WebSelectionResponse* response) {
        return response.selectedText.length == 0;
      });
  if (success_response != parsed_responses.end()) {
    std::move(final_callback).Run(*success_response);
    return;
  }

  auto valid_response = base::ranges::find_if(
      parsed_responses,
      [](WebSelectionResponse* response) { return response.valid; });
  if (valid_response != parsed_responses.end()) {
    std::move(final_callback).Run(*valid_response);
    return;
  }

  // All the frames have invalid selection, return an invalid response.
  std::move(final_callback).Run([WebSelectionResponse invalidResponse]);
}
