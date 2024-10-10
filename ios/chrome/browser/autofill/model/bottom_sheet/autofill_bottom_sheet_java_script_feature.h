// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_BOTTOM_SHEET_AUTOFILL_BOTTOM_SHEET_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_BOTTOM_SHEET_AUTOFILL_BOTTOM_SHEET_JAVA_SCRIPT_FEATURE_H_

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
      web::WebFrame* frame,
      bool allow_autofocus);

  // This function will result in detaching listeners from the provided renderer
  // ids, which will prevent the associated bottom sheet from showing up until
  // the form reloads.
  // - If "refocus" is true, it will also re-trigger a focus event on the field
  // that had originally triggered the bottom sheet.
  void DetachListeners(const std::set<autofill::FieldRendererId>& renderer_ids,
                       web::WebFrame* frame,
                       bool refocus);

  // Refocuses the last element that was blurred by the bottom sheet listeners
  // if needed. Does the same job as calling DetachListeners() with `refocus`
  // set to true. The last element is reset after focusing, meaning that
  // refocusing multiple times will be no op until a new sheet is presented.
  void RefocusElementIfNeeded(web::WebFrame* frame);

 private:
  friend class base::NoDestructor<AutofillBottomSheetJavaScriptFeature>;

  // web::JavaScriptFeature
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(web::WebState* web_state,
                             const web::ScriptMessage& message) override;

  AutofillBottomSheetJavaScriptFeature();
  ~AutofillBottomSheetJavaScriptFeature() override;
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_BOTTOM_SHEET_AUTOFILL_BOTTOM_SHEET_JAVA_SCRIPT_FEATURE_H_
