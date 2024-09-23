// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cookie_manager_mojom_traits.h"

#include <vector>

#include "base/test/gtest_util.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/cookies/cookie_constants.h"
#include "services/network/public/cpp/cookie_manager_mojom_traits.h"
#include "services/network/public/mojom/cookie_manager.mojom-shared.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/third_party/mozilla/url_parse.h"

namespace network {
namespace {

TEST(CookieManagerTraitsTest, Roundtrips_CanonicalCookie) {
  auto original = net::CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "B", "x.y", "/path", base::Time(), base::Time(), base::Time(),
      base::Time(), false, false, net::CookieSameSite::NO_RESTRICTION,
      net::COOKIE_PRIORITY_LOW, std::nullopt, net::CookieSourceScheme::kSecure,
      8433);

  net::CanonicalCookie copied;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::CanonicalCookie>(
      *original, copied));

  EXPECT_EQ(original->Name(), copied.Name());
  EXPECT_EQ(original->Value(), copied.Value());
  EXPECT_EQ(original->Domain(), copied.Domain());
  EXPECT_EQ(original->Path(), copied.Path());
  EXPECT_EQ(original->CreationDate(), copied.CreationDate());
  EXPECT_EQ(original->LastAccessDate(), copied.LastAccessDate());
  EXPECT_EQ(original->ExpiryDate(), copied.ExpiryDate());
  EXPECT_EQ(original->LastUpdateDate(), copied.LastUpdateDate());
  EXPECT_EQ(original->SecureAttribute(), copied.SecureAttribute());
  EXPECT_EQ(original->IsHttpOnly(), copied.IsHttpOnly());
  EXPECT_EQ(original->SameSite(), copied.SameSite());
  EXPECT_EQ(original->Priority(), copied.Priority());
  EXPECT_EQ(std::nullopt, copied.PartitionKey());
  EXPECT_EQ(original->SourceScheme(), copied.SourceScheme());
  EXPECT_EQ(original->SourcePort(), copied.SourcePort());
  EXPECT_EQ(original->SourceType(), copied.SourceType());

  // Test port edge cases: unspecified.
  auto original_unspecified =
      net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A", "B", "x.y", "/path", base::Time(), base::Time(), base::Time(),
          base::Time(), false, false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_LOW, std::nullopt,
          net::CookieSourceScheme::kSecure, url::PORT_UNSPECIFIED);
  net::CanonicalCookie copied_unspecified;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::CanonicalCookie>(
      *original_unspecified, copied_unspecified));

  EXPECT_EQ(original_unspecified->SourcePort(),
            copied_unspecified.SourcePort());

  // Test port edge cases: invalid.
  auto original_invalid = net::CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "B", "x.y", "/path", base::Time(), base::Time(), base::Time(),
      base::Time(), false, false, net::CookieSameSite::NO_RESTRICTION,
      net::COOKIE_PRIORITY_LOW, std::nullopt, net::CookieSourceScheme::kSecure,
      url::PORT_INVALID);
  net::CanonicalCookie copied_invalid;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::CanonicalCookie>(
      *original_invalid, copied_invalid));

  EXPECT_EQ(original_invalid->SourcePort(), copied_invalid.SourcePort());

  // Serializer returns false if cookie is non-canonical.
  // Example is non-canonical because of newline in name.

  original = net::CanonicalCookie::CreateUnsafeCookieForTesting(
      "A\n", "B", "x.y", "/path", base::Time(), base::Time(), base::Time(),
      base::Time(), false, false, net::CookieSameSite::NO_RESTRICTION,
      net::COOKIE_PRIORITY_LOW);

  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<mojom::CanonicalCookie>(
      *original, copied));
}

