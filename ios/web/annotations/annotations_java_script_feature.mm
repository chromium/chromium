// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/annotations/annotations_java_script_feature.h"

#import <vector>

#import "base/logging.h"
#import "base/metrics/histogram_macros.h"
#import "base/no_destructor.h"
#import "base/strings/sys_string_conversions.h"
#import "components/shared_highlighting/ios/parsing_utils.h"
#import "ios/web/annotations/annotations_text_manager_impl.h"
#import "ios/web/common/features.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"

namespace {
const char kLegacyScriptName[] = "annotations";
const char kScriptName[] = "text_main";
const char kScriptHandlerName[] = "annotations";
}  // namespace

namespace web {

const char* GetScriptName() {
  return base::FeatureList::IsEnabled(features::kEnableViewportIntents)
             ? kScriptName
             : kLegacyScriptName;
}

AnnotationsJavaScriptFeature::AnnotationsJavaScriptFeature()
    : JavaScriptFeature(
          ContentWorld::kIsolatedWorld,
          {FeatureScript::CreateWithFilename(
              GetScriptName(),
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
  WebFrame* frame = GetWebFramesManager(web_state)->GetMainWebFrame();
  if (!frame) {
    return;
  }

  base::Value::List parameters;
  if (base::FeatureList::IsEnabled(features::kEnableViewportIntents)) {
    CallJavaScriptFunction(frame, "annotations.start", parameters);
  } else {
    parameters.Append(maximum_text_length);
    parameters.Append(seq_id);
    CallJavaScriptFunction(frame, "annotations.extractText", parameters);
  }
}

void AnnotationsJavaScriptFeature::DecorateAnnotations(WebState* web_state,
                                                       base::Value& annotations,
                                                       int seq_id) {
  DCHECK(web_state);
  WebFrame* frame = GetWebFramesManager(web_state)->GetMainWebFrame();
  if (!frame) {
    return;
  }

  base::Value::List parameters;
  parameters.Append(std::move(annotations));
  parameters.Append(seq_id);
  CallJavaScriptFunction(frame, "annotations.decorateAnnotations", parameters);
}

void AnnotationsJavaScriptFeature::RemoveDecorations(WebState* web_state) {
  DCHECK(web_state);
  WebFrame* frame = GetWebFramesManager(web_state)->GetMainWebFrame();
  if (!frame) {
    return;
  }

  CallJavaScriptFunction(frame, "annotations.removeDecorations", {});
}

void AnnotationsJavaScriptFeature::RemoveDecorationsWithType(
    WebState* web_state,
    const std::string& type) {
  DCHECK(web_state);
  WebFrame* frame = GetWebFramesManager(web_state)->GetMainWebFrame();
  if (!frame) {
    return;
  }

  base::Value::List parameters;
  parameters.Append(std::move(type));

  CallJavaScriptFunction(frame, "annotations.removeDecorationsWithType",
                         parameters);
}

void AnnotationsJavaScriptFeature::RemoveHighlight(WebState* web_state) {
  DCHECK(web_state);
  WebFrame* frame = GetWebFramesManager(web_state)->GetMainWebFrame();
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

  const base::Value::Dict& dict = response->GetDict();

  const std::string* command = dict.FindString("command");
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
    const std::string* text = dict.FindString("text");
    std::optional<double> seq_id = dict.FindDouble("seqId");
    const base::Value::Dict* metadata = dict.FindDict("metadata");
    if (!text || !seq_id || !metadata) {
      return;
    }
    manager->OnTextExtracted(web_state, *text, static_cast<int>(seq_id.value()),
                             *metadata);
  } else if (*command == "annotations.decoratingComplete") {
    std::optional<double> optional_annotations = dict.FindDouble("annotations");
    std::optional<double> optional_successes = dict.FindDouble("successes");
    std::optional<double> optional_failures = dict.FindDouble("failures");
    const base::Value::List* cancelled = dict.FindList("cancelled");
    if (!optional_annotations || !optional_successes || !optional_failures ||
        !cancelled) {
      return;
    }
    int annotations = static_cast<int>(optional_annotations.value());
    int successes = static_cast<int>(optional_successes.value());
    int failures = static_cast<int>(optional_failures.value());
    manager->OnDecorated(web_state, annotations, successes, failures,
                         *cancelled);
  } else if (*command == "annotations.onClick") {
    const std::string* data = dict.FindString("data");
    std::optional<CGRect> rect =
        shared_highlighting::ParseRect(dict.FindDict("rect"));
    const std::string* text = dict.FindString("text");
    std::optional<bool> cancel = dict.FindBool("cancel");
    if (!data || !rect || !text || !cancel) {
      return;
    }
    UMA_HISTOGRAM_BOOLEAN("IOS.Annotations.UserTap.Cancelled", *cancel);
    if (!*cancel) {
      manager->OnClick(
          web_state, *text,
          shared_highlighting::ConvertToBrowserRect(*rect, web_state), *data);
    }
  }
}

std::optional<std::string>
AnnotationsJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kScriptHandlerName;
}

}  // namespace web
