// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_scroll_anchor_java_script_feature.h"

#import "base/strings/string_number_conversions.h"
#import "base/values.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"

namespace {
const char kScriptName[] = "reader_mode_scroll_anchor";
const char kScriptHandlerName[] = "ReaderModeScrollAnchorMessageHandler";
}  // namespace

ReaderModeScrollAnchorJavaScriptFeature::
    ReaderModeScrollAnchorJavaScriptFeature()
    : JavaScriptFeature(
          web::ContentWorld::kIsolatedWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentEnd,
              FeatureScript::TargetFrames::kMainFrame,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)}) {}

ReaderModeScrollAnchorJavaScriptFeature::
    ~ReaderModeScrollAnchorJavaScriptFeature() = default;

// static
ReaderModeScrollAnchorJavaScriptFeature*
ReaderModeScrollAnchorJavaScriptFeature::GetInstance() {
  static base::NoDestructor<ReaderModeScrollAnchorJavaScriptFeature> instance;
  return instance.get();
}

std::optional<std::string>
ReaderModeScrollAnchorJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kScriptHandlerName;
}

void ReaderModeScrollAnchorJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  ReaderModeTabHelper* tab_helper =
      ReaderModeTabHelper::FromWebState(web_state);
  if (!tab_helper) {
    return;
  }
  const base::Value::Dict* dict = message.body()->GetIfDict();
  if (!dict) {
    return;
  }
  std::optional<double> hash = dict->FindDouble("hash");
  std::optional<double> char_count = dict->FindDouble("charCount");
  std::optional<double> progress = dict->FindDouble("progress");
  std::optional<bool> is_scrolled_at_top = dict->FindBool("isScrolledAtTop");

  if (is_scrolled_at_top.has_value() && is_scrolled_at_top.value()) {
    // If the original page is scrolled at the top, disable automatic scrolling
    // script when entering Reader mode.
    tab_helper->SetScrollAnchorScript(std::string());
    return;
  }

  if (hash.has_value() && char_count.has_value() && progress.has_value()) {
    std::string script = "scrollToParagraphByHash(" +
                         base::NumberToString(hash.value()) + ", " +
                         base::NumberToString(char_count.value()) + ", " +
                         base::NumberToString(progress.value()) + ");";
    tab_helper->SetScrollAnchorScript(std::move(script));
  }
}

void ReaderModeScrollAnchorJavaScriptFeature::FindScrollAnchor(
    web::WebFrame* web_frame) {
  CallJavaScriptFunction(web_frame, "readerModeScrollAnchor.findScrollAnchor",
                         {});
}