TEST(CookieManagerTraitsTest, Roundtrips_CookieAccessResult) {
  net::CookieAccessResult original = net::CookieAccessResult(
      net::CookieEffectiveSameSite::LAX_MODE,
      net::CookieInclusionStatus(
          net::CookieInclusionStatus::
              EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX,
          net::CookieInclusionStatus::
              WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT),
      net::CookieAccessSemantics::LEGACY,
      true /* is_allowed_to_access_secure_cookies */);
  net::CookieAccessResult copied;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::CookieAccessResult>(
      original, copied));

  EXPECT_EQ(original.effective_same_site, copied.effective_same_site);
  EXPECT_TRUE(copied.status.HasExactlyExclusionReasonsForTesting(
      {net::CookieInclusionStatus::
           EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX}));
  EXPECT_TRUE(copied.status.HasExactlyWarningReasonsForTesting(
      {net::CookieInclusionStatus::
           WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT}));
  EXPECT_EQ(original.is_allowed_to_access_secure_cookies,
            copied.is_allowed_to_access_secure_cookies);
}

TEST(CookieManagerTraitsTest, Rountrips_CookieWithAccessResult) {
  auto original_cookie = net::CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "B", "x.y", "/path", base::Time(), base::Time(), base::Time(),
      base::Time(),
      /*secure=*/true, /*httponly=*/false, net::CookieSameSite::NO_RESTRICTION,
      net::COOKIE_PRIORITY_LOW);

  net::CookieWithAccessResult original = {*original_cookie,
                                          net::CookieAccessResult()};
  net::CookieWithAccessResult copied;

  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::CookieWithAccessResult>(
          original, copied));

  EXPECT_EQ(original.cookie.Name(), copied.cookie.Name());
  EXPECT_EQ(original.cookie.Value(), copied.cookie.Value());
  EXPECT_EQ(original.cookie.Domain(), copied.cookie.Domain());
  EXPECT_EQ(original.cookie.Path(), copied.cookie.Path());
  EXPECT_EQ(original.cookie.CreationDate(), copied.cookie.CreationDate());
  EXPECT_EQ(original.cookie.LastAccessDate(), copied.cookie.LastAccessDate());
  EXPECT_EQ(original.cookie.ExpiryDate(), copied.cookie.ExpiryDate());
  EXPECT_EQ(original.cookie.LastUpdateDate(), copied.cookie.LastUpdateDate());
  EXPECT_EQ(original.cookie.SecureAttribute(), copied.cookie.SecureAttribute());
  EXPECT_EQ(original.cookie.IsHttpOnly(), copied.cookie.IsHttpOnly());
  EXPECT_EQ(original.cookie.SameSite(), copied.cookie.SameSite());
  EXPECT_EQ(original.cookie.Priority(), copied.cookie.Priority());
  EXPECT_EQ(original.cookie.SourceType(), copied.cookie.SourceType());
  EXPECT_EQ(original.access_result.effective_same_site,
            copied.access_result.effective_same_site);
  EXPECT_EQ(original.access_result.status, copied.access_result.status);
}

TEST(CookieManagerTraitsTest, Roundtrips_CookieAndLineWithAccessResult) {
  auto original_cookie = net::CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "B", "x.y", "/path", base::Time(), base::Time(), base::Time(),
      base::Time(),
      /*secure=*/true, /*httponly=*/false, net::CookieSameSite::NO_RESTRICTION,
      net::COOKIE_PRIORITY_LOW);

  net::CookieAndLineWithAccessResult original(*original_cookie, "cookie-string",
                                              net::CookieAccessResult());
  net::CookieAndLineWithAccessResult copied;

  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::CookieAndLineWithAccessResult>(
          original, copied));

  EXPECT_EQ(original.cookie->Name(), copied.cookie->Name());
  EXPECT_EQ(original.cookie->Value(), copied.cookie->Value());
  EXPECT_EQ(original.cookie->Domain(), copied.cookie->Domain());
  EXPECT_EQ(original.cookie->Path(), copied.cookie->Path());
  EXPECT_EQ(original.cookie->CreationDate(), copied.cookie->CreationDate());
  EXPECT_EQ(original.cookie->LastAccessDate(), copied.cookie->LastAccessDate());
  EXPECT_EQ(original.cookie->ExpiryDate(), copied.cookie->ExpiryDate());
  EXPECT_EQ(original.cookie->LastUpdateDate(), copied.cookie->LastUpdateDate());
  EXPECT_EQ(original.cookie->SecureAttribute(),
            copied.cookie->SecureAttribute());
  EXPECT_EQ(original.cookie->IsHttpOnly(), copied.cookie->IsHttpOnly());
  EXPECT_EQ(original.cookie->SameSite(), copied.cookie->SameSite());
  EXPECT_EQ(original.cookie->Priority(), copied.cookie->Priority());
  EXPECT_EQ(original.cookie->SourceType(), copied.cookie->SourceType());
  EXPECT_EQ(original.access_result.effective_same_site,
            copied.access_result.effective_same_site);
  EXPECT_EQ(original.cookie_string, copied.cookie_string);
}

