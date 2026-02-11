// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_IOS_PASSKEY_SUGGESTION_UTILS_H_
#define COMPONENTS_WEBAUTHN_IOS_PASSKEY_SUGGESTION_UTILS_H_

#import <Foundation/Foundation.h>

#import <vector>

#import "components/password_manager/core/browser/passkey_credential.h"

@class FormSuggestion;

namespace webauthn {

// Returns an array of FormSuggestions based on the provided `passkeys`.
NSArray<FormSuggestion*>* FormSuggestionsFromPasskeyCredentials(
    const std::vector<password_manager::PasskeyCredential>& passkeys);

// Returns the encoded credential ID associated with the provided
// `passkey_suggestion`. Returns an empty string if the ID cannot be retrieved.
// This function should only be called on suggestions of type
// autofill::SuggestionType::kWebauthnCredential.
std::string GetPasskeySuggestionEncodedCredentialId(
    FormSuggestion* passkey_suggestion);

// Merges passkey and password suggestions into a single array.
NSArray<FormSuggestion*>* MergePasskeyAndPasswordSuggestions(
    NSArray<FormSuggestion*>* passkey_suggestions,
    NSArray<FormSuggestion*>* password_suggestions);

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_IOS_PASSKEY_SUGGESTION_UTILS_H_
