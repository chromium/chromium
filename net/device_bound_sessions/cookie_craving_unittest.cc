// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/cookie_craving.h"

#include "base/strings/string_util.h"
#include "base/unguessable_token.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_partition_key.h"
#include "net/device_bound_sessions/proto/storage.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::device_bound_sessions {

// Default values for tests.
constexpr char kUrlString[] = "https://www.example.test/foo";
constexpr char kName[] = "name";
const base::Time kCreationTime = base::Time::Now();

// Helper to Create() and unwrap a CookieCraving, expecting it to be valid.
CookieCraving CreateValidCookieCraving(
    const GURL& url,
    const std::string& name,
    const std::string& attributes,
    base::Time creation_time = kCreationTime,
    std::optional<CookiePartitionKey> cookie_partition_key = std::nullopt) {
  std::optional<CookieCraving> maybe_cc = CookieCraving::Create(
      url, name, attributes, creation_time, cookie_partition_key);
  EXPECT_TRUE(maybe_cc);
  EXPECT_TRUE(maybe_cc->IsValid());
  return std::move(*maybe_cc);
}

// Helper to create and unwrap a CanonicalCookie.
CanonicalCookie CreateCanonicalCookie(
    const GURL& url,
    const std::string& cookie_line,
    base::Time creation_time = kCreationTime,
    std::optional<CookiePartitionKey> cookie_partition_key = std::nullopt) {
  std::unique_ptr<CanonicalCookie> canonical_cookie =
      CanonicalCookie::CreateForTesting(url, cookie_line, creation_time,
                                        /*server_time=*/std::nullopt,
                                        cookie_partition_key);
  EXPECT_TRUE(canonical_cookie);
  EXPECT_TRUE(canonical_cookie->IsCanonical());
  return *canonical_cookie;
}

TEST(CookieCravingTest, CreateBasic) {
  // Default cookie.
  CookieCraving cc = CreateValidCookieCraving(GURL(kUrlString), kName, "");
  EXPECT_EQ(cc.Name(), kName);
  EXPECT_EQ(cc.Domain(), "www.example.test");
  EXPECT_EQ(cc.Path(), "/");
  EXPECT_EQ(cc.CreationDate(), kCreationTime);
  EXPECT_FALSE(cc.SecureAttribute());
  EXPECT_FALSE(cc.IsHttpOnly());
  EXPECT_EQ(cc.SameSite(), CookieSameSite::UNSPECIFIED);
  EXPECT_EQ(cc.PartitionKey(), std::nullopt);
  EXPECT_EQ(cc.SourceScheme(), CookieSourceScheme::kSecure);
  EXPECT_EQ(cc.SourcePort(), 443);

  // Non-default attributes.
  cc = CreateValidCookieCraving(
      GURL(kUrlString), kName,
      "Secure; HttpOnly; Path=/foo; Domain=example.test; SameSite=Lax");
  EXPECT_EQ(cc.Name(), kName);
  EXPECT_EQ(cc.Domain(), ".example.test");
  EXPECT_EQ(cc.Path(), "/foo");
  EXPECT_EQ(cc.CreationDate(), kCreationTime);
  EXPECT_TRUE(cc.SecureAttribute());
  EXPECT_TRUE(cc.IsHttpOnly());
  EXPECT_EQ(cc.SameSite(), CookieSameSite::LAX_MODE);
  EXPECT_EQ(cc.PartitionKey(), std::nullopt);
  EXPECT_EQ(cc.SourceScheme(), CookieSourceScheme::kSecure);
  EXPECT_EQ(cc.SourcePort(), 443);

  // Normalize whitespace.
  cc = CreateValidCookieCraving(
      GURL(kUrlString), "     name    ",
      "  Secure;HttpOnly;Path = /foo;   Domain= example.test; SameSite =Lax  ");
  EXPECT_EQ(cc.Name(), "name");
  EXPECT_EQ(cc.Domain(), ".example.test");
  EXPECT_EQ(cc.Path(), "/foo");
  EXPECT_EQ(cc.CreationDate(), kCreationTime);
  EXPECT_TRUE(cc.SecureAttribute());
  EXPECT_TRUE(cc.IsHttpOnly());
  EXPECT_EQ(cc.SameSite(), CookieSameSite::LAX_MODE);
  EXPECT_EQ(cc.PartitionKey(), std::nullopt);
  EXPECT_EQ(cc.SourceScheme(), CookieSourceScheme::kSecure);
  EXPECT_EQ(cc.SourcePort(), 443);
}

