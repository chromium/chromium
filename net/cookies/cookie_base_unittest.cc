// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_base.h"

#include <string>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_partition_key.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/third_party/mozilla/url_parse.h"

namespace net {
namespace {

// A subclass of CookieBase to allow access to its protected members. Allows
// customizing the Lax-allow-unsafe threshold age.
class TestCookie : public CookieBase {
 public:
  // Builder interface to allow easier creation, with default values for
  // unspecified fields.
  class Builder {
   public:
    Builder() = default;

    Builder& SetLaxUnsafeAge(base::TimeDelta lax_unsafe_age) {
      lax_unsafe_age_ = lax_unsafe_age;
      return *this;
    }

    Builder& SetName(const std::string& name) {
      name_ = name;
      return *this;
    }

    Builder& SetDomain(const std::string& domain) {
      domain_ = domain;
      return *this;
    }

    Builder& SetPath(const std::string& path) {
      path_ = path;
      return *this;
    }

    Builder& SetCreation(base::Time creation) {
      creation_ = creation;
      return *this;
    }

    Builder& SetSecure(bool secure) {
      secure_ = secure;
      return *this;
    }

    Builder& SetHttpOnly(bool httponly) {
      httponly_ = httponly;
      return *this;
    }

    Builder& SetSameSite(CookieSameSite same_site) {
      same_site_ = same_site;
      return *this;
    }

    Builder& SetPartitionKey(std::optional<CookiePartitionKey> partition_key) {
      partition_key_ = std::move(partition_key);
      return *this;
    }

    Builder& SetSourceScheme(CookieSourceScheme source_scheme) {
      source_scheme_ = source_scheme;
      return *this;
    }

    Builder& SetSourcePort(int source_port) {
      source_port_ = source_port;
      return *this;
    }

    TestCookie Build() {
      return TestCookie(
          lax_unsafe_age_, name_.value_or("name"),
          domain_.value_or("www.example.test"), path_.value_or("/foo"),
          creation_.value_or(base::Time::Now()), secure_.value_or(false),
          httponly_.value_or(false),
          same_site_.value_or(CookieSameSite::UNSPECIFIED), partition_key_,
          source_scheme_.value_or(CookieSourceScheme::kUnset),
          source_port_.value_or(url::PORT_UNSPECIFIED));
    }

   private:
    std::optional<base::TimeDelta> lax_unsafe_age_;
    std::optional<std::string> name_;
    std::optional<std::string> domain_;
    std::optional<std::string> path_;
    std::optional<base::Time> creation_;
    std::optional<bool> secure_;
    std::optional<bool> httponly_;
    std::optional<CookieSameSite> same_site_;
    std::optional<CookiePartitionKey> partition_key_;
    std::optional<CookieSourceScheme> source_scheme_;
    std::optional<int> source_port_;
  };

  CookieEffectiveSameSite GetEffectiveSameSiteForTesting(
      CookieAccessSemantics access_semantics) const {
    return GetEffectiveSameSite(access_semantics);
  }

  bool IsRecentlyCreatedForTesting() const {
    return IsRecentlyCreated(GetLaxAllowUnsafeThresholdAge());
  }

  // CookieBase:
  base::TimeDelta GetLaxAllowUnsafeThresholdAge() const override {
    return lax_unsafe_age_.value_or(
        CookieBase::GetLaxAllowUnsafeThresholdAge());
  }

 private:
  friend class Builder;

  TestCookie(std::optional<base::TimeDelta> lax_unsafe_age,
             std::string name,
             std::string domain,
             std::string path,
             base::Time creation,
             bool secure,
             bool httponly,
             CookieSameSite same_site,
             std::optional<CookiePartitionKey> partition_key,
             CookieSourceScheme source_scheme,
             int source_port)
      : CookieBase(std::move(name),
                   std::move(domain),
                   std::move(path),
                   creation,
                   secure,
                   httponly,
                   same_site,
                   std::move(partition_key),
                   source_scheme,
                   source_port),
        lax_unsafe_age_(lax_unsafe_age) {}

