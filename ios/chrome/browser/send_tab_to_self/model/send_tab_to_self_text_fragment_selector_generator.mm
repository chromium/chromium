// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_text_fragment_selector_generator.h"

#import <utility>
#import <vector>

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/no_destructor.h"
#import "base/strings/string_util.h"
#import "base/time/time.h"
#import "base/values.h"
#import "components/shared_highlighting/core/common/shared_highlighting_features.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"

namespace {
const char kScriptName[] = "send_tab_to_self_fragments";
const char kGetLinkToTextFunction[] = "stts.getLinkToText";
const char kScrollToTextFragmentFunction[] = "stts.scrollToTextFragment";

// Translates the JS-layer result into a SendTabToSelfTextFragment struct.
void OnGetTextFragmentResult(
    base::OnceCallback<void(std::optional<SendTabToSelfTextFragment>)> callback,
    const base::Value* value) {
  if (!value || !value->is_dict()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  const base::DictValue* dict = value->GetIfDict();
  if (!dict) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::optional<double> status_double = dict->FindDouble("status");
  if (!status_double) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  SendTabToSelfTextFragment result;
  result.status = static_cast<TextFragmentGenerationStatus>(
      static_cast<int>(*status_double));

  const base::DictValue* fragment = dict->FindDict("fragment");
  if (fragment) {
    const std::string* text_start = fragment->FindString("textStart");
    if (text_start) {
      base::TrimWhitespaceASCII(*text_start, base::TRIM_ALL,
                                &result.text_start);
    }
    const std::string* text_end = fragment->FindString("textEnd");
    if (text_end) {
      base::TrimWhitespaceASCII(*text_end, base::TRIM_ALL, &result.text_end);
    }
    const std::string* prefix = fragment->FindString("prefix");
    if (prefix) {
      base::TrimWhitespaceASCII(*prefix, base::TRIM_ALL, &result.prefix);
    }
    const std::string* suffix = fragment->FindString("suffix");
    if (suffix) {
      base::TrimWhitespaceASCII(*suffix, base::TRIM_ALL, &result.suffix);
    }
  }
  std::move(callback).Run(std::move(result));
}

}  // namespace

SendTabToSelfTextFragment::SendTabToSelfTextFragment() = default;
SendTabToSelfTextFragment::SendTabToSelfTextFragment(
    const SendTabToSelfTextFragment&) = default;
SendTabToSelfTextFragment::SendTabToSelfTextFragment(
    SendTabToSelfTextFragment&&) = default;
SendTabToSelfTextFragment& SendTabToSelfTextFragment::operator=(
    const SendTabToSelfTextFragment&) = default;
SendTabToSelfTextFragment& SendTabToSelfTextFragment::operator=(
    SendTabToSelfTextFragment&&) = default;
SendTabToSelfTextFragment::~SendTabToSelfTextFragment() = default;

// static
SendTabToSelfTextFragmentSelectorGenerator*
SendTabToSelfTextFragmentSelectorGenerator::GetInstance() {
  static base::NoDestructor<SendTabToSelfTextFragmentSelectorGenerator>
      instance;
  return instance.get();
}

SendTabToSelfTextFragmentSelectorGenerator::
    SendTabToSelfTextFragmentSelectorGenerator()
    : web::JavaScriptFeature(
          // Run in an isolated world to avoid interfering with page scripts
          // and to ensure our text fragment script is self-contained.
          web::ContentWorld::kIsolatedWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              // Text fragments are generated relative to the main frame's
              // viewport.
              FeatureScript::TargetFrames::kMainFrame,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)}) {}

SendTabToSelfTextFragmentSelectorGenerator::
    ~SendTabToSelfTextFragmentSelectorGenerator() = default;

void SendTabToSelfTextFragmentSelectorGenerator::GetTextFragment(
    web::WebState* web_state,
    base::OnceCallback<void(std::optional<SendTabToSelfTextFragment>)>
        callback) {
  web::WebFrame* main_frame = GetWebFramesManager(web_state)->GetMainWebFrame();
  if (!main_frame) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  // Execute the text fragment generation script on the page. We use a
  // consistent timeout with other text fragment generation features in Chrome
  // to ensure a predictable user experience.
  auto [callback1, callback2] = base::SplitOnceCallback(std::move(callback));
  bool called = CallJavaScriptFunction(
      main_frame, kGetLinkToTextFunction, {},
      base::BindOnce(&OnGetTextFragmentResult, std::move(callback1)),
      base::Milliseconds(
          shared_highlighting::GetPreemptiveLinkGenTimeoutLengthMs()));

  if (!called) {
    std::move(callback2).Run(std::nullopt);
  }
}

void SendTabToSelfTextFragmentSelectorGenerator::ScrollToTextFragment(
    web::WebState* web_state,
    std::string_view text_fragment) {
  web::WebFrame* main_frame = GetWebFramesManager(web_state)->GetMainWebFrame();
  if (!main_frame) {
    return;
  }

  base::ListValue parameters;
  parameters.Append(text_fragment);

  CallJavaScriptFunction(main_frame, kScrollToTextFragmentFunction, parameters,
                         base::DoNothing(), base::TimeDelta());
}
