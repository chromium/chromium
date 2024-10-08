// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/gaia_auth_util.h"

#include "base/base64url.h"
#include "google_apis/gaia/gaia_auth_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace gaia {

namespace {

const char kGaiaId[] = "fake_gaia_id";

ListedAccount ValidListedAccount(const std::string& raw_email,
                                 const std::string& gaia_id) {
  gaia::ListedAccount result;
  result.email = CanonicalizeEmail(raw_email);
  result.gaia_id = gaia_id;
  result.raw_email = raw_email;
  return result;
}

ListedAccount InvalidListedAccount(const std::string& raw_email,
                                   const std::string& gaia_id) {
  gaia::ListedAccount result = ValidListedAccount(raw_email, gaia_id);
  result.valid = false;
  return result;
}

ListedAccount SignedOutListedAccount(const std::string& raw_email,
                                     const std::string& gaia_id) {
  gaia::ListedAccount result = ValidListedAccount(raw_email, gaia_id);
  result.signed_out = true;
  return result;
}

ListedAccount NonverifiedListedAccount(const std::string& raw_email,
                                       const std::string& gaia_id) {
  gaia::ListedAccount result = ValidListedAccount(raw_email, gaia_id);
  result.verified = false;
  return result;
}

}  // namespace

using ::testing::ElementsAre;

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

TEST(GaiaAuthUtilTest, IsGoogleRobotAccountEmail) {
  EXPECT_FALSE(IsGoogleRobotAccountEmail(""));
  EXPECT_FALSE(IsGoogleRobotAccountEmail("foo"));
  EXPECT_FALSE(IsGoogleRobotAccountEmail("1234567890"));
  EXPECT_FALSE(IsGoogleRobotAccountEmail("foo@gmail.com"));
  EXPECT_FALSE(IsGoogleRobotAccountEmail("system.gserviceaccount.com"));
  EXPECT_TRUE(IsGoogleRobotAccountEmail("foo@system.gserviceaccount.com"));
  EXPECT_TRUE(IsGoogleRobotAccountEmail("foo@system.googleusercontent.com"));
  EXPECT_TRUE(IsGoogleRobotAccountEmail("foo@System.Gserviceaccount.com"));
  EXPECT_TRUE(IsGoogleRobotAccountEmail("foo@System.Googleusercontent.com"));
}

TEST(GaiaAuthUtilTest, GmailAndGooglemailAreSame) {
  EXPECT_TRUE(AreEmailsSame("foo@gmail.com", "foo@googlemail.com"));
  EXPECT_FALSE(AreEmailsSame("bar@gmail.com", "foo@googlemail.com"));
}

TEST(GaiaAuthUtilTest, HasGaiaSchemeHostPort) {
  EXPECT_TRUE(HasGaiaSchemeHostPort(GURL("https://accounts.google.com/")));

  // Paths and queries should be ignored.
  EXPECT_TRUE(HasGaiaSchemeHostPort(GURL("https://accounts.google.com/foo")));
  EXPECT_TRUE(
      HasGaiaSchemeHostPort(GURL("https://accounts.google.com/foo?bar=1#baz")));

  // Scheme mismatch should lead to false.
  EXPECT_FALSE(HasGaiaSchemeHostPort(GURL("http://accounts.google.com/")));

  // Port mismatch should lead to false.
  EXPECT_FALSE(HasGaiaSchemeHostPort(GURL("https://accounts.google.com:123/")));

  // Host mismatch should lead to false, including Google URLs.
  EXPECT_FALSE(HasGaiaSchemeHostPort(GURL("https://example.com/")));
  EXPECT_FALSE(HasGaiaSchemeHostPort(GURL("https://www.example.com/")));
  EXPECT_FALSE(HasGaiaSchemeHostPort(GURL("https://www.google.com/")));
  EXPECT_FALSE(HasGaiaSchemeHostPort(GURL("https://google.com/")));
  EXPECT_FALSE(HasGaiaSchemeHostPort(GURL("https://mail.google.com/")));

  // about: scheme.
  EXPECT_FALSE(HasGaiaSchemeHostPort(GURL("about:blank")));
  EXPECT_FALSE(HasGaiaSchemeHostPort(GURL("about:srcdoc")));

  // blob: scheme.
  EXPECT_FALSE(HasGaiaSchemeHostPort(
      GURL("blob:https://accounts.google.com/mocked-blob-guid")));

  // Invalid/empty URL.
  EXPECT_FALSE(HasGaiaSchemeHostPort(GURL()));
}