TEST(CookieCravingTest, CreateWithPartitionKey) {
  // The site of the partition key is not checked in Create(), so these two
  // should behave the same.
  const CookiePartitionKey kSameSitePartitionKey =
      CookiePartitionKey::FromURLForTesting(GURL("https://auth.example.test"));
  const CookiePartitionKey kCrossSitePartitionKey =
      CookiePartitionKey::FromURLForTesting(GURL("https://www.other.test"));
  // A key with a nonce might be used for a fenced frame or anonymous iframe.
  const CookiePartitionKey kNoncedPartitionKey =
      CookiePartitionKey::FromURLForTesting(
          GURL("https://www.anonymous-iframe.test"),
          CookiePartitionKey::AncestorChainBit::kCrossSite,
          base::UnguessableToken::Create());

  for (const CookiePartitionKey& partition_key :
       {kSameSitePartitionKey, kCrossSitePartitionKey, kNoncedPartitionKey}) {
    // Partitioned cookies must be set with Secure. The __Host- prefix is not
    // required.
    CookieCraving cc =
        CreateValidCookieCraving(GURL(kUrlString), kName, "Secure; Partitioned",
                                 kCreationTime, partition_key);
    EXPECT_TRUE(cc.SecureAttribute());
    EXPECT_TRUE(cc.IsPartitioned());
    EXPECT_EQ(cc.PartitionKey(), partition_key);
  }

  // If a cookie is not set with a Partitioned attribute, the partition key
  // should be ignored and cleared (if it's a normal partition key).
  for (const CookiePartitionKey& partition_key :
       {kSameSitePartitionKey, kCrossSitePartitionKey}) {
    CookieCraving cc = CreateValidCookieCraving(
        GURL(kUrlString), kName, "Secure", kCreationTime, partition_key);
    EXPECT_TRUE(cc.SecureAttribute());
    EXPECT_FALSE(cc.IsPartitioned());
    EXPECT_EQ(cc.PartitionKey(), std::nullopt);
  }

  // For nonced partition keys, the Partitioned attribute is not explicitly
  // required in order for the cookie to be considered partitioned.
  CookieCraving cc = CreateValidCookieCraving(
      GURL(kUrlString), kName, "Secure", kCreationTime, kNoncedPartitionKey);
  EXPECT_TRUE(cc.SecureAttribute());
  EXPECT_TRUE(cc.IsPartitioned());
  EXPECT_EQ(cc.PartitionKey(), kNoncedPartitionKey);

  // The Secure attribute is also not required for a nonced partition key.
  cc = CreateValidCookieCraving(GURL(kUrlString), kName, "", kCreationTime,
                                kNoncedPartitionKey);
  EXPECT_FALSE(cc.SecureAttribute());
  EXPECT_TRUE(cc.IsPartitioned());
  EXPECT_EQ(cc.PartitionKey(), kNoncedPartitionKey);
}

TEST(CookieCravingTest, CreateWithPrefix) {
  // Valid __Host- cookie.
  CookieCraving cc = CreateValidCookieCraving(GURL(kUrlString), "__Host-blah",
                                              "Secure; Path=/");
  EXPECT_EQ(cc.Domain(), "www.example.test");
  EXPECT_EQ(cc.Path(), "/");
  EXPECT_TRUE(cc.SecureAttribute());

  // Valid __Secure- cookie.
  cc = CreateValidCookieCraving(GURL(kUrlString), "__Secure-blah",
                                "Secure; Path=/foo; Domain=example.test");
  EXPECT_TRUE(cc.SecureAttribute());
}

// Test various strange inputs that should still be valid.
TEST(CookieCravingTest, CreateStrange) {
  const char* kStrangeNames[] = {
      // Empty name is permitted.
      "",
      // Leading and trailing whitespace should get trimmed.
      "   name     ",
      // Internal whitespace is allowed.
      "n a m e",
      // Trim leading and trailing whitespace while preserving internal
      // whitespace.
      "   n a m e   ",
  };
  for (const char* name : kStrangeNames) {
    CookieCraving cc = CreateValidCookieCraving(GURL(kUrlString), name, "");
    EXPECT_EQ(cc.Name(), base::TrimWhitespaceASCII(name, base::TRIM_ALL));
  }

  const char* kStrangeAttributesLines[] = {
      // Capitalization.
      "SECURE; PATH=/; SAMESITE=LAX",
      // Leading semicolon.
      "; Secure; Path=/; SameSite=Lax",
      // Empty except for semicolons.
      ";;;",
      // Extra whitespace.
      "     Secure;     Path=/;     SameSite=Lax     ",
      // No whitespace.
      "Secure;Path=/;SameSite=Lax",
      // Domain attribute with leading dot.
      "Domain=.example.test",
      // Different path from the URL is allowed.
      "Path=/different",
      // Path not beginning with '/' is allowed. (It's just ignored.)
      "Path=noslash",
      // Attributes with extraneous values.
      "Secure=true; HttpOnly=yes; Partitioned=absolutely",
      // Unknown attributes or attribute values.
      "Fake=totally; SameSite=SuperStrict",
  };
  for (const char* attributes : kStrangeAttributesLines) {
    CreateValidCookieCraving(GURL(kUrlString), kName, attributes);
  }
}

