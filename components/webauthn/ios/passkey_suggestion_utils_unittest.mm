// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/webauthn/ios/passkey_suggestion_utils.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using autofill::SuggestionType;
using PasskeySuggestionUtilsTest = PlatformTest;
using password_manager::PasskeyCredential;

namespace webauthn {

namespace {

constexpr NSString* kPasskeySuggestionValue = @"passkey";
constexpr NSString* kPasswordSuggestionValue = @"password";
constexpr std::string kRpId = "example.com";
constexpr std::string kUsername = "username";

// Returns a generic PasskeyCredential.
PasskeyCredential CreatePasskeyCredential() {
  PasskeyCredential passkey_credential(
      PasskeyCredential::Source::kGooglePasswordManager,
      PasskeyCredential::RpId(kRpId),
      PasskeyCredential::CredentialId({1, 2, 3, 4}),
      PasskeyCredential::UserId(), PasskeyCredential::Username(kUsername));
  return passkey_credential;
}

// Returns a generic FormSuggestion of type `type`.
FormSuggestion* CreateFormSuggestion(NSString* value, SuggestionType type) {
  return [FormSuggestion suggestionWithValue:value
                          displayDescription:nil
                                        icon:nil
                                        type:type
                                     payload:autofill::Suggestion::Payload()
                              requiresReauth:NO];
}

}  // namespace

// Tests that PasskeyCredentials are correctly converted to FormSuggestions.
TEST_F(PasskeySuggestionUtilsTest, FormSuggestionsFromPasskeyCredentials) {
  std::vector<password_manager::PasskeyCredential> passkeys = {
      CreatePasskeyCredential()};

  NSArray<FormSuggestion*>* suggestions =
      FormSuggestionsFromPasskeyCredentials(passkeys);

  ASSERT_EQ(1u, suggestions.count);
  EXPECT_EQ(base::SysUTF8ToNSString(kUsername), suggestions[0].value);
  EXPECT_EQ(base::SysUTF8ToNSString(kRpId), suggestions[0].displayDescription);
}

// Tests that passkey and password suggestion are merged into a single array as
// expected.
TEST_F(PasskeySuggestionUtilsTest, MergePasskeyAndPasswordSuggestions) {
  NSArray<FormSuggestion*>* passkey_suggestions = @[ CreateFormSuggestion(
      kPasskeySuggestionValue, SuggestionType::kWebauthnCredential) ];
  NSArray<FormSuggestion*>* password_suggestions = @[ CreateFormSuggestion(
      kPasswordSuggestionValue, SuggestionType::kPasswordEntry) ];

  NSArray<FormSuggestion*>* merged = MergePasskeyAndPasswordSuggestions(
      passkey_suggestions, password_suggestions);

  ASSERT_EQ(2u, merged.count);
  EXPECT_EQ(kPasskeySuggestionValue, merged[0].value);
  EXPECT_EQ(kPasswordSuggestionValue, merged[1].value);
}

}  // namespace webauthn
