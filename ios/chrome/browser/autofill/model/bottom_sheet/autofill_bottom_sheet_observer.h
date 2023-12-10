// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_BOTTOM_SHEET_AUTOFILL_BOTTOM_SHEET_OBSERVER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_BOTTOM_SHEET_AUTOFILL_BOTTOM_SHEET_OBSERVER_H_

namespace autofill {

struct FormActivityParams;

// Interface for observing autofill bottom sheet activity.
class AutofillBottomSheetObserver {
 public:
  AutofillBottomSheetObserver() {}

  AutofillBottomSheetObserver(const AutofillBottomSheetObserver&) = delete;
  AutofillBottomSheetObserver& operator=(const AutofillBottomSheetObserver&) =
      delete;

  virtual ~AutofillBottomSheetObserver() {}

  // Called when the payments bottom sheet is about to be shown. Sends the
  // params used to open the payments bottom sheet.
  virtual void WillShowPaymentsBottomSheet(const FormActivityParams& params) {}
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_BOTTOM_SHEET_AUTOFILL_BOTTOM_SHEET_OBSERVER_H_
