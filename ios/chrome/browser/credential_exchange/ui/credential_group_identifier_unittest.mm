// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_exchange/ui/credential_group_identifier.h"

#import "components/affiliations/core/browser/affiliation_utils.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/ui/affiliated_group.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

using password_manager::AffiliatedGroup;
using password_manager::CredentialUIEntry;

namespace {

password_manager::PasswordForm CreatePasswordForm(
    const std::string& url_spec,
    const std::string& signon_realm,
    const std::u16string& username) {
  password_manager::PasswordForm form;
  form.url = GURL(url_spec);
  form.signon_realm = signon_realm;
  form.username_value = username;
  form.password_value = u"password";
  return form;
}

CredentialUIEntry CreateEntry(const std::string& url_spec,
                              const std::string& signon_realm,
                              const std::u16string& username) {
  return CredentialUIEntry(
      CreatePasswordForm(url_spec, signon_realm, username));
}

}  // namespace

using CredentialGroupIdentifierTest = PlatformTest;

// Tests that two groups with identical URLs but different sign-on realms
// are strictly evaluated as not equal, and have different hashes.
TEST_F(CredentialGroupIdentifierTest, DifferentSignonRealmsNotEqual) {
  // Both entries have exactly the same base URL and username.
  CredentialUIEntry entry1 =
      CreateEntry("https://example.com", "http://example.com", u"user");
  CredentialUIEntry entry2 =
      CreateEntry("https://example.com", "https://example.com", u"user");

  affiliations::FacetBrandingInfo branding;

  AffiliatedGroup group1({entry1}, branding);
  AffiliatedGroup group2({entry2}, branding);

  CredentialGroupIdentifier* id1 =
      [[CredentialGroupIdentifier alloc] initWithGroup:group1];
  CredentialGroupIdentifier* id2 =
      [[CredentialGroupIdentifier alloc] initWithGroup:group2];

  EXPECT_FALSE([id1 isEqual:id2]);
  EXPECT_NE([id1 hash], [id2 hash]);
}

// Tests that two groups with the identical entries evaluate as equal.
TEST_F(CredentialGroupIdentifierTest, IdenticalGroupsEqual) {
  CredentialUIEntry entry =
      CreateEntry("https://example.com", "https://example.com", u"user");
  affiliations::FacetBrandingInfo branding;

  AffiliatedGroup group1({entry}, branding);
  AffiliatedGroup group2({entry}, branding);

  CredentialGroupIdentifier* id1 =
      [[CredentialGroupIdentifier alloc] initWithGroup:group1];
  CredentialGroupIdentifier* id2 =
      [[CredentialGroupIdentifier alloc] initWithGroup:group2];

  EXPECT_TRUE([id1 isEqual:id2]);
  EXPECT_EQ([id1 hash], [id2 hash]);
}

// Tests that two empty identifiers are equal to each other, but not to
// populated identifiers.
TEST_F(CredentialGroupIdentifierTest, EmptyIdentifiers) {
  AffiliatedGroup empty_group;
  CredentialGroupIdentifier* empty1 =
      [[CredentialGroupIdentifier alloc] initWithGroup:empty_group];
  CredentialGroupIdentifier* empty2 =
      [[CredentialGroupIdentifier alloc] initWithGroup:empty_group];

  EXPECT_TRUE([empty1 isEqual:empty2]);
  EXPECT_EQ([empty1 hash], [empty2 hash]);

  CredentialUIEntry entry =
      CreateEntry("https://example.com", "https://example.com", u"user");
  affiliations::FacetBrandingInfo branding;
  AffiliatedGroup group({entry}, branding);
  CredentialGroupIdentifier* populated =
      [[CredentialGroupIdentifier alloc] initWithGroup:group];

  EXPECT_FALSE([empty1 isEqual:populated]);
  EXPECT_NE([empty1 hash], [populated hash]);
}

// Tests that two groups evaluating to equal maintain the same hash, even if the
// distribution of duplicate realms among their credentials differs.
TEST_F(CredentialGroupIdentifierTest,
       EqualGroupsWithDuplicateRealmsHaveSameHash) {
  // entry_a and entry_b are identical except for the signon_realm.
  // Because CredentialUIEntry operator== ignores the signon_realm, they will
  // evaluate as equal when compared by AffiliatedGroup::operator==.
  CredentialUIEntry entry_a =
      CreateEntry("https://example.com", "https://example.com", u"user");
  CredentialUIEntry entry_b =
      CreateEntry("https://example.com", "http://example.com", u"user");

  affiliations::FacetBrandingInfo branding;

  AffiliatedGroup group1({entry_a, entry_a, entry_b}, branding);
  AffiliatedGroup group2({entry_a, entry_b, entry_b}, branding);

  CredentialGroupIdentifier* id1 =
      [[CredentialGroupIdentifier alloc] initWithGroup:group1];
  CredentialGroupIdentifier* id2 =
      [[CredentialGroupIdentifier alloc] initWithGroup:group2];

  // We must guarantee their hashes match despite the different duplicate
  // counts.
  EXPECT_TRUE([id1 isEqual:id2]);
  EXPECT_EQ([id1 hash], [id2 hash]);
}
