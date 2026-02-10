// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_CREDENTIAL_SUGGESTION_BOTTOM_SHEET_COORDINATOR_H_
#define IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_CREDENTIAL_SUGGESTION_BOTTOM_SHEET_COORDINATOR_H_

#import <string>

#import "components/webauthn/ios/ios_passkey_client.h"
#import "ios/chrome/browser/passwords/bottom_sheet/ui/credential_suggestion_bottom_sheet_handler.h"
#import "ios/chrome/browser/passwords/bottom_sheet/ui/credential_suggestion_bottom_sheet_presenter.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

namespace autofill {
struct FormActivityParams;
}  // namespace autofill

@protocol BrowserCoordinatorCommands;
@protocol PasswordControllerDelegate;
@protocol SettingsCommands;

// This coordinator is responsible for creating the bottom sheet's mediator and
// view controller.
@interface CredentialSuggestionBottomSheetCoordinator
    : ChromeCoordinator <CredentialSuggestionBottomSheetHandler,
                         CredentialSuggestionBottomSheetPresenter>

// Initializer for password only bottom sheet and for conditional passkey
// requests, which can show both password and passkey suggestions.
// `viewController` is the VC used to present the bottom sheet.
// `params` comes from the form (in bottom_sheet.ts) and contains
// the information required to query password suggestions.
- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                        params:(const autofill::FormActivityParams&)params
                      delegate:(id<PasswordControllerDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

// Initializer for modal passkey requests, which will only show passkey
// suggestions. `viewController` is the VC used to present the bottom sheet.
// `requestInfo` comes from the PasskeyTabHelper and contains information on the
// passkey request which triggered this bottom sheet.
- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                   requestInfo:
                       (webauthn::IOSPasskeyClient::RequestInfo)requestInfo
                      delegate:(id<PasswordControllerDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Handler for Browser Coordinator Commands.
@property(nonatomic, weak) id<BrowserCoordinatorCommands>
    browserCoordinatorCommandsHandler;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_CREDENTIAL_SUGGESTION_BOTTOM_SHEET_COORDINATOR_H_
