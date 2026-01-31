// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/webauthn/ios/passkey_suggestion_utils.h"

#import "base/base64.h"
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
