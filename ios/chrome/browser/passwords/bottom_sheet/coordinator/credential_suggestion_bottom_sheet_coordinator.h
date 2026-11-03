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

@class CredentialSuggestionBottomSheetCoordinator;
@class FormSuggestion;

// Delegate protocol for `CredentialSuggestionBottomSheetCoordinator`.
@protocol CredentialSuggestionBottomSheetCoordinatorDelegate <NSObject>

// Called when the user selects a suggestion in the bottom sheet.
- (void)credentialSuggestionBottomSheetCoordinator:
            (CredentialSuggestionBottomSheetCoordinator*)coordinator
                               didSelectSuggestion:(FormSuggestion*)suggestion
                                           atIndex:(NSInteger)index;

// Called when the bottom sheet was dismissed.
- (void)credentialSuggestionBottomSheetCoordinatorDidDismiss:
    (CredentialSuggestionBottomSheetCoordinator*)coordinator;

@end

// This coordinator is responsible for creating the bottom sheet's mediator and
// view controller.
@interface CredentialSuggestionBottomSheetCoordinator
    : ChromeCoordinator <CredentialSuggestionBottomSheetHandler,
                         CredentialSuggestionBottomSheetPresenter>

// Delegate for this coordinator.
@property(nonatomic, weak)
    id<CredentialSuggestionBottomSheetCoordinatorDelegate>
        delegate;

// Initializer for password only bottom sheet and for conditional passkey
// requests, which can show both password and passkey suggestions.
// `viewController` is the VC used to present the bottom sheet.
// `params` comes from the form (in bottom_sheet.ts) and contains
// the information required to query password suggestions.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                    params:(const autofill::FormActivityParams&)
                                               params NS_DESIGNATED_INITIALIZER;

// Initializer for modal passkey requests, which will only show passkey
// suggestions. `viewController` is the VC used to present the bottom sheet.
// `requestInfo` comes from the PasskeyTabHelper and contains information on the
// passkey request which triggered this bottom sheet.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                               requestInfo:
                                   (webauthn::IOSPasskeyClient::RequestInfo)
                                       requestInfo NS_DESIGNATED_INITIALIZER;

// Initializer for preset credential suggestions. `viewController` is
// the view controller used to present the bottom sheet. `suggestions` contains
// the form suggestions to display. `activeIndex` is the index of the suggestion
// that should be initially selected.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                               suggestions:
                                   (NSArray<FormSuggestion*>*)suggestions
                               activeIndex:(NSInteger)activeIndex
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Returns YES if the bottom sheet is currently displaying the given request.
- (BOOL)hasPendingRequest:
    (const webauthn::IOSPasskeyClient::RequestInfo&)requestInfo;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_CREDENTIAL_SUGGESTION_BOTTOM_SHEET_COORDINATOR_H_
