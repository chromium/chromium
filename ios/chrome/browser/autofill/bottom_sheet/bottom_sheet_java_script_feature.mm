// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/bottom_sheet/bottom_sheet_java_script_feature.h"

#import "base/values.h"
#import "components/autofill/core/common/password_form_fill_data.h"
#import "ios/chrome/browser/autofill/bottom_sheet/bottom_sheet_tab_helper.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
constexpr char kScriptName[] = "bottom_sheet";
constexpr char kScriptMessageName[] = "BottomSheetMessage";
}  // namespace

absl::optional<std::string>
BottomSheetJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kScriptMessageName;
}

void BottomSheetJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  BottomSheetTabHelper* helper = BottomSheetTabHelper::FromWebState(web_state);
  if (helper) {
    helper->OnFormMessageReceived(message);
  }
}

// static
BottomSheetJavaScriptFeature* BottomSheetJavaScriptFeature::GetInstance() {
  static base::NoDestructor<BottomSheetJavaScriptFeature> instance;
  return instance.get();
}

BottomSheetJavaScriptFeature::BottomSheetJavaScriptFeature()
    : web::JavaScriptFeature(
          // TODO(crbug.com/1175793): Move autofill code to kIsolatedWorld
          // once all scripts are converted to JavaScriptFeatures.
          web::ContentWorld::kPageContentWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentEnd,
              FeatureScript::TargetFrames::kAllFrames,
              FeatureScript::ReinjectionBehavior::
                  kReinjectOnDocumentRecreation)}) {}

BottomSheetJavaScriptFeature::~BottomSheetJavaScriptFeature() = default;

void BottomSheetJavaScriptFeature::AttachListeners(
    const std::vector<autofill::FieldRendererId>& renderer_ids,
    web::WebFrame* frame) {
  base::Value::List renderer_id_list =
      base::Value::List::with_capacity(renderer_ids.size());
  for (auto renderer_id : renderer_ids) {
    renderer_id_list.Append(static_cast<int>(renderer_id.value()));
  }
  std::vector<base::Value> parameters;
  parameters.push_back(base::Value(std::move(renderer_id_list)));
  CallJavaScriptFunction(frame, "bottomSheet.attachListeners", parameters);
}

void BottomSheetJavaScriptFeature::DetachListenersAndRefocus(
    web::WebFrame* frame) {
  CallJavaScriptFunction(frame, "bottomSheet.detachListenersAndRefocus", {});
}
