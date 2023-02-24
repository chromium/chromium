// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/annotations/annotations_java_script_feature.h"

#import <vector>

#import "base/logging.h"
#import "base/no_destructor.h"
#import "base/strings/sys_string_conversions.h"
#import "components/shared_highlighting/ios/parsing_utils.h"
#import "ios/web/annotations/annotations_text_manager_impl.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frame_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kScriptName[] = "annotations";
const char kScriptHandlerName[] = "annotations";
}  // namespace

namespace web {

AnnotationsJavaScriptFeature::AnnotationsJavaScriptFeature()
    : JavaScriptFeature(
          ContentWorld::kIsolatedWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kMainFrame,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)}) {}

AnnotationsJavaScriptFeature::~AnnotationsJavaScriptFeature() = default;

// static
AnnotationsJavaScriptFeature* AnnotationsJavaScriptFeature::GetInstance() {
  static base::NoDestructor<AnnotationsJavaScriptFeature> instance;
  return instance.get();
}

void AnnotationsJavaScriptFeature::ExtractText(WebState* web_state,
                                               int maximum_text_length,
                                               int seq_id) {
  DCHECK(web_state);
  auto* frame = web::GetMainFrame(web_state);
  if (!frame) {
    return;
  }

  std::vector<base::Value> parameters;
  parameters.push_back(base::Value(maximum_text_length));
  parameters.push_back(base::Value(seq_id));
  CallJavaScriptFunction(frame, "annotations.extractText", parameters);
}

void AnnotationsJavaScriptFeature::DecorateAnnotations(
    WebState* web_state,
    base::Value& annotations) {
  DCHECK(web_state);
  auto* frame = web::GetMainFrame(web_state);
  if (!frame) {
    return;
  }

  std::vector<base::Value> parameters;
  parameters.push_back(base::Value(std::move(annotations)));
  CallJavaScriptFunction(frame, "annotations.decorateAnnotations", parameters);
}

void AnnotationsJavaScriptFeature::RemoveDecorations(WebState* web_state) {
  DCHECK(web_state);
  auto* frame = web::GetMainFrame(web_state);
  if (!frame) {
    return;
  }

  CallJavaScriptFunction(frame, "annotations.removeDecorations", {});
}

void AnnotationsJavaScriptFeature::RemoveHighlight(WebState* web_state) {
  DCHECK(web_state);
  auto* frame = web::GetMainFrame(web_state);
  if (!frame) {
    return;
  }

  CallJavaScriptFunction(frame, "annotations.removeHighlight", {});
}

void AnnotationsJavaScriptFeature::ScriptMessageReceived(
    WebState* web_state,
    const ScriptMessage& script_message) {
  if (!script_message.is_main_frame()) {
    return;
  }

  AnnotationsTextManagerImpl* manager =
      static_cast<AnnotationsTextManagerImpl*>(
          AnnotationsTextManager::FromWebState(web_state));
  if (!manager) {
    return;
  }

  base::Value* response = script_message.body();
  if (!response || !response->is_dict()) {
    return;
  }

  const std::string* command = response->FindStringKey("command");
  if (!command) {
    return;
  }

  // Discard messages if we've navigated away.
  auto sender_url = script_message.request_url();
  GURL current_url = web_state->GetLastCommittedURL();
  if (!sender_url || !(*sender_url).EqualsIgnoringRef(current_url)) {
    return;
  }

  if (*command == "annotations.extractedText") {
    const std::string* text = response->FindStringKey("text");
    absl::optional<double> seq_id = response->FindDoubleKey("seqId");
    if (!text || !seq_id) {
      return;
    }
    manager->OnTextExtracted(web_state, *text,
                             static_cast<int>(seq_id.value()));
  } else if (*command == "annotations.decoratingComplete") {
    absl::optional<double> optional_annotations =
        response->FindDoubleKey("annotations");
    absl::optional<double> optional_successes =
        response->FindDoubleKey("successes");
    if (!optional_annotations || !optional_successes) {
      return;
    }
    int annotations = static_cast<int>(optional_annotations.value());
    int successes = static_cast<int>(optional_successes.value());
    manager->OnDecorated(web_state, successes, annotations);
  } else if (*command == "annotations.onClick") {
    const std::string* data = response->FindStringKey("data");
    absl::optional<CGRect> rect =
        shared_highlighting::ParseRect(response->FindDictKey("rect"));
    const std::string* text = response->FindStringKey("text");
    if (!data || !rect || !text) {
      return;
    }
    manager->OnClick(
        web_state, *text,
        shared_highlighting::ConvertToBrowserRect(*rect, web_state), *data);
  }
}

absl::optional<std::string>
AnnotationsJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kScriptHandlerName;
}

}  // namespace web
