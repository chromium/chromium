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

#include "services/network/public/mojom/cors.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

TEST(SecurityPolicyTest, EmptyReferrerForUnauthorizedScheme) {
  const KURL example_http_url = KURL("http://example.com/");
  EXPECT_TRUE(String() == SecurityPolicy::GenerateReferrer(
                              kReferrerPolicyAlways, example_http_url,
                              String::FromUTF8("chrome://somepage/"))
                              .referrer);
}

TEST(SecurityPolicyTest, GenerateReferrerRespectsReferrerSchemesRegistry) {
  const KURL example_http_url = KURL("http://example.com/");
  const String foobar_url = String::FromUTF8("foobar://somepage/");
  const String foobar_scheme = String::FromUTF8("foobar");

  EXPECT_EQ(String(), SecurityPolicy::GenerateReferrer(
                          kReferrerPolicyAlways, example_http_url, foobar_url)
                          .referrer);
  SchemeRegistry::RegisterURLSchemeAsAllowedForReferrer(foobar_scheme);
  EXPECT_EQ(foobar_url, SecurityPolicy::GenerateReferrer(
                            kReferrerPolicyAlways, example_http_url, foobar_url)
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
    ReferrerPolicy policy;
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

  TestCase inputs[] = {
      // HTTP -> HTTP: Same Origin
      {kReferrerPolicyAlways, kInsecureURLA, kInsecureURLA, kInsecureURLA},
      {kReferrerPolicyDefault, kInsecureURLA, kInsecureURLA, kInsecureURLA},
      {kReferrerPolicyNoReferrerWhenDowngrade, kInsecureURLA, kInsecureURLA,
       kInsecureURLA},
      {kReferrerPolicyNever, kInsecureURLA, kInsecureURLA, nullptr},
      {kReferrerPolicyOrigin, kInsecureURLA, kInsecureURLA, kInsecureOriginA},
      {kReferrerPolicyOriginWhenCrossOrigin, kInsecureURLA, kInsecureURLA,
       kInsecureURLA},
      {kReferrerPolicySameOrigin, kInsecureURLA, kInsecureURLA, kInsecureURLA},
      {kReferrerPolicyStrictOrigin, kInsecureURLA, kInsecureURLA,
       kInsecureOriginA},
      {kReferrerPolicyStrictOriginWhenCrossOrigin, kInsecureURLA, kInsecureURLA,
       kInsecureURLA},

      // HTTP -> HTTP: Cross Origin
      {kReferrerPolicyAlways, kInsecureURLA, kInsecureURLB, kInsecureURLA},
      {kReferrerPolicyDefault, kInsecureURLA, kInsecureURLB, kInsecureURLA},
      {kReferrerPolicyNoReferrerWhenDowngrade, kInsecureURLA, kInsecureURLB,
       kInsecureURLA},
      {kReferrerPolicyNever, kInsecureURLA, kInsecureURLB, nullptr},
      {kReferrerPolicyOrigin, kInsecureURLA, kInsecureURLB, kInsecureOriginA},
      {kReferrerPolicyOriginWhenCrossOrigin, kInsecureURLA, kInsecureURLB,
       kInsecureOriginA},
      {kReferrerPolicySameOrigin, kInsecureURLA, kInsecureURLB, nullptr},
      {kReferrerPolicyStrictOrigin, kInsecureURLA, kInsecureURLB,
       kInsecureOriginA},
      {kReferrerPolicyStrictOriginWhenCrossOrigin, kInsecureURLA, kInsecureURLB,
       kInsecureOriginA},

      // HTTPS -> HTTPS: Same Origin
      {kReferrerPolicyAlways, kSecureURLA, kSecureURLA, kSecureURLA},
      {kReferrerPolicyDefault, kSecureURLA, kSecureURLA, kSecureURLA},
      {kReferrerPolicyNoReferrerWhenDowngrade, kSecureURLA, kSecureURLA,
       kSecureURLA},
      {kReferrerPolicyNever, kSecureURLA, kSecureURLA, nullptr},
      {kReferrerPolicyOrigin, kSecureURLA, kSecureURLA, kSecureOriginA},
      {kReferrerPolicyOriginWhenCrossOrigin, kSecureURLA, kSecureURLA,
       kSecureURLA},
      {kReferrerPolicySameOrigin, kSecureURLA, kSecureURLA, kSecureURLA},
      {kReferrerPolicyStrictOrigin, kSecureURLA, kSecureURLA, kSecureOriginA},
      {kReferrerPolicyStrictOriginWhenCrossOrigin, kSecureURLA, kSecureURLA,
       kSecureURLA},

      // HTTPS -> HTTPS: Cross Origin
      {kReferrerPolicyAlways, kSecureURLA, kSecureURLB, kSecureURLA},
      {kReferrerPolicyDefault, kSecureURLA, kSecureURLB, kSecureURLA},
      {kReferrerPolicyNoReferrerWhenDowngrade, kSecureURLA, kSecureURLB,
       kSecureURLA},
      {kReferrerPolicyNever, kSecureURLA, kSecureURLB, nullptr},
      {kReferrerPolicyOrigin, kSecureURLA, kSecureURLB, kSecureOriginA},
      {kReferrerPolicyOriginWhenCrossOrigin, kSecureURLA, kSecureURLB,
       kSecureOriginA},
      {kReferrerPolicySameOrigin, kSecureURLA, kSecureURLB, nullptr},
      {kReferrerPolicyStrictOrigin, kSecureURLA, kSecureURLB, kSecureOriginA},
      {kReferrerPolicyStrictOriginWhenCrossOrigin, kSecureURLA, kSecureURLB,
       kSecureOriginA},

      // HTTP -> HTTPS
      {kReferrerPolicyAlways, kInsecureURLA, kSecureURLB, kInsecureURLA},
      {kReferrerPolicyDefault, kInsecureURLA, kSecureURLB, kInsecureURLA},
      {kReferrerPolicyNoReferrerWhenDowngrade, kInsecureURLA, kSecureURLB,
       kInsecureURLA},
      {kReferrerPolicyNever, kInsecureURLA, kSecureURLB, nullptr},
      {kReferrerPolicyOrigin, kInsecureURLA, kSecureURLB, kInsecureOriginA},
      {kReferrerPolicyOriginWhenCrossOrigin, kInsecureURLA, kSecureURLB,
       kInsecureOriginA},
      {kReferrerPolicySameOrigin, kInsecureURLA, kSecureURLB, nullptr},
      {kReferrerPolicyStrictOrigin, kInsecureURLA, kSecureURLB,
       kInsecureOriginA},
      {kReferrerPolicyStrictOriginWhenCrossOrigin, kInsecureURLA, kSecureURLB,
       kInsecureOriginA},

      // HTTPS -> HTTP
      {kReferrerPolicyAlways, kSecureURLA, kInsecureURLB, kSecureURLA},
      {kReferrerPolicyDefault, kSecureURLA, kInsecureURLB, nullptr},
      {kReferrerPolicyNoReferrerWhenDowngrade, kSecureURLA, kInsecureURLB,
       nullptr},
      {kReferrerPolicyNever, kSecureURLA, kInsecureURLB, nullptr},
      {kReferrerPolicyOrigin, kSecureURLA, kInsecureURLB, kSecureOriginA},
      {kReferrerPolicyOriginWhenCrossOrigin, kSecureURLA, kSecureURLB,
       kSecureOriginA},
      {kReferrerPolicySameOrigin, kSecureURLA, kInsecureURLB, nullptr},
      {kReferrerPolicyStrictOrigin, kSecureURLA, kInsecureURLB, nullptr},
      {kReferrerPolicyStrictOriginWhenCrossOrigin, kSecureURLA, kInsecureURLB,
       nullptr},

      // blob and filesystem URL handling
      {kReferrerPolicyAlways, kInsecureURLA, kBlobURL, nullptr},
      {kReferrerPolicyAlways, kBlobURL, kInsecureURLA, nullptr},
      {kReferrerPolicyAlways, kInsecureURLA, kFilesystemURL, nullptr},
      {kReferrerPolicyAlways, kFilesystemURL, kInsecureURLA, nullptr},
  };

  for (TestCase test : inputs) {
    KURL destination(test.destination);
    Referrer result = SecurityPolicy::GenerateReferrer(
        test.policy, destination, String::FromUTF8(test.referrer));
    if (test.expected) {
      EXPECT_EQ(String::FromUTF8(test.expected), result.referrer)
          << "'" << test.referrer << "' to '" << test.destination
          << "' should have been '" << test.expected << "': was '"
          << result.referrer.Utf8().data() << "'.";
    } else {
      EXPECT_TRUE(result.referrer.IsEmpty())
          << "'" << test.referrer << "' to '" << test.destination
          << "' should have been empty: was '" << result.referrer.Utf8().data()
          << "'.";
    }
    EXPECT_EQ(test.policy == kReferrerPolicyDefault
                  ? kReferrerPolicyNoReferrerWhenDowngrade
                  : test.policy,
              result.referrer_policy);
  }
}

TEST(SecurityPolicyTest, ReferrerPolicyFromHeaderValue) {
  struct TestCase {
    const char* header;
    bool is_valid;
    ReferrerPolicyLegacyKeywordsSupport keywords;
    ReferrerPolicy expected_policy;
  };

  TestCase inputs[] = {
      {"origin", true, kDoNotSupportReferrerPolicyLegacyKeywords,
       kReferrerPolicyOrigin},
      {"none", true, kSupportReferrerPolicyLegacyKeywords,
       kReferrerPolicyNever},
      {"none", false, kDoNotSupportReferrerPolicyLegacyKeywords,
       kReferrerPolicyDefault},
      {"foo", false, kDoNotSupportReferrerPolicyLegacyKeywords,
       kReferrerPolicyDefault},
      {"origin, foo", true, kDoNotSupportReferrerPolicyLegacyKeywords,
       kReferrerPolicyOrigin},
      {"origin, foo-bar", true, kDoNotSupportReferrerPolicyLegacyKeywords,
       kReferrerPolicyOrigin},
      {"origin, foo bar", false, kDoNotSupportReferrerPolicyLegacyKeywords,
       kReferrerPolicyDefault},
  };

  for (TestCase test : inputs) {
    ReferrerPolicy actual_policy = kReferrerPolicyDefault;
    EXPECT_EQ(test.is_valid, SecurityPolicy::ReferrerPolicyFromHeaderValue(
                                 test.header, test.keywords, &actual_policy));
    if (test.is_valid)
      EXPECT_EQ(test.expected_policy, actual_policy);
  }
}

TEST(SecurityPolicyTest, TrustworthyWhiteList) {
  const char* insecure_urls[] = {
      "http://a.test/path/to/file.html", "http://b.test/path/to/file.html",
      "blob:http://c.test/b3aae9c8-7f90-440d-8d7c-43aa20d72fde",
      "filesystem:http://d.test/path/t/file.html",
  };

  for (const char* url : insecure_urls) {
    scoped_refptr<const SecurityOrigin> origin =
        SecurityOrigin::CreateFromString(url);
    EXPECT_FALSE(origin->IsPotentiallyTrustworthy());
    SecurityPolicy::AddOriginTrustworthyWhiteList(origin->ToString());
    EXPECT_TRUE(origin->IsPotentiallyTrustworthy());
  }

  // Tests that adding URLs that have inner-urls to the whitelist
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
    SecurityPolicy::AddOriginTrustworthyWhiteList(origin1->ToString());
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

  void TearDown() override {
    SecurityPolicy::ClearOriginAccessAllowList();
    SecurityPolicy::ClearOriginAccessBlockList();
  }

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
      *https_chromium_origin(), "https", "example.com", false,
      network::mojom::CORSOriginAccessMatchPriority::kDefaultPriority);
  EXPECT_TRUE(SecurityPolicy::IsOriginAccessAllowed(https_chromium_origin(),
                                                    https_example_origin()));
  EXPECT_FALSE(SecurityPolicy::IsOriginAccessAllowed(
      https_chromium_origin(), https_sub_example_origin()));
  EXPECT_FALSE(SecurityPolicy::IsOriginAccessAllowed(https_chromium_origin(),
                                                     http_example_origin()));

  // Clearing the map should revoke all special access.
  SecurityPolicy::ClearOriginAccessAllowList();
  EXPECT_FALSE(SecurityPolicy::IsOriginAccessAllowed(https_chromium_origin(),
                                                     https_example_origin()));
  EXPECT_FALSE(SecurityPolicy::IsOriginAccessAllowed(
      https_chromium_origin(), https_sub_example_origin()));
  EXPECT_FALSE(SecurityPolicy::IsOriginAccessAllowed(https_chromium_origin(),
                                                     http_example_origin()));

  // Adding an entry that matches subdomains should grant access to any
  // subdomains.
  SecurityPolicy::AddOriginAccessAllowListEntry(
      *https_chromium_origin(), "https", "example.com", true,
      network::mojom::CORSOriginAccessMatchPriority::kDefaultPriority);
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
      *https_chromium_origin(), "https", "", true,
      network::mojom::CORSOriginAccessMatchPriority::kDefaultPriority);
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
      *https_chromium_origin(), "https", "example.com", true,
      network::mojom::CORSOriginAccessMatchPriority::kDefaultPriority);
  SecurityPolicy::AddOriginAccessBlockListEntry(
      *https_chromium_origin(), "https", "example.com", false,
      network::mojom::CORSOriginAccessMatchPriority::kDefaultPriority);

  EXPECT_FALSE(SecurityPolicy::IsOriginAccessAllowed(https_chromium_origin(),
                                                     https_example_origin()));
  EXPECT_TRUE(SecurityPolicy::IsOriginAccessAllowed(
      https_chromium_origin(), https_sub_example_origin()));
}