TEST(CookieManagerTraitsTest, Roundtrips_CookieSameSite) {
  for (net::CookieSameSite cookie_state :
       {net::CookieSameSite::NO_RESTRICTION, net::CookieSameSite::LAX_MODE,
        net::CookieSameSite::STRICT_MODE, net::CookieSameSite::UNSPECIFIED}) {
    net::CookieSameSite roundtrip;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::CookieSameSite>(
        cookie_state, roundtrip));
    EXPECT_EQ(cookie_state, roundtrip);
  }
}

TEST(CookieManagerTraitsTest, Roundtrips_CookieEffectiveSameSite) {
  for (net::CookieEffectiveSameSite cookie_state :
       {net::CookieEffectiveSameSite::NO_RESTRICTION,
        net::CookieEffectiveSameSite::LAX_MODE,
        net::CookieEffectiveSameSite::STRICT_MODE,
        net::CookieEffectiveSameSite::LAX_MODE_ALLOW_UNSAFE,
        net::CookieEffectiveSameSite::UNDEFINED}) {
    net::CookieEffectiveSameSite roundtrip;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<mojom::CookieEffectiveSameSite>(
            cookie_state, roundtrip));
    EXPECT_EQ(cookie_state, roundtrip);
  }
}

TEST(CookieManagerTraitsTest, Roundtrips_ContextType) {
  using ContextType = net::CookieOptions::SameSiteCookieContext::ContextType;
  for (ContextType context_type :
       {ContextType::CROSS_SITE, ContextType::SAME_SITE_LAX_METHOD_UNSAFE,
        ContextType::SAME_SITE_LAX, ContextType::SAME_SITE_STRICT}) {
    ContextType roundtrip;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::ContextType>(
        context_type, roundtrip));
    EXPECT_EQ(context_type, roundtrip);
  }
}

TEST(CookieManagerTraitsTest, Roundtrips_CookieAccessSemantics) {
  for (net::CookieAccessSemantics access_semantics :
       {net::CookieAccessSemantics::UNKNOWN,
        net::CookieAccessSemantics::NONLEGACY,
        net::CookieAccessSemantics::LEGACY}) {
    net::CookieAccessSemantics roundtrip;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<mojom::CookieAccessSemantics>(
            access_semantics, roundtrip));
    EXPECT_EQ(access_semantics, roundtrip);
  }
}

TEST(CookieManagerTraitsTest, Roundtrips_CookieSourceScheme) {
  for (net::CookieSourceScheme source_scheme :
       {net::CookieSourceScheme::kUnset, net::CookieSourceScheme::kNonSecure,
        net::CookieSourceScheme::kSecure}) {
    net::CookieSourceScheme roundtrip;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::CookieSourceScheme>(
        source_scheme, roundtrip));
    EXPECT_EQ(source_scheme, roundtrip);
  }
}

