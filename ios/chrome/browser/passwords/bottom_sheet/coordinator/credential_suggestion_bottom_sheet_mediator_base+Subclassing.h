// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_CREDENTIAL_SUGGESTION_BOTTOM_SHEET_MEDIATOR_BASE_SUBCLASSING_H_
#define IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_CREDENTIAL_SUGGESTION_BOTTOM_SHEET_MEDIATOR_BASE_SUBCLASSING_H_

#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/credential_suggestion_bottom_sheet_mediator_base.h"

@class FormSuggestion;

namespace webauthn {
class IOSWebAuthnCredentialsDelegate;
}

@interface CredentialSuggestionBottomSheetMediatorBase (Subclassing)

// Origin to fetch credentials for.
@property(nonatomic, assign) GURL URL;

// Domain of the URL to fetch credentials for.
@property(nonatomic, readonly) NSString* domain;

// List of suggestions to be shown in the bottom sheet.
@property(nonatomic, strong) NSArray<FormSuggestion*>* suggestions;

// The WebStateList observed by this mediator.
@property(nonatomic, readonly) WebStateList* webStateList;

// Performs the actual suggestion selection after reauthentication (if required)
// has succeeded.
- (void)selectSuggestion:(FormSuggestion*)suggestion
                 atIndex:(NSInteger)index
              completion:(ProceduralBlock)completion;

// Delegate used to fetch and select passkey suggestions.
@property(nonatomic, assign)
    base::WeakPtr<webauthn::IOSWebAuthnCredentialsDelegate>
        webAuthnCredentialsDelegate;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_CREDENTIAL_SUGGESTION_BOTTOM_SHEET_MEDIATOR_BASE_SUBCLASSING_H_
