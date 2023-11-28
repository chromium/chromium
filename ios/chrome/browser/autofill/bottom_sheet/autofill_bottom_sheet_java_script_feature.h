// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_BOTTOM_SHEET_AUTOFILL_BOTTOM_SHEET_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_AUTOFILL_BOTTOM_SHEET_AUTOFILL_BOTTOM_SHEET_JAVA_SCRIPT_FEATURE_H_

#import "base/no_destructor.h"
#import "components/autofill/core/common/unique_ids.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

#import <set>

class AutofillBottomSheetJavaScriptFeature : public web::JavaScriptFeature {
 public:
  static AutofillBottomSheetJavaScriptFeature* GetInstance();

  // This function sends the relevant renderer IDs to the bottom_sheet.ts
  // script, which will result in attaching listeners for the focus events
  // on these fields.
  void AttachListeners(
      const std::vector<autofill::FieldRendererId>& renderer_ids,
      web::WebFrame* frame);

  // This function will result in detaching listeners from the provided renderer
  // ids, which will prevent the associated bottom sheet from showing up until
  // the form reloads.
  // - If "must_be_empty" is true, this means that the field must be empty in
  // order to handle the focus event.
  // - If "refocus" is true, it will also re-trigger a focus event on the field
  // that had originally triggered the bottom sheet.
  void DetachListeners(const std::set<autofill::FieldRendererId>& renderer_ids,
                       web::WebFrame* frame,
                       bool refocus);

 private:
  friend class base::NoDestructor<AutofillBottomSheetJavaScriptFeature>;

  // web::JavaScriptFeature
  absl::optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(web::WebState* web_state,
                             const web::ScriptMessage& message) override;

  AutofillBottomSheetJavaScriptFeature();
  ~AutofillBottomSheetJavaScriptFeature() override;
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_BOTTOM_SHEET_AUTOFILL_BOTTOM_SHEET_JAVA_SCRIPT_FEATURE_H_
