// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/gaia_auth_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace gaia {

TEST(GaiaAuthUtilTest, EmailAddressNoOp) {
  const char lower_case[] = "user@what.com";
  EXPECT_EQ(lower_case, CanonicalizeEmail(lower_case));
}

TEST(GaiaAuthUtilTest, InvalidEmailAddress) {
  const char invalid_email1[] = "user";
  const char invalid_email2[] = "user@@what.com";
  EXPECT_EQ(invalid_email1, CanonicalizeEmail(invalid_email1));
  EXPECT_EQ(invalid_email2, CanonicalizeEmail(invalid_email2));
  EXPECT_EQ("user", CanonicalizeEmail("USER"));
}

TEST(GaiaAuthUtilTest, EmailAddressIgnoreCaps) {
  EXPECT_EQ(CanonicalizeEmail("user@what.com"),
            CanonicalizeEmail("UsEr@what.com"));
}

TEST(GaiaAuthUtilTest, EmailAddressIgnoreDomainCaps) {
  EXPECT_EQ(CanonicalizeEmail("user@what.com"),
            CanonicalizeEmail("UsEr@what.COM"));
}

TEST(GaiaAuthUtilTest, EmailAddressRejectOneUsernameDot) {
  EXPECT_NE(CanonicalizeEmail("u.ser@what.com"),
            CanonicalizeEmail("UsEr@what.com"));
}

TEST(GaiaAuthUtilTest, EmailAddressMatchWithOneUsernameDot) {
  EXPECT_EQ(CanonicalizeEmail("u.ser@what.com"),
            CanonicalizeEmail("U.sEr@what.com"));
}

TEST(GaiaAuthUtilTest, EmailAddressIgnoreOneUsernameDot) {
  EXPECT_EQ(CanonicalizeEmail("us.er@gmail.com"),
            CanonicalizeEmail("UsEr@gmail.com"));
}

TEST(GaiaAuthUtilTest, EmailAddressIgnoreOneUsernameDotAndIgnoreCaps) {
  EXPECT_EQ(CanonicalizeEmail("user@gmail.com"),
            CanonicalizeEmail("US.ER@GMAIL.COM"));
}

TEST(GaiaAuthUtilTest, EmailAddressIgnoreManyUsernameDots) {
  EXPECT_EQ(CanonicalizeEmail("u.ser@gmail.com"),
            CanonicalizeEmail("Us.E.r@gmail.com"));
}

TEST(GaiaAuthUtilTest, EmailAddressIgnoreConsecutiveUsernameDots) {
  EXPECT_EQ(CanonicalizeEmail("use.r@gmail.com"),
            CanonicalizeEmail("Us....E.r@gmail.com"));
}

TEST(GaiaAuthUtilTest, EmailAddressDifferentOnesRejected) {
  EXPECT_NE(CanonicalizeEmail("who@what.com"),
            CanonicalizeEmail("Us....E.r@what.com"));
}

TEST(GaiaAuthUtilTest, GooglemailNotCanonicalizedToGmail) {
  const char googlemail[] = "user@googlemail.com";
  EXPECT_EQ(googlemail, CanonicalizeEmail(googlemail));
}

TEST(GaiaAuthUtilTest, CanonicalizeDomain) {
  const char domain[] = "example.com";
  EXPECT_EQ(domain, CanonicalizeDomain("example.com"));
  EXPECT_EQ(domain, CanonicalizeDomain("EXAMPLE.cOm"));
}

TEST(GaiaAuthUtilTest, ExtractDomainName) {
  const char domain[] = "example.com";
  EXPECT_EQ(domain, ExtractDomainName("who@example.com"));
  EXPECT_EQ(domain, ExtractDomainName("who@EXAMPLE.cOm"));
}

TEST(GaiaAuthUtilTest, IsGoogleInternalAccountEmail) {
  EXPECT_TRUE(IsGoogleInternalAccountEmail("hello@google.com"));
  EXPECT_FALSE(IsGoogleInternalAccountEmail("internal@gmail.com"));
  EXPECT_FALSE(IsGoogleInternalAccountEmail(" "));
}