TEST_F(SecurityPolicyAccessTest,
       IsOriginAccessAllowedWildcardWithBlockListEntry) {
  SecurityPolicy::AddOriginAccessAllowListEntry(
      *https_chromium_origin(), "https", "", true,
      network::mojom::CORSOriginAccessMatchPriority::kDefaultPriority);
  SecurityPolicy::AddOriginAccessBlockListEntry(
      *https_chromium_origin(), "https", "google.com", false,
      network::mojom::CORSOriginAccessMatchPriority::kDefaultPriority);

  EXPECT_TRUE(SecurityPolicy::IsOriginAccessAllowed(https_chromium_origin(),
                                                    https_example_origin()));
  EXPECT_FALSE(SecurityPolicy::IsOriginAccessAllowed(https_chromium_origin(),
                                                     https_google_origin()));
}

TEST_F(SecurityPolicyAccessTest, ClearOriginAccessAllowListForOrigin) {
  SecurityPolicy::AddOriginAccessAllowListEntry(
      *https_chromium_origin(), "https", "example.com", true,
      network::mojom::CORSOriginAccessMatchPriority::kDefaultPriority);
  SecurityPolicy::AddOriginAccessAllowListEntry(
      *https_chromium_origin(), "https", "google.com", true,
      network::mojom::CORSOriginAccessMatchPriority::kDefaultPriority);
  SecurityPolicy::AddOriginAccessAllowListEntry(
      *https_example_origin(), "https", "google.com", true,
      network::mojom::CORSOriginAccessMatchPriority::kDefaultPriority);

  SecurityPolicy::ClearOriginAccessAllowListForOrigin(*https_chromium_origin());

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
      *https_chromium_origin(), "https", "sub.example.com", false,
      network::mojom::CORSOriginAccessMatchPriority::kLowPriority);
  EXPECT_TRUE(SecurityPolicy::IsOriginAccessAllowed(
      https_chromium_origin(), https_sub_example_origin()));
  SecurityPolicy::AddOriginAccessBlockListEntry(
      *https_chromium_origin(), "https", "example.com", true,
      network::mojom::CORSOriginAccessMatchPriority::kMediumPriority);
  EXPECT_FALSE(SecurityPolicy::IsOriginAccessAllowed(
      https_chromium_origin(), https_sub_example_origin()));
  SecurityPolicy::AddOriginAccessAllowListEntry(
      *https_chromium_origin(), "https", "example.com", true,
      network::mojom::CORSOriginAccessMatchPriority::kHighPriority);

  EXPECT_TRUE(SecurityPolicy::IsOriginAccessAllowed(
      https_chromium_origin(), https_sub_example_origin()));
}

}  // namespace blink
