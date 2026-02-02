// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_PASSKEY_SUGGESTION_BOTTOM_SHEET_MEDIATOR_H_
#define IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_PASSKEY_SUGGESTION_BOTTOM_SHEET_MEDIATOR_H_

#import <string>

#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/credential_suggestion_bottom_sheet_mediator_base.h"

// Mediator responsible for providing and handling passkey suggestions shown in
// the Credential Suggestion Bottom Sheet. Used when passkey suggestions are
// presented in the context of a modal passkey request.
@interface PasskeySuggestionBottomSheetMediator
    : CredentialSuggestionBottomSheetMediatorBase

// Designated initializer for this mediator. `requestID` identifies the passkey
// request which triggered the bottom sheet.
- (instancetype)initWithRequestID:(std::string)requestID
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_PASSKEY_SUGGESTION_BOTTOM_SHEET_MEDIATOR_H_
