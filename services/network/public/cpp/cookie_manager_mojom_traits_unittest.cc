// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cookie_manager_mojom_traits.h"

#include <set>
#include <vector>

#include "base/test/gtest_util.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/cookie_constants.h"
#include "services/network/public/cpp/cookie_manager_mojom_traits.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/third_party/mozilla/url_parse.h"

namespace network {
namespace {

template <typename MojoType, typename NativeType>
bool SerializeAndDeserializeEnum(NativeType in, NativeType* out) {
  MojoType intermediate = mojo::EnumTraits<MojoType, NativeType>::ToMojom(in);
  return mojo::EnumTraits<MojoType, NativeType>::FromMojom(intermediate, out);
}

TEST(CookieManagerTraitsTest, Roundtrips_CanonicalCookie) {
  net::CanonicalCookie original(
      "A", "B", "x.y", "/path", base::Time(), base::Time(), base::Time(), false,
      false, net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_LOW,
      false, net::CookieSourceScheme::kSecure, 8433);

  net::CanonicalCookie copied;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::CanonicalCookie>(
      &original, &copied));

  EXPECT_EQ(original.Name(), copied.Name());
  EXPECT_EQ(original.Value(), copied.Value());
  EXPECT_EQ(original.Domain(), copied.Domain());
  EXPECT_EQ(original.Path(), copied.Path());
  EXPECT_EQ(original.CreationDate(), copied.CreationDate());
  EXPECT_EQ(original.LastAccessDate(), copied.LastAccessDate());
  EXPECT_EQ(original.ExpiryDate(), copied.ExpiryDate());
  EXPECT_EQ(original.IsSecure(), copied.IsSecure());
  EXPECT_EQ(original.IsHttpOnly(), copied.IsHttpOnly());
  EXPECT_EQ(original.SameSite(), copied.SameSite());
  EXPECT_EQ(original.Priority(), copied.Priority());
  EXPECT_EQ(original.IsSameParty(), copied.IsSameParty());
  EXPECT_EQ(original.SourceScheme(), copied.SourceScheme());
  EXPECT_EQ(original.SourcePort(), copied.SourcePort());

  // Test port edge cases: unspecified.
  net::CanonicalCookie original_unspecified(
      "A", "B", "x.y", "/path", base::Time(), base::Time(), base::Time(), false,
      false, net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_LOW,
      false, net::CookieSourceScheme::kSecure, url::PORT_UNSPECIFIED);
  net::CanonicalCookie copied_unspecified;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::CanonicalCookie>(
      &original_unspecified, &copied_unspecified));

  EXPECT_EQ(original_unspecified.SourcePort(), copied_unspecified.SourcePort());

  // Test port edge cases: invalid.
  net::CanonicalCookie original_invalid(
      "A", "B", "x.y", "/path", base::Time(), base::Time(), base::Time(), false,
      false, net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_LOW,
      false, net::CookieSourceScheme::kSecure, url::PORT_INVALID);
  net::CanonicalCookie copied_invalid;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::CanonicalCookie>(
      &original_invalid, &copied_invalid));

  EXPECT_EQ(original_invalid.SourcePort(), copied_invalid.SourcePort());

  // Serializer returns false if cookie is non-canonical.
  // Example is non-canonical because of newline in name.

  original = net::CanonicalCookie("A\n", "B", "x.y", "/path", base::Time(),
                                  base::Time(), base::Time(), false, false,
                                  net::CookieSameSite::NO_RESTRICTION,
                                  net::COOKIE_PRIORITY_LOW, false);

  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<mojom::CanonicalCookie>(
      &original, &copied));
}