  const std::optional<base::TimeDelta> lax_unsafe_age_;
};

class CookieBaseTest : public ::testing::Test, public WithTaskEnvironment {
 public:
  // Use MOCK_TIME to test the Lax-allow-unsafe age threshold behavior.
  CookieBaseTest()
      : WithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
};

// TODO(crbug.com/324405105): Add tests for other CookieBase functionality.

TEST_F(CookieBaseTest, GetLaxAllowUnsafeThresholdAge) {
  // Create a TestCookie with no override for the Lax-allow-unsafe threshold
  // age. This should just return the base class's value.
  TestCookie c = TestCookie::Builder().Build();

  EXPECT_EQ(c.GetLaxAllowUnsafeThresholdAge(), base::TimeDelta::Min());
}

TEST_F(CookieBaseTest, GetEffectiveSameSite) {
  // Cases whose behavior does not depend on cookie age relative to the
  // threshold.
  const struct {
    CookieSameSite same_site;
    CookieAccessSemantics access_semantics;
    CookieEffectiveSameSite expected_effective_same_site;
  } kCommonTestCases[] = {
      {CookieSameSite::UNSPECIFIED, CookieAccessSemantics::LEGACY,
       CookieEffectiveSameSite::NO_RESTRICTION},
      {CookieSameSite::NO_RESTRICTION, CookieAccessSemantics::NONLEGACY,
       CookieEffectiveSameSite::NO_RESTRICTION},
      {CookieSameSite::NO_RESTRICTION, CookieAccessSemantics::LEGACY,
       CookieEffectiveSameSite::NO_RESTRICTION},
      {CookieSameSite::LAX_MODE, CookieAccessSemantics::NONLEGACY,
       CookieEffectiveSameSite::LAX_MODE},
      {CookieSameSite::LAX_MODE, CookieAccessSemantics::LEGACY,
       CookieEffectiveSameSite::LAX_MODE},
      {CookieSameSite::STRICT_MODE, CookieAccessSemantics::NONLEGACY,
       CookieEffectiveSameSite::STRICT_MODE},
      {CookieSameSite::STRICT_MODE, CookieAccessSemantics::LEGACY,
       CookieEffectiveSameSite::STRICT_MODE},
  };

  for (const auto& test_case : kCommonTestCases) {
    TestCookie c = TestCookie::Builder()
                       .SetLaxUnsafeAge(base::Minutes(1))
                       .SetSameSite(test_case.same_site)
                       .Build();
    EXPECT_EQ(c.GetLaxAllowUnsafeThresholdAge(), base::Minutes(1));
    EXPECT_TRUE(c.IsRecentlyCreatedForTesting());
    EXPECT_EQ(c.GetEffectiveSameSiteForTesting(test_case.access_semantics),
              test_case.expected_effective_same_site);

    // Fast forward time so the cookie is now older than the threshold.
    FastForwardBy(base::Minutes(5));

    EXPECT_EQ(c.GetLaxAllowUnsafeThresholdAge(), base::Minutes(1));
    EXPECT_FALSE(c.IsRecentlyCreatedForTesting());
    EXPECT_EQ(c.GetEffectiveSameSiteForTesting(test_case.access_semantics),
              test_case.expected_effective_same_site);
  }
}

// Test behavior where the effective samesite depends on whether the cookie is
// newer than the Lax-allow-unsafe age threshold.
TEST_F(CookieBaseTest, GetEffectiveSameSiteAgeThreshold) {
  TestCookie c = TestCookie::Builder()
                     .SetLaxUnsafeAge(base::Minutes(1))
                     .SetSameSite(CookieSameSite::UNSPECIFIED)
                     .Build();

  EXPECT_EQ(c.GetLaxAllowUnsafeThresholdAge(), base::Minutes(1));
  EXPECT_TRUE(c.IsRecentlyCreatedForTesting());
  EXPECT_EQ(c.GetEffectiveSameSiteForTesting(CookieAccessSemantics::NONLEGACY),
            CookieEffectiveSameSite::LAX_MODE_ALLOW_UNSAFE);

  // Fast forward time so the cookie is now older than the threshold.
  FastForwardBy(base::Minutes(5));

  EXPECT_FALSE(c.IsRecentlyCreatedForTesting());
  EXPECT_EQ(c.GetEffectiveSameSiteForTesting(CookieAccessSemantics::NONLEGACY),
            CookieEffectiveSameSite::LAX_MODE);
}

}  // namespace
}  // namespace net
