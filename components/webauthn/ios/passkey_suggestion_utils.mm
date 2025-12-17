// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/webauthn/ios/passkey_suggestion_utils.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/ios/browser/form_suggestion.h"

namespace webauthn {

NSArray<FormSuggestion*>* FormSuggestionsFromPasskeyCredentials(
    const std::vector<password_manager::PasskeyCredential>& passkeys) {
  NSMutableArray<FormSuggestion*>* passkey_suggestions = [NSMutableArray array];

  for (const auto& passkey : passkeys) {
    // TODO(crbug.com/463429359): Set right value and display description
    // depending on scenario.
    FormSuggestion* suggestion = [FormSuggestion
        suggestionWithValue:base::SysUTF8ToNSString(passkey.username())
         displayDescription:base::SysUTF8ToNSString(passkey.rp_id())
                       icon:nil
                       type:autofill::SuggestionType::kWebauthnCredential
                    payload:autofill::Suggestion::Payload()
             requiresReauth:YES];
    [passkey_suggestions addObject:suggestion];
  }

  return passkey_suggestions;
}

NSArray<FormSuggestion*>* MergePasskeyAndPasswordSuggestions(
    NSArray<FormSuggestion*>* passkey_suggestions,
    NSArray<FormSuggestion*>* password_suggestions) {
  if (!passkey_suggestions.count) {
    return password_suggestions;
  }

  if (!password_suggestions.count) {
    return passkey_suggestions;
  }

  // TODO(crbug.com/463429359): Implement right merge logic.
  return
      [passkey_suggestions arrayByAddingObjectsFromArray:password_suggestions];
}

}  // namespace webauthn