TEST(CookieManagerTraitsTest, Roundtrips_CookieChangeCause) {
  for (net::CookieChangeCause change_cause :
       {net::CookieChangeCause::INSERTED, net::CookieChangeCause::EXPLICIT,
        net::CookieChangeCause::UNKNOWN_DELETION,
        net::CookieChangeCause::OVERWRITE, net::CookieChangeCause::EXPIRED,
        net::CookieChangeCause::EVICTED,
        net::CookieChangeCause::EXPIRED_OVERWRITE}) {
    net::CookieChangeCause roundtrip;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::CookieChangeCause>(
        change_cause, roundtrip));
    EXPECT_EQ(change_cause, roundtrip);
  }
}

TEST(CookieManagerTraitsTest,
     Roundtrips_CookieSameSiteContextMetadataDowngradeType) {
  for (auto type : {net::CookieOptions::SameSiteCookieContext::ContextMetadata::
                        ContextDowngradeType::kNoDowngrade,
                    net::CookieOptions::SameSiteCookieContext::ContextMetadata::
                        ContextDowngradeType::kStrictToLax,
                    net::CookieOptions::SameSiteCookieContext::ContextMetadata::
                        ContextDowngradeType::kStrictToCross,
                    net::CookieOptions::SameSiteCookieContext::ContextMetadata::
                        ContextDowngradeType::kLaxToCross}) {
    net::CookieOptions::SameSiteCookieContext::ContextMetadata::
        ContextDowngradeType roundtrip;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
                mojom::CookieSameSiteContextMetadataDowngradeType>(type,
                                                                   roundtrip));
    EXPECT_EQ(type, roundtrip);
  }
}

TEST(CookieManagerTraitsTest, Roundtrips_ContextRedirectTypeBug1221316) {
  for (auto type : {net::CookieOptions::SameSiteCookieContext::ContextMetadata::
                        ContextRedirectTypeBug1221316::kUnset,
                    net::CookieOptions::SameSiteCookieContext::ContextMetadata::
                        ContextRedirectTypeBug1221316::kNoRedirect,
                    net::CookieOptions::SameSiteCookieContext::ContextMetadata::
                        ContextRedirectTypeBug1221316::kCrossSiteRedirect,
                    net::CookieOptions::SameSiteCookieContext::ContextMetadata::
                        ContextRedirectTypeBug1221316::kPartialSameSiteRedirect,
                    net::CookieOptions::SameSiteCookieContext::ContextMetadata::
                        ContextRedirectTypeBug1221316::kAllSameSiteRedirect}) {
    net::CookieOptions::SameSiteCookieContext::ContextMetadata::
        ContextRedirectTypeBug1221316 roundtrip;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
                mojom::ContextRedirectTypeBug1221316>(type, roundtrip));
    EXPECT_EQ(type, roundtrip);
  }
}

TEST(CookieManagerTraitsTest, Roundtrips_CookieSameSiteContextMetadata) {
  net::CookieOptions::SameSiteCookieContext::ContextMetadata metadata,
      roundtrip;

  // Default values.
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::CookieSameSiteContextMetadata>(
          metadata, roundtrip));
  EXPECT_EQ(metadata, roundtrip);

  // Arbitrary values.
  metadata.cross_site_redirect_downgrade =
      net::CookieOptions::SameSiteCookieContext::ContextMetadata::
          ContextDowngradeType::kStrictToLax;
  metadata.redirect_type_bug_1221316 =
      net::CookieOptions::SameSiteCookieContext::ContextMetadata::
          ContextRedirectTypeBug1221316::kPartialSameSiteRedirect;
  metadata.http_method_bug_1221316 = net::CookieOptions::SameSiteCookieContext::
      ContextMetadata::HttpMethod::kPost;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::CookieSameSiteContextMetadata>(
          metadata, roundtrip));
  EXPECT_EQ(metadata, roundtrip);
}