TEST(GaiaAuthUtilTest, SanitizeMissingDomain) {
  EXPECT_EQ("nodomain@gmail.com", SanitizeEmail("nodomain"));
}

TEST(GaiaAuthUtilTest, SanitizeExistingDomain) {
  const char existing[] = "test@example.com";
  EXPECT_EQ(existing, SanitizeEmail(existing));
}

TEST(GaiaAuthUtilTest, AreEmailsSame) {
  EXPECT_TRUE(AreEmailsSame("foo", "foo"));
  EXPECT_TRUE(AreEmailsSame("foo", "foo@gmail.com"));
  EXPECT_TRUE(AreEmailsSame("foo@gmail.com", "Foo@Gmail.com"));
  EXPECT_FALSE(AreEmailsSame("foo@gmail.com", "foo@othermail.com"));
  EXPECT_FALSE(AreEmailsSame("user@gmail.com", "foo@gmail.com"));
}

TEST(GaiaAuthUtilTest, GmailAndGooglemailAreSame) {
  EXPECT_TRUE(AreEmailsSame("foo@gmail.com", "foo@googlemail.com"));
  EXPECT_FALSE(AreEmailsSame("bar@gmail.com", "foo@googlemail.com"));
}

TEST(GaiaAuthUtilTest, IsGaiaSignonRealm) {
  // Only https versions of Gaia URLs should be considered valid.
  EXPECT_TRUE(IsGaiaSignonRealm(GURL("https://accounts.google.com/")));
  EXPECT_FALSE(IsGaiaSignonRealm(GURL("http://accounts.google.com/")));

  // Other Google URLs are not valid.
  EXPECT_FALSE(IsGaiaSignonRealm(GURL("https://www.google.com/")));
  EXPECT_FALSE(IsGaiaSignonRealm(GURL("http://www.google.com/")));
  EXPECT_FALSE(IsGaiaSignonRealm(GURL("https://google.com/")));
  EXPECT_FALSE(IsGaiaSignonRealm(GURL("https://mail.google.com/")));

  // Other https URLs are not valid.
  EXPECT_FALSE(IsGaiaSignonRealm(GURL("https://www.example.com/")));
}

TEST(GaiaAuthUtilTest, ParseListAccountsData) {
  std::vector<ListedAccount> accounts;
  std::vector<ListedAccount> signed_out_accounts;
  ASSERT_FALSE(ParseListAccountsData("", &accounts, &signed_out_accounts));
  ASSERT_EQ(0u, accounts.size());
  ASSERT_EQ(0u, signed_out_accounts.size());

  ASSERT_FALSE(ParseListAccountsData("1", &accounts, &signed_out_accounts));
  ASSERT_EQ(0u, accounts.size());
  ASSERT_EQ(0u, signed_out_accounts.size());

  ASSERT_FALSE(ParseListAccountsData("[]", &accounts, &signed_out_accounts));
  ASSERT_EQ(0u, accounts.size());
  ASSERT_EQ(0u, signed_out_accounts.size());

  ASSERT_FALSE(ParseListAccountsData(
      "[\"foo\", \"bar\"]", &accounts, &signed_out_accounts));
  ASSERT_EQ(0u, accounts.size());
  ASSERT_EQ(0u, signed_out_accounts.size());

  ASSERT_TRUE(ParseListAccountsData(
      "[\"foo\", []]", &accounts, &signed_out_accounts));
  ASSERT_EQ(0u, accounts.size());
  ASSERT_EQ(0u, signed_out_accounts.size());

  ASSERT_TRUE(ParseListAccountsData(
      "[\"foo\", [[\"bar\", 0, \"name\", 0, \"photo\", 0, 0, 0]]]",
      &accounts,
      &signed_out_accounts));
  ASSERT_EQ(0u, accounts.size());
  ASSERT_EQ(0u, signed_out_accounts.size());

  ASSERT_TRUE(ParseListAccountsData(
      "[\"foo\", "
          "[[\"bar\", 0, \"name\", \"u@g.c\", \"p\", 0, 0, 0, 0, 1, \"45\"]]]",
      &accounts,
      &signed_out_accounts));
  ASSERT_EQ(1u, accounts.size());
  ASSERT_EQ("u@g.c", accounts[0].email);
  ASSERT_TRUE(accounts[0].valid);
  ASSERT_EQ(0u, signed_out_accounts.size());

  ASSERT_TRUE(ParseListAccountsData(
      "[\"foo\", "
          "[[\"bar1\",0,\"name1\",\"u1@g.c\",\"photo1\",0,0,0,0,1,\"45\"], "
          "[\"bar2\",0,\"name2\",\"u2@g.c\",\"photo2\",0,0,0,0,1,\"6\"]]]",
      &accounts,
      &signed_out_accounts));
  ASSERT_EQ(2u, accounts.size());
  ASSERT_EQ("u1@g.c", accounts[0].email);
  ASSERT_TRUE(accounts[0].valid);
  ASSERT_EQ("u2@g.c", accounts[1].email);
  ASSERT_TRUE(accounts[1].valid);
  ASSERT_EQ(0u, signed_out_accounts.size());

  ASSERT_TRUE(ParseListAccountsData(
      "[\"foo\", "
          "[[\"b1\", 0,\"name1\",\"U1@g.c\",\"photo1\",0,0,0,0,1,\"45\"], "
          "[\"b2\",0,\"name2\",\"u.2@g.c\",\"photo2\",0,0,0,0,1,\"46\"]]]",
      &accounts,
      &signed_out_accounts));
  ASSERT_EQ(2u, accounts.size());
  ASSERT_EQ(CanonicalizeEmail("U1@g.c"), accounts[0].email);
  ASSERT_TRUE(accounts[0].valid);
  ASSERT_EQ(CanonicalizeEmail("u.2@g.c"), accounts[1].email);
  ASSERT_TRUE(accounts[1].valid);
  ASSERT_EQ(0u, signed_out_accounts.size());
}

