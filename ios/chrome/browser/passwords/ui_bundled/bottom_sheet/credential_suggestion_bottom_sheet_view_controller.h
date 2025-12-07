// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_UI_BUNDLED_BOTTOM_SHEET_CREDENTIAL_SUGGESTION_BOTTOM_SHEET_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_PASSWORDS_UI_BUNDLED_BOTTOM_SHEET_CREDENTIAL_SUGGESTION_BOTTOM_SHEET_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/passwords/ui_bundled/bottom_sheet/credential_suggestion_bottom_sheet_consumer.h"
#import "ios/chrome/browser/shared/ui/bottom_sheet/table_view_bottom_sheet_view_controller.h"

@protocol CredentialSuggestionBottomSheetDelegate;
@protocol CredentialSuggestionBottomSheetHandler;

class GURL;

// Credential Bottom Sheet UI, which includes a table to display password and
// passkey suggestions, a button to use a suggestion and a button to revert to
// using the keyboard to enter a password.
@interface CredentialSuggestionBottomSheetViewController
    : TableViewBottomSheetViewController <
          CredentialSuggestionBottomSheetConsumer>

// Initialize with the delegate used to open the password manager and the URL of
// the current page.
- (instancetype)initWithHandler:
                    (id<CredentialSuggestionBottomSheetHandler>)handler
                            URL:(const GURL&)URL;

// The delegate for the bottom sheet view controller.
@property(nonatomic, strong) id<CredentialSuggestionBottomSheetDelegate>
    delegate;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_UI_BUNDLED_BOTTOM_SHEET_CREDENTIAL_SUGGESTION_BOTTOM_SHEET_VIEW_CONTROLLER_H_
