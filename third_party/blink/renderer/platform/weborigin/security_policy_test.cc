/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/mojom/cors.mojom-blink.h"
#include "services/network/public/mojom/cors_origin_pattern.mojom-blink.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "url/url_canon.h"
#include "url/url_util.h"

namespace blink {

TEST(SecurityPolicyTest, EmptyReferrerForUnauthorizedScheme) {
  const KURL example_http_url = KURL("http://example.com/");
  EXPECT_TRUE(String() == SecurityPolicy::GenerateReferrer(
                              network::mojom::ReferrerPolicy::kAlways,
                              example_http_url,
                              String::FromUTF8("chrome://somepage/"))
                              .referrer);
}

TEST(SecurityPolicyTest, GenerateReferrerRespectsReferrerSchemesRegistry) {
  const KURL example_http_url = KURL("http://example.com/");
  const String foobar_url = String::FromUTF8("foobar://somepage/");
  const String foobar_scheme = String::FromUTF8("foobar");

  EXPECT_EQ(String(), SecurityPolicy::GenerateReferrer(
                          network::mojom::ReferrerPolicy::kAlways,
                          example_http_url, foobar_url)
                          .referrer);
  SchemeRegistry::RegisterURLSchemeAsAllowedForReferrer(foobar_scheme);
  EXPECT_EQ(foobar_url, SecurityPolicy::GenerateReferrer(
                            network::mojom::ReferrerPolicy::kAlways,
                            example_http_url, foobar_url)
                            .referrer);
  SchemeRegistry::RemoveURLSchemeAsAllowedForReferrer(foobar_scheme);
}

TEST(SecurityPolicyTest, ShouldHideReferrerRespectsReferrerSchemesRegistry) {
  const KURL example_http_url = KURL("http://example.com/");
  const KURL foobar_url = KURL("foobar://somepage/");
  const String foobar_scheme = String::FromUTF8("foobar");

  EXPECT_TRUE(SecurityPolicy::ShouldHideReferrer(example_http_url, foobar_url));
  SchemeRegistry::RegisterURLSchemeAsAllowedForReferrer(foobar_scheme);
  EXPECT_FALSE(
      SecurityPolicy::ShouldHideReferrer(example_http_url, foobar_url));
  SchemeRegistry::RemoveURLSchemeAsAllowedForReferrer(foobar_scheme);
}

TEST(SecurityPolicyTest, GenerateReferrer) {
  struct TestCase {
    network::mojom::ReferrerPolicy policy;
    const char* referrer;
    const char* destination;
    const char* expected;
  };

  const char kInsecureURLA[] = "http://a.test/path/to/file.html";
  const char kInsecureURLB[] = "http://b.test/path/to/file.html";
  const char kInsecureOriginA[] = "http://a.test/";

  const char kSecureURLA[] = "https://a.test/path/to/file.html";
  const char kSecureURLB[] = "https://b.test/path/to/file.html";
  const char kSecureOriginA[] = "https://a.test/";

  const char kBlobURL[] =
      "blob:http://a.test/b3aae9c8-7f90-440d-8d7c-43aa20d72fde";
  const char kFilesystemURL[] = "filesystem:http://a.test/path/t/file.html";
  const char kInvalidURL[] = "not-a-valid-url";
  const char kEmptyURL[] = "";

  bool reduced_granularity =
      base::FeatureList::IsEnabled(features::kReducedReferrerGranularity);

  TestCase inputs[] = {
      // HTTP -> HTTP: Same Origin
      {network::mojom::ReferrerPolicy::kAlways, kInsecureURLA, kInsecureURLA,
       kInsecureURLA},
      {network::mojom::ReferrerPolicy::kDefault, kInsecureURLA, kInsecureURLA,
       kInsecureURLA},
      {network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade, kInsecureURLA,
       kInsecureURLA, kInsecureURLA},
      {network::mojom::ReferrerPolicy::kNever, kInsecureURLA, kInsecureURLA,
       nullptr},
      {network::mojom::ReferrerPolicy::kOrigin, kInsecureURLA, kInsecureURLA,
       kInsecureOriginA},
      {network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin, kInsecureURLA,
       kInsecureURLA, kInsecureURLA},
      {network::mojom::ReferrerPolicy::kSameOrigin, kInsecureURLA,
       kInsecureURLA, kInsecureURLA},
      {network::mojom::ReferrerPolicy::kStrictOrigin, kInsecureURLA,
       kInsecureURLA, kInsecureOriginA},
      {network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin,
       kInsecureURLA, kInsecureURLA, kInsecureURLA},

      // HTTP -> HTTP: Cross Origin
      {network::mojom::ReferrerPolicy::kAlways, kInsecureURLA, kInsecureURLB,
       kInsecureURLA},
      {network::mojom::ReferrerPolicy::kDefault, kInsecureURLA, kInsecureURLB,
       reduced_granularity ? kInsecureOriginA : kInsecureURLA},
      {network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade, kInsecureURLA,
       kInsecureURLB, kInsecureURLA},
      {network::mojom::ReferrerPolicy::kNever, kInsecureURLA, kInsecureURLB,
       nullptr},
      {network::mojom::ReferrerPolicy::kOrigin, kInsecureURLA, kInsecureURLB,
       kInsecureOriginA},
      {network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin, kInsecureURLA,
       kInsecureURLB, kInsecureOriginA},
      {network::mojom::ReferrerPolicy::kSameOrigin, kInsecureURLA,
       kInsecureURLB, nullptr},
      {network::mojom::ReferrerPolicy::kSameOrigin, kInsecureURLB,
       kFilesystemURL, nullptr},
      {network::mojom::ReferrerPolicy::kSameOrigin, kInsecureURLB, kBlobURL,
       nullptr},
      {network::mojom::ReferrerPolicy::kStrictOrigin, kInsecureURLA,
       kInsecureURLB, kInsecureOriginA},
      {network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin,
       kInsecureURLA, kInsecureURLB, kInsecureOriginA},

      // HTTPS -> HTTPS: Same Origin
      {network::mojom::ReferrerPolicy::kAlways, kSecureURLA, kSecureURLA,
       kSecureURLA},
      {network::mojom::ReferrerPolicy::kDefault, kSecureURLA, kSecureURLA,
       kSecureURLA},
      {network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade, kSecureURLA,
       kSecureURLA, kSecureURLA},
      {network::mojom::ReferrerPolicy::kNever, kSecureURLA, kSecureURLA,
       nullptr},
      {network::mojom::ReferrerPolicy::kOrigin, kSecureURLA, kSecureURLA,
       kSecureOriginA},
      {network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin, kSecureURLA,
       kSecureURLA, kSecureURLA},
      {network::mojom::ReferrerPolicy::kSameOrigin, kSecureURLA, kSecureURLA,
       kSecureURLA},
      {network::mojom::ReferrerPolicy::kStrictOrigin, kSecureURLA, kSecureURLA,
       kSecureOriginA},
      {network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin,
       kSecureURLA, kSecureURLA, kSecureURLA},

      // HTTPS -> HTTPS: Cross Origin
      {network::mojom::ReferrerPolicy::kAlways, kSecureURLA, kSecureURLB,
       kSecureURLA},
      {network::mojom::ReferrerPolicy::kDefault, kSecureURLA, kSecureURLB,
       reduced_granularity ? kSecureOriginA : kSecureURLA},
      {network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade, kSecureURLA,
       kSecureURLB, kSecureURLA},
      {network::mojom::ReferrerPolicy::kNever, kSecureURLA, kSecureURLB,
       nullptr},
      {network::mojom::ReferrerPolicy::kOrigin, kSecureURLA, kSecureURLB,
       kSecureOriginA},
      {network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin, kSecureURLA,
       kSecureURLB, kSecureOriginA},
      {network::mojom::ReferrerPolicy::kSameOrigin, kSecureURLA, kSecureURLB,
       nullptr},
      {network::mojom::ReferrerPolicy::kStrictOrigin, kSecureURLA, kSecureURLB,
       kSecureOriginA},
      {network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin,
       kSecureURLA, kSecureURLB, kSecureOriginA},

      // HTTP -> HTTPS
      {network::mojom::ReferrerPolicy::kAlways, kInsecureURLA, kSecureURLB,
       kInsecureURLA},
      {network::mojom::ReferrerPolicy::kDefault, kInsecureURLA, kSecureURLB,
       reduced_granularity ? kInsecureOriginA : kInsecureURLA},
      {network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade, kInsecureURLA,
       kSecureURLB, kInsecureURLA},
      {network::mojom::ReferrerPolicy::kNever, kInsecureURLA, kSecureURLB,
       nullptr},
      {network::mojom::ReferrerPolicy::kOrigin, kInsecureURLA, kSecureURLB,
       kInsecureOriginA},
      {network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin, kInsecureURLA,
       kSecureURLB, kInsecureOriginA},
      {network::mojom::ReferrerPolicy::kSameOrigin, kInsecureURLA, kSecureURLB,
       nullptr},
      {network::mojom::ReferrerPolicy::kStrictOrigin, kInsecureURLA,
       kSecureURLB, kInsecureOriginA},
      {network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin,
       kInsecureURLA, kSecureURLB, kInsecureOriginA},

      // HTTPS -> HTTP
      {network::mojom::ReferrerPolicy::kAlways, kSecureURLA, kInsecureURLB,
       kSecureURLA},
      {network::mojom::ReferrerPolicy::kDefault, kSecureURLA, kInsecureURLB,
       nullptr},
      {network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade, kSecureURLA,
       kInsecureURLB, nullptr},
      {network::mojom::ReferrerPolicy::kNever, kSecureURLA, kInsecureURLB,
       nullptr},
      {network::mojom::ReferrerPolicy::kOrigin, kSecureURLA, kInsecureURLB,
       kSecureOriginA},
      {network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin, kSecureURLA,
       kSecureURLB, kSecureOriginA},
      {network::mojom::ReferrerPolicy::kSameOrigin, kSecureURLA, kInsecureURLB,
       nullptr},
      {network::mojom::ReferrerPolicy::kStrictOrigin, kSecureURLA,
       kInsecureURLB, nullptr},
      {network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin,
       kSecureURLA, kInsecureURLB, nullptr},

      // blob, filesystem, and invalid URL handling
      {network::mojom::ReferrerPolicy::kAlways, kInsecureURLA, kBlobURL,
       kInsecureURLA},
      {network::mojom::ReferrerPolicy::kAlways, kBlobURL, kInsecureURLA,
       nullptr},
      {network::mojom::ReferrerPolicy::kAlways, kInsecureURLA, kFilesystemURL,
       kInsecureURLA},
      {network::mojom::ReferrerPolicy::kAlways, kFilesystemURL, kInsecureURLA,
       nullptr},
      {network::mojom::ReferrerPolicy::kAlways, kInsecureURLA, kInvalidURL,
       kInsecureURLA},
      {network::mojom::ReferrerPolicy::kAlways, kInvalidURL, kInsecureURLA,
       nullptr},
      {network::mojom::ReferrerPolicy::kAlways, kEmptyURL, kInsecureURLA,
       nullptr},
  };

  for (TestCase test : inputs) {
    KURL destination(test.destination);
    Referrer result = SecurityPolicy::GenerateReferrer(
        test.policy, destination, String::FromUTF8(test.referrer));
    if (test.expected) {
      EXPECT_EQ(String::FromUTF8(test.expected), result.referrer)
          << "'" << test.referrer << "' to '" << test.destination
          << "' with policy=" << static_cast<int>(test.policy)
          << " should have been '" << test.expected << "': was '"
          << result.referrer.Utf8() << "'.";
    } else {
      EXPECT_TRUE(result.referrer.empty())
          << "'" << test.referrer << "' to '" << test.destination
          << "' should have been empty: was '" << result.referrer.Utf8()
          << "'.";
    }

    network::mojom::ReferrerPolicy expected_policy = test.policy;
    if (expected_policy == network::mojom::ReferrerPolicy::kDefault) {
      if (reduced_granularity) {
        expected_policy =
            network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin;
      } else {
        expected_policy =
            network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade;
      }
    }
    EXPECT_EQ(expected_policy, result.referrer_policy);
  }
}

TEST(SecurityPolicyTest, GenerateReferrerTruncatesLongUrl) {
  char buffer[4097];
  std::fill_n(std::begin(buffer), 4097, 'a');

  String base = "https://a.com/";
  String string_with_4096 = base + String(buffer, 4096 - base.length());
  ASSERT_EQ(string_with_4096.length(), 4096u);

  network::mojom::ReferrerPolicy kAlways =
      network::mojom::ReferrerPolicy::kAlways;
  EXPECT_EQ(SecurityPolicy::GenerateReferrer(
                kAlways, KURL("https://destination.example"), string_with_4096)
                .referrer,
            string_with_4096);

  String string_with_4097 = base + String(buffer, 4097 - base.length());
  ASSERT_EQ(string_with_4097.length(), 4097u);
  EXPECT_EQ(SecurityPolicy::GenerateReferrer(
                kAlways, KURL("https://destination.example"), string_with_4097)
                .referrer,
            "https://a.com/");

  // Since refs get stripped from outgoing referrers prior to the "if the length
  // is greater than 4096, strip the referrer to its origin" check, a
  // referrer with length > 4096 due to its path should not get stripped to its
  // outgoing origin.
  String string_with_4097_because_of_long_ref =
      base + "path#" + String(buffer, 4097 - 5 - base.length());
  ASSERT_EQ(string_with_4097_because_of_long_ref.length(), 4097u);
  EXPECT_EQ(SecurityPolicy::GenerateReferrer(
                kAlways, KURL("https://destination.example"),
                string_with_4097_because_of_long_ref)
                .referrer,
            "https://a.com/path");
}

TEST(SecurityPolicyTest, ReferrerPolicyFromHeaderValue) {
  struct TestCase {
    const char* header;
    bool is_valid;
    ReferrerPolicyLegacyKeywordsSupport keywords;
    network::mojom::ReferrerPolicy expected_policy;
  };

  TestCase inputs[] = {
      {"origin", true, kDoNotSupportReferrerPolicyLegacyKeywords,
       network::mojom::ReferrerPolicy::kOrigin},
      {"none", true, kSupportReferrerPolicyLegacyKeywords,
       network::mojom::ReferrerPolicy::kNever},
      {"none", false, kDoNotSupportReferrerPolicyLegacyKeywords,
       network::mojom::ReferrerPolicy::kDefault},
      {"foo", false, kDoNotSupportReferrerPolicyLegacyKeywords,
       network::mojom::ReferrerPolicy::kDefault},
      {"origin, foo", true, kDoNotSupportReferrerPolicyLegacyKeywords,
       network::mojom::ReferrerPolicy::kOrigin},
      {"origin, foo-bar", true, kDoNotSupportReferrerPolicyLegacyKeywords,
       network::mojom::ReferrerPolicy::kOrigin},
      {"origin, foo bar", false, kDoNotSupportReferrerPolicyLegacyKeywords,
       network::mojom::ReferrerPolicy::kDefault},
  };

  for (TestCase test : inputs) {
    network::mojom::ReferrerPolicy actual_policy =
        network::mojom::ReferrerPolicy::kDefault;
    EXPECT_EQ(test.is_valid, SecurityPolicy::ReferrerPolicyFromHeaderValue(
                                 test.header, test.keywords, &actual_policy));
    if (test.is_valid)
      EXPECT_EQ(test.expected_policy, actual_policy);
  }
}

TEST(SecurityPolicyTest, TrustworthySafelist) {
  const char* insecure_urls[] = {
      "http://a.test/path/to/file.html", "http://b.test/path/to/file.html",
      "blob:http://c.test/b3aae9c8-7f90-440d-8d7c-43aa20d72fde",
      "filesystem:http://d.test/path/t/file.html",
  };

  for (const char* url : insecure_urls) {
    scoped_refptr<const SecurityOrigin> origin =
        SecurityOrigin::CreateFromString(url);
    EXPECT_FALSE(origin->IsPotentiallyTrustworthy());

    {
      base::test::ScopedCommandLine scoped_command_line;
      base::CommandLine* command_line =
          scoped_command_line.GetProcessCommandLine();
      command_line->AppendSwitchASCII(
          network::switches::kUnsafelyTreatInsecureOriginAsSecure,
          origin->ToString().Latin1());
      network::SecureOriginAllowlist::GetInstance().ResetForTesting();
      EXPECT_TRUE(origin->IsPotentiallyTrustworthy());
    }
  }

  // Tests that adding URLs that have inner-urls to the safelist
  // takes effect on the origins of the inner-urls (and vice versa).
  struct TestCase {
    const char* url;
    const char* another_url_in_origin;
  };
  TestCase insecure_urls_with_inner_origin[] = {
      {"blob:http://e.test/b3aae9c8-7f90-440d-8d7c-43aa20d72fde",
       "http://e.test/foo.html"},
      {"filesystem:http://f.test/path/t/file.html", "http://f.test/bar.html"},
      {"http://g.test/foo.html",
       "blob:http://g.test/b3aae9c8-7f90-440d-8d7c-43aa20d72fde"},
      {"http://h.test/bar.html", "filesystem:http://h.test/path/t/file.html"},
  };
  for (const TestCase& test : insecure_urls_with_inner_origin) {
    // Actually origins of both URLs should be same.
    scoped_refptr<const SecurityOrigin> origin1 =
        SecurityOrigin::CreateFromString(test.url);
    scoped_refptr<const SecurityOrigin> origin2 =
        SecurityOrigin::CreateFromString(test.another_url_in_origin);

    EXPECT_FALSE(origin1->IsPotentiallyTrustworthy());
    EXPECT_FALSE(origin2->IsPotentiallyTrustworthy());
    {
      base::test::ScopedCommandLine scoped_command_line;
      base::CommandLine* command_line =
          scoped_command_line.GetProcessCommandLine();
      command_line->AppendSwitchASCII(
          network::switches::kUnsafelyTreatInsecureOriginAsSecure,
          origin1->ToString().Latin1());
      network::SecureOriginAllowlist::GetInstance().ResetForTesting();
      EXPECT_TRUE(origin1->IsPotentiallyTrustworthy());
      EXPECT_TRUE(origin2->IsPotentiallyTrustworthy());
    }
  }
}

TEST(SecurityPolicyTest, ReferrerPolicyToAndFromString) {
  const char* policies[] = {"no-referrer",
                            "unsafe-url",
                            "origin",
                            "origin-when-cross-origin",
                            "same-origin",
                            "strict-origin",
                            "strict-origin-when-cross-origin",
                            "no-referrer-when-downgrade"};

  for (const char* policy : policies) {
    network::mojom::ReferrerPolicy result =
        network::mojom::ReferrerPolicy::kDefault;
    EXPECT_TRUE(SecurityPolicy::ReferrerPolicyFromString(
        policy, kDoNotSupportReferrerPolicyLegacyKeywords, &result));
    String string_result = SecurityPolicy::ReferrerPolicyAsString(result);
    EXPECT_EQ(string_result, policy);
  }
}

class SecurityPolicyAccessTest : public testing::Test {
 public:
  SecurityPolicyAccessTest() = default;
  SecurityPolicyAccessTest(const SecurityPolicyAccessTest&) = delete;
  SecurityPolicyAccessTest& operator=(const SecurityPolicyAccessTest&) = delete;
  ~SecurityPolicyAccessTest() override = default;

