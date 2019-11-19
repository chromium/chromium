// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cookie_manager_mojom_traits.h"

#include <vector>

#include "base/test/gtest_util.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/public/cpp/cookie_manager_mojom_traits.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

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
      false, net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_LOW);

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

  // Serializer returns false if cookie is non-canonical.
  // Example is non-canonical because of newline in name.

  original = net::CanonicalCookie("A\n", "B", "x.y", "/path", base::Time(),
                                  base::Time(), base::Time(), false, false,
                                  net::CookieSameSite::NO_RESTRICTION,
                                  net::COOKIE_PRIORITY_LOW);

  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<mojom::CanonicalCookie>(
      &original, &copied));
}

TEST(CookieManagerTraitsTest, Roundtrips_CookieInclusionStatus) {
  // This status + warning combo doesn't really make sense. It's just an
  // arbitrary selection of values to test the serialization/deserialization.
  net::CanonicalCookie::CookieInclusionStatus original =
      net::CanonicalCookie::CookieInclusionStatus::MakeFromReasonsForTesting(
          {net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_SAMESITE_LAX,
           net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_INVALID_PREFIX,
           net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_SECURE_ONLY},
          net::CanonicalCookie::CookieInclusionStatus::
              WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT);

  net::CanonicalCookie::CookieInclusionStatus copied;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::CookieInclusionStatus>(
      &original, &copied));
  EXPECT_TRUE(copied.HasExactlyExclusionReasonsForTesting(
      {net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_SAMESITE_LAX,
       net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_INVALID_PREFIX,
       net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_SECURE_ONLY}));
  EXPECT_EQ(net::CanonicalCookie::CookieInclusionStatus::
                WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT,
            copied.warning());

  net::CanonicalCookie::CookieInclusionStatus invalid;
  invalid.set_exclusion_reasons(~0u);

  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::CookieInclusionStatus>(
          &invalid, &copied));
}

TEST(CookieManagerTraitsTest, Roundtrips_CookieWithStatus) {
  net::CanonicalCookie original_cookie(
      "A", "B", "x.y", "/path", base::Time(), base::Time(), base::Time(),
      /* secure = */ true, /* http_only = */ false,
      net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_LOW);

  net::CookieWithStatus original = {
      original_cookie, net::CanonicalCookie::CookieInclusionStatus()};

  net::CookieWithStatus copied;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::CookieWithStatus>(
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
  EXPECT_EQ(original.status, copied.status);
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
  for (net::CookieOptions::SameSiteCookieContext context_state :
       {net::CookieOptions::SameSiteCookieContext::CROSS_SITE,
        net::CookieOptions::SameSiteCookieContext::SAME_SITE_LAX,
        net::CookieOptions::SameSiteCookieContext::SAME_SITE_LAX_METHOD_UNSAFE,
        net::CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT,
        net::CookieOptions::SameSiteCookieContext::
            SAME_SITE_LAX_METHOD_UNSAFE_CROSS_SCHEME_SECURE_URL,
        net::CookieOptions::SameSiteCookieContext::
            SAME_SITE_LAX_CROSS_SCHEME_SECURE_URL,
        net::CookieOptions::SameSiteCookieContext::
            SAME_SITE_STRICT_CROSS_SCHEME_SECURE_URL,
        net::CookieOptions::SameSiteCookieContext::
            SAME_SITE_LAX_METHOD_UNSAFE_CROSS_SCHEME_INSECURE_URL,
        net::CookieOptions::SameSiteCookieContext::
            SAME_SITE_LAX_CROSS_SCHEME_INSECURE_URL,
        net::CookieOptions::SameSiteCookieContext::
            SAME_SITE_STRICT_CROSS_SCHEME_INSECURE_URL}) {
    net::CookieOptions::SameSiteCookieContext roundtrip;
    ASSERT_TRUE(SerializeAndDeserializeEnum<mojom::CookieSameSiteContext>(
        context_state, &roundtrip));
    EXPECT_EQ(context_state, roundtrip);
  }
}

TEST(CookieManagerTraitsTest, Roundtrips_CookieInclusionStatusWarningReason) {
  for (net::CanonicalCookie::CookieInclusionStatus::WarningReason warning :
       {net::CanonicalCookie::CookieInclusionStatus::WarningReason::DO_NOT_WARN,
        net::CanonicalCookie::CookieInclusionStatus::WarningReason::
            WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT,
        net::CanonicalCookie::CookieInclusionStatus::WarningReason::
            WARN_SAMESITE_NONE_INSECURE,
        net::CanonicalCookie::CookieInclusionStatus::WarningReason::
            WARN_SAMESITE_UNSPECIFIED_LAX_ALLOW_UNSAFE}) {
    net::CanonicalCookie::CookieInclusionStatus::WarningReason roundtrip;
    ASSERT_TRUE(
        SerializeAndDeserializeEnum<mojom::CookieInclusionStatusWarningReason>(
            warning, &roundtrip));
    EXPECT_EQ(warning, roundtrip);
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
    EXPECT_EQ(net::CookieOptions::SameSiteCookieContext::CROSS_SITE,
              copy.same_site_cookie_context());
    EXPECT_TRUE(copy.return_excluded_cookies());
  }

  {
    net::CookieOptions very_trusted, copy;
    very_trusted.set_include_httponly();
    very_trusted.set_same_site_cookie_context(
        net::CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT);

    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::CookieOptions>(
        &very_trusted, &copy));
    EXPECT_FALSE(copy.exclude_httponly());
    EXPECT_EQ(net::CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT,
              copy.same_site_cookie_context());
    EXPECT_FALSE(copy.return_excluded_cookies());
  }
}

TEST(CookieManagerTraitsTest, Roundtrips_CookieChangeInfo) {
  net::CanonicalCookie original_cookie(
      "A", "B", "x.y", "/path", base::Time(), base::Time(), base::Time(),
      /* secure = */ false, /* http_only = */ false,
      net::CookieSameSite::UNSPECIFIED, net::COOKIE_PRIORITY_LOW);

  net::CookieChangeInfo original(original_cookie,
                                 net::CookieAccessSemantics::LEGACY,
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
  EXPECT_EQ(original.access_semantics, copied.access_semantics);
  EXPECT_EQ(original.cause, copied.cause);
}

// TODO: Add tests for CookiePriority, more extensive CookieOptions ones

}  // namespace
}  // namespace network