// Another strange/maybe unexpected case is that Create() does not check the
// secureness of the URL against the cookie's Secure attribute. (This is
// documented in the method comment.)
TEST(CookieCravingTest, CreateSecureFromInsecureUrl) {
  CookieCraving cc =
      CreateValidCookieCraving(GURL("http://insecure.test"), kName, "Secure");
  EXPECT_TRUE(cc.SecureAttribute());
  EXPECT_EQ(cc.SourceScheme(), CookieSourceScheme::kNonSecure);
}

// Test inputs that should result in a failure to parse the cookie line.
TEST(CookieCravingTest, CreateFailParse) {
  const struct {
    const char* name;
    const char* attributes;
  } kParseFailInputs[] = {
      // Invalid characters in name.
      {"blah\nsomething", "Secure; Path=/"},
      {"blah=something", "Secure; Path=/"},
      {"blah;something", "Secure; Path=/"},
      // Truncated lines are blocked.
      {"name", "Secure;\n Path=/"},
  };
  for (const auto& input : kParseFailInputs) {
    std::optional<CookieCraving> cc =
        CookieCraving::Create(GURL(kUrlString), input.name, input.attributes,
                              kCreationTime, std::nullopt);
    EXPECT_FALSE(cc);
  }
}

// Test cases where the Create() params are not valid.
TEST(CookieCravingTest, CreateFailInvalidParams) {
  // Invalid URL.
  std::optional<CookieCraving> cc =
      CookieCraving::Create(GURL(), kName, "", kCreationTime, std::nullopt);
  EXPECT_FALSE(cc);

  // Null creation time.
  cc = CookieCraving::Create(GURL(kUrlString), kName, "", base::Time(),
                             std::nullopt);
  EXPECT_FALSE(cc);
}

TEST(CookieCravingTest, CreateFailBadDomain) {
  // URL does not match domain.
  std::optional<CookieCraving> cc =
      CookieCraving::Create(GURL(kUrlString), kName, "Domain=other.test",
                            kCreationTime, std::nullopt);
  EXPECT_FALSE(cc);

  // Public suffix is not allowed to be Domain attribute.
  cc = CookieCraving::Create(GURL(kUrlString), kName, "Domain=test",
                             kCreationTime, std::nullopt);
  EXPECT_FALSE(cc);

  // IP addresses cannot set suffixes as the Domain attribute.
  cc = CookieCraving::Create(GURL("http://1.2.3.4"), kName, "Domain=2.3.4",
                             kCreationTime, std::nullopt);
  EXPECT_FALSE(cc);
}

TEST(CookieCravingTest, CreateFailBadPartitioned) {
  const CookiePartitionKey kPartitionKey =
      CookiePartitionKey::FromURLForTesting(GURL("https://example.test"));

  // Not Secure.
  std::optional<CookieCraving> cc = CookieCraving::Create(
      GURL(kUrlString), kName, "Partitioned", kCreationTime, kPartitionKey);
  EXPECT_FALSE(cc);

  // The URL scheme is not cryptographic.
  cc = CookieCraving::Create(GURL("http://example.test"), kName,
                             "Secure; Partitioned", kCreationTime,
                             kPartitionKey);
  EXPECT_FALSE(cc);
}

