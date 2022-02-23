// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/link_to_text/link_to_text_java_script_feature.h"

#include "base/no_destructor.h"
#import "base/timer/elapsed_timer.h"
#import "ios/chrome/browser/link_to_text/link_to_text_constants.h"
#import "ios/web/public/js_messaging/web_frame.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const char kScriptName[] = "link_to_text_js";
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

void LinkToTextJavaScriptFeature::HandleResponse(
    web::WebState* web_state,
    base::ElapsedTimer link_generation_timer,
    base::OnceCallback<void(LinkToTextResponse*)> callback,
    const base::Value* response) {
  std::move(callback).Run([LinkToTextResponse
      linkToTextResponseWithValue:response
                         webState:web_state
                          latency:link_generation_timer.Elapsed()]);
}