  void SetUp() override {
    https_example_origin_ =
        SecurityOrigin::CreateFromString("https://example.com");
    https_sub_example_origin_ =
        SecurityOrigin::CreateFromString("https://sub.example.com");
    http_example_origin_ =
        SecurityOrigin::CreateFromString("http://example.com");
    https_chromium_origin_ =
        SecurityOrigin::CreateFromString("https://chromium.org");
    https_google_origin_ =
        SecurityOrigin::CreateFromString("https://google.com");
  }

  void TearDown() override { SecurityPolicy::ClearOriginAccessList(); }

  const SecurityOrigin* https_example_origin() const {
    return https_example_origin_.get();
  }
  const SecurityOrigin* https_sub_example_origin() const {
    return https_sub_example_origin_.get();
  }
  const SecurityOrigin* http_example_origin() const {
    return http_example_origin_.get();
  }
  const SecurityOrigin* https_chromium_origin() const {
    return https_chromium_origin_.get();
  }
  const SecurityOrigin* https_google_origin() const {
    return https_google_origin_.get();
  }

 private:
  scoped_refptr<const SecurityOrigin> https_example_origin_;
  scoped_refptr<const SecurityOrigin> https_sub_example_origin_;
  scoped_refptr<const SecurityOrigin> http_example_origin_;
  scoped_refptr<const SecurityOrigin> https_chromium_origin_;
  scoped_refptr<const SecurityOrigin> https_google_origin_;
};

// TODO(toyoshim): Simplify origin access related tests since all we need here
// is to check think wrapper functions to the network::cors::OriginAccessList.
TEST_F(SecurityPolicyAccessTest, IsOriginAccessAllowed) {
  // By default, no access should be allowed.
  EXPECT_FALSE(SecurityPolicy::IsOriginAccessAllowed(https_chromium_origin(),
                                                     https_example_origin()));
  EXPECT_FALSE(SecurityPolicy::IsOriginAccessAllowed(
      https_chromium_origin(), https_sub_example_origin()));
  EXPECT_FALSE(SecurityPolicy::IsOriginAccessAllowed(https_chromium_origin(),
                                                     http_example_origin()));

  // Adding access for https://example.com should work, but should not grant
  // access to subdomains or other schemes.
  SecurityPolicy::AddOriginAccessAllowListEntry(
      *https_chromium_origin(), "https", "example.com",
      /*destination_port=*/0,
      network::mojom::CorsDomainMatchMode::kDisallowSubdomains,
      network::mojom::CorsPortMatchMode::kAllowAnyPort,
      network::mojom::CorsOriginAccessMatchPriority::kDefaultPriority);
  EXPECT_TRUE(SecurityPolicy::IsOriginAccessAllowed(https_chromium_origin(),
                                                    https_example_origin()));
  EXPECT_FALSE(SecurityPolicy::IsOriginAccessAllowed(
      https_chromium_origin(), https_sub_example_origin()));
  EXPECT_FALSE(SecurityPolicy::IsOriginAccessAllowed(https_chromium_origin(),
                                                     http_example_origin()));

  // Clearing the map should revoke all special access.
  SecurityPolicy::ClearOriginAccessList();
  EXPECT_FALSE(SecurityPolicy::IsOriginAccessAllowed(https_chromium_origin(),
                                                     https_example_origin()));
  EXPECT_FALSE(SecurityPolicy::IsOriginAccessAllowed(
      https_chromium_origin(), https_sub_example_origin()));
  EXPECT_FALSE(SecurityPolicy::IsOriginAccessAllowed(https_chromium_origin(),
                                                     http_example_origin()));

  // Adding an entry that matches subdomains should grant access to any
  // subdomains.
  SecurityPolicy::AddOriginAccessAllowListEntry(
      *https_chromium_origin(), "https", "example.com",
      /*destination_port=*/0,
      network::mojom::CorsDomainMatchMode::kAllowSubdomains,
      network::mojom::CorsPortMatchMode::kAllowAnyPort,
      network::mojom::CorsOriginAccessMatchPriority::kDefaultPriority);
  EXPECT_TRUE(SecurityPolicy::IsOriginAccessAllowed(https_chromium_origin(),
                                                    https_example_origin()));
  EXPECT_TRUE(SecurityPolicy::IsOriginAccessAllowed(
      https_chromium_origin(), https_sub_example_origin()));
  EXPECT_FALSE(SecurityPolicy::IsOriginAccessAllowed(https_chromium_origin(),
                                                     http_example_origin()));
}

TEST_F(SecurityPolicyAccessTest, IsOriginAccessAllowedWildCard) {
  // An empty domain that matches subdomains results in matching every domain.
  SecurityPolicy::AddOriginAccessAllowListEntry(
      *https_chromium_origin(), "https", "",
      /*destination_port=*/0,
      network::mojom::CorsDomainMatchMode::kAllowSubdomains,
      network::mojom::CorsPortMatchMode::kAllowAnyPort,
      network::mojom::CorsOriginAccessMatchPriority::kDefaultPriority);
  EXPECT_TRUE(SecurityPolicy::IsOriginAccessAllowed(https_chromium_origin(),
                                                    https_example_origin()));
  EXPECT_TRUE(SecurityPolicy::IsOriginAccessAllowed(https_chromium_origin(),
                                                    https_google_origin()));
  EXPECT_FALSE(SecurityPolicy::IsOriginAccessAllowed(https_chromium_origin(),
                                                     http_example_origin()));
}

TEST_F(SecurityPolicyAccessTest, IsOriginAccessAllowedWithBlockListEntry) {
  // The block list takes priority over the allow list.
  SecurityPolicy::AddOriginAccessAllowListEntry(
      *https_chromium_origin(), "https", "example.com",
      /*destination_port=*/0,
      network::mojom::CorsDomainMatchMode::kAllowSubdomains,
      network::mojom::CorsPortMatchMode::kAllowAnyPort,
      network::mojom::CorsOriginAccessMatchPriority::kDefaultPriority);
  SecurityPolicy::AddOriginAccessBlockListEntry(
      *https_chromium_origin(), "https", "example.com",
      /*destination_port=*/0,
      network::mojom::CorsDomainMatchMode::kDisallowSubdomains,
      network::mojom::CorsPortMatchMode::kAllowAnyPort,
      network::mojom::CorsOriginAccessMatchPriority::kDefaultPriority);

  EXPECT_FALSE(SecurityPolicy::IsOriginAccessAllowed(https_chromium_origin(),
                                                     https_example_origin()));
  EXPECT_TRUE(SecurityPolicy::IsOriginAccessAllowed(
      https_chromium_origin(), https_sub_example_origin()));
}

TEST_F(SecurityPolicyAccessTest,
       IsOriginAccessAllowedWildcardWithBlockListEntry) {
  SecurityPolicy::AddOriginAccessAllowListEntry(
      *https_chromium_origin(), "https", "",
      /*destination_port=*/0,
      network::mojom::CorsDomainMatchMode::kAllowSubdomains,
      network::mojom::CorsPortMatchMode::kAllowAnyPort,
      network::mojom::CorsOriginAccessMatchPriority::kDefaultPriority);
  SecurityPolicy::AddOriginAccessBlockListEntry(
      *https_chromium_origin(), "https", "google.com",
      /*destination_port=*/0,
      network::mojom::CorsDomainMatchMode::kDisallowSubdomains,
      network::mojom::CorsPortMatchMode::kAllowAnyPort,
      network::mojom::CorsOriginAccessMatchPriority::kDefaultPriority);

  EXPECT_TRUE(SecurityPolicy::IsOriginAccessAllowed(https_chromium_origin(),
                                                    https_example_origin()));
  EXPECT_FALSE(SecurityPolicy::IsOriginAccessAllowed(https_chromium_origin(),
                                                     https_google_origin()));
}

TEST_F(SecurityPolicyAccessTest, ClearOriginAccessListForOrigin) {
  SecurityPolicy::AddOriginAccessAllowListEntry(
      *https_chromium_origin(), "https", "example.com",
      /*destination_port=*/0,
      network::mojom::CorsDomainMatchMode::kAllowSubdomains,
      network::mojom::CorsPortMatchMode::kAllowAnyPort,
      network::mojom::CorsOriginAccessMatchPriority::kDefaultPriority);
  SecurityPolicy::AddOriginAccessAllowListEntry(
      *https_chromium_origin(), "https", "google.com",
      /*destination_port=*/0,
      network::mojom::CorsDomainMatchMode::kAllowSubdomains,
      network::mojom::CorsPortMatchMode::kAllowAnyPort,
      network::mojom::CorsOriginAccessMatchPriority::kDefaultPriority);
  SecurityPolicy::AddOriginAccessAllowListEntry(
      *https_example_origin(), "https", "google.com",
      /*destination_port=*/0,
      network::mojom::CorsDomainMatchMode::kAllowSubdomains,
      network::mojom::CorsPortMatchMode::kAllowAnyPort,
      network::mojom::CorsOriginAccessMatchPriority::kDefaultPriority);

  SecurityPolicy::ClearOriginAccessListForOrigin(*https_chromium_origin());

  EXPECT_FALSE(SecurityPolicy::IsOriginAccessAllowed(https_chromium_origin(),
                                                     https_example_origin()));
  EXPECT_FALSE(SecurityPolicy::IsOriginAccessAllowed(https_chromium_origin(),
                                                     https_google_origin()));
  EXPECT_TRUE(SecurityPolicy::IsOriginAccessAllowed(https_example_origin(),
                                                    https_google_origin()));
}

TEST_F(SecurityPolicyAccessTest, IsOriginAccessAllowedPriority) {
  EXPECT_FALSE(SecurityPolicy::IsOriginAccessAllowed(
      https_chromium_origin(), https_sub_example_origin()));
  SecurityPolicy::AddOriginAccessAllowListEntry(
      *https_chromium_origin(), "https", "sub.example.com",
      /*destination_port=*/0,
      network::mojom::CorsDomainMatchMode::kDisallowSubdomains,
      network::mojom::CorsPortMatchMode::kAllowAnyPort,
      network::mojom::CorsOriginAccessMatchPriority::kLowPriority);
  EXPECT_TRUE(SecurityPolicy::IsOriginAccessAllowed(
      https_chromium_origin(), https_sub_example_origin()));
  SecurityPolicy::AddOriginAccessBlockListEntry(
      *https_chromium_origin(), "https", "example.com",
      /*destination_port=*/0,
      network::mojom::CorsDomainMatchMode::kAllowSubdomains,
      network::mojom::CorsPortMatchMode::kAllowAnyPort,
      network::mojom::CorsOriginAccessMatchPriority::kMediumPriority);
  EXPECT_FALSE(SecurityPolicy::IsOriginAccessAllowed(
      https_chromium_origin(), https_sub_example_origin()));
  SecurityPolicy::AddOriginAccessAllowListEntry(
      *https_chromium_origin(), "https", "example.com",
      /*destination_port=*/0,
      network::mojom::CorsDomainMatchMode::kAllowSubdomains,
      network::mojom::CorsPortMatchMode::kAllowAnyPort,
      network::mojom::CorsOriginAccessMatchPriority::kHighPriority);
  EXPECT_TRUE(SecurityPolicy::IsOriginAccessAllowed(
      https_chromium_origin(), https_sub_example_origin()));
}

// Test that referrers for custom hierarchical (standard) schemes are correctly
// handled by the new policy. (For instance, this covers android-app://.)
TEST(SecurityPolicyTest, ReferrerForCustomScheme) {
  url::ScopedSchemeRegistryForTests scoped_registry;
  const char kCustomStandardScheme[] = "my-new-scheme";
  url::AddStandardScheme(kCustomStandardScheme, url::SCHEME_WITH_HOST);
  SchemeRegistry::RegisterURLSchemeAsAllowedForReferrer(kCustomStandardScheme);

  String kFullReferrer = "my-new-scheme://com.foo.me/this-should-be-truncated";
  String kTruncatedReferrer = "my-new-scheme://com.foo.me/";

  // The default policy of strict-origin-when-cross-origin should truncate the
  // referrer.
  EXPECT_EQ(SecurityPolicy::GenerateReferrer(
                network::mojom::ReferrerPolicy::kDefault,
                KURL("https://www.example.com/"), kFullReferrer)
                .referrer,
            kTruncatedReferrer);

  // no-referrer-when-downgrade shouldn't truncate the referrer.
  EXPECT_EQ(SecurityPolicy::GenerateReferrer(
                network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade,
                KURL("https://www.example.com/"), kFullReferrer)
                .referrer,
            kFullReferrer);
}

}  // namespace blink
