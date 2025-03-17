// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_INFOBAR_AUTOFILL_EDIT_PROFILE_BOTTOM_SHEET_HANDLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_INFOBAR_AUTOFILL_EDIT_PROFILE_BOTTOM_SHEET_HANDLER_H_

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/autofill_edit_profile_bottom_sheet_handler.h"

namespace web {
class WebState;
}  // namespace web

// Handler that provides the AutofillEditProfileBottomSheetCoordinator with the
// logic that is specific to an infobar-triggerred address edit.
@interface InfobarAutofillEditProfileBottomSheetHandler
    : NSObject <AutofillEditProfileBottomSheetHandler>

- (instancetype)initWithWebState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_INFOBAR_AUTOFILL_EDIT_PROFILE_BOTTOM_SHEET_HANDLER_H_
