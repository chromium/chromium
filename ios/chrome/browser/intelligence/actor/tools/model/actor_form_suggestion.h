// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_FORM_SUGGESTION_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_FORM_SUGGESTION_H_

#import <Foundation/Foundation.h>

#import <optional>

#import "components/password_manager/core/browser/actor_login/actor_login_types.h"

namespace autofill {
enum class ActorFormFillingRequestedData;
enum class SuggestionType;
struct ActorSuggestion;
}  // namespace autofill
@class FormSuggestion;

// An Objective-C class used to convert actor types to FormSuggestion objects.
@interface ActorFormSuggestion : NSObject

// Designated initializer taking a credential.
- (instancetype)initWithCredential:(const actor_login::Credential&)credential;

// Designated initializer taking an autofill suggestion.
- (instancetype)
    initWithActorSuggestion:(const autofill::ActorSuggestion&)suggestion
                   dataType:(autofill::ActorFormFillingRequestedData)dataType;

- (instancetype)init NS_UNAVAILABLE;

// The corresponding form suggestion.
@property(nonatomic, readonly) FormSuggestion* formSuggestion;

// The suggestion type.
@property(assign, readonly, nonatomic) autofill::SuggestionType type;

// The credential. Will be empty if the suggestion type is not
// `kPasswordEntry` or `kIdentityCredential`.
@property(nonatomic, readonly) std::optional<actor_login::Credential>
    credential;

// The autofill suggestion. Will be empty if the suggestion type `kAddressEntry`
// or `kCreditCard`.
@property(nonatomic, readonly) std::optional<autofill::ActorSuggestion>
    autofillSuggestion;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_FORM_SUGGESTION_H_