TEST(CookieCravingTest, CreateFailInvalidPrefix) {
  // __Host- with insecure URL.
  std::optional<CookieCraving> cc =
      CookieCraving::Create(GURL("http://insecure.test"), "__Host-blah",
                            "Secure; Path=/", kCreationTime, std::nullopt);
  EXPECT_FALSE(cc);

  // __Host- with non-Secure cookie.
  cc = CookieCraving::Create(GURL(kUrlString), "__Host-blah", "Path=/",
                             kCreationTime, std::nullopt);
  EXPECT_FALSE(cc);

  // __Host- with Domain attribute value.
  cc = CookieCraving::Create(GURL(kUrlString), "__Host-blah",
                             "Secure; Path=/; Domain=example.test",
                             kCreationTime, std::nullopt);
  EXPECT_FALSE(cc);

  // __Host- with non-root path.
  cc = CookieCraving::Create(GURL(kUrlString), "__Host-blah",
                             "Secure; Path=/foo", kCreationTime, std::nullopt);
  EXPECT_FALSE(cc);

  // __Secure- with non-Secure cookie.
  cc = CookieCraving::Create(GURL(kUrlString), "__Secure-blah", "",
                             kCreationTime, std::nullopt);
  EXPECT_FALSE(cc);

  // Prefixes are checked case-insensitively, so these CookieCravings are also
  // invalid for not satisfying the prefix requirements.
  // Missing Secure.
  cc = CookieCraving::Create(GURL(kUrlString), "__host-blah", "Path=/",
                             kCreationTime, std::nullopt);
  EXPECT_FALSE(cc);
  // Specifies Domain.
  cc = CookieCraving::Create(GURL(kUrlString), "__HOST-blah",
                             "Secure; Path=/; Domain=example.test",
                             kCreationTime, std::nullopt);
  EXPECT_FALSE(cc);
  // Missing Secure.
  cc = CookieCraving::Create(GURL(kUrlString), "__SeCuRe-blah", "",
                             kCreationTime, std::nullopt);
  EXPECT_FALSE(cc);
}

// Valid cases were tested as part of the successful Create() tests above, so
// this only tests the invalid cases.
TEST(CookieCravingTest, IsNotValid) {
  const struct {
    const char* name;
    const char* domain;
    const char* path;
    bool secure;
    base::Time creation = kCreationTime;
  } kTestCases[] = {
      // Invalid name.
      {" name", "www.example.test", "/", true},
      {";", "www.example.test", "/", true},
      {"=", "www.example.test", "/", true},
      {"na\nme", "www.example.test", "/", true},
      // Empty domain.
      {"name", "", "/", true},
      // Non-canonical domain.
      {"name", "ExAmPlE.test", "/", true},
      // Empty path.
      {"name", "www.example.test", "", true},
      // Path not beginning with slash.
      {"name", "www.example.test", "noslash", true},
      // Invalid __Host- prefix.
      {"__Host-name", ".example.test", "/", true},
      {"__Host-name", "www.example.test", "/", false},
      {"__Host-name", "www.example.test", "/foo", false},
      // Invalid __Secure- prefix.
      {"__Secure-name", "www.example.test", "/", false},
      // Invalid __Host- prefix (case insensitive).
      {"__HOST-name", ".example.test", "/", true},
      {"__HoSt-name", "www.example.test", "/", false},
      {"__host-name", "www.example.test", "/foo", false},
      // Invalid __Secure- prefix (case insensitive).
      {"__secure-name", "www.example.test", "/", false},
      // Null creation date.
      {"name", "www.example.test", "/", true, base::Time()},
  };

  for (const auto& test_case : kTestCases) {
    CookieCraving cc = CookieCraving::CreateUnsafeForTesting(
        test_case.name, test_case.domain, test_case.path, test_case.creation,
        test_case.secure,
        /*httponly=*/false, CookieSameSite::LAX_MODE,
        /*partition_key=*/std::nullopt, CookieSourceScheme::kSecure, 443);
    SCOPED_TRACE(cc.DebugString());
    EXPECT_FALSE(cc.IsValid());
  }

  // Additionally, Partitioned requires the Secure attribute.
  CookieCraving cc = CookieCraving::CreateUnsafeForTesting(
      "name", "www.example.test", "/", kCreationTime, /*secure=*/false,
      /*httponly=*/false, CookieSameSite::LAX_MODE,
      CookiePartitionKey::FromURLForTesting(GURL("https://example.test")),
      CookieSourceScheme::kSecure, 443);
  EXPECT_FALSE(cc.IsValid());
}