TEST(CookieManagerTraitsTest, Roundtrips_CookieInclusionStatus) {
  // This status + warning combo doesn't really make sense. It's just an
  // arbitrary selection of values to test the serialization/deserialization.
  net::CookieInclusionStatus original =
      net::CookieInclusionStatus::MakeFromReasonsForTesting(
          {net::CookieInclusionStatus::EXCLUDE_SAMESITE_LAX,
           net::CookieInclusionStatus::EXCLUDE_INVALID_PREFIX,
           net::CookieInclusionStatus::EXCLUDE_SECURE_ONLY},
          {net::CookieInclusionStatus::
               WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT,
           net::CookieInclusionStatus::WARN_SAMESITE_NONE_INSECURE,
           net::CookieInclusionStatus::
               WARN_SAMESITE_UNSPECIFIED_LAX_ALLOW_UNSAFE});

  net::CookieInclusionStatus copied;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::CookieInclusionStatus>(
      &original, &copied));
  EXPECT_TRUE(copied.HasExactlyExclusionReasonsForTesting(
      {net::CookieInclusionStatus::EXCLUDE_SAMESITE_LAX,
       net::CookieInclusionStatus::EXCLUDE_INVALID_PREFIX,
       net::CookieInclusionStatus::EXCLUDE_SECURE_ONLY}));
  EXPECT_TRUE(copied.HasExactlyWarningReasonsForTesting(
      {net::CookieInclusionStatus::WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT,
       net::CookieInclusionStatus::WARN_SAMESITE_NONE_INSECURE,
       net::CookieInclusionStatus::
           WARN_SAMESITE_UNSPECIFIED_LAX_ALLOW_UNSAFE}));

  net::CookieInclusionStatus invalid;
  invalid.set_exclusion_reasons(~0u);

  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::CookieInclusionStatus>(
          &invalid, &copied));
}

TEST(CookieManagerTraitsTest, Rountrips_CookieAccessResult) {
  net::CookieAccessResult original = net::CookieAccessResult(
      net::CookieEffectiveSameSite::LAX_MODE,
      net::CookieInclusionStatus(
          net::CookieInclusionStatus::
              EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX,
          net::CookieInclusionStatus::
              WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT),
      net::CookieAccessSemantics::LEGACY);
  net::CookieAccessResult copied;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::CookieAccessResult>(
      &original, &copied));

  EXPECT_EQ(original.effective_same_site, copied.effective_same_site);
  EXPECT_TRUE(copied.status.HasExactlyExclusionReasonsForTesting(
      {net::CookieInclusionStatus::
           EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX}));
  EXPECT_TRUE(copied.status.HasExactlyWarningReasonsForTesting(
      {net::CookieInclusionStatus::
           WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT}));
}

TEST(CookieManagerTraitsTest, Rountrips_CookieWithAccessResult) {
  net::CanonicalCookie original_cookie(
      "A", "B", "x.y", "/path", base::Time(), base::Time(), base::Time(),
      /* secure = */ true, /* http_only = */ false,
      net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_LOW, false);

  net::CookieWithAccessResult original = {original_cookie,
                                          net::CookieAccessResult()};
  net::CookieWithAccessResult copied;

  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::CookieWithAccessResult>(
          &original, &copied));

  EXPECT_EQ(original.cookie.Name(), copied.cookie.Name());
  EXPECT_EQ(original.cookie.Value(), copied.cookie.Value());
  EXPECT_EQ(original.cookie.Domain(), copied.cookie.Domain());
  EXPECT_EQ(original.cookie.Path(), copied.cookie.Path());
  EXPECT_EQ(original.cookie.CreationDate(), copied.cookie.CreationDate());
  EXPECT_EQ(original.cookie.LastAccessDate(), copied.cookie.LastAccessDate());
  EXPECT_EQ(original.cookie.ExpiryDate(), copied.cookie.ExpiryDate());
  EXPECT_EQ(original.cookie.IsSecure(), copied.cookie.IsSecure());
  EXPECT_EQ(original.cookie.IsHttpOnly(), copied.cookie.IsHttpOnly());
  EXPECT_EQ(original.cookie.SameSite(), copied.cookie.SameSite());
  EXPECT_EQ(original.cookie.Priority(), copied.cookie.Priority());
  EXPECT_EQ(original.cookie.IsSameParty(), copied.cookie.IsSameParty());
  EXPECT_EQ(original.access_result.effective_same_site,
            copied.access_result.effective_same_site);
  EXPECT_EQ(original.access_result.status, copied.access_result.status);
}