TEST(CookieManagerTraitsTest, Roundtrips_CookieSameSiteContext) {
  using ContextType = net::CookieOptions::SameSiteCookieContext::ContextType;
  using ContextMetadata =
      net::CookieOptions::SameSiteCookieContext::ContextMetadata;

  const ContextType all_context_types[]{
      ContextType::CROSS_SITE, ContextType::SAME_SITE_LAX_METHOD_UNSAFE,
      ContextType::SAME_SITE_LAX, ContextType::SAME_SITE_STRICT};

  ContextMetadata metadata1;
  metadata1.cross_site_redirect_downgrade =
      ContextMetadata::ContextDowngradeType::kStrictToLax;
  metadata1.redirect_type_bug_1221316 =
      ContextMetadata::ContextRedirectTypeBug1221316::kCrossSiteRedirect;
  metadata1.http_method_bug_1221316 = net::CookieOptions::
      SameSiteCookieContext::ContextMetadata::HttpMethod::kGet;
  ContextMetadata metadata2;
  metadata2.cross_site_redirect_downgrade =
      ContextMetadata::ContextDowngradeType::kLaxToCross;
  metadata2.redirect_type_bug_1221316 =
      ContextMetadata::ContextRedirectTypeBug1221316::kNoRedirect;
  metadata2.http_method_bug_1221316 = net::CookieOptions::
      SameSiteCookieContext::ContextMetadata::HttpMethod::kGet;

  const ContextMetadata metadatas[]{ContextMetadata(), metadata1, metadata2};

  for (ContextType context_type : all_context_types) {
    for (ContextType schemeful_context_type : all_context_types) {
      for (const ContextMetadata& metadata : metadatas) {
        for (const ContextMetadata& schemeful_metadata : metadatas) {
          net::CookieOptions::SameSiteCookieContext copy;
          net::CookieOptions::SameSiteCookieContext context_in(
              context_type, context_type, metadata, schemeful_metadata);
          // We want to test malformed SameSiteCookieContexts. Since the
          // constructor will DCHECK for these use this setter to bypass it.
          context_in.SetContextTypesForTesting(context_type,
                                               schemeful_context_type);

          EXPECT_EQ(
              mojo::test::SerializeAndDeserialize<mojom::CookieSameSiteContext>(
                  context_in, copy),
              schemeful_context_type <= context_type);

          if (schemeful_context_type <= context_type)
            EXPECT_TRUE(context_in.CompleteEquivalenceForTesting(copy));
        }
      }
    }
  }
}

TEST(CookieManagerTraitsTest, Roundtrips_PartitionKey) {
  auto original = net::CanonicalCookie::CreateUnsafeCookieForTesting(
      "__Host-A", "B", "x.y", "/", base::Time(), base::Time(), base::Time(),
      base::Time(), true, false, net::CookieSameSite::NO_RESTRICTION,
      net::COOKIE_PRIORITY_LOW,
      net::CookiePartitionKey::FromURLForTesting(
          GURL("https://toplevelsite.com")),
      net::CookieSourceScheme::kSecure, 8433);

  net::CanonicalCookie copied;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::CanonicalCookie>(
      *original, copied));
  EXPECT_EQ(original->PartitionKey(), copied.PartitionKey());
  EXPECT_FALSE(copied.PartitionKey()->from_script());

  original = net::CanonicalCookie::CreateUnsafeCookieForTesting(
      "__Host-A", "B", "x.y", "/", base::Time(), base::Time(), base::Time(),
      base::Time(), true, false, net::CookieSameSite::NO_RESTRICTION,
      net::COOKIE_PRIORITY_LOW, net::CookiePartitionKey::FromScript(),
      net::CookieSourceScheme::kSecure, 8433);
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::CanonicalCookie>(
      *original, copied));
  EXPECT_TRUE(copied.PartitionKey()->from_script());
}

