// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/text_fragments/text_fragments_java_script_feature.h"

#import <vector>

#import "base/no_destructor.h"
#import "base/strings/sys_string_conversions.h"
#import "components/shared_highlighting/ios/parsing_utils.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/text_fragments/text_fragments_manager_impl.h"

namespace {
const char kScriptName[] = "text_fragments";
const char kScriptHandlerName[] = "textFragments";
const char kHandleFragmentsScript[] = "textFragments.handleTextFragments";
const char kRemoveHighlightsScript[] = "textFragments.removeHighlights";

const double kMaxSelectorCount = 200.0;
const double kMinSelectorCount = 0.0;
}

namespace web {

TextFragmentsJavaScriptFeature::TextFragmentsJavaScriptFeature()
    : JavaScriptFeature(
          ContentWorld::kIsolatedWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kMainFrame,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)}) {}

TextFragmentsJavaScriptFeature::~TextFragmentsJavaScriptFeature() = default;

// static
TextFragmentsJavaScriptFeature* TextFragmentsJavaScriptFeature::GetInstance() {
  static base::NoDestructor<TextFragmentsJavaScriptFeature> instance;
  return instance.get();
}

void TextFragmentsJavaScriptFeature::ProcessTextFragments(
    WebState* web_state,
    base::Value parsed_fragments,
    std::string background_color_hex_rgb,
    std::string foreground_color_hex_rgb) {
  DCHECK(web_state);
  WebFrame* frame = GetWebFramesManager(web_state)->GetMainWebFrame();
  if (!frame) {
    return;
  }

  base::Value bg_color = background_color_hex_rgb.empty()
                             ? base::Value()
                             : base::Value(background_color_hex_rgb);
  base::Value fg_color = foreground_color_hex_rgb.empty()
                             ? base::Value()
                             : base::Value(foreground_color_hex_rgb);

  auto parameters = base::Value::List()
                        .Append(std::move(parsed_fragments))
                        .Append(/*scroll=*/true)
                        .Append(std::move(bg_color))
                        .Append(std::move(fg_color));
  CallJavaScriptFunction(frame, kHandleFragmentsScript, parameters);
}

void TextFragmentsJavaScriptFeature::RemoveHighlights(WebState* web_state,
                                                      const GURL& new_url) {
  DCHECK(web_state);
  WebFrame* frame = GetWebFramesManager(web_state)->GetMainWebFrame();
  if (!frame) {
    return;
  }

  auto parameters =
      base::Value::List().Append(new_url.is_valid() ? new_url.spec() : "");
  CallJavaScriptFunction(frame, kRemoveHighlightsScript, parameters);
}

void TextFragmentsJavaScriptFeature::ScriptMessageReceived(
    WebState* web_state,
    const ScriptMessage& script_message) {
  auto* manager = TextFragmentsManagerImpl::FromWebState(web_state);
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
  if (!sender_url || *sender_url != current_url) {
    return;
  }

  if (*command == "textFragments.processingComplete") {
    // Extract success metrics.
    std::optional<double> optional_fragment_count =
        dict.FindDoubleByDottedPath("result.fragmentsCount");
    std::optional<double> optional_success_count =
        dict.FindDoubleByDottedPath("result.successCount");

    // Since the response can't be trusted, don't log metrics if the results
    // look invalid.
    if (!optional_fragment_count ||
        optional_fragment_count.value() > kMaxSelectorCount ||
        optional_fragment_count.value() <= kMinSelectorCount) {
      return;
    }
    if (!optional_success_count ||
        optional_success_count.value() > kMaxSelectorCount ||
        optional_success_count.value() < kMinSelectorCount) {
      return;
    }
    if (optional_success_count.value() > optional_fragment_count.value()) {
      return;
    }

    int fragment_count = static_cast<int>(optional_fragment_count.value());
    int success_count = static_cast<int>(optional_success_count.value());

    manager->OnProcessingComplete(success_count, fragment_count);
  } else if (*command == "textFragments.onClick") {
    manager->OnClick();
  } else if (*command == "textFragments.onClickWithSender") {
    std::optional<CGRect> rect =
        shared_highlighting::ParseRect(dict.FindDict("rect"));
    const std::string* text = dict.FindString("text");

    const base::Value::List* fragment_values_list = dict.FindList("fragments");
    std::vector<shared_highlighting::TextFragment> fragments;
    if (fragment_values_list) {
      for (const base::Value& val : *fragment_values_list) {
        std::optional<shared_highlighting::TextFragment> fragment =
            shared_highlighting::TextFragment::FromValue(&val);
        if (fragment) {
          fragments.push_back(*fragment);
        }
      }
    }

    if (!rect || !text || fragments.empty()) {
      return;
    }
    manager->OnClickWithSender(
        shared_highlighting::ConvertToBrowserRect(*rect, web_state),
        base::SysUTF8ToNSString(*text), std::move(fragments));
  }
}

std::optional<std::string>
TextFragmentsJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kScriptHandlerName;
}

}  // namespace web