TEST(CookieCravingTest, IsSatisfiedBy) {
  // Default case with no attributes.
  CanonicalCookie canonical_cookie =
      CreateCanonicalCookie(GURL(kUrlString), "name=somevalue");
  CookieCraving cookie_craving =
      CreateValidCookieCraving(GURL(kUrlString), "name", "");
  EXPECT_TRUE(cookie_craving.IsSatisfiedBy(canonical_cookie));

  // With attributes.
  canonical_cookie =
      CreateCanonicalCookie(GURL(kUrlString),
                            "name=somevalue; Domain=example.test; Path=/; "
                            "Secure; HttpOnly; SameSite=Lax");
  cookie_craving = CreateValidCookieCraving(
      GURL(kUrlString), "name",
      "Domain=example.test; Path=/; Secure; HttpOnly; SameSite=Lax");
  EXPECT_TRUE(cookie_craving.IsSatisfiedBy(canonical_cookie));

  // The URL may differ as long as the cookie attributes match.
  canonical_cookie = CreateCanonicalCookie(
      GURL(kUrlString), "name=somevalue; Domain=example.test");
  cookie_craving = CreateValidCookieCraving(
      GURL("https://subdomain.example.test"), "name", "Domain=example.test");
  EXPECT_TRUE(cookie_craving.IsSatisfiedBy(canonical_cookie));

  // Creation time is not required to match.
  canonical_cookie = CreateCanonicalCookie(
      GURL(kUrlString), "name=somevalue; Domain=example.test", kCreationTime);
  cookie_craving =
      CreateValidCookieCraving(GURL(kUrlString), "name", "Domain=example.test",
                               kCreationTime + base::Hours(1));
  EXPECT_TRUE(cookie_craving.IsSatisfiedBy(canonical_cookie));

  // Source scheme and port (and indeed source host) are not required to match.
  canonical_cookie = CreateCanonicalCookie(
      GURL(kUrlString), "name=somevalue; Domain=example.test");
  cookie_craving =
      CreateValidCookieCraving(GURL("http://subdomain.example.test:8080"),
                               "name", "Domain=example.test");
  EXPECT_TRUE(cookie_craving.IsSatisfiedBy(canonical_cookie));
}

TEST(CookieCravingTest, IsNotSatisfiedBy) {
  // Name does not match.
  CanonicalCookie canonical_cookie =
      CreateCanonicalCookie(GURL(kUrlString), "realname=somevalue");
  CookieCraving cookie_craving =
      CreateValidCookieCraving(GURL(kUrlString), "fakename", "");
  EXPECT_FALSE(cookie_craving.IsSatisfiedBy(canonical_cookie));

  // Domain does not match.
  canonical_cookie = CreateCanonicalCookie(
      GURL(kUrlString), "name=somevalue; Domain=example.test");
  cookie_craving = CreateValidCookieCraving(GURL(kUrlString), "name",
                                            "Domain=www.example.test");
  EXPECT_FALSE(cookie_craving.IsSatisfiedBy(canonical_cookie));

  // Host cookie vs domain cookie.
  canonical_cookie = CreateCanonicalCookie(GURL(kUrlString), "name=somevalue");
  cookie_craving = CreateValidCookieCraving(GURL(kUrlString), "name",
                                            "Domain=www.example.test");
  EXPECT_FALSE(cookie_craving.IsSatisfiedBy(canonical_cookie));

  // Domain cookie vs host cookie.
  canonical_cookie = CreateCanonicalCookie(
      GURL(kUrlString), "name=somevalue; Domain=www.example.test");
  cookie_craving = CreateValidCookieCraving(GURL(kUrlString), "name", "");
  EXPECT_FALSE(cookie_craving.IsSatisfiedBy(canonical_cookie));

  // Path does not match.
  canonical_cookie = CreateCanonicalCookie(
      GURL(kUrlString), "name=somevalue; Domain=example.test; Path=/");
  cookie_craving = CreateValidCookieCraving(GURL(kUrlString), "name",
                                            "Domain=example.test; Path=/foo");
  EXPECT_FALSE(cookie_craving.IsSatisfiedBy(canonical_cookie));

  // Secure vs non-Secure.
  canonical_cookie = CreateCanonicalCookie(
      GURL(kUrlString), "name=somevalue; Secure; Domain=example.test; Path=/");
  cookie_craving = CreateValidCookieCraving(GURL(kUrlString), "name",
                                            "Domain=example.test; Path=/foo");
  EXPECT_FALSE(cookie_craving.IsSatisfiedBy(canonical_cookie));

  // Non-Secure vs Secure.
  canonical_cookie = CreateCanonicalCookie(
      GURL(kUrlString), "name=somevalue; Domain=example.test; Path=/");
  cookie_craving = CreateValidCookieCraving(
      GURL(kUrlString), "name", "Secure; Domain=example.test; Path=/foo");
  EXPECT_FALSE(cookie_craving.IsSatisfiedBy(canonical_cookie));

  // HttpOnly vs non-HttpOnly.
  canonical_cookie = CreateCanonicalCookie(
      GURL(kUrlString),
      "name=somevalue; HttpOnly; Domain=example.test; Path=/");
  cookie_craving = CreateValidCookieCraving(GURL(kUrlString), "name",
                                            "Domain=example.test; Path=/foo");
  EXPECT_FALSE(cookie_craving.IsSatisfiedBy(canonical_cookie));

  // Non-HttpOnly vs HttpOnly.
  canonical_cookie = CreateCanonicalCookie(
      GURL(kUrlString), "name=somevalue; Domain=example.test; Path=/");
  cookie_craving = CreateValidCookieCraving(
      GURL(kUrlString), "name", "HttpOnly; Domain=example.test; Path=/foo");
  EXPECT_FALSE(cookie_craving.IsSatisfiedBy(canonical_cookie));

  // SameSite does not match.
  canonical_cookie =
      CreateCanonicalCookie(GURL(kUrlString), "name=somevalue; SameSite=Lax");
  cookie_craving =
      CreateValidCookieCraving(GURL(kUrlString), "name", "SameSite=Strict");
  EXPECT_FALSE(cookie_craving.IsSatisfiedBy(canonical_cookie));

  // SameSite vs unspecified SameSite. (Note that the SameSite attribute value
  // is compared, not the effective SameSite enforcement mode.)
  canonical_cookie =
      CreateCanonicalCookie(GURL(kUrlString), "name=somevalue; SameSite=Lax");
  cookie_craving = CreateValidCookieCraving(GURL(kUrlString), "name", "");
  EXPECT_FALSE(cookie_craving.IsSatisfiedBy(canonical_cookie));
}