TEST(CookieManagerTraitsTest, Roundtrips_AncestorChainBit) {
  struct {
    net::CookiePartitionKey key;
    bool expected_third_party;
  } cases[]{
      {net::CookiePartitionKey::FromURLForTesting(
           GURL("https://toplevelsite.com"),
           net::CookiePartitionKey::AncestorChainBit::kCrossSite),
       true},
      {net::CookiePartitionKey::FromURLForTesting(
           GURL("https://toplevelsite.com"),
           net::CookiePartitionKey::AncestorChainBit::kSameSite),
       false},
  };

  for (auto tc : cases) {
    net::CookiePartitionKey copied =
        net::CookiePartitionKey::FromURLForTesting(GURL(""));
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::CookiePartitionKey>(
        tc.key, copied));
    // IsThirdParty() is used to access the value of the ancestor chain bit in
    // net::CookiePartitionKey.
    EXPECT_EQ(tc.key.IsThirdParty(), copied.IsThirdParty());
    EXPECT_EQ(tc.key.IsThirdParty(), tc.expected_third_party);
  };
}

TEST(CookieManagerTraitsTest, Roundtrips_CookiePartitionKeyCollection) {
  {
    net::CookiePartitionKeyCollection original;
    net::CookiePartitionKeyCollection copied;

    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
                mojom::CookiePartitionKeyCollection>(original, copied));
    EXPECT_FALSE(copied.ContainsAllKeys());
    EXPECT_EQ(0u, copied.PartitionKeys().size());
  }

  {
    net::CookiePartitionKeyCollection original(
        net::CookiePartitionKey::FromURLForTesting(
            GURL("https://www.example.com")));
    net::CookiePartitionKeyCollection copied;

    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
                mojom::CookiePartitionKeyCollection>(original, copied));
    EXPECT_FALSE(copied.ContainsAllKeys());
    EXPECT_THAT(copied.PartitionKeys(),
                testing::UnorderedElementsAre(
                    net::CookiePartitionKey::FromURLForTesting(
                        GURL("https://www.example.com"))));
  }

  {
    net::CookiePartitionKeyCollection original({
        net::CookiePartitionKey::FromURLForTesting(GURL("https://a.foo.com")),
        net::CookiePartitionKey::FromURLForTesting(GURL("https://b.bar.com")),
    });
    net::CookiePartitionKeyCollection copied;

    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
                mojom::CookiePartitionKeyCollection>(original, copied));
    EXPECT_FALSE(copied.ContainsAllKeys());
    EXPECT_THAT(copied.PartitionKeys(),
                testing::UnorderedElementsAre(
                    net::CookiePartitionKey::FromURLForTesting(
                        GURL("https://b.foo.com")),
                    net::CookiePartitionKey::FromURLForTesting(
                        GURL("https://a.bar.com"))));
  }

  {
    net::CookiePartitionKeyCollection original =
        net::CookiePartitionKeyCollection::ContainsAll();
    net::CookiePartitionKeyCollection copied;

    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
                mojom::CookiePartitionKeyCollection>(original, copied));
    EXPECT_TRUE(copied.ContainsAllKeys());
  }
}

TEST(CookieManagerTraitsTest, Roundtrips_CookieOptions) {
  {
    net::CookieOptions least_trusted, copy;
    EXPECT_FALSE(least_trusted.return_excluded_cookies());
    least_trusted.set_return_excluded_cookies();  // differ from default.

    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::CookieOptions>(
        least_trusted, copy));
    EXPECT_TRUE(copy.exclude_httponly());
    EXPECT_EQ(
        net::CookieOptions::SameSiteCookieContext(
            net::CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE),
        copy.same_site_cookie_context());
    EXPECT_TRUE(copy.return_excluded_cookies());
  }

  {
    net::CookieOptions very_trusted, copy;
    very_trusted.set_include_httponly();
    very_trusted.set_same_site_cookie_context(
        net::CookieOptions::SameSiteCookieContext::MakeInclusive());

    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::CookieOptions>(
        very_trusted, copy));
    EXPECT_FALSE(copy.exclude_httponly());
    EXPECT_EQ(net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
              copy.same_site_cookie_context());
    EXPECT_FALSE(copy.return_excluded_cookies());
  }
}

