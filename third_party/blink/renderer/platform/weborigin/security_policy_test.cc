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

#include "services/network/public/mojom/cors.mojom-blink.h"
#include "services/network/public/mojom/cors_origin_pattern.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

TEST(SecurityPolicyTest, EmptyReferrerForUnauthorizedScheme) {
  const KURL example_http_url = KURL("http://example.com/");
  const String chrome_url = String::FromUTF8("chrome://somepage/");
  scoped_refptr<const SecurityOrigin> origin =
      SecurityOrigin::CreateFromString(chrome_url);
  EXPECT_TRUE(String() == SecurityPolicy::GenerateReferrer(
                              network::mojom::ReferrerPolicy::kAlways, origin,
                              example_http_url, chrome_url)
                              .referrer);
}

TEST(SecurityPolicyTest, GenerateReferrerRespectsReferrerSchemesRegistry) {
  const KURL example_http_url = KURL("http://example.com/");
  const String foobar_url = String::FromUTF8("foobar://somepage/");
  const String foobar_scheme = String::FromUTF8("foobar");
  scoped_refptr<const SecurityOrigin> origin =
      SecurityOrigin::CreateFromString(foobar_url);

  EXPECT_EQ(String(), SecurityPolicy::GenerateReferrer(
                          network::mojom::ReferrerPolicy::kAlways, origin,
                          example_http_url, foobar_url)
                          .referrer);
  SchemeRegistry::RegisterURLSchemeAsAllowedForReferrer(foobar_scheme);
  EXPECT_EQ(foobar_url, SecurityPolicy::GenerateReferrer(
                            network::mojom::ReferrerPolicy::kAlways, origin,
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
    scoped_refptr<const SecurityOrigin> origin;
    const char* destination;
    const char* expected;
  };

  const char kInsecureURLA[] = "http://a.test/path/to/file.html";
  const char kInsecureURLB[] = "http://b.test/path/to/file.html";
  const char kInsecureOriginA[] = "http://a.test/";
  scoped_refptr<const SecurityOrigin> insecure_origin_a =
      SecurityOrigin::CreateFromString(kInsecureOriginA);

  const char kSecureURLA[] = "https://a.test/path/to/file.html";
  const char kSecureURLB[] = "https://b.test/path/to/file.html";
  const char kSecureOriginA[] = "https://a.test/";
  scoped_refptr<const SecurityOrigin> secure_origin_a =
      SecurityOrigin::CreateFromString(kSecureOriginA);
  scoped_refptr<const SecurityOrigin> cross_origin =
      SecurityOrigin::CreateUniqueOpaque();

  const char kBlobURL[] =
      "blob:http://a.test/b3aae9c8-7f90-440d-8d7c-43aa20d72fde";
  const char kFilesystemURL[] = "filesystem:http://a.test/path/t/file.html";

  TestCase inputs[] = {
      // HTTP -> HTTP: Same Origin
      {network::mojom::ReferrerPolicy::kAlways, kInsecureURLA,
       insecure_origin_a, kInsecureURLA, kInsecureURLA},
      {network::mojom::ReferrerPolicy::kDefault, kInsecureURLA,
       insecure_origin_a, kInsecureURLA, kInsecureURLA},
      {network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade, kInsecureURLA,
       insecure_origin_a, kInsecureURLA, kInsecureURLA},
      {network::mojom::ReferrerPolicy::kNever, kInsecureURLA, insecure_origin_a,
       kInsecureURLA, nullptr},
      {network::mojom::ReferrerPolicy::kOrigin, kInsecureURLA,
       insecure_origin_a, kInsecureURLA, kInsecureOriginA},
      {network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin, kInsecureURLA,
       insecure_origin_a, kInsecureURLA, kInsecureURLA},
      {network::mojom::ReferrerPolicy::kSameOrigin, kInsecureURLA,
       insecure_origin_a, kInsecureURLA, kInsecureURLA},
      {network::mojom::ReferrerPolicy::kStrictOrigin, kInsecureURLA,
       insecure_origin_a, kInsecureURLA, kInsecureOriginA},
      {network::mojom::ReferrerPolicy::
           kNoReferrerWhenDowngradeOriginWhenCrossOrigin,
       kInsecureURLA, insecure_origin_a, kInsecureURLA, kInsecureURLA},

      // HTTP -> HTTP: Cross Origin
      {network::mojom::ReferrerPolicy::kAlways, kInsecureURLA,
       insecure_origin_a, kInsecureURLB, kInsecureURLA},
      {network::mojom::ReferrerPolicy::kDefault, kInsecureURLA,
       insecure_origin_a, kInsecureURLB, kInsecureURLA},
      {network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade, kInsecureURLA,
       insecure_origin_a, kInsecureURLB, kInsecureURLA},
      {network::mojom::ReferrerPolicy::kNever, kInsecureURLA, insecure_origin_a,
       kInsecureURLB, nullptr},
      {network::mojom::ReferrerPolicy::kOrigin, kInsecureURLA,
       insecure_origin_a, kInsecureURLB, kInsecureOriginA},
      {network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin, kInsecureURLA,
       insecure_origin_a, kInsecureURLB, kInsecureOriginA},
      {network::mojom::ReferrerPolicy::kSameOrigin, kInsecureURLA,
       insecure_origin_a, kInsecureURLB, nullptr},
      {network::mojom::ReferrerPolicy::kStrictOrigin, kInsecureURLA,
       insecure_origin_a, kInsecureURLB, kInsecureOriginA},
      {network::mojom::ReferrerPolicy::
           kNoReferrerWhenDowngradeOriginWhenCrossOrigin,
       kInsecureURLA, insecure_origin_a, kInsecureURLB, kInsecureOriginA},

      // HTTPS -> HTTPS: Same Origin
      {network::mojom::ReferrerPolicy::kAlways, kSecureURLA, secure_origin_a,
       kSecureURLA, kSecureURLA},
      {network::mojom::ReferrerPolicy::kDefault, kSecureURLA, secure_origin_a,
       kSecureURLA, kSecureURLA},
      {network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade, kSecureURLA,
       secure_origin_a, kSecureURLA, kSecureURLA},
      {network::mojom::ReferrerPolicy::kNever, kSecureURLA, secure_origin_a,
       kSecureURLA, nullptr},
      {network::mojom::ReferrerPolicy::kOrigin, kSecureURLA, secure_origin_a,
       kSecureURLA, kSecureOriginA},
      {network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin, kSecureURLA,
       secure_origin_a, kSecureURLA, kSecureURLA},
      {network::mojom::ReferrerPolicy::kSameOrigin, kSecureURLA,
       secure_origin_a, kSecureURLA, kSecureURLA},
      {network::mojom::ReferrerPolicy::kStrictOrigin, kSecureURLA,
       secure_origin_a, kSecureURLA, kSecureOriginA},
      {network::mojom::ReferrerPolicy::
           kNoReferrerWhenDowngradeOriginWhenCrossOrigin,
       kSecureURLA, secure_origin_a, kSecureURLA, kSecureURLA},

      // HTTPS -> HTTPS: Cross Origin
      {network::mojom::ReferrerPolicy::kAlways, kSecureURLA, secure_origin_a,
       kSecureURLB, kSecureURLA},
      {network::mojom::ReferrerPolicy::kDefault, kSecureURLA, secure_origin_a,
       kSecureURLB, kSecureURLA},
      {network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade, kSecureURLA,
       secure_origin_a, kSecureURLB, kSecureURLA},
      {network::mojom::ReferrerPolicy::kNever, kSecureURLA, secure_origin_a,
       kSecureURLB, nullptr},
      {network::mojom::ReferrerPolicy::kOrigin, kSecureURLA, secure_origin_a,
       kSecureURLB, kSecureOriginA},
      {network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin, kSecureURLA,
       secure_origin_a, kSecureURLB, kSecureOriginA},
      {network::mojom::ReferrerPolicy::kSameOrigin, kSecureURLA,
       secure_origin_a, kSecureURLB, nullptr},
      {network::mojom::ReferrerPolicy::kStrictOrigin, kSecureURLA,
       secure_origin_a, kSecureURLB, kSecureOriginA},
      {network::mojom::ReferrerPolicy::
           kNoReferrerWhenDowngradeOriginWhenCrossOrigin,
       kSecureURLA, secure_origin_a, kSecureURLB, kSecureOriginA},

      // HTTP -> HTTPS
      {network::mojom::ReferrerPolicy::kAlways, kInsecureURLA,
       insecure_origin_a, kSecureURLB, kInsecureURLA},
      {network::mojom::ReferrerPolicy::kDefault, kInsecureURLA,
       insecure_origin_a, kSecureURLB, kInsecureURLA},
      {network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade, kInsecureURLA,
       insecure_origin_a, kSecureURLB, kInsecureURLA},
      {network::mojom::ReferrerPolicy::kNever, kInsecureURLA, insecure_origin_a,
       kSecureURLB, nullptr},
      {network::mojom::ReferrerPolicy::kOrigin, kInsecureURLA,
       insecure_origin_a, kSecureURLB, kInsecureOriginA},
      {network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin, kInsecureURLA,
       insecure_origin_a, kSecureURLB, kInsecureOriginA},
      {network::mojom::ReferrerPolicy::kSameOrigin, kInsecureURLA,
       insecure_origin_a, kSecureURLB, nullptr},
      {network::mojom::ReferrerPolicy::kStrictOrigin, kInsecureURLA,
       insecure_origin_a, kSecureURLB, kInsecureOriginA},
      {network::mojom::ReferrerPolicy::
           kNoReferrerWhenDowngradeOriginWhenCrossOrigin,
       kInsecureURLA, insecure_origin_a, kSecureURLB, kInsecureOriginA},

      // HTTPS -> HTTP
      {network::mojom::ReferrerPolicy::kAlways, kSecureURLA, secure_origin_a,
       kInsecureURLB, kSecureURLA},
      {network::mojom::ReferrerPolicy::kDefault, kSecureURLA, secure_origin_a,
       kInsecureURLB, nullptr},
      {network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade, kSecureURLA,
       secure_origin_a, kInsecureURLB, nullptr},
      {network::mojom::ReferrerPolicy::kNever, kSecureURLA, secure_origin_a,
       kInsecureURLB, nullptr},
      {network::mojom::ReferrerPolicy::kOrigin, kSecureURLA, secure_origin_a,
       kInsecureURLB, kSecureOriginA},
      {network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin, kSecureURLA,
       secure_origin_a, kSecureURLB, kSecureOriginA},
      {network::mojom::ReferrerPolicy::kSameOrigin, kSecureURLA,
       secure_origin_a, kInsecureURLB, nullptr},
      {network::mojom::ReferrerPolicy::kStrictOrigin, kSecureURLA,
       secure_origin_a, kInsecureURLB, nullptr},
      {network::mojom::ReferrerPolicy::
           kNoReferrerWhenDowngradeOriginWhenCrossOrigin,
       kSecureURLA, secure_origin_a, kInsecureURLB, nullptr},

      // blob and filesystem URL handling
      {network::mojom::ReferrerPolicy::kAlways, kInsecureURLA,
       insecure_origin_a, kBlobURL, nullptr},
      {network::mojom::ReferrerPolicy::kAlways, kBlobURL,
       SecurityOrigin::CreateFromString(kBlobURL), kInsecureURLA, nullptr},
      {network::mojom::ReferrerPolicy::kAlways, kInsecureURLA,
       insecure_origin_a, kFilesystemURL, nullptr},
      {network::mojom::ReferrerPolicy::kAlways, kFilesystemURL,
       SecurityOrigin::CreateFromString(kFilesystemURL), kInsecureURLA,
       nullptr},

      // Request's origin is cross-origin with referrer URL.
      {network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin, kSecureURLA,
       cross_origin, kSecureURLA, kSecureOriginA},
      {network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin, kSecureURLA,
       cross_origin, kSecureURLB, kSecureOriginA},
      {network::mojom::ReferrerPolicy::kSameOrigin, kSecureURLA, cross_origin,
       kSecureURLA, nullptr},
      {network::mojom::ReferrerPolicy::kSameOrigin, kSecureURLA, cross_origin,
       kSecureURLB, nullptr},
  };

  for (TestCase test : inputs) {
    KURL destination(test.destination);
    String referrer_string = String::FromUTF8(test.referrer);
    scoped_refptr<const SecurityOrigin> origin = test.origin;
    Referrer result = SecurityPolicy::GenerateReferrer(
        test.policy, origin, destination, referrer_string);
    if (test.expected) {
      EXPECT_EQ(String::FromUTF8(test.expected), result.referrer)
          << "'" << test.referrer << "' to '" << test.destination
          << "' should have been '" << test.expected << "': was '"
          << result.referrer.Utf8() << "'.";
    } else {
      EXPECT_TRUE(result.referrer.IsEmpty())
          << "'" << test.referrer << "' to '" << test.destination
          << "' should have been empty: was '" << result.referrer.Utf8()
          << "'.";
    }
    EXPECT_EQ(test.policy == network::mojom::ReferrerPolicy::kDefault
                  ? network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade
                  : test.policy,
              result.referrer_policy);
  }
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
    SecurityPolicy::AddOriginToTrustworthySafelist(origin->ToString());
    EXPECT_TRUE(origin->IsPotentiallyTrustworthy());
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
    SecurityPolicy::AddOriginToTrustworthySafelist(origin1->ToString());
    EXPECT_TRUE(origin1->IsPotentiallyTrustworthy());
    EXPECT_TRUE(origin2->IsPotentiallyTrustworthy());
  }
}

class SecurityPolicyAccessTest : public testing::Test {
 public:
  SecurityPolicyAccessTest() = default;
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

  DISALLOW_COPY_AND_ASSIGN(SecurityPolicyAccessTest);
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

}  // namespace blink
