// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_UI_BUNDLED_BOTTOM_SHEET_CREDENTIAL_SUGGESTION_BOTTOM_SHEET_MEDIATOR_H_
#define IOS_CHROME_BROWSER_PASSWORDS_UI_BUNDLED_BOTTOM_SHEET_CREDENTIAL_SUGGESTION_BOTTOM_SHEET_MEDIATOR_H_

#import <optional>

#import "base/ios/block_types.h"
#import "base/memory/scoped_refptr.h"
#import "ios/chrome/browser/passwords/ui_bundled/bottom_sheet/credential_suggestion_bottom_sheet_delegate.h"
#import "ios/chrome/browser/passwords/ui_bundled/bottom_sheet/password_suggestion_bottom_sheet_exit_reason.h"

namespace autofill {
struct FormActivityParams;
}  // namespace autofill

namespace feature_engagement {
class Tracker;
}  // namespace feature_engagement

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace password_manager {
struct CredentialUIEntry;
class PasswordStoreInterface;
}  // namespace password_manager

class FaviconLoader;
class PrefService;
class WebStateList;
class GURL;

@class FormSuggestion;

@protocol CredentialSuggestionBottomSheetConsumer;
@protocol CredentialSuggestionBottomSheetPresenter;
@protocol ReauthenticationProtocol;

// This mediator fetches a list suggestions to display in the bottom sheet.
// It also manages filling the form when a suggestion is selected, as well
// as showing the keyboard if requested when the bottom sheet is dismissed.
@interface CredentialSuggestionBottomSheetMediator
    : NSObject <CredentialSuggestionBottomSheetDelegate>

- (instancetype)
      initWithWebStateList:(WebStateList*)webStateList
             faviconLoader:(FaviconLoader*)faviconLoader
               prefService:(PrefService*)prefService
                    params:(const autofill::FormActivityParams&)params
              reauthModule:(id<ReauthenticationProtocol>)reauthModule
                       URL:(const GURL&)URL
      profilePasswordStore:
          (scoped_refptr<password_manager::PasswordStoreInterface>)
              profilePasswordStore
      accountPasswordStore:
          (scoped_refptr<password_manager::PasswordStoreInterface>)
              accountPasswordStore
    sharedURLLoaderFactory:
        (scoped_refptr<network::SharedURLLoaderFactory>)sharedURLLoaderFactory
         engagementTracker:(feature_engagement::Tracker*)engagementTracker
                 presenter:
                     (id<CredentialSuggestionBottomSheetPresenter>)presenter;

// Disconnects the mediator.
- (void)disconnect;

// Whether the mediator has any suggestions for the user.
- (BOOL)hasSuggestions;

// Return the credential associated with the form suggestion. It is an optional,
// in case the credential can't be find.
- (std::optional<password_manager::CredentialUIEntry>)
    getCredentialForFormSuggestion:(FormSuggestion*)formSuggestion;

// The bottom sheet suggestions consumer.
@property(nonatomic, strong) id<CredentialSuggestionBottomSheetConsumer>
    consumer;

// Logs bottom sheet exit reasons, like dismissal or using a credential.
- (void)logExitReason:(PasswordSuggestionBottomSheetExitReason)exitReason;

// Sends the information about which suggestion from the bottom sheet was
// selected by the user, which is expected to fill the relevant fields.
- (void)didSelectSuggestion:(FormSuggestion*)formSuggestion
                    atIndex:(NSInteger)index
                 completion:(ProceduralBlock)completion;

// Handler called to perform operations (e.g. increment the dismiss count) when
// the sheet was dismissed without using any credential action.
- (void)onDismissWithoutAnyCredentialAction;

// Refocuses the login field that was blurred to show this bottom sheet, if
// deemded needed.
- (void)refocus;

// Set vector of credentials that is used for testing.
- (void)setCredentialsForTesting:
    (std::vector<password_manager::CredentialUIEntry>)credentials;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_UI_BUNDLED_BOTTOM_SHEET_CREDENTIAL_SUGGESTION_BOTTOM_SHEET_MEDIATOR_H_