TEST(GaiaAuthUtilTest, ParseListAccountsDataValidSession) {
  std::vector<ListedAccount> accounts;
  std::vector<ListedAccount> signed_out_accounts;

  // Valid session is true means: return account.
  ASSERT_TRUE(ParseListAccountsData(
      "[\"foo\", [[\"b\",0,\"n\",\"u@g.c\",\"photo\",0,0,0,0,1,\"45\"]]]",
      &accounts,
      &signed_out_accounts));
  ASSERT_EQ(1u, accounts.size());
  ASSERT_EQ("u@g.c", accounts[0].email);
  ASSERT_TRUE(accounts[0].valid);
  ASSERT_EQ(0u, signed_out_accounts.size());

  // Valid session is false means: return account with valid bit false.
  ASSERT_TRUE(ParseListAccountsData(
      "[\"foo\", [[\"b\",0,\"n\",\"u@g.c\",\"photo\",0,0,0,0,0,\"45\"]]]",
      &accounts,
      &signed_out_accounts));
  ASSERT_EQ(1u, accounts.size());
  ASSERT_FALSE(accounts[0].valid);
  ASSERT_EQ(0u, signed_out_accounts.size());
}

TEST(GaiaAuthUtilTest, ParseListAccountsDataGaiaId) {
  std::vector<ListedAccount> accounts;
  std::vector<ListedAccount> signed_out_accounts;

  // Missing gaia id means: do not return account.
  ASSERT_TRUE(ParseListAccountsData(
      "[\"foo\", [[\"b\", 0, \"n\", \"u@g.c\", \"photo\", 0, 0, 0, 0, 1]]]",
      &accounts,
      &signed_out_accounts));
  ASSERT_EQ(0u, accounts.size());
  ASSERT_EQ(0u, signed_out_accounts.size());

  // Valid gaia session means: return gaia session
  ASSERT_TRUE(ParseListAccountsData(
      "[\"foo\", "
          "[[\"b\",0,\"n\",\"u@g.c\",\"photo\",0,0,0,0,1,\"9863\"]]]",
      &accounts,
      &signed_out_accounts));
  ASSERT_EQ(1u, accounts.size());
  ASSERT_EQ("u@g.c", accounts[0].email);
  ASSERT_TRUE(accounts[0].valid);
  ASSERT_EQ("9863", accounts[0].gaia_id);
  ASSERT_EQ(0u, signed_out_accounts.size());
}