TEST(CookieCravingTest, IsSatisfiedByWithPartitionKey) {
  const CookiePartitionKey kPartitionKey =
      CookiePartitionKey::FromURLForTesting(GURL("https://example.test"));
  const CookiePartitionKey kOtherPartitionKey =
      CookiePartitionKey::FromURLForTesting(GURL("https://other.test"));

  const base::UnguessableToken kNonce = base::UnguessableToken::Create();
  const CookiePartitionKey kNoncedPartitionKey =
      CookiePartitionKey::FromURLForTesting(
          GURL("https://example.test"),
          CookiePartitionKey::AncestorChainBit::kCrossSite, kNonce);

  // Partition keys match.
  CanonicalCookie canonical_cookie = CreateCanonicalCookie(
      GURL(kUrlString), "name=somevalue; Secure; Partitioned", kCreationTime,
      kPartitionKey);
  CookieCraving cookie_craving =
      CreateValidCookieCraving(GURL(kUrlString), "name", "Secure; Partitioned",
                               kCreationTime, kPartitionKey);
  EXPECT_TRUE(cookie_craving.IsSatisfiedBy(canonical_cookie));

  // Cookie line doesn't specified Partitioned so key gets cleared for both.
  canonical_cookie = CreateCanonicalCookie(
      GURL(kUrlString), "name=somevalue; Secure", kCreationTime, kPartitionKey);
  cookie_craving = CreateValidCookieCraving(GURL(kUrlString), "name", "Secure",
                                            kCreationTime, kOtherPartitionKey);
  EXPECT_TRUE(cookie_craving.IsSatisfiedBy(canonical_cookie));

  // Without partition key for the CookieCraving, but cookie line doesn't
  // specify Partitioned so they are equivalent.
  canonical_cookie = CreateCanonicalCookie(
      GURL(kUrlString), "name=somevalue; Secure", kCreationTime, kPartitionKey);
  cookie_craving = CreateValidCookieCraving(GURL(kUrlString), "name", "Secure");
  EXPECT_TRUE(cookie_craving.IsSatisfiedBy(canonical_cookie));

  // Without partition key for the CanonicalCookie, but cookie line doesn't
  // specify Partitioned so they are equivalent.
  canonical_cookie =
      CreateCanonicalCookie(GURL(kUrlString), "name=somevalue; Secure");
  cookie_craving = CreateValidCookieCraving(GURL(kUrlString), "name", "Secure",
                                            kCreationTime, kPartitionKey);
  EXPECT_TRUE(cookie_craving.IsSatisfiedBy(canonical_cookie));

  // Identical nonced partition keys.
  canonical_cookie =
      CreateCanonicalCookie(GURL(kUrlString), "name=somevalue; Secure",
                            kCreationTime, kNoncedPartitionKey);
  cookie_craving = CreateValidCookieCraving(GURL(kUrlString), "name", "Secure",
                                            kCreationTime, kNoncedPartitionKey);
  EXPECT_TRUE(cookie_craving.IsSatisfiedBy(canonical_cookie));
}

