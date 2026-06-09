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
const char kScriptName[] = "text_main";
const char kScriptHandlerName[] = "annotations";

web::AnnotationsJavaScriptFeature* g_instance_for_testing = nullptr;

web::JavaScriptFeature::FeatureScript::PlaceholderReplacements
GetAnnotationsReplacements(bool trusted_event_check_enabled) {
  return @{
    @"{{SkipTrustedCheckForTesting}}" : trusted_event_check_enabled ? @"false"
                                                                    : @"true"
  };
}
}  // namespace

namespace web {

const int kMaxAnnotationsTextLength = 65535;
const int kMaxAnnotationsMetadataLength = 256;

AnnotationsJavaScriptFeature::AnnotationsJavaScriptFeature()
    : AnnotationsJavaScriptFeature(true) {}

AnnotationsJavaScriptFeature::AnnotationsJavaScriptFeature(
    bool trusted_event_check_enabled)
    : JavaScriptFeature(
          ContentWorld::kIsolatedWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kMainFrame,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow,
              base::BindRepeating(&GetAnnotationsReplacements,
                                  trusted_event_check_enabled))}),
      trusted_event_check_enabled_(trusted_event_check_enabled) {}

AnnotationsJavaScriptFeature::~AnnotationsJavaScriptFeature() = default;

// static
AnnotationsJavaScriptFeature* AnnotationsJavaScriptFeature::GetInstance() {
  if (g_instance_for_testing) {
    return g_instance_for_testing;
  }
  static base::NoDestructor<AnnotationsJavaScriptFeature> instance;
  return instance.get();
}

// static
void AnnotationsJavaScriptFeature::SetInstanceForTesting(
    AnnotationsJavaScriptFeature* instance) {
  g_instance_for_testing = instance;
}

void AnnotationsJavaScriptFeature::ExtractText(WebState* web_state,
                                               int maximum_text_length,
                                               int seq_id) {
  DCHECK(web_state);
  WebFrame* frame = GetWebFramesManager(web_state)->GetMainWebFrame();
  if (!frame) {
    return;
  }

  base::ListValue parameters;
  parameters.Append(maximum_text_length);
  CallJavaScriptFunction(frame, "annotations.start", parameters);
}

void AnnotationsJavaScriptFeature::DecorateAnnotations(WebState* web_state,
                                                       base::Value& annotations,
                                                       int seq_id) {
  DCHECK(web_state);
  WebFrame* frame = GetWebFramesManager(web_state)->GetMainWebFrame();
  if (!frame) {
    return;
  }

  base::ListValue parameters;
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

  base::ListValue parameters;
  parameters.Append(std::move(type));

  CallJavaScriptFunction(frame, "annotations.removeDecorationsWithType",
                         parameters);
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

  const base::DictValue& dict = response->GetDict();

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
    for (const auto pair : dict) {
      const std::string& key = pair.first;
      if (key != "command" && key != "text" && key != "seqId" &&
          key != "metadata") {
        return;
      }
    }
    const std::string* text = dict.FindString("text");
    std::optional<double> seq_id = dict.FindDouble("seqId");
    const base::DictValue* metadata = dict.FindDict("metadata");
    if (!text || !seq_id || !metadata) {
      return;
    }
    for (const auto pair : *metadata) {
      const std::string& key = pair.first;
      if (key != "htmlLang" && key != "httpContentLanguage" &&
          key != "wkNoTelephone" && key != "wkNoEmail" &&
          key != "wkNoAddress" && key != "wkNoDate" && key != "wkNoUnit") {
        return;
      }
    }
    if (text->size() > kMaxAnnotationsTextLength) {
      return;
    }
    const std::string* html_lang = metadata->FindString("htmlLang");
    if (html_lang && html_lang->size() > kMaxAnnotationsMetadataLength) {
      return;
    }
    const std::string* http_content_language =
        metadata->FindString("httpContentLanguage");
    if (http_content_language &&
        http_content_language->size() > kMaxAnnotationsMetadataLength) {
      return;
    }
    manager->OnTextExtracted(web_state, *text, static_cast<int>(seq_id.value()),
                             *metadata);
  } else if (*command == "annotations.decoratingComplete") {
    for (const auto pair : dict) {
      const std::string& key = pair.first;
      if (key != "command" && key != "annotations" && key != "successes" &&
          key != "failures" && key != "cancelled") {
        return;
      }
    }
    std::optional<double> optional_annotations = dict.FindDouble("annotations");
    std::optional<double> optional_successes = dict.FindDouble("successes");
    std::optional<double> optional_failures = dict.FindDouble("failures");
    const base::ListValue* cancelled = dict.FindList("cancelled");
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
    if (trusted_event_check_enabled_ && !script_message.is_user_interacting()) {
      return;
    }
    for (const auto pair : dict) {
      const std::string& key = pair.first;
      if (key != "command" && key != "data" && key != "rect" && key != "text" &&
          key != "cancel") {
        return;
      }
    }
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