TEST(CookieManagerTraitsTest, Rountrips_CookieAndLineWithAccessResult) {
  net::CanonicalCookie original_cookie(
      "A", "B", "x.y", "/path", base::Time(), base::Time(), base::Time(),
      /* secure = */ true, /* http_only = */ false,
      net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_LOW, false);

  net::CookieAndLineWithAccessResult original(original_cookie, "cookie-string",
                                              net::CookieAccessResult());
  net::CookieAndLineWithAccessResult copied;

  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::CookieAndLineWithAccessResult>(
          &original, &copied));

  EXPECT_EQ(original.cookie->Name(), copied.cookie->Name());
  EXPECT_EQ(original.cookie->Value(), copied.cookie->Value());
  EXPECT_EQ(original.cookie->Domain(), copied.cookie->Domain());
  EXPECT_EQ(original.cookie->Path(), copied.cookie->Path());
  EXPECT_EQ(original.cookie->CreationDate(), copied.cookie->CreationDate());
  EXPECT_EQ(original.cookie->LastAccessDate(), copied.cookie->LastAccessDate());
  EXPECT_EQ(original.cookie->ExpiryDate(), copied.cookie->ExpiryDate());
  EXPECT_EQ(original.cookie->IsSecure(), copied.cookie->IsSecure());
  EXPECT_EQ(original.cookie->IsHttpOnly(), copied.cookie->IsHttpOnly());
  EXPECT_EQ(original.cookie->SameSite(), copied.cookie->SameSite());
  EXPECT_EQ(original.cookie->Priority(), copied.cookie->Priority());
  EXPECT_EQ(original.access_result.effective_same_site,
            copied.access_result.effective_same_site);
  EXPECT_EQ(original.cookie_string, copied.cookie_string);
}

TEST(CookieManagerTraitsTest, Roundtrips_CookieSameSite) {
  for (net::CookieSameSite cookie_state :
       {net::CookieSameSite::NO_RESTRICTION, net::CookieSameSite::LAX_MODE,
        net::CookieSameSite::STRICT_MODE, net::CookieSameSite::UNSPECIFIED}) {
    net::CookieSameSite roundtrip;
    ASSERT_TRUE(SerializeAndDeserializeEnum<mojom::CookieSameSite>(cookie_state,
                                                                   &roundtrip));
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
    ASSERT_TRUE(SerializeAndDeserializeEnum<mojom::CookieEffectiveSameSite>(
        cookie_state, &roundtrip));
    EXPECT_EQ(cookie_state, roundtrip);
  }
}

TEST(CookieManagerTraitsTest, Roundtrips_ContextType) {
  using ContextType = net::CookieOptions::SameSiteCookieContext::ContextType;
  for (ContextType context_type :
       {ContextType::CROSS_SITE, ContextType::SAME_SITE_LAX_METHOD_UNSAFE,
        ContextType::SAME_SITE_LAX, ContextType::SAME_SITE_STRICT}) {
    ContextType roundtrip;
    ASSERT_TRUE(SerializeAndDeserializeEnum<mojom::ContextType>(context_type,
                                                                &roundtrip));
    EXPECT_EQ(context_type, roundtrip);
  }
}

TEST(CookieManagerTraitsTest, Roundtrips_CookieAccessSemantics) {
  for (net::CookieAccessSemantics access_semantics :
       {net::CookieAccessSemantics::UNKNOWN,
        net::CookieAccessSemantics::NONLEGACY,
        net::CookieAccessSemantics::LEGACY}) {
    net::CookieAccessSemantics roundtrip;
    ASSERT_TRUE(SerializeAndDeserializeEnum<mojom::CookieAccessSemantics>(
        access_semantics, &roundtrip));
    EXPECT_EQ(access_semantics, roundtrip);
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
    ASSERT_TRUE(SerializeAndDeserializeEnum<mojom::CookieChangeCause>(
        change_cause, &roundtrip));
    EXPECT_EQ(change_cause, roundtrip);
  }
}

TEST(CookieManagerTraitsTest, Roundtrips_CookieSameSiteContext) {
  using ContextType = net::CookieOptions::SameSiteCookieContext::ContextType;

  const ContextType all_context_types[]{
      ContextType::CROSS_SITE, ContextType::SAME_SITE_LAX_METHOD_UNSAFE,
      ContextType::SAME_SITE_LAX, ContextType::SAME_SITE_STRICT};

  for (ContextType context_type : all_context_types) {
    for (ContextType schemeful_context_type : all_context_types) {
      net::CookieOptions::SameSiteCookieContext context_in, copy;
      // We want to test malformed SameSiteCookieContexts. Since the constructor
      // will DCHECK for these use the setters to bypass it.
      context_in.set_context(context_type);
      context_in.set_schemeful_context(schemeful_context_type);

      EXPECT_EQ(
          mojo::test::SerializeAndDeserialize<mojom::CookieSameSiteContext>(
              &context_in, &copy),
          schemeful_context_type <= context_type);

      if (schemeful_context_type <= context_type)
        EXPECT_EQ(context_in, copy);
    }
  }
}

