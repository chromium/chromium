// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_CONSTANTS_H_

#import <Foundation/Foundation.h>

namespace manual_fill {

// Accessibility identifier for the expanded manual fill view.
extern NSString* const kExpandedManualFillViewID;

// Accessibility identifier for the header view of the expanded manual fill
// view.
extern NSString* const kExpandedManualFillHeaderViewID;

// Accessibility identifier for the header top view of the expanded manual fill
// view.
extern NSString* const kExpandedManualFillHeaderTopViewID;

// Accessibility identifier for the Chrome logo of the expanded manual fill
// view.
extern NSString* const kExpandedManualFillChromeLogoID;

// Possible data types when manually filling a form.
enum class ManualFillDataType {
  kPassword = 0,
  kPaymentMethod,
  kAddress,
};

}  // namespace manual_fill

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_CONSTANTS_H_