TEST(CookieManagerTraitsTest, Roundtrips_CookieChangeInfo) {
  auto original_cookie = net::CanonicalCookie::CreateUnsafeCookieForTesting(
      "A", "B", "x.y", "/path", base::Time(), base::Time(), base::Time(),
      base::Time(),
      /*secure=*/false, /*httponly =*/false, net::CookieSameSite::UNSPECIFIED,
      net::COOKIE_PRIORITY_LOW);

  net::CookieChangeInfo original(
      *original_cookie,
      net::CookieAccessResult(net::CookieEffectiveSameSite::UNDEFINED,
                              net::CookieInclusionStatus(),
                              net::CookieAccessSemantics::LEGACY,
                              false /* is_allowed_to_access_secure_cookies */),
      net::CookieChangeCause::EXPLICIT);

  net::CookieChangeInfo copied;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::CookieChangeInfo>(
      original, copied));

  EXPECT_EQ(original.cookie.Name(), copied.cookie.Name());
  EXPECT_EQ(original.cookie.Value(), copied.cookie.Value());
  EXPECT_EQ(original.cookie.Domain(), copied.cookie.Domain());
  EXPECT_EQ(original.cookie.Path(), copied.cookie.Path());
  EXPECT_EQ(original.cookie.CreationDate(), copied.cookie.CreationDate());
  EXPECT_EQ(original.cookie.LastAccessDate(), copied.cookie.LastAccessDate());
  EXPECT_EQ(original.cookie.ExpiryDate(), copied.cookie.ExpiryDate());
  EXPECT_EQ(original.cookie.LastUpdateDate(), copied.cookie.LastUpdateDate());
  EXPECT_EQ(original.cookie.SecureAttribute(), copied.cookie.SecureAttribute());
  EXPECT_EQ(original.cookie.IsHttpOnly(), copied.cookie.IsHttpOnly());
  EXPECT_EQ(original.cookie.SameSite(), copied.cookie.SameSite());
  EXPECT_EQ(original.cookie.Priority(), copied.cookie.Priority());
  EXPECT_EQ(original.cookie.SourceType(), copied.cookie.SourceType());
  EXPECT_EQ(original.access_result.access_semantics,
            copied.access_result.access_semantics);
  EXPECT_EQ(original.cause, copied.cause);
}

TEST(CookieManagerTraitsTest, Roundtrips_HttpMethod) {
  for (auto type : {
           net::CookieOptions::SameSiteCookieContext::ContextMetadata::
               HttpMethod::kUnset,
           net::CookieOptions::SameSiteCookieContext::ContextMetadata::
               HttpMethod::kUnknown,
           net::CookieOptions::SameSiteCookieContext::ContextMetadata::
               HttpMethod::kGet,
           net::CookieOptions::SameSiteCookieContext::ContextMetadata::
               HttpMethod::kHead,
           net::CookieOptions::SameSiteCookieContext::ContextMetadata::
               HttpMethod::kPost,
           net::CookieOptions::SameSiteCookieContext::ContextMetadata::
               HttpMethod::KPut,
           net::CookieOptions::SameSiteCookieContext::ContextMetadata::
               HttpMethod::kDelete,
           net::CookieOptions::SameSiteCookieContext::ContextMetadata::
               HttpMethod::kConnect,
           net::CookieOptions::SameSiteCookieContext::ContextMetadata::
               HttpMethod::kOptions,
           net::CookieOptions::SameSiteCookieContext::ContextMetadata::
               HttpMethod::kTrace,
           net::CookieOptions::SameSiteCookieContext::ContextMetadata::
               HttpMethod::kPatch,
       }) {
    net::CookieOptions::SameSiteCookieContext::ContextMetadata::HttpMethod
        roundtrip;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::HttpMethod>(
        type, roundtrip));
    EXPECT_EQ(type, roundtrip);
  }
}

// TODO: Add tests for CookiePriority, more extensive CookieOptions ones

}  // namespace
}  // namespace network