TEST(GaiaAuthUtilTest, ParseListAccountsWithSignedOutAccounts) {
  std::vector<ListedAccount> accounts;
  std::vector<ListedAccount> signed_out_accounts;

  ASSERT_TRUE(ParseListAccountsData(
      "[\"foo\","
          "[[\"b\",0,\"n\",\"u@g.c\",\"photo\",0,0,0,0,1,\"45\"],"
          "[\"c\",0,\"n\",\"u.2@g.c\",\"photo\",0,0,0,0,1,\"45\","
             "null,null,null,1]]]",
      &accounts,
      &signed_out_accounts));
  ASSERT_EQ(1u, accounts.size());
  ASSERT_EQ("u@g.c", accounts[0].email);
  ASSERT_FALSE(accounts[0].signed_out);
  ASSERT_EQ(1u, signed_out_accounts.size());
  ASSERT_EQ("u.2@g.c", signed_out_accounts[0].email);
  ASSERT_TRUE(signed_out_accounts[0].signed_out);
}

TEST(GaiaAuthUtilTest, ParseListAccountsVerifiedAccounts) {
  std::vector<ListedAccount> accounts;
  std::vector<ListedAccount> signed_out_accounts;

  ASSERT_TRUE(ParseListAccountsData(
      "[\"foo\","
      "[[\"a\",0,\"n\",\"a@g.c\",\"photo\",0,0,0,0,1,\"45\"],"
      "[\"b\",0,\"n\",\"b@g.c\",\"photo\",0,0,0,0,1,\"45\","
      "null,null,null,null,0],"
      "[\"c\",0,\"n\",\"c@g.c\",\"photo\",0,0,0,0,1,\"45\","
      "null,null,null,null,1]]]",
      &accounts, &signed_out_accounts));

  ASSERT_EQ(3u, accounts.size());
  ASSERT_EQ("a@g.c", accounts[0].email);
  EXPECT_TRUE(accounts[0].verified);  // Accounts are verified by default.
  ASSERT_EQ("b@g.c", accounts[1].email);
  EXPECT_FALSE(accounts[1].verified);
  ASSERT_EQ("c@g.c", accounts[2].email);
  EXPECT_TRUE(accounts[2].verified);
  ASSERT_EQ(0u, signed_out_accounts.size());
}

TEST(GaiaAuthUtilTest, ParseListAccountsAcceptsNull) {
  ASSERT_TRUE(ParseListAccountsData(
      "[\"foo\","
          "[[\"b\",0,\"n\",\"u@g.c\",\"photo\",0,0,0,0,1,\"45\"],"
          "[\"c\",0,\"n\",\"u.2@g.c\",\"photo\",0,0,0,0,1,\"45\","
             "null,null,null,1]]]",
      nullptr,
      nullptr));

  std::vector<ListedAccount> accounts;
  ASSERT_TRUE(ParseListAccountsData(
      "[\"foo\","
          "[[\"b\",0,\"n\",\"u@g.c\",\"photo\",0,0,0,0,1,\"45\"],"
          "[\"c\",0,\"n\",\"u.2@g.c\",\"photo\",0,0,0,0,1,\"45\","
             "null,null,null,1]]]",
      &accounts,
      nullptr));
  ASSERT_EQ(1u, accounts.size());
  ASSERT_EQ("u@g.c", accounts[0].email);
  ASSERT_FALSE(accounts[0].signed_out);

  std::vector<ListedAccount> signed_out_accounts;
  ASSERT_TRUE(ParseListAccountsData(
      "[\"foo\","
          "[[\"b\",0,\"n\",\"u@g.c\",\"photo\",0,0,0,0,1,\"45\"],"
          "[\"c\",0,\"n\",\"u.2@g.c\",\"photo\",0,0,0,0,1,\"45\","
             "null,null,null,1]]]",
      nullptr,
      &signed_out_accounts));
  ASSERT_EQ(1u, signed_out_accounts.size());
  ASSERT_EQ("u.2@g.c", signed_out_accounts[0].email);
  ASSERT_TRUE(signed_out_accounts[0].signed_out);
}

}  // namespace gaia
