// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_CREDENTIAL_SUGGESTION_BOTTOM_SHEET_MEDIATOR_H_
#define IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_CREDENTIAL_SUGGESTION_BOTTOM_SHEET_MEDIATOR_H_

#import <optional>
#import <vector>

#import "base/memory/scoped_refptr.h"
#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/credential_suggestion_bottom_sheet_mediator_base.h"

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

@class FormSuggestion;

@protocol CredentialSuggestionBottomSheetPresenter;
@protocol ReauthenticationProtocol;

// This mediator fetches a list suggestions to display in the bottom sheet.
// It also manages filling the form when a suggestion is selected, as well
// as showing the keyboard if requested when the bottom sheet is dismissed.
@interface CredentialSuggestionBottomSheetMediator
    : CredentialSuggestionBottomSheetMediatorBase

- (instancetype)
      initWithWebStateList:(WebStateList*)webStateList
             faviconLoader:(FaviconLoader*)faviconLoader
               prefService:(PrefService*)prefService
                    params:(const autofill::FormActivityParams&)params
              reauthModule:(id<ReauthenticationProtocol>)reauthModule
      profilePasswordStore:
          (scoped_refptr<password_manager::PasswordStoreInterface>)
              profilePasswordStore
      accountPasswordStore:
          (scoped_refptr<password_manager::PasswordStoreInterface>)
              accountPasswordStore
    sharedURLLoaderFactory:
        (scoped_refptr<network::SharedURLLoaderFactory>)sharedURLLoaderFactory
         engagementTracker:(feature_engagement::Tracker*)engagementTracker
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithWebStateList:(WebStateList*)webStateList NS_UNAVAILABLE;

- (instancetype)init NS_UNAVAILABLE;

// Returns the credential associated with the form suggestion. It is an
// optional, in case the credential can't be found.
- (std::optional<password_manager::CredentialUIEntry>)
    getCredentialForFormSuggestion:(FormSuggestion*)formSuggestion;

// Refocuses the login field that was blurred to show this bottom sheet, if
// deemded needed.
- (void)refocus;

// Set vector of credentials that is used for testing.
- (void)setCredentialsForTesting:
    (std::vector<password_manager::CredentialUIEntry>)credentials;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_CREDENTIAL_SUGGESTION_BOTTOM_SHEET_MEDIATOR_H_