TEST(CookieCravingTest, IsNotSatisfiedByWithPartitionKey) {
  const CookiePartitionKey kPartitionKey =
      CookiePartitionKey::FromURLForTesting(GURL("https://example.test"));
  const CookiePartitionKey kOtherPartitionKey =
      CookiePartitionKey::FromURLForTesting(GURL("https://other.test"));

  const base::UnguessableToken kNonce = base::UnguessableToken::Create();
  const base::UnguessableToken kOtherNonce = base::UnguessableToken::Create();
  const CookiePartitionKey kNoncedPartitionKey =
      CookiePartitionKey::FromURLForTesting(
          GURL("https://example.test"),
          CookiePartitionKey::AncestorChainBit::kCrossSite, kNonce);
  const CookiePartitionKey kOtherNoncedPartitionKey =
      CookiePartitionKey::FromURLForTesting(
          GURL("https://example.test"),
          CookiePartitionKey::AncestorChainBit::kCrossSite, kOtherNonce);

  // Partition keys do not match.
  CanonicalCookie canonical_cookie = CreateCanonicalCookie(
      GURL(kUrlString), "name=somevalue; Secure; Partitioned", kCreationTime,
      kPartitionKey);
  CookieCraving cookie_craving =
      CreateValidCookieCraving(GURL(kUrlString), "name", "Secure; Partitioned",
                               kCreationTime, kOtherPartitionKey);
  EXPECT_FALSE(cookie_craving.IsSatisfiedBy(canonical_cookie));

  // Nonced partition keys do not match.
  canonical_cookie = CreateCanonicalCookie(
      GURL(kUrlString), "name=somevalue; Secure; Partitioned", kCreationTime,
      kNoncedPartitionKey);
  cookie_craving =
      CreateValidCookieCraving(GURL(kUrlString), "name", "Secure; Partitioned",
                               kCreationTime, kOtherNoncedPartitionKey);
  EXPECT_FALSE(cookie_craving.IsSatisfiedBy(canonical_cookie));

  // Nonced partition key vs regular partition key.
  canonical_cookie = CreateCanonicalCookie(
      GURL(kUrlString), "name=somevalue; Secure; Partitioned", kCreationTime,
      kNoncedPartitionKey);
  cookie_craving =
      CreateValidCookieCraving(GURL(kUrlString), "name", "Secure; Partitioned",
                               kCreationTime, kPartitionKey);
  EXPECT_FALSE(cookie_craving.IsSatisfiedBy(canonical_cookie));

  // Regular partition key vs nonced partition key.
  canonical_cookie = CreateCanonicalCookie(
      GURL(kUrlString), "name=somevalue; Secure; Partitioned", kCreationTime,
      kPartitionKey);
  cookie_craving =
      CreateValidCookieCraving(GURL(kUrlString), "name", "Secure; Partitioned",
                               kCreationTime, kNoncedPartitionKey);
  EXPECT_FALSE(cookie_craving.IsSatisfiedBy(canonical_cookie));
}

TEST(CookieCravingTest, BasicCookieToFromProto) {
  // Default cookie.
  CookieCraving cc = CreateValidCookieCraving(GURL(kUrlString), kName, "");

  proto::CookieCraving proto = cc.ToProto();
  EXPECT_EQ(proto.name(), kName);
  EXPECT_EQ(proto.domain(), "www.example.test");
  EXPECT_EQ(proto.path(), "/");
  EXPECT_EQ(proto.creation_time(),
            kCreationTime.ToDeltaSinceWindowsEpoch().InMicroseconds());
  EXPECT_FALSE(proto.secure());
  EXPECT_FALSE(proto.httponly());
  EXPECT_EQ(proto.same_site(),
            proto::CookieSameSite::COOKIE_SAME_SITE_UNSPECIFIED);
  EXPECT_FALSE(proto.has_serialized_partition_key());
  EXPECT_EQ(proto.source_scheme(), proto::CookieSourceScheme::SECURE);
  EXPECT_EQ(proto.source_port(), 443);

  std::optional<CookieCraving> restored_cc =
      CookieCraving::CreateFromProto(proto);
  ASSERT_TRUE(restored_cc.has_value());
  EXPECT_TRUE(restored_cc->IsEqualForTesting(cc));

  // Non-default attributes.
  cc = CreateValidCookieCraving(
      GURL(kUrlString), kName,
      "Secure; HttpOnly; Path=/foo; Domain=example.test; SameSite=Lax");

  proto = cc.ToProto();
  EXPECT_EQ(proto.name(), kName);
  EXPECT_EQ(proto.domain(), ".example.test");
  EXPECT_EQ(proto.path(), "/foo");
  EXPECT_EQ(proto.creation_time(),
            kCreationTime.ToDeltaSinceWindowsEpoch().InMicroseconds());
  EXPECT_TRUE(proto.secure());
  EXPECT_TRUE(proto.httponly());
  EXPECT_EQ(proto.same_site(), proto::CookieSameSite::LAX_MODE);
  EXPECT_FALSE(proto.has_serialized_partition_key());
  EXPECT_EQ(proto.source_scheme(), proto::CookieSourceScheme::SECURE);
  EXPECT_EQ(proto.source_port(), 443);

  restored_cc = CookieCraving::CreateFromProto(proto);
  ASSERT_TRUE(restored_cc.has_value());
  EXPECT_TRUE(restored_cc->IsEqualForTesting(cc));
}