TEST(GaiaAuthUtilTest, ParseListAccountsData) {
  std::vector<ListedAccount> accounts;
  ASSERT_FALSE(ParseListAccountsData("", &accounts));
  ASSERT_EQ(0u, accounts.size());

  ASSERT_FALSE(ParseListAccountsData("1", &accounts));
  ASSERT_EQ(0u, accounts.size());

  ASSERT_FALSE(ParseListAccountsData("[]", &accounts));
  ASSERT_EQ(0u, accounts.size());

  ASSERT_FALSE(ParseListAccountsData("[\"foo\", \"bar\"]", &accounts));
  ASSERT_EQ(0u, accounts.size());

  ASSERT_TRUE(ParseListAccountsData("[\"foo\", []]", &accounts));
  ASSERT_EQ(0u, accounts.size());

  ASSERT_TRUE(ParseListAccountsData(
      "[\"foo\", [[\"bar\", 0, \"name\", 0, \"photo\", 0, 0, 0]]]", &accounts));
  ASSERT_EQ(0u, accounts.size());

  ASSERT_TRUE(ParseListAccountsData(
      "[\"foo\", "
      "[[\"bar\", 0, \"name\", \"u@g.c\", \"p\", 0, 0, 0, 0, 1, \"45\"]]]",
      &accounts));
  ASSERT_THAT(accounts, ElementsAre(ValidListedAccount("u@g.c", "45")));

  ASSERT_TRUE(ParseListAccountsData(
      "[\"foo\", "
      "[[\"bar1\",0,\"name1\",\"u1@g.c\",\"photo1\",0,0,0,0,1,\"45\"], "
      "[\"bar2\",0,\"name2\",\"u2@g.c\",\"photo2\",0,0,0,0,1,\"6\"]]]",
      &accounts));
  ASSERT_THAT(accounts, ElementsAre(ValidListedAccount("u1@g.c", "45"),
                                    ValidListedAccount("u2@g.c", "6")));

  ASSERT_TRUE(ParseListAccountsData(
      "[\"foo\", "
      "[[\"b1\", 0,\"name1\",\"U1@g.c\",\"photo1\",0,0,0,0,1,\"45\"], "
      "[\"b2\",0,\"name2\",\"u.2@g.c\",\"photo2\",0,0,0,0,1,\"46\"]]]",
      &accounts));
  ASSERT_THAT(accounts, ElementsAre(ValidListedAccount("U1@g.c", "45"),
                                    ValidListedAccount("u.2@g.c", "46")));
}

TEST(GaiaAuthUtilTest, ParseListAccountsDataValidSession) {
  std::vector<ListedAccount> accounts;

  // Valid session is true means: return account.
  ASSERT_TRUE(ParseListAccountsData(
      "[\"foo\", [[\"b\",0,\"n\",\"u@g.c\",\"photo\",0,0,0,0,1,\"45\"]]]",
      &accounts));
  ASSERT_THAT(accounts, ElementsAre(ValidListedAccount("u@g.c", "45")));

  // Valid session is false means: return account with valid bit false.
  ASSERT_TRUE(ParseListAccountsData(
      "[\"foo\", [[\"b\",0,\"n\",\"u@g.c\",\"photo\",0,0,0,0,0,\"45\"]]]",
      &accounts));
  ASSERT_THAT(accounts, ElementsAre(InvalidListedAccount("u@g.c", "45")));
}