TEST(CookieManagerTraitsTest, Roundtrips_CookieOptions) {
  {
    net::CookieOptions least_trusted, copy;
    EXPECT_FALSE(least_trusted.return_excluded_cookies());

    least_trusted.set_return_excluded_cookies();  // differ from default.

    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::CookieOptions>(
        &least_trusted, &copy));
    EXPECT_TRUE(copy.exclude_httponly());
    EXPECT_EQ(
        net::CookieOptions::SameSiteCookieContext(
            net::CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE),
        copy.same_site_cookie_context());
    EXPECT_TRUE(copy.return_excluded_cookies());
  }

  {
    net::CookieOptions very_trusted, copy;
    auto kPartyContext = std::set<net::SchemefulSite>{
        net::SchemefulSite(url::Origin::Create(GURL("https://a.test")))};
    very_trusted.set_include_httponly();
    very_trusted.set_same_site_cookie_context(
        net::CookieOptions::SameSiteCookieContext::MakeInclusive());
    very_trusted.set_full_party_context(kPartyContext);

    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::CookieOptions>(
        &very_trusted, &copy));
    EXPECT_FALSE(copy.exclude_httponly());
    EXPECT_EQ(net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
              copy.same_site_cookie_context());
    EXPECT_FALSE(copy.return_excluded_cookies());
    EXPECT_EQ(kPartyContext, copy.full_party_context());
  }
}

TEST(CookieManagerTraitsTest, Roundtrips_FullPartyContext) {
  {
    std::vector<std::set<net::SchemefulSite>> kTestCases = {
        std::set<net::SchemefulSite>(),
        std::set<net::SchemefulSite>{net::SchemefulSite()},
        std::set<net::SchemefulSite>{
            net::SchemefulSite(url::Origin::Create(GURL("https://a.test")))},
        std::set<net::SchemefulSite>{
            net::SchemefulSite(url::Origin::Create(GURL("http://a.test"))),
            net::SchemefulSite(url::Origin::Create(GURL("http://b.test")))},
    };

    for (const std::set<net::SchemefulSite>& fpc : kTestCases) {
      net::CookieOptions options, copy;
      options.set_full_party_context(fpc);
      EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::CookieOptions>(
          &options, &copy));
      EXPECT_EQ(fpc, copy.full_party_context());
    }
  }
  {
    base::Optional<std::set<net::SchemefulSite>> kFullPartyContext =
        base::nullopt;
    net::CookieOptions options, copy;
    options.set_full_party_context(kFullPartyContext);
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::CookieOptions>(
        &options, &copy));
    EXPECT_EQ(kFullPartyContext, copy.full_party_context());
  }
}

TEST(CookieManagerTraitsTest, Roundtrips_CookieChangeInfo) {
  net::CanonicalCookie original_cookie(
      "A", "B", "x.y", "/path", base::Time(), base::Time(), base::Time(),
      /* secure = */ false, /* http_only = */ false,
      net::CookieSameSite::UNSPECIFIED, net::COOKIE_PRIORITY_LOW, false);

  net::CookieChangeInfo original(
      original_cookie,
      net::CookieAccessResult(net::CookieEffectiveSameSite::UNDEFINED,
                              net::CookieInclusionStatus(),
                              net::CookieAccessSemantics::LEGACY),
      net::CookieChangeCause::EXPLICIT);

  net::CookieChangeInfo copied;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::CookieChangeInfo>(
      &original, &copied));

  EXPECT_EQ(original.cookie.Name(), copied.cookie.Name());
  EXPECT_EQ(original.cookie.Value(), copied.cookie.Value());
  EXPECT_EQ(original.cookie.Domain(), copied.cookie.Domain());
  EXPECT_EQ(original.cookie.Path(), copied.cookie.Path());
  EXPECT_EQ(original.cookie.CreationDate(), copied.cookie.CreationDate());
  EXPECT_EQ(original.cookie.LastAccessDate(), copied.cookie.LastAccessDate());
  EXPECT_EQ(original.cookie.ExpiryDate(), copied.cookie.ExpiryDate());
  EXPECT_EQ(original.cookie.IsSecure(), copied.cookie.IsSecure());
  EXPECT_EQ(original.cookie.IsHttpOnly(), copied.cookie.IsHttpOnly());
  EXPECT_EQ(original.cookie.SameSite(), copied.cookie.SameSite());
  EXPECT_EQ(original.cookie.Priority(), copied.cookie.Priority());
  EXPECT_EQ(original.access_result.access_semantics,
            copied.access_result.access_semantics);
  EXPECT_EQ(original.cause, copied.cause);
}

// TODO: Add tests for CookiePriority, more extensive CookieOptions ones

}  // namespace
}  // namespace network