TEST(CookieCravingTest, PartitionedCookieToFromProto) {
  const CookiePartitionKey kSameSitePartitionKey =
      CookiePartitionKey::FromURLForTesting(GURL("https://auth.example.test"));
  const CookiePartitionKey kCrossSitePartitionKey =
      CookiePartitionKey::FromURLForTesting(GURL("https://www.other.test"));

  for (const CookiePartitionKey& partition_key :
       {kSameSitePartitionKey, kCrossSitePartitionKey}) {
    // Partitioned cookies must be set with Secure. The __Host- prefix is not
    // required.
    CookieCraving cc =
        CreateValidCookieCraving(GURL(kUrlString), kName, "Secure; Partitioned",
                                 kCreationTime, partition_key);
    EXPECT_EQ(cc.PartitionKey(), partition_key);
    base::expected<net::CookiePartitionKey::SerializedCookiePartitionKey,
                   std::string>
        serialized_partition_key =
            net::CookiePartitionKey::Serialize(partition_key);
    CHECK(serialized_partition_key.has_value());

    proto::CookieCraving proto = cc.ToProto();
    EXPECT_TRUE(proto.secure());
    ASSERT_TRUE(proto.has_serialized_partition_key());
    EXPECT_EQ(proto.serialized_partition_key().top_level_site(),
              serialized_partition_key->TopLevelSite());
    EXPECT_EQ(proto.serialized_partition_key().has_cross_site_ancestor(),
              serialized_partition_key->has_cross_site_ancestor());

    std::optional<CookieCraving> restored_cc =
        CookieCraving::CreateFromProto(proto);
    ASSERT_TRUE(restored_cc.has_value());
    EXPECT_TRUE(restored_cc->IsEqualForTesting(cc));
  }
}

TEST(CookieCravingTest, FailCreateFromInvalidProto) {
  // Empty proto.
  proto::CookieCraving proto;
  std::optional<CookieCraving> cc = CookieCraving::CreateFromProto(proto);
  EXPECT_FALSE(cc.has_value());

  cc = CreateValidCookieCraving(
      GURL(kUrlString), kName,
      "Secure; HttpOnly; Path=/foo; Domain=example.test; SameSite=Lax");
  proto = cc->ToProto();

  // Missing parameters.
  {
    proto::CookieCraving p(proto);
    p.clear_name();
    std::optional<CookieCraving> c = CookieCraving::CreateFromProto(p);
    EXPECT_FALSE(c.has_value());
  }
  {
    proto::CookieCraving p(proto);
    p.clear_domain();
    std::optional<CookieCraving> c = CookieCraving::CreateFromProto(p);
    EXPECT_FALSE(c.has_value());
  }
  {
    proto::CookieCraving p(proto);
    p.clear_path();
    std::optional<CookieCraving> c = CookieCraving::CreateFromProto(p);
    EXPECT_FALSE(c.has_value());
  }
  {
    proto::CookieCraving p(proto);
    p.clear_secure();
    std::optional<CookieCraving> c = CookieCraving::CreateFromProto(p);
    EXPECT_FALSE(c.has_value());
  }
  {
    proto::CookieCraving p(proto);
    p.clear_httponly();
    std::optional<CookieCraving> c = CookieCraving::CreateFromProto(p);
    EXPECT_FALSE(c.has_value());
  }
  {
    proto::CookieCraving p(proto);
    p.clear_source_port();
    std::optional<CookieCraving> c = CookieCraving::CreateFromProto(p);
    EXPECT_FALSE(c.has_value());
  }
  {
    proto::CookieCraving p(proto);
    p.clear_creation_time();
    std::optional<CookieCraving> c = CookieCraving::CreateFromProto(p);
    EXPECT_FALSE(c.has_value());
  }
  {
    proto::CookieCraving p(proto);
    p.clear_same_site();
    std::optional<CookieCraving> c = CookieCraving::CreateFromProto(p);
    EXPECT_FALSE(c.has_value());
  }
  {
    proto::CookieCraving p(proto);
    p.clear_source_scheme();
    std::optional<CookieCraving> c = CookieCraving::CreateFromProto(p);
    EXPECT_FALSE(c.has_value());
  }
  // Malformed serialized partition key.
  {
    proto::CookieCraving p(proto);
    p.mutable_serialized_partition_key()->set_top_level_site("");
    p.mutable_serialized_partition_key()->set_has_cross_site_ancestor(false);
    std::optional<CookieCraving> c = CookieCraving::CreateFromProto(p);
    EXPECT_FALSE(c.has_value());
  }
}

}  // namespace net::device_bound_sessions