TEST(GaiaAuthUtilTest, ParseListAccountsDataGaiaId) {
  std::vector<ListedAccount> accounts;

  // Missing gaia id means: do not return account.
  ASSERT_TRUE(ParseListAccountsData(
      "[\"foo\", [[\"b\", 0, \"n\", \"u@g.c\", \"photo\", 0, 0, 0, 0, 1]]]",
      &accounts));
  ASSERT_EQ(0u, accounts.size());

  // Valid gaia session means: return gaia session
  ASSERT_TRUE(ParseListAccountsData(
      "[\"foo\", "
      "[[\"b\",0,\"n\",\"u@g.c\",\"photo\",0,0,0,0,1,\"9863\"]]]",
      &accounts));
  ASSERT_THAT(accounts, ElementsAre(ValidListedAccount("u@g.c", "9863")));
}

TEST(GaiaAuthUtilTest, ParseListAccountsWithSignedOutAccounts) {
  std::vector<ListedAccount> accounts;

  ASSERT_TRUE(ParseListAccountsData(
      "[\"foo\","
      "[[\"b\",0,\"n\",\"u@g.c\",\"photo\",0,0,0,0,1,\"45\"],"
      "[\"c\",0,\"n\",\"u.2@g.c\",\"photo\",0,0,0,0,1,\"46\","
      "null,null,null,1]]]",
      &accounts));
  ASSERT_THAT(accounts, ElementsAre(ValidListedAccount("u@g.c", "45"),
                                    SignedOutListedAccount("u.2@g.c", "46")));
}

TEST(GaiaAuthUtilTest, ParseListAccountsVerifiedAccounts) {
  std::vector<ListedAccount> accounts;

  ASSERT_TRUE(ParseListAccountsData(
      "[\"foo\","
      "[[\"a\",0,\"n\",\"a@g.c\",\"photo\",0,0,0,0,1,\"45\"],"
      "[\"b\",0,\"n\",\"b@g.c\",\"photo\",0,0,0,0,1,\"46\","
      "null,null,null,null,0],"
      "[\"c\",0,\"n\",\"c@g.c\",\"photo\",0,0,0,0,1,\"47\","
      "null,null,null,null,1]]]",
      &accounts));
  ASSERT_THAT(accounts, ElementsAre(ValidListedAccount("a@g.c", "45"),
                                    NonverifiedListedAccount("b@g.c", "46"),
                                    ValidListedAccount("c@g.c", "47")));
}

TEST(GaiaAuthUtilTest, ParseListAccountsAcceptsNull) {
  ASSERT_TRUE(ParseListAccountsData(
      "[\"foo\","
      "[[\"b\",0,\"n\",\"u@g.c\",\"photo\",0,0,0,0,1,\"45\"],"
      "[\"c\",0,\"n\",\"u.2@g.c\",\"photo\",0,0,0,0,1,\"45\","
      "null,null,null,1]]]",
      nullptr));
}

TEST(GaiaAuthUtilTest, ParseConsentResultApproved) {
  const char kApprovedConsent[] = "CAESCUVOQ1JZUFRFRBoMZmFrZV9nYWlhX2lk";
  EXPECT_EQ(kApprovedConsent,
            GenerateOAuth2MintTokenConsentResult(true, "ENCRYPTED", kGaiaId));
  bool approved = false;
  std::string gaia_id;
  ASSERT_TRUE(
      ParseOAuth2MintTokenConsentResult(kApprovedConsent, &approved, &gaia_id));
  EXPECT_TRUE(approved);
  EXPECT_EQ(gaia_id, kGaiaId);
}

TEST(GaiaAuthUtilTest, ParseConsentResultApprovedEmptyData) {
  const char kApprovedConsent[] = "CAEaDGZha2VfZ2FpYV9pZA";
  EXPECT_EQ(kApprovedConsent,
            GenerateOAuth2MintTokenConsentResult(true, std::nullopt, kGaiaId));
  bool approved = false;
  std::string gaia_id;
  ASSERT_TRUE(
      ParseOAuth2MintTokenConsentResult(kApprovedConsent, &approved, &gaia_id));
  EXPECT_TRUE(approved);
  EXPECT_EQ(gaia_id, kGaiaId);
}

