// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/webauthn/ios/passkey_suggestion_utils.h"

#import "base/base64.h"
#import "base/debug/dump_without_crashing.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/strings/grit/components_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace webauthn {

NSArray<FormSuggestion*>* FormSuggestionsFromPasskeyCredentials(
    const std::vector<password_manager::PasskeyCredential>& passkeys) {
  if (passkeys.empty()) {
    return @[];
  }

  NSMutableArray<FormSuggestion*>* passkey_suggestions =
      [NSMutableArray arrayWithCapacity:passkeys.size()];

  NSString* passkey_label =
      l10n_util::GetNSString(IDS_IOS_PASSKEY_SUGGESTION_LABEL);

  for (const auto& passkey : passkeys) {
    NSString* value;
    NSString* display_description;

    const std::string& display_name = passkey.display_name();
    NSString* username = base::SysUTF8ToNSString(passkey.username());

    if (display_name.empty()) {
      value = username;
      display_description = passkey_label;
    } else {
      value = base::SysUTF8ToNSString(display_name);
      display_description =
          [NSString stringWithFormat:@"%@ • %@", username, passkey_label];
    }

    FormSuggestion* suggestion = [FormSuggestion
        suggestionWithValue:value
         displayDescription:display_description
                       icon:nil
                       type:autofill::SuggestionType::kWebauthnCredential
                    payload:autofill::Suggestion::Guid(
                                base::Base64Encode(passkey.credential_id()))
             requiresReauth:YES];
    [passkey_suggestions addObject:suggestion];
  }

  return passkey_suggestions;
}

std::string GetPasskeySuggestionEncodedCredentialId(
    FormSuggestion* passkey_suggestion) {
  CHECK_EQ(passkey_suggestion.type,
           autofill::SuggestionType::kWebauthnCredential);

  // Get the encoded ID of the passkey credential associated with the
  // `passkey_suggestion`. Fall back to an empty ID if one wasn't added to the
  // suggestion, which will result in deferring the passkey selection to the
  // renderer.
  const std::string encoded_credential_id =
      std::holds_alternative<autofill::Suggestion::Guid>(
          passkey_suggestion.payload)
          ? std::get<autofill::Suggestion::Guid>(passkey_suggestion.payload)
                .value()
          : std::string();

  // An empty `encodedCredentialID` shouldn't cause a crash as there's a
  // deferring mechanism in place, but it is unexpected.
  if (encoded_credential_id.empty()) {
    base::debug::DumpWithoutCrashing();
  }

  return encoded_credential_id;
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

  return
      [passkey_suggestions arrayByAddingObjectsFromArray:password_suggestions];
}

}  // namespace webauthn
