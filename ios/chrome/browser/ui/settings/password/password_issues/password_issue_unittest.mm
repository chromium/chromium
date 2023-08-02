// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_issues/password_issue.h"

#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using PasswordIssueTest = PlatformTest;

using password_manager::CredentialUIEntry;
using password_manager::InsecureType;
using password_manager::InsecurityMetadata;
using password_manager::PasswordForm;

namespace {

// Creates a PasswordIssue for testing with the provided insecurity issues.
// Pass `enable_compromised_description` as true if the compromised description
// property should have a value for compromised credentials.
PasswordIssue* MakeTestPasswordIssue(
    const std::vector<InsecureType>& insecure_types,
    bool enable_compromised_description) {
  PasswordForm form;
  form.signon_realm = "https://example.com";
  form.username_value = u"user";
  form.password_value = u"password";
  form.url = GURL(form.signon_realm);
  form.password_issues = base::flat_map<InsecureType, InsecurityMetadata>();

  for (auto insecure_type : insecure_types) {
    form.password_issues[insecure_type] = InsecurityMetadata();
  }

  CredentialUIEntry entry = CredentialUIEntry(form);

  return
      [[PasswordIssue alloc] initWithCredential:entry
                   enableCompromisedDescription:enable_compromised_description];
}

}  // namespace

// Verifies the compromised description property for leaked passwords.
TEST_F(PasswordIssueTest, TestLeakedCompromisedDescription) {
  PasswordIssue* issue = MakeTestPasswordIssue({InsecureType::kLeaked}, true);

  EXPECT_NSEQ(issue.compromisedDescription, @"Found in data breach");
}

// Verifies the compromised description property for phished passwords.
TEST_F(PasswordIssueTest, TestPhishedCompromisedDescription) {
  PasswordIssue* issue = MakeTestPasswordIssue({InsecureType::kPhished}, true);

  EXPECT_NSEQ(issue.compromisedDescription, @"Entered on deceptive site");
}

// Verifies the compromised description property for passwords that were leaked
// and phished.
TEST_F(PasswordIssueTest, TestLeakedAndPhishedCompromisedDescription) {
  PasswordIssue* issue = MakeTestPasswordIssue(
      {InsecureType::kPhished, InsecureType::kLeaked}, true);

  EXPECT_NSEQ(issue.compromisedDescription,
              @"Entered on a deceptive site and found in a data breach");
}

// Verifies the compromised description property doesn't have value when
// disabled.
TEST_F(PasswordIssueTest, TestDisabledCompromisedDescription) {
  PasswordIssue* issue = MakeTestPasswordIssue(
      {InsecureType::kPhished, InsecureType::kLeaked}, false);

  EXPECT_FALSE(issue.compromisedDescription);
}

// Verifies the compromised description property doesn't have value for non
// compromised passwords.
TEST_F(PasswordIssueTest,
       TestCompromisedDescriptionForNonCompromisedPasswords) {
  PasswordIssue* password_without_issues = MakeTestPasswordIssue({}, true);

  EXPECT_FALSE(password_without_issues.compromisedDescription);

  PasswordIssue* password_with_other_insecurity_types =
      MakeTestPasswordIssue({InsecureType::kWeak, InsecureType::kReused}, true);

  EXPECT_FALSE(password_with_other_insecurity_types.compromisedDescription);
}