TEST(GaiaAuthUtilTest, ParseConsentResultApprovedEmptyGaiaId) {
  const char kApprovedConsent[] = "CAESCUVOQ1JZUFRFRA";
  EXPECT_EQ(kApprovedConsent, GenerateOAuth2MintTokenConsentResult(
                                  true, "ENCRYPTED", std::nullopt));
  bool approved = false;
  std::string gaia_id;
  ASSERT_TRUE(
      ParseOAuth2MintTokenConsentResult(kApprovedConsent, &approved, &gaia_id));
  EXPECT_TRUE(approved);
  EXPECT_TRUE(gaia_id.empty());
}

TEST(GaiaAuthUtilTest, ParseConsentResultNotApproved) {
  const char kNoGrantConsent[] = "CAAaDGZha2VfZ2FpYV9pZA";
  EXPECT_EQ(kNoGrantConsent,
            GenerateOAuth2MintTokenConsentResult(false, std::nullopt, kGaiaId));
  bool approved = false;
  std::string gaia_id;
  ASSERT_TRUE(
      ParseOAuth2MintTokenConsentResult(kNoGrantConsent, &approved, &gaia_id));
  EXPECT_FALSE(approved);
  EXPECT_EQ(gaia_id, kGaiaId);
}

TEST(GaiaAuthUtilTest, ParseConsentResultEmpty) {
  EXPECT_EQ("", GenerateOAuth2MintTokenConsentResult(std::nullopt, std::nullopt,
                                                     std::nullopt));
  bool approved = false;
  std::string gaia_id;
  ASSERT_TRUE(ParseOAuth2MintTokenConsentResult("", &approved, &gaia_id));
  // false is the default value for a bool in proto.
  EXPECT_FALSE(approved);
  // String is empty in proto by default.
  EXPECT_TRUE(gaia_id.empty());
}

TEST(GaiaAuthUtilTest, ParseConsentResultBase64UrlDisallowedPadding) {
  const char kApprovedConsentWithPadding[] = "CAE=";
  EXPECT_EQ(kApprovedConsentWithPadding,
            GenerateOAuth2MintTokenConsentResult(
                true, std::nullopt, std::nullopt,
                base::Base64UrlEncodePolicy::INCLUDE_PADDING));
  bool approved = false;
  std::string gaia_id;
  EXPECT_FALSE(ParseOAuth2MintTokenConsentResult(kApprovedConsentWithPadding,
                                                 &approved, &gaia_id));
}

TEST(GaiaAuthUtilTest, ParseConsentResultInvalidBase64Url) {
  const char kMalformedConsent[] =
      "+/";  // '+' and '/' are disallowed in base64url alphabet.
  bool approved = false;
  std::string gaia_id;
  EXPECT_FALSE(ParseOAuth2MintTokenConsentResult(kMalformedConsent, &approved,
                                                 &gaia_id));
}

TEST(GaiaAuthUtilTest, ParseConsentResultInvalidProto) {
  const char kMalformedConsent[] =
      "ab";  // Valid base64url string but invalid proto.
  bool approved = false;
  std::string gaia_id;
  EXPECT_FALSE(ParseOAuth2MintTokenConsentResult(kMalformedConsent, &approved,
                                                 &gaia_id));
}

TEST(GaiaAuthUtilTest, CreateBoundOAuthToken) {
  // base64url encoded serialized BoundOAuthToken message with the following
  // fields:
  // {
  //  "gaiaId": "test_gaia_id",
  //  "token": "test_token",
  //  "tokenBindingAssertion": "test_assertion"
  // }
  const char kExpected[] =
      "Cgx0ZXN0X2dhaWFfaWQSCnRlc3RfdG9rZW4aDnRlc3RfYXNzZXJ0aW9u";
  std::string actual =
      CreateBoundOAuthToken("test_gaia_id", "test_token", "test_assertion");
  EXPECT_EQ(actual, kExpected);
}

TEST(GaiaAuthUtilTest, CreateBoundOAuthTokenEmpty) {
  const char kExpected[] =
      "CgASABoA";  // Encodes a proto with all fields being empty.
  std::string actual = CreateBoundOAuthToken("", "", "");
  EXPECT_EQ(actual, kExpected);
}

}  // namespace gaia
