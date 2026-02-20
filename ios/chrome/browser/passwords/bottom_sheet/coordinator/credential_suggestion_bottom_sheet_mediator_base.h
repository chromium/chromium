// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_CREDENTIAL_SUGGESTION_BOTTOM_SHEET_MEDIATOR_BASE_H_
#define IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_CREDENTIAL_SUGGESTION_BOTTOM_SHEET_MEDIATOR_BASE_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/passwords/bottom_sheet/ui/credential_suggestion_bottom_sheet_delegate.h"

class WebStateList;
enum class PasswordSuggestionBottomSheetExitReason;

@class FormSuggestion;
@protocol CredentialSuggestionBottomSheetConsumer;
@protocol CredentialSuggestionBottomSheetPresenter;

// Base class for the mediators responsible for fetching and handling the
// password and passkey suggestions shown in the Credential Suggestion Bottom
// Sheet.
@interface CredentialSuggestionBottomSheetMediatorBase
    : NSObject <CredentialSuggestionBottomSheetDelegate>

// The consumer for this mediator.
@property(nonatomic, weak) id<CredentialSuggestionBottomSheetConsumer> consumer;

// Presenter that controls the presentation of the bottom sheet.
@property(nonatomic, weak) id<CredentialSuggestionBottomSheetPresenter>
    presenter;

// Designated initializer. `webStateList` is the list of web states to observe.
- (instancetype)initWithWebStateList:(WebStateList*)webStateList
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator.
- (void)disconnect;

// Whether the mediator has any suggestions for the user.
- (BOOL)hasSuggestions;

// Sends the information about which suggestion from the bottom sheet was
// selected by the user, which is expected to fill the relevant fields.
- (void)didSelectSuggestion:(FormSuggestion*)formSuggestion
                    atIndex:(NSInteger)index
                 completion:(ProceduralBlock)completion;

// Logs the reason for exiting the bottom sheet, like dismissal or using a
// credential.
- (void)logExitReason:(PasswordSuggestionBottomSheetExitReason)exitReason;

// Handler called to perform operations when the sheet was dismissed without
// using any credential action.
- (void)onDismissWithoutAnyCredentialAction;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_CREDENTIAL_SUGGESTION_BOTTOM_SHEET_MEDIATOR_BASE_H_
