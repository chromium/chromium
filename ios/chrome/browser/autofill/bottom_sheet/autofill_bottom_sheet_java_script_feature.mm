// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/bottom_sheet/autofill_bottom_sheet_java_script_feature.h"

#import "base/values.h"
#import "components/autofill/core/common/password_form_fill_data.h"
#import "ios/chrome/browser/autofill/bottom_sheet/autofill_bottom_sheet_tab_helper.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
constexpr char kScriptName[] = "bottom_sheet";
constexpr char kScriptMessageName[] = "BottomSheetMessage";
}  // namespace

absl::optional<std::string>
AutofillBottomSheetJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kScriptMessageName;
}

void AutofillBottomSheetJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  AutofillBottomSheetTabHelper* helper =
      AutofillBottomSheetTabHelper::FromWebState(web_state);
  if (helper) {
    helper->OnFormMessageReceived(message);
  }
}

// static
AutofillBottomSheetJavaScriptFeature*
AutofillBottomSheetJavaScriptFeature::GetInstance() {
  static base::NoDestructor<AutofillBottomSheetJavaScriptFeature> instance;
  return instance.get();
}

AutofillBottomSheetJavaScriptFeature::AutofillBottomSheetJavaScriptFeature()
    : web::JavaScriptFeature(web::ContentWorld::kIsolatedWorld,
                             {FeatureScript::CreateWithFilename(
                                 kScriptName,
                                 FeatureScript::InjectionTime::kDocumentEnd,
                                 FeatureScript::TargetFrames::kAllFrames,
                                 FeatureScript::ReinjectionBehavior::
                                     kReinjectOnDocumentRecreation)}) {}

AutofillBottomSheetJavaScriptFeature::~AutofillBottomSheetJavaScriptFeature() =
    default;

void AutofillBottomSheetJavaScriptFeature::AttachListeners(
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

void AutofillBottomSheetJavaScriptFeature::DetachListenersAndRefocus(
    web::WebFrame* frame) {
  CallJavaScriptFunction(frame, "bottomSheet.detachListenersAndRefocus", {});
}
