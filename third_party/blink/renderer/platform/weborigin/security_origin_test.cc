/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

#include <stdint.h>

#include <string_view>

#include "base/test/scoped_command_line.h"
#include "base/unguessable_token.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/is_potentially_trustworthy_unittest.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/mojom/cors.mojom-blink.h"
#include "services/network/public/mojom/cors_origin_pattern.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/blob/blob_url.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_operators.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "url/gurl.h"
#include "url/origin_abstract_tests.h"
#include "url/url_util.h"

namespace blink {

const uint16_t kMaxAllowedPort = UINT16_MAX;

class SecurityOriginTest : public testing::Test {
 protected:
  void TearDown() override { SecurityPolicy::ClearOriginAccessList(); }

  const std::optional<url::Origin::Nonce>& GetNonceForOrigin(
      const SecurityOrigin& origin) {
    return origin.nonce_if_opaque_;
  }

  const base::UnguessableToken* GetNonceForSerializationForOrigin(
      const SecurityOrigin& origin) {
    return origin.GetNonceForSerialization();
  }
};

TEST_F(SecurityOriginTest, ValidPortsCreateTupleOrigins) {
  uint16_t ports[] = {0, 80, 443, 5000, kMaxAllowedPort};

  for (size_t i = 0; i < std::size(ports); ++i) {
    scoped_refptr<const SecurityOrigin> origin =
        SecurityOrigin::CreateFromValidTuple("http", "example.com", ports[i]);
    EXPECT_FALSE(origin->IsOpaque())
        << "Port " << ports[i] << " should have generated a tuple origin.";
  }
}

TEST_F(SecurityOriginTest, LocalAccess) {
  scoped_refptr<SecurityOrigin> file1 =
      SecurityOrigin::CreateFromString("file:///etc/passwd");
  scoped_refptr<const SecurityOrigin> file2 =
      SecurityOrigin::CreateFromString("file:///etc/shadow");

  EXPECT_TRUE(file1->IsSameOriginWith(file1.get()));
  EXPECT_TRUE(file1->IsSameOriginWith(file2.get()));
  EXPECT_TRUE(file2->IsSameOriginWith(file1.get()));

  EXPECT_TRUE(file1->CanAccess(file1.get()));
  EXPECT_TRUE(file1->CanAccess(file2.get()));
  EXPECT_TRUE(file2->CanAccess(file1.get()));

  // Block |file1|'s access to local origins. It should now be same-origin
  // with itself, but shouldn't have access to |file2|.
  file1->BlockLocalAccessFromLocalOrigin();
  EXPECT_TRUE(file1->IsSameOriginWith(file1.get()));
  EXPECT_FALSE(file1->IsSameOriginWith(file2.get()));
  EXPECT_FALSE(file2->IsSameOriginWith(file1.get()));

  EXPECT_TRUE(file1->CanAccess(file1.get()));
  EXPECT_FALSE(file1->CanAccess(file2.get()));
  EXPECT_FALSE(file2->CanAccess(file1.get()));
}

TEST_F(SecurityOriginTest, IsNullURLSecure) {
  EXPECT_FALSE(network::IsUrlPotentiallyTrustworthy(GURL(NullURL())));
}

TEST_F(SecurityOriginTest, CanAccess) {
  struct TestCase {
    bool can_access;
    const char* origin1;
    const char* origin2;
  };

  TestCase tests[] = {
      {true, "https://foobar.com", "https://foobar.com"},
      {false, "https://foobar.com", "https://bazbar.com"},
      {true, "file://localhost/", "file://localhost/"},
      {false, "file:///", "file://localhost/"},
  };

  for (size_t i = 0; i < std::size(tests); ++i) {
    scoped_refptr<const SecurityOrigin> origin1 =
        SecurityOrigin::CreateFromString(tests[i].origin1);
    scoped_refptr<const SecurityOrigin> origin2 =
        SecurityOrigin::CreateFromString(tests[i].origin2);
    EXPECT_EQ(tests[i].can_access, origin1->CanAccess(origin2.get()));
    EXPECT_EQ(tests[i].can_access, origin2->CanAccess(origin1.get()));
    EXPECT_FALSE(origin1->DeriveNewOpaqueOrigin()->CanAccess(origin1.get()));
    EXPECT_FALSE(origin2->DeriveNewOpaqueOrigin()->CanAccess(origin1.get()));
    EXPECT_FALSE(origin1->DeriveNewOpaqueOrigin()->CanAccess(origin2.get()));
    EXPECT_FALSE(origin2->DeriveNewOpaqueOrigin()->CanAccess(origin2.get()));
    EXPECT_FALSE(origin2->CanAccess(origin1->DeriveNewOpaqueOrigin().get()));
    EXPECT_FALSE(origin2->CanAccess(origin1->DeriveNewOpaqueOrigin().get()));
    EXPECT_FALSE(origin1->CanAccess(origin2->DeriveNewOpaqueOrigin().get()));
    EXPECT_FALSE(origin2->CanAccess(origin2->DeriveNewOpaqueOrigin().get()));
    EXPECT_FALSE(origin1->DeriveNewOpaqueOrigin()->CanAccess(
        origin1->DeriveNewOpaqueOrigin().get()));
    EXPECT_FALSE(origin2->DeriveNewOpaqueOrigin()->CanAccess(
        origin2->DeriveNewOpaqueOrigin().get()));
  }
}

TEST_F(SecurityOriginTest, CanAccessDetail) {
  struct TestCase {
    SecurityOrigin::AccessResultDomainDetail expected;
    const char* origin1;
    const char* domain1;
    const char* origin2;
    const char* domain2;
  };

  TestCase tests[] = {
      // Actually cross-origin origins
      {SecurityOrigin::AccessResultDomainDetail::kDomainNotSet,
       "https://example.com", nullptr, "https://not-example.com", nullptr},
      {SecurityOrigin::AccessResultDomainDetail::kDomainNotRelevant,
       "https://example.com", "example.com", "https://not-example.com",
       nullptr},
      {SecurityOrigin::AccessResultDomainDetail::kDomainNotRelevant,
       "https://example.com", nullptr, "https://not-example.com",
       "not-example.com"},
      {SecurityOrigin::AccessResultDomainDetail::kDomainNotRelevant,
       "https://example.com", "example.com", "https://not-example.com",
       "not-example.com"},

      // Same-origin origins
      {SecurityOrigin::AccessResultDomainDetail::kDomainNotSet,
       "https://example.com", nullptr, "https://example.com", nullptr},
      {SecurityOrigin::AccessResultDomainDetail::kDomainSetByOnlyOneOrigin,
       "https://example.com", "example.com", "https://example.com", nullptr},
      {SecurityOrigin::AccessResultDomainDetail::kDomainSetByOnlyOneOrigin,
       "https://example.com", nullptr, "https://example.com", "example.com"},
      {SecurityOrigin::AccessResultDomainDetail::kDomainMismatch,
       "https://www.example.com", "www.example.com", "https://www.example.com",
       "example.com"},
      {SecurityOrigin::AccessResultDomainDetail::kDomainMatchUnnecessary,
       "https://example.com", "example.com", "https://example.com",
       "example.com"},

      // Same-origin-domain origins
      {SecurityOrigin::AccessResultDomainDetail::kDomainNotSet,
       "https://a.example.com", nullptr, "https://b.example.com", nullptr},
      {SecurityOrigin::AccessResultDomainDetail::kDomainNotRelevant,
       "https://a.example.com", "example.com", "https://b.example.com",
       nullptr},
      {SecurityOrigin::AccessResultDomainDetail::kDomainNotRelevant,
       "https://a.example.com", nullptr, "https://b.example.com",
       "example.com"},
      {SecurityOrigin::AccessResultDomainDetail::kDomainMatchNecessary,
       "https://a.example.com", "example.com", "https://b.example.com",
       "example.com"},
  };

  for (TestCase test : tests) {
    SCOPED_TRACE(testing::Message()
                 << "\nOrigin 1: `" << test.origin1 << "` ("
                 << (test.domain1 ? test.domain1 : "") << ") \n"
                 << "Origin 2: `" << test.origin2 << "` ("
                 << (test.domain2 ? test.domain2 : "") << ")\n");
    scoped_refptr<SecurityOrigin> origin1 =
        SecurityOrigin::CreateFromString(test.origin1);
    if (test.domain1)
      origin1->SetDomainFromDOM(test.domain1);
    scoped_refptr<SecurityOrigin> origin2 =
        SecurityOrigin::CreateFromString(test.origin2);
    if (test.domain2)
      origin2->SetDomainFromDOM(test.domain2);
    SecurityOrigin::AccessResultDomainDetail detail;
    origin1->CanAccess(origin2.get(), detail);
    EXPECT_EQ(test.expected, detail);
    origin2->CanAccess(origin1.get(), detail);
    EXPECT_EQ(test.expected, detail);
  }
}

TEST_F(SecurityOriginTest, CanRequest) {
  struct TestCase {
    bool can_request;
    const char* origin;
    const char* url;
  };

  TestCase tests[] = {
      {true, "https://foobar.com", "https://foobar.com"},
      {false, "https://foobar.com", "https://bazbar.com"},
  };

  for (size_t i = 0; i < std::size(tests); ++i) {
    scoped_refptr<const SecurityOrigin> origin =
        SecurityOrigin::CreateFromString(tests[i].origin);
    blink::KURL url(tests[i].url);
    EXPECT_EQ(tests[i].can_request, origin->CanRequest(url));
  }
}

TEST_F(SecurityOriginTest, CanRequestWithAllowListedAccess) {
  scoped_refptr<const SecurityOrigin> origin =
      SecurityOrigin::CreateFromString("https://chromium.org");
  const blink::KURL url("https://example.com");

  EXPECT_FALSE(origin->CanRequest(url));
  // Adding the url to the access allowlist should allow the request.
  SecurityPolicy::AddOriginAccessAllowListEntry(
      *origin, "https", "example.com",
      /*destination_port=*/0,
      network::mojom::CorsDomainMatchMode::kDisallowSubdomains,
      network::mojom::CorsPortMatchMode::kAllowAnyPort,
      network::mojom::CorsOriginAccessMatchPriority::kMediumPriority);
  EXPECT_TRUE(origin->CanRequest(url));
}

TEST_F(SecurityOriginTest, CannotRequestWithBlockListedAccess) {
  scoped_refptr<const SecurityOrigin> origin =
      SecurityOrigin::CreateFromString("https://chromium.org");
  const blink::KURL allowed_url("https://test.example.com");
  const blink::KURL blocked_url("https://example.com");

  // BlockList that is more or same specificity wins.
  SecurityPolicy::AddOriginAccessAllowListEntry(
      *origin, "https", "example.com",
      /*destination_port=*/0,
      network::mojom::CorsDomainMatchMode::kAllowSubdomains,
      network::mojom::CorsPortMatchMode::kAllowAnyPort,
      network::mojom::CorsOriginAccessMatchPriority::kDefaultPriority);
  SecurityPolicy::AddOriginAccessBlockListEntry(
      *origin, "https", "example.com",
      /*destination_port=*/0,
      network::mojom::CorsDomainMatchMode::kDisallowSubdomains,
      network::mojom::CorsPortMatchMode::kAllowAnyPort,
      network::mojom::CorsOriginAccessMatchPriority::kLowPriority);
  // Block since example.com is on the allowlist & blocklist.
  EXPECT_FALSE(origin->CanRequest(blocked_url));
  // Allow since *.example.com is on the allowlist but not the blocklist.
  EXPECT_TRUE(origin->CanRequest(allowed_url));
}

TEST_F(SecurityOriginTest, CanRequestWithMoreSpecificAllowList) {
  scoped_refptr<const SecurityOrigin> origin =
      SecurityOrigin::CreateFromString("https://chromium.org");
  const blink::KURL allowed_url("https://test.example.com");
  const blink::KURL blocked_url("https://example.com");

  SecurityPolicy::AddOriginAccessAllowListEntry(
      *origin, "https", "test.example.com",
      /*destination_port=*/0,
      network::mojom::CorsDomainMatchMode::kAllowSubdomains,
      network::mojom::CorsPortMatchMode::kAllowAnyPort,
      network::mojom::CorsOriginAccessMatchPriority::kMediumPriority);
  SecurityPolicy::AddOriginAccessBlockListEntry(
      *origin, "https", "example.com",
      /*destination_port=*/0,
      network::mojom::CorsDomainMatchMode::kAllowSubdomains,
      network::mojom::CorsPortMatchMode::kAllowAnyPort,
      network::mojom::CorsOriginAccessMatchPriority::kLowPriority);
  // Allow since test.example.com (allowlist) has a higher priority than
  // *.example.com (blocklist).
  EXPECT_TRUE(origin->CanRequest(allowed_url));
  // Block since example.com isn't on the allowlist.
  EXPECT_FALSE(origin->CanRequest(blocked_url));
}

TEST_F(SecurityOriginTest, CanRequestWithPortSpecificAllowList) {
  scoped_refptr<const SecurityOrigin> origin =
      SecurityOrigin::CreateFromString("https://chromium.org");
  SecurityPolicy::AddOriginAccessAllowListEntry(
      *origin, "https", "test1.example.com", 443,
      network::mojom::CorsDomainMatchMode::kAllowSubdomains,
      network::mojom::CorsPortMatchMode::kAllowOnlySpecifiedPort,
      network::mojom::CorsOriginAccessMatchPriority::kMediumPriority);
  SecurityPolicy::AddOriginAccessAllowListEntry(
      *origin, "https", "test2.example.com", 444,
      network::mojom::CorsDomainMatchMode::kAllowSubdomains,
      network::mojom::CorsPortMatchMode::kAllowOnlySpecifiedPort,
      network::mojom::CorsOriginAccessMatchPriority::kMediumPriority);

  EXPECT_TRUE(origin->CanRequest(blink::KURL("https://test1.example.com")));
  EXPECT_TRUE(origin->CanRequest(blink::KURL("https://test1.example.com:443")));
  EXPECT_FALSE(origin->CanRequest(blink::KURL("https://test1.example.com:43")));

  EXPECT_FALSE(origin->CanRequest(blink::KURL("https://test2.example.com")));
  EXPECT_FALSE(origin->CanRequest(blink::KURL("https://test2.example.com:44")));
  EXPECT_TRUE(origin->CanRequest(blink::KURL("https://test2.example.com:444")));
}

TEST_F(SecurityOriginTest, PunycodeNotUnicode) {
  scoped_refptr<const SecurityOrigin> origin =
      SecurityOrigin::CreateFromString("https://chromium.org");
  const blink::KURL unicode_url("https://☃.net/");
  const blink::KURL punycode_url("https://xn--n3h.net/");

  // Sanity check: Origin blocked by default.
  EXPECT_FALSE(origin->CanRequest(punycode_url));
  EXPECT_FALSE(origin->CanRequest(unicode_url));

  // Verify unicode origin can not be allowlisted.
  SecurityPolicy::AddOriginAccessAllowListEntry(
      *origin, "https", "☃.net",
      /*destination_port=*/0,
      network::mojom::CorsDomainMatchMode::kAllowSubdomains,
      network::mojom::CorsPortMatchMode::kAllowAnyPort,
      network::mojom::CorsOriginAccessMatchPriority::kMediumPriority);
  EXPECT_FALSE(origin->CanRequest(punycode_url));
  EXPECT_FALSE(origin->CanRequest(unicode_url));

  // Verify punycode allowlist only affects punycode URLs.
  SecurityPolicy::AddOriginAccessAllowListEntry(
      *origin, "https", "xn--n3h.net",
      /*destination_port=*/0,
      network::mojom::CorsDomainMatchMode::kAllowSubdomains,
      network::mojom::CorsPortMatchMode::kAllowAnyPort,
      network::mojom::CorsOriginAccessMatchPriority::kMediumPriority);
  EXPECT_TRUE(origin->CanRequest(punycode_url));
  EXPECT_FALSE(origin->CanRequest(unicode_url));

  // Clear enterprise policy allow/block lists.
  SecurityPolicy::ClearOriginAccessListForOrigin(*origin);

  EXPECT_FALSE(origin->CanRequest(punycode_url));
  EXPECT_FALSE(origin->CanRequest(unicode_url));

  // Simulate <all_urls> being in the extension permissions.
  SecurityPolicy::AddOriginAccessAllowListEntry(
      *origin, "https", "",
      /*destination_port=*/0,
      network::mojom::CorsDomainMatchMode::kAllowSubdomains,
      network::mojom::CorsPortMatchMode::kAllowAnyPort,
      network::mojom::CorsOriginAccessMatchPriority::kDefaultPriority);

  EXPECT_TRUE(origin->CanRequest(punycode_url));
  EXPECT_FALSE(origin->CanRequest(unicode_url));

  // Verify unicode origin can not be blocklisted.
  SecurityPolicy::AddOriginAccessBlockListEntry(
      *origin, "https", "☃.net",
      /*destination_port=*/0,
      network::mojom::CorsDomainMatchMode::kAllowSubdomains,
      network::mojom::CorsPortMatchMode::kAllowAnyPort,
      network::mojom::CorsOriginAccessMatchPriority::kLowPriority);
  EXPECT_TRUE(origin->CanRequest(punycode_url));
  EXPECT_FALSE(origin->CanRequest(unicode_url));

  // Verify punycode blocklist only affects punycode URLs.
  SecurityPolicy::AddOriginAccessBlockListEntry(
      *origin, "https", "xn--n3h.net",
      /*destination_port=*/0,
      network::mojom::CorsDomainMatchMode::kAllowSubdomains,
      network::mojom::CorsPortMatchMode::kAllowAnyPort,
      network::mojom::CorsOriginAccessMatchPriority::kLowPriority);
  EXPECT_FALSE(origin->CanRequest(punycode_url));
  EXPECT_FALSE(origin->CanRequest(unicode_url));
}

TEST_F(SecurityOriginTest, CreateFromTuple) {
  struct TestCase {
    const char* scheme;
    const char* host;
    uint16_t port;
    const char* origin;
  } cases[] = {
      {"http", "example.com", 80, "http://example.com"},
      {"http", "example.com", 0, "http://example.com:0"},
      {"http", "example.com", 81, "http://example.com:81"},
      {"https", "example.com", 443, "https://example.com"},
      {"https", "example.com", 444, "https://example.com:444"},
      {"file", "", 0, "file://"},
      {"file", "example.com", 0, "file://"},
  };

  for (const auto& test : cases) {
    scoped_refptr<const SecurityOrigin> origin =
        SecurityOrigin::CreateFromValidTuple(test.scheme, test.host, test.port);
    EXPECT_EQ(test.origin, origin->ToString()) << test.origin;
  }
}

TEST_F(SecurityOriginTest, OpaquenessPropagatesToBlobUrls) {
  struct TestCase {
    const char* url;
    bool expected_opaqueness;
    const char* expected_origin_string;
  } cases[]{
      {"", true, "null"},
      {"null", true, "null"},
      {"data:text/plain,hello_world", true, "null"},
      {"file:///path", false, "file://"},
      {"filesystem:http://host/filesystem-path", false, "http://host"},
      {"filesystem:file:///filesystem-path", false, "file://"},
      {"filesystem:null/filesystem-path", true, "null"},
      {"blob:http://host/blob-id", false, "http://host"},
      {"blob:file:///blob-id", false, "file://"},
      {"blob:null/blob-id", true, "null"},
  };

  for (const TestCase& test : cases) {
    scoped_refptr<const SecurityOrigin> origin =
        SecurityOrigin::CreateFromString(test.url);
    EXPECT_EQ(test.expected_opaqueness, origin->IsOpaque());
    EXPECT_EQ(test.expected_origin_string, origin->ToString());

    KURL blob_url = BlobURL::CreatePublicURL(origin.get());
    scoped_refptr<const SecurityOrigin> blob_url_origin =
        SecurityOrigin::Create(blob_url);
    EXPECT_EQ(blob_url_origin->IsOpaque(), origin->IsOpaque());
    EXPECT_EQ(blob_url_origin->ToString(), origin->ToString());
    EXPECT_EQ(blob_url_origin->ToRawString(), origin->ToRawString());
  }
}

TEST_F(SecurityOriginTest, OpaqueOriginIsSameOriginWith) {
  scoped_refptr<const SecurityOrigin> opaque_origin =
      SecurityOrigin::CreateUniqueOpaque();
  scoped_refptr<const SecurityOrigin> tuple_origin =
      SecurityOrigin::CreateFromString("http://example.com");

  EXPECT_TRUE(opaque_origin->IsSameOriginWith(opaque_origin.get()));
  EXPECT_FALSE(SecurityOrigin::CreateUniqueOpaque()->IsSameOriginWith(
      opaque_origin.get()));
  EXPECT_FALSE(tuple_origin->IsSameOriginWith(opaque_origin.get()));
  EXPECT_FALSE(opaque_origin->IsSameOriginWith(tuple_origin.get()));
}

TEST_F(SecurityOriginTest, CanonicalizeHost) {
  struct TestCase {
    const char* host;
    const char* canonical_output;
    bool expected_success;
  } cases[] = {
      {"", "", true},
      {"example.test", "example.test", true},
      {"EXAMPLE.TEST", "example.test", true},
      {"eXaMpLe.TeSt/path", "example.test%2Fpath", false},
      {",", ",", true},
      {"💩", "xn--ls8h", true},
      {"[]", "[]", false},
      {"%yo", "%25yo", false},
  };

  for (const TestCase& test : cases) {
    SCOPED_TRACE(testing::Message() << "raw host: '" << test.host << "'");
    String host = String::FromUTF8(test.host);
    bool success = false;
    String canonical_host =
        SecurityOrigin::CanonicalizeSpecialHost(host, &success);
    EXPECT_EQ(test.canonical_output, canonical_host);
    EXPECT_EQ(test.expected_success, success);
  }
}

TEST_F(SecurityOriginTest, UrlOriginConversions) {
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddNoAccessScheme("no-access");
  url::AddLocalScheme("nonstandard-but-local");
  struct TestCases {
    const char* const url;
    const char* const scheme;
    const char* const host;
    uint16_t port;
    bool opaque = false;
  } cases[] = {
      // Nonstandard scheme registered as local scheme
      {"nonstandard-but-local:really?really", "nonstandard-but-local", "", 0},

      // IP Addresses
      {"http://192.168.9.1/", "http", "192.168.9.1", 80},
      {"http://[2001:db8::1]/", "http", "[2001:db8::1]", 80},

      // Punycode
      {"http://☃.net/", "http", "xn--n3h.net", 80},
      {"blob:http://☃.net/", "http", "xn--n3h.net", 80},

      // Generic URLs
      {"http://example.com/", "http", "example.com", 80},
      {"http://example.com:123/", "http", "example.com", 123},
      {"https://example.com/", "https", "example.com", 443},
      {"https://example.com:123/", "https", "example.com", 123},
      {"http://user:pass@example.com/", "http", "example.com", 80},
      {"http://example.com:123/?query", "http", "example.com", 123},
      {"https://example.com/#1234", "https", "example.com", 443},
      {"https://u:p@example.com:123/?query#1234", "https", "example.com", 123},
      {"https://example.com:0/", "https", "example.com", 0},

      // Nonstandard schemes.
      {"unrecognized-scheme://localhost/", "", "", 0, true},
      {"mailto:localhost/", "", "", 0, true},
      {"about:blank", "", "", 0, true},

      // Custom no-access scheme.
      {"no-access:blah", "", "", 0, true},

      // Registered URLs
      {"ftp://example.com/", "ftp", "example.com", 21},
      {"ws://example.com/", "ws", "example.com", 80},
      {"wss://example.com/", "wss", "example.com", 443},

      // file: URLs
      {"file:///etc/passwd", "file", "", 0},
      {"file://example.com/etc/passwd", "file", "example.com", 0},

      // Filesystem:
      {"filesystem:http://example.com/type/", "http", "example.com", 80},
      {"filesystem:http://example.com:123/type/", "http", "example.com", 123},
      {"filesystem:https://example.com/type/", "https", "example.com", 443},
      {"filesystem:https://example.com:123/type/", "https", "example.com", 123},

      // Blob:
      {"blob:http://example.com/guid-goes-here", "http", "example.com", 80},
      {"blob:http://example.com:123/guid-goes-here", "http", "example.com",
       123},
      {"blob:https://example.com/guid-goes-here", "https", "example.com", 443},
      {"blob:http://u:p@example.com/guid-goes-here", "http", "example.com", 80},
  };

  for (const auto& test_case : cases) {
    SCOPED_TRACE(test_case.url);
    GURL gurl(test_case.url);
    KURL kurl(String::FromUTF8(test_case.url));
    EXPECT_TRUE(gurl.is_valid());
    EXPECT_TRUE(kurl.IsValid());
    url::Origin origin_via_gurl = url::Origin::Create(gurl);
    scoped_refptr<const SecurityOrigin> security_origin_via_kurl =
        SecurityOrigin::Create(kurl);
    EXPECT_EQ(origin_via_gurl.scheme(), test_case.scheme);

    // Test CreateFromUrlOrigin
    scoped_refptr<const SecurityOrigin> security_origin_via_gurl =
        SecurityOrigin::CreateFromUrlOrigin(origin_via_gurl);
    EXPECT_EQ(test_case.scheme, security_origin_via_gurl->Protocol());
    EXPECT_EQ(test_case.scheme, security_origin_via_kurl->Protocol());
    EXPECT_EQ(test_case.host, security_origin_via_gurl->Host());
    EXPECT_EQ(test_case.host, security_origin_via_kurl->Host());
    EXPECT_EQ(test_case.port, security_origin_via_gurl->Port());
    EXPECT_EQ(test_case.port, security_origin_via_kurl->Port());
    EXPECT_EQ(test_case.opaque, security_origin_via_gurl->IsOpaque());
    EXPECT_EQ(test_case.opaque, security_origin_via_kurl->IsOpaque());
    EXPECT_EQ(!test_case.opaque, security_origin_via_kurl->IsSameOriginWith(
                                     security_origin_via_gurl.get()));
    EXPECT_EQ(!test_case.opaque, security_origin_via_gurl->IsSameOriginWith(
                                     security_origin_via_kurl.get()));

    if (!test_case.opaque) {
      scoped_refptr<const SecurityOrigin> security_origin =
          SecurityOrigin::CreateFromValidTuple(test_case.scheme, test_case.host,
                                               test_case.port);
      EXPECT_TRUE(
          security_origin->IsSameOriginWith(security_origin_via_gurl.get()));
      EXPECT_TRUE(
          security_origin->IsSameOriginWith(security_origin_via_kurl.get()));
      EXPECT_TRUE(
          security_origin_via_gurl->IsSameOriginWith(security_origin.get()));
      EXPECT_TRUE(
          security_origin_via_kurl->IsSameOriginWith(security_origin.get()));
    }

    // Test ToUrlOrigin
    url::Origin origin_roundtrip_via_kurl =
        security_origin_via_kurl->ToUrlOrigin();
    url::Origin origin_roundtrip_via_gurl =
        security_origin_via_gurl->ToUrlOrigin();

    EXPECT_EQ(test_case.opaque, origin_roundtrip_via_kurl.opaque());
    EXPECT_EQ(test_case.opaque, origin_roundtrip_via_gurl.opaque());
    EXPECT_EQ(origin_roundtrip_via_gurl, origin_via_gurl);
    if (!test_case.opaque) {
      EXPECT_EQ(origin_via_gurl, origin_roundtrip_via_kurl);
      EXPECT_EQ(origin_roundtrip_via_kurl, origin_roundtrip_via_gurl);
    }
  }
}

TEST_F(SecurityOriginTest, InvalidWrappedUrls) {
  const char* kTestCases[] = {
      "blob:filesystem:ws:b/.",
      "blob:filesystem:ftp://a/b",
      "filesystem:filesystem:http://example.org:88/foo/bar",
      "blob:blob:file://localhost/foo/bar",
  };

  for (const char* test_url : kTestCases) {
    scoped_refptr<SecurityOrigin> target_origin =
        SecurityOrigin::CreateFromString(test_url);
    EXPECT_TRUE(target_origin->IsOpaque())
        << test_url << " is not opaque as a blink::SecurityOrigin";
    url::Origin origin = target_origin->ToUrlOrigin();
    EXPECT_TRUE(origin.opaque())
        << test_url << " is not opaque as a url::Origin";
  }
}

TEST_F(SecurityOriginTest, EffectiveDomain) {
  constexpr struct {
    const char* expected_effective_domain;
    const char* origin;
  } kTestCases[] = {
      {NULL, ""},
      {NULL, "null"},
      {"", "file://"},
      {"127.0.0.1", "https://127.0.0.1"},
      {"[::1]", "https://[::1]"},
      {"example.com", "file://example.com/foo"},
      {"example.com", "http://example.com"},
      {"example.com", "http://example.com:80"},
      {"example.com", "https://example.com"},
      {"suborigin.example.com", "https://suborigin.example.com"},
  };

  for (const auto& test : kTestCases) {
    scoped_refptr<const SecurityOrigin> origin =
        SecurityOrigin::CreateFromString(test.origin);
    if (test.expected_effective_domain) {
      EXPECT_EQ(test.expected_effective_domain, origin->Domain());
    } else {
      EXPECT_TRUE(origin->Domain().empty());
    }
  }
}

TEST_F(SecurityOriginTest, EffectiveDomainSetFromDom) {
  constexpr struct {
    const char* domain_set_from_dom;
    const char* expected_effective_domain;
    const char* origin;
  } kDomainTestCases[] = {
      {"example.com", "example.com", "http://www.suborigin.example.com"}};

  for (const auto& test : kDomainTestCases) {
    scoped_refptr<SecurityOrigin> origin =
        SecurityOrigin::CreateFromString(test.origin);
    origin->SetDomainFromDOM(test.domain_set_from_dom);
    EXPECT_EQ(test.expected_effective_domain, origin->Domain());
  }
}

TEST_F(SecurityOriginTest, ToTokenForFastCheck) {
  base::UnguessableToken agent_cluster_id = base::UnguessableToken::Create();
  constexpr struct {
    const char* url;
    const char* token;
  } kTestCases[] = {
      {"", nullptr},
      {"null", nullptr},
      {"data:text/plain,hello, world", nullptr},
      {"http://example.org/foo/bar", "http://example.org"},
      {"http://example.org:8080/foo/bar", "http://example.org:8080"},
      {"https://example.org:443/foo/bar", "https://example.org"},
      {"https://example.org:444/foo/bar", "https://example.org:444"},
      {"file:///foo/bar", "file://"},
      {"file://localhost/foo/bar", "file://localhost"},
      {"filesystem:http://example.org:88/foo/bar", "http://example.org:88"},
      // Somehow the host part in the inner URL is dropped.
      // See https://crbug.com/867914 for details.
      {"filesystem:file://localhost/foo/bar", "file://"},
      {"blob:http://example.org:88/foo/bar", "http://example.org:88"},
      {"blob:file://localhost/foo/bar", "file://localhost"},
  };

  for (const auto& test : kTestCases) {
    SCOPED_TRACE(test.url);
    scoped_refptr<SecurityOrigin> origin =
        SecurityOrigin::CreateFromString(test.url)->GetOriginForAgentCluster(
            agent_cluster_id);
    String expected_token;
    if (test.token)
      expected_token = test.token + String(agent_cluster_id.ToString().c_str());
    EXPECT_EQ(expected_token, origin->ToTokenForFastCheck()) << expected_token;
  }
}

TEST_F(SecurityOriginTest, OpaqueIsolatedCopy) {
  scoped_refptr<const SecurityOrigin> origin =
      SecurityOrigin::CreateUniqueOpaque();
  scoped_refptr<const SecurityOrigin> copied = origin->IsolatedCopy();
  EXPECT_TRUE(origin->CanAccess(copied.get()));
  EXPECT_TRUE(origin->IsSameOriginWith(copied.get()));
  EXPECT_EQ(WTF::GetHash(origin), WTF::GetHash(copied));
  EXPECT_TRUE(
      HashTraits<scoped_refptr<const SecurityOrigin>>::Equal(origin, copied));
}

TEST_F(SecurityOriginTest, EdgeCases) {
  scoped_refptr<SecurityOrigin> nulled_domain =
      SecurityOrigin::CreateFromString("http://localhost");
  nulled_domain->SetDomainFromDOM("null");
  EXPECT_TRUE(nulled_domain->CanAccess(nulled_domain.get()));

  scoped_refptr<SecurityOrigin> local =
      SecurityOrigin::CreateFromString("file:///foo/bar");
  local->BlockLocalAccessFromLocalOrigin();
  EXPECT_TRUE(local->IsSameOriginWith(local.get()));
}

TEST_F(SecurityOriginTest, RegistrableDomain) {
  scoped_refptr<SecurityOrigin> opaque = SecurityOrigin::CreateUniqueOpaque();
  EXPECT_TRUE(opaque->RegistrableDomain().IsNull());

  scoped_refptr<SecurityOrigin> ip_address =
      SecurityOrigin::CreateFromString("http://0.0.0.0");
  EXPECT_TRUE(ip_address->RegistrableDomain().IsNull());

  scoped_refptr<SecurityOrigin> public_suffix =
      SecurityOrigin::CreateFromString("http://com");
  EXPECT_TRUE(public_suffix->RegistrableDomain().IsNull());

  scoped_refptr<SecurityOrigin> registrable =
      SecurityOrigin::CreateFromString("http://example.com");
  EXPECT_EQ(String("example.com"), registrable->RegistrableDomain());

  scoped_refptr<SecurityOrigin> subdomain =
      SecurityOrigin::CreateFromString("http://foo.example.com");
  EXPECT_EQ(String("example.com"), subdomain->RegistrableDomain());
}

TEST_F(SecurityOriginTest, IsSameOriginWith) {
  struct TestCase {
    bool same_origin;
    const char* a;
    const char* b;
  } tests[] = {{true, "https://a.com", "https://a.com"},

               // Schemes
               {false, "https://a.com", "http://a.com"},

               // Hosts
               {false, "https://a.com", "https://not-a.com"},
               {false, "https://a.com", "https://sub.a.com"},

               // Ports
               {true, "https://a.com", "https://a.com:443"},
               {false, "https://a.com", "https://a.com:444"},
               {false, "https://a.com:442", "https://a.com:443"},

               // Opaque
               {false, "data:text/html,whatever", "data:text/html,whatever"}};

  for (const auto& test : tests) {
    SCOPED_TRACE(testing::Message() << "Origin 1: `" << test.a << "` "
                                    << "Origin 2: `" << test.b << "`\n");
    scoped_refptr<SecurityOrigin> a = SecurityOrigin::CreateFromString(test.a);
    scoped_refptr<SecurityOrigin> b = SecurityOrigin::CreateFromString(test.b);
    EXPECT_EQ(test.same_origin, a->IsSameOriginWith(b.get()));
    EXPECT_EQ(test.same_origin, b->IsSameOriginWith(a.get()));

    // Self-comparison
    EXPECT_TRUE(a->IsSameOriginWith(a.get()));
    EXPECT_TRUE(b->IsSameOriginWith(b.get()));

    // DeriveNewOpaqueOrigin
    EXPECT_FALSE(a->DeriveNewOpaqueOrigin()->IsSameOriginWith(a.get()));
    EXPECT_FALSE(b->DeriveNewOpaqueOrigin()->IsSameOriginWith(a.get()));
    EXPECT_FALSE(a->DeriveNewOpaqueOrigin()->IsSameOriginWith(b.get()));
    EXPECT_FALSE(b->DeriveNewOpaqueOrigin()->IsSameOriginWith(b.get()));
    EXPECT_FALSE(a->IsSameOriginWith(a->DeriveNewOpaqueOrigin().get()));
    EXPECT_FALSE(b->IsSameOriginWith(a->DeriveNewOpaqueOrigin().get()));
    EXPECT_FALSE(a->IsSameOriginWith(b->DeriveNewOpaqueOrigin().get()));
    EXPECT_FALSE(b->IsSameOriginWith(b->DeriveNewOpaqueOrigin().get()));
    EXPECT_FALSE(a->DeriveNewOpaqueOrigin()->IsSameOriginWith(
        a->DeriveNewOpaqueOrigin().get()));
    EXPECT_FALSE(b->DeriveNewOpaqueOrigin()->IsSameOriginWith(
        b->DeriveNewOpaqueOrigin().get()));

    // UniversalAccess does not change the result.
    a->GrantUniversalAccess();
    EXPECT_EQ(test.same_origin, a->IsSameOriginWith(b.get()));
    EXPECT_EQ(test.same_origin, b->IsSameOriginWith(a.get()));
  }
}

TEST_F(SecurityOriginTest, IsSameOriginWithWithLocalScheme) {
  scoped_refptr<SecurityOrigin> a =
      SecurityOrigin::CreateFromString("file:///etc/passwd");
  scoped_refptr<SecurityOrigin> b =
      SecurityOrigin::CreateFromString("file:///etc/hosts");

  // Self-comparison
  EXPECT_TRUE(a->IsSameOriginWith(a.get()));
  EXPECT_TRUE(b->IsSameOriginWith(b.get()));

  // block_local_access_from_local_origin_ defaults to `false`:
  EXPECT_TRUE(a->IsSameOriginWith(b.get()));
  EXPECT_TRUE(b->IsSameOriginWith(a.get()));

  // DeriveNewOpaqueOrigin
  EXPECT_FALSE(a->DeriveNewOpaqueOrigin()->IsSameOriginWith(a.get()));
  EXPECT_FALSE(b->DeriveNewOpaqueOrigin()->IsSameOriginWith(a.get()));
  EXPECT_FALSE(a->DeriveNewOpaqueOrigin()->IsSameOriginWith(b.get()));
  EXPECT_FALSE(b->DeriveNewOpaqueOrigin()->IsSameOriginWith(b.get()));
  EXPECT_FALSE(a->IsSameOriginWith(a->DeriveNewOpaqueOrigin().get()));
  EXPECT_FALSE(b->IsSameOriginWith(a->DeriveNewOpaqueOrigin().get()));
  EXPECT_FALSE(a->IsSameOriginWith(b->DeriveNewOpaqueOrigin().get()));
  EXPECT_FALSE(b->IsSameOriginWith(b->DeriveNewOpaqueOrigin().get()));
  EXPECT_FALSE(a->DeriveNewOpaqueOrigin()->IsSameOriginWith(
      a->DeriveNewOpaqueOrigin().get()));
  EXPECT_FALSE(b->DeriveNewOpaqueOrigin()->IsSameOriginWith(
      b->DeriveNewOpaqueOrigin().get()));

  // Set block_local_access_from_local_origin_ to `true`:
  a->BlockLocalAccessFromLocalOrigin();
  EXPECT_FALSE(a->IsSameOriginWith(b.get()));
  EXPECT_FALSE(b->IsSameOriginWith(a.get()));

  // Self-comparison should still be true.
  EXPECT_TRUE(a->IsSameOriginWith(a.get()));
  EXPECT_TRUE(b->IsSameOriginWith(b.get()));

  // UniversalAccess does not override
  a->GrantUniversalAccess();
  EXPECT_FALSE(a->IsSameOriginWith(b.get()));
  EXPECT_FALSE(b->IsSameOriginWith(a.get()));
}

TEST_F(SecurityOriginTest, IsSameOriginDomainWith) {
  struct TestCase {
    bool same_origin_domain;
    const char* a;
    const char* domain_a;  // empty string === no `domain` set.
    const char* b;
    const char* domain_b;
  } tests[] = {
      {true, "https://a.com", "", "https://a.com", ""},
      {false, "https://a.com", "a.com", "https://a.com", ""},
      {true, "https://a.com", "a.com", "https://a.com", "a.com"},
      {false, "https://sub.a.com", "", "https://a.com", ""},
      {false, "https://sub.a.com", "a.com", "https://a.com", ""},
      {true, "https://sub.a.com", "a.com", "https://a.com", "a.com"},
      {true, "https://sub.a.com", "a.com", "https://sub.a.com", "a.com"},
      {true, "https://sub.a.com", "a.com", "https://sub.sub.a.com", "a.com"},

      // Schemes.
      {false, "https://a.com", "", "http://a.com", ""},
      {false, "https://a.com", "a.com", "http://a.com", ""},
      {false, "https://a.com", "a.com", "http://a.com", "a.com"},
      {false, "https://sub.a.com", "a.com", "http://a.com", ""},
      {false, "https://a.com", "a.com", "http://sub.a.com", "a.com"},

      // Ports? Why would they matter?
      {true, "https://a.com:443", "", "https://a.com", ""},
      {false, "https://a.com:444", "", "https://a.com", ""},
      {false, "https://a.com:444", "", "https://a.com:442", ""},

      {false, "https://a.com:443", "a.com", "https://a.com", ""},
      {false, "https://a.com:444", "a.com", "https://a.com", ""},
      {false, "https://a.com:444", "a.com", "https://a.com:442", ""},

      {true, "https://a.com:443", "a.com", "https://a.com", "a.com"},
      {true, "https://a.com:444", "a.com", "https://a.com", "a.com"},
      {true, "https://a.com:444", "a.com", "https://a.com:442", "a.com"},

      {false, "https://sub.a.com:443", "", "https://a.com", ""},
      {false, "https://sub.a.com:444", "", "https://a.com", ""},
      {false, "https://sub.a.com:444", "", "https://a.com:442", ""},

      {false, "https://sub.a.com:443", "a.com", "https://a.com", ""},
      {false, "https://sub.a.com:444", "a.com", "https://a.com", ""},
      {false, "https://sub.a.com:444", "a.com", "https://a.com:442", ""},

      {true, "https://sub.a.com:443", "a.com", "https://a.com", "a.com"},
      {true, "https://sub.a.com:444", "a.com", "https://a.com", "a.com"},
      {true, "https://sub.a.com:444", "a.com", "https://a.com:442", "a.com"},
  };

  for (const auto& test : tests) {
    SCOPED_TRACE(testing::Message()
                 << "Origin 1: `" << test.a << "` (`" << test.domain_a << "`)\n"
                 << "Origin 2: `" << test.b << "` (`" << test.domain_b
                 << "`)\n");
    scoped_refptr<SecurityOrigin> a = SecurityOrigin::CreateFromString(test.a);
    if (strlen(test.domain_a))
      a->SetDomainFromDOM(test.domain_a);
    scoped_refptr<SecurityOrigin> b = SecurityOrigin::CreateFromString(test.b);
    if (strlen(test.domain_b))
      b->SetDomainFromDOM(test.domain_b);
    EXPECT_EQ(test.same_origin_domain, a->IsSameOriginDomainWith(b.get()));
    EXPECT_EQ(test.same_origin_domain, b->IsSameOriginDomainWith(a.get()));

    // Self-comparison
    EXPECT_TRUE(a->IsSameOriginDomainWith(a.get()));
    EXPECT_TRUE(b->IsSameOriginDomainWith(b.get()));

    // DeriveNewOpaqueOrigin
    EXPECT_FALSE(a->DeriveNewOpaqueOrigin()->IsSameOriginDomainWith(a.get()));
    EXPECT_FALSE(b->DeriveNewOpaqueOrigin()->IsSameOriginDomainWith(a.get()));
    EXPECT_FALSE(a->DeriveNewOpaqueOrigin()->IsSameOriginDomainWith(b.get()));
    EXPECT_FALSE(b->DeriveNewOpaqueOrigin()->IsSameOriginDomainWith(b.get()));
    EXPECT_FALSE(a->IsSameOriginDomainWith(a->DeriveNewOpaqueOrigin().get()));
    EXPECT_FALSE(b->IsSameOriginDomainWith(a->DeriveNewOpaqueOrigin().get()));
    EXPECT_FALSE(a->IsSameOriginDomainWith(b->DeriveNewOpaqueOrigin().get()));
    EXPECT_FALSE(b->IsSameOriginDomainWith(b->DeriveNewOpaqueOrigin().get()));
    EXPECT_FALSE(a->DeriveNewOpaqueOrigin()->IsSameOriginDomainWith(
        a->DeriveNewOpaqueOrigin().get()));
    EXPECT_FALSE(b->DeriveNewOpaqueOrigin()->IsSameOriginDomainWith(
        b->DeriveNewOpaqueOrigin().get()));

    // UniversalAccess does not override.
    a->GrantUniversalAccess();
    EXPECT_EQ(test.same_origin_domain, a->IsSameOriginDomainWith(b.get()));
    EXPECT_EQ(test.same_origin_domain, b->IsSameOriginDomainWith(a.get()));
  }
}

TEST_F(SecurityOriginTest, IsSameOriginDomainWithWithLocalScheme) {
  scoped_refptr<SecurityOrigin> a =
      SecurityOrigin::CreateFromString("file:///etc/passwd");
  scoped_refptr<SecurityOrigin> b =
      SecurityOrigin::CreateFromString("file:///etc/hosts");

  // Self-comparison
  EXPECT_TRUE(a->IsSameOriginDomainWith(a.get()));
  EXPECT_TRUE(b->IsSameOriginDomainWith(b.get()));

  // block_local_access_from_local_origin_ defaults to `false`:
  EXPECT_TRUE(a->IsSameOriginDomainWith(b.get()));
  EXPECT_TRUE(b->IsSameOriginDomainWith(a.get()));

  // DeriveNewOpaqueOrigin
  EXPECT_FALSE(a->DeriveNewOpaqueOrigin()->IsSameOriginDomainWith(a.get()));
  EXPECT_FALSE(b->DeriveNewOpaqueOrigin()->IsSameOriginDomainWith(a.get()));
  EXPECT_FALSE(a->DeriveNewOpaqueOrigin()->IsSameOriginDomainWith(b.get()));
  EXPECT_FALSE(b->DeriveNewOpaqueOrigin()->IsSameOriginDomainWith(b.get()));
  EXPECT_FALSE(a->IsSameOriginDomainWith(a->DeriveNewOpaqueOrigin().get()));
  EXPECT_FALSE(b->IsSameOriginDomainWith(a->DeriveNewOpaqueOrigin().get()));
  EXPECT_FALSE(a->IsSameOriginDomainWith(b->DeriveNewOpaqueOrigin().get()));
  EXPECT_FALSE(b->IsSameOriginDomainWith(b->DeriveNewOpaqueOrigin().get()));
  EXPECT_FALSE(a->DeriveNewOpaqueOrigin()->IsSameOriginDomainWith(
      a->DeriveNewOpaqueOrigin().get()));
  EXPECT_FALSE(b->DeriveNewOpaqueOrigin()->IsSameOriginDomainWith(
      b->DeriveNewOpaqueOrigin().get()));

  // Set block_local_access_from_local_origin_ to `true`:
  a->BlockLocalAccessFromLocalOrigin();
  EXPECT_FALSE(a->IsSameOriginDomainWith(b.get()));
  EXPECT_FALSE(b->IsSameOriginDomainWith(a.get()));

  // Self-comparison should still be true.
  EXPECT_TRUE(a->IsSameOriginDomainWith(a.get()));
  EXPECT_TRUE(b->IsSameOriginDomainWith(b.get()));

  // UniversalAccess does not override
  a->GrantUniversalAccess();
  EXPECT_FALSE(a->IsSameOriginDomainWith(b.get()));
  EXPECT_FALSE(b->IsSameOriginDomainWith(a.get()));
}

TEST_F(SecurityOriginTest, IsSameSiteWith) {
  struct TestCase {
    bool same_site;
    const char* a;
    const char* b;
  } tests[] = {
      // Same tuple origin.
      {true, "https://a.com", "https://a.com"},
      // Same registrable domain.
      {true, "https://a.com", "https://sub.a.com"},
      {true, "https://sub1.a.com", "https://sub2.a.com"},
      // Schemes differ.
      {false, "https://a.com", "http://a.com"},
      {false, "https://a.com", "wss://a.com"},
      // Registrable domains differ.
      {false, "https://a.com", "https://b.com"},
      {false, "https://sub.a.com", "https://sub.b.com"},
      {false, "https://a.com", "https://aaaaa.com"},
      // If there is no registrable domain, the hosts must match.
      {true, "https://com", "https://com"},
      {true, "https://123.4.5.6:788", "https://123.4.5.6:789"},
      // Ports don't matter.
      {true, "https://a.com:443", "https://a.com:444"},
      // Opaque vs tuple origins cannot be same site.
      {false, "data:text/html,whatever", "https://a.com"},
      // Two different opaque origins cannot be same site.
      {false, "data:text/html,whatever", "data:text/html,whatever"},
  };

  for (const auto& test : tests) {
    SCOPED_TRACE(testing::Message() << "Origin 1: `" << test.a << "` "
                                    << "Origin 2: `" << test.b << "`\n");
    scoped_refptr<SecurityOrigin> a = SecurityOrigin::CreateFromString(test.a);
    scoped_refptr<SecurityOrigin> b = SecurityOrigin::CreateFromString(test.b);
    EXPECT_EQ(test.same_site, a->IsSameSiteWith(b.get()));
    EXPECT_EQ(test.same_site, b->IsSameSiteWith(a.get()));

    // Self-comparison
    EXPECT_TRUE(a->IsSameSiteWith(a.get()));
    EXPECT_TRUE(b->IsSameSiteWith(b.get()));

    // DeriveNewOpaqueOrigin
    EXPECT_FALSE(a->DeriveNewOpaqueOrigin()->IsSameSiteWith(a.get()));
    EXPECT_FALSE(b->DeriveNewOpaqueOrigin()->IsSameSiteWith(a.get()));
    EXPECT_FALSE(a->DeriveNewOpaqueOrigin()->IsSameSiteWith(b.get()));
    EXPECT_FALSE(b->DeriveNewOpaqueOrigin()->IsSameSiteWith(b.get()));
    EXPECT_FALSE(a->IsSameSiteWith(a->DeriveNewOpaqueOrigin().get()));
    EXPECT_FALSE(b->IsSameSiteWith(a->DeriveNewOpaqueOrigin().get()));
    EXPECT_FALSE(a->IsSameSiteWith(b->DeriveNewOpaqueOrigin().get()));
    EXPECT_FALSE(b->IsSameSiteWith(b->DeriveNewOpaqueOrigin().get()));
    EXPECT_FALSE(a->DeriveNewOpaqueOrigin()->IsSameSiteWith(
        a->DeriveNewOpaqueOrigin().get()));
    EXPECT_FALSE(b->DeriveNewOpaqueOrigin()->IsSameSiteWith(
        b->DeriveNewOpaqueOrigin().get()));

    // UniversalAccess does not change the result.
    a->GrantUniversalAccess();
    EXPECT_EQ(test.same_site, a->IsSameSiteWith(b.get()));
    EXPECT_EQ(test.same_site, b->IsSameSiteWith(a.get()));
  }

  // Identical opaque origins are same site.
  scoped_refptr<SecurityOrigin> opaque = SecurityOrigin::CreateUniqueOpaque();
  scoped_refptr<SecurityOrigin> opaque_copy = opaque->IsolatedCopy();
  EXPECT_TRUE(opaque->IsSameSiteWith(opaque_copy.get()));
  EXPECT_TRUE(opaque_copy->IsSameSiteWith(opaque.get()));
}

TEST_F(SecurityOriginTest, IsSameSiteWithWithLocalScheme) {
  scoped_refptr<SecurityOrigin> a =
      SecurityOrigin::CreateFromString("file:///etc/passwd");
  scoped_refptr<SecurityOrigin> b =
      SecurityOrigin::CreateFromString("file:///etc/hosts");

  // Self-comparison
  EXPECT_TRUE(a->IsSameSiteWith(a.get()));
  EXPECT_TRUE(b->IsSameSiteWith(b.get()));

  // block_local_access_from_local_origin_ defaults to `false`:
  EXPECT_TRUE(a->IsSameSiteWith(b.get()));
  EXPECT_TRUE(b->IsSameSiteWith(a.get()));

  // DeriveNewOpaqueOrigin
  EXPECT_FALSE(a->DeriveNewOpaqueOrigin()->IsSameSiteWith(a.get()));
  EXPECT_FALSE(b->DeriveNewOpaqueOrigin()->IsSameSiteWith(a.get()));
  EXPECT_FALSE(a->DeriveNewOpaqueOrigin()->IsSameSiteWith(b.get()));
  EXPECT_FALSE(b->DeriveNewOpaqueOrigin()->IsSameSiteWith(b.get()));
  EXPECT_FALSE(a->IsSameSiteWith(a->DeriveNewOpaqueOrigin().get()));
  EXPECT_FALSE(b->IsSameSiteWith(a->DeriveNewOpaqueOrigin().get()));
  EXPECT_FALSE(a->IsSameSiteWith(b->DeriveNewOpaqueOrigin().get()));
  EXPECT_FALSE(b->IsSameSiteWith(b->DeriveNewOpaqueOrigin().get()));
  EXPECT_FALSE(a->DeriveNewOpaqueOrigin()->IsSameSiteWith(
      a->DeriveNewOpaqueOrigin().get()));
  EXPECT_FALSE(b->DeriveNewOpaqueOrigin()->IsSameSiteWith(
      b->DeriveNewOpaqueOrigin().get()));

  // Set block_local_access_from_local_origin_ to `true`:
  // They are still same site because the schemes and hosts are the same.
  a->BlockLocalAccessFromLocalOrigin();
  EXPECT_TRUE(a->IsSameSiteWith(b.get()));
  EXPECT_TRUE(b->IsSameSiteWith(a.get()));

  // Self-comparison should still be true.
  EXPECT_TRUE(a->IsSameSiteWith(a.get()));
  EXPECT_TRUE(b->IsSameSiteWith(b.get()));
}

// Non-canonical hosts provided to the string constructor should end up
// canonicalized:
TEST_F(SecurityOriginTest, PercentEncodesHost) {
  EXPECT_EQ(
      SecurityOrigin::CreateFromString("http://foo,.example.test/")->Host(),
      "foo,.example.test");

  EXPECT_EQ(
      SecurityOrigin::CreateFromString("http://foo%2C.example.test/")->Host(),
      "foo,.example.test");
}

TEST_F(SecurityOriginTest, NewOpaqueOriginLazyInitsNonce) {
  scoped_refptr<SecurityOrigin> opaque_origin =
      SecurityOrigin::CreateUniqueOpaque();

  scoped_refptr<SecurityOrigin> tuple_origin =
      SecurityOrigin::Create(KURL("https://example.com/"));
  scoped_refptr<SecurityOrigin> derived_opaque_origin =
      tuple_origin->DeriveNewOpaqueOrigin();

  EXPECT_TRUE(opaque_origin->IsOpaque());
  // There should be a nonce...
  EXPECT_TRUE(GetNonceForOrigin(*opaque_origin).has_value());
  // ...but it should not be initialised yet.
  EXPECT_TRUE(GetNonceForOrigin(*opaque_origin)->raw_token().is_empty());

  EXPECT_TRUE(derived_opaque_origin->IsOpaque());
  // There should be a nonce...
  EXPECT_TRUE(GetNonceForOrigin(*derived_opaque_origin).has_value());
  // ...but it should not be initialised yet.
  EXPECT_TRUE(
      GetNonceForOrigin(*derived_opaque_origin)->raw_token().is_empty());

  // Even checking CanAccess does not need to trigger initialisation: two
  // uninitialised nonces can only be equal if they are the same object.
  EXPECT_TRUE(opaque_origin->CanAccess(opaque_origin));
  EXPECT_FALSE(opaque_origin->CanAccess(derived_opaque_origin));
  EXPECT_TRUE(derived_opaque_origin->CanAccess(derived_opaque_origin));

  EXPECT_TRUE(GetNonceForOrigin(*opaque_origin)->raw_token().is_empty());
  EXPECT_TRUE(
      GetNonceForOrigin(*derived_opaque_origin)->raw_token().is_empty());

  // However, forcing the nonce to be serialized should trigger initialisation.
  (void)GetNonceForSerializationForOrigin(*opaque_origin);
  (void)GetNonceForSerializationForOrigin(*derived_opaque_origin);

  EXPECT_FALSE(GetNonceForOrigin(*opaque_origin)->raw_token().is_empty());
  EXPECT_FALSE(
      GetNonceForOrigin(*derived_opaque_origin)->raw_token().is_empty());
}

}  // namespace blink

// Apparently INSTANTIATE_TYPED_TEST_SUITE_P needs to be used in the same
// namespace as where the typed test suite was defined.
namespace url {

class BlinkSecurityOriginTestTraits {
 public:
  using OriginType = scoped_refptr<blink::SecurityOrigin>;

  static OriginType CreateOriginFromString(std::string_view s) {
    return blink::SecurityOrigin::CreateFromString(String::FromUTF8(s));
  }

  static OriginType CreateUniqueOpaqueOrigin() {
    return blink::SecurityOrigin::CreateUniqueOpaque();
  }

  static OriginType CreateWithReferenceOrigin(
      std::string_view url,
      const OriginType& reference_origin) {
    return blink::SecurityOrigin::CreateWithReferenceOrigin(
        blink::KURL(String::FromUTF8(url)), reference_origin.get());
  }

  static OriginType DeriveNewOpaqueOrigin(const OriginType& reference_origin) {
    return reference_origin->DeriveNewOpaqueOrigin();
  }

  static bool IsOpaque(const OriginType& origin) { return origin->IsOpaque(); }

  static std::string GetScheme(const OriginType& origin) {
    return origin->Protocol().Utf8();
  }

  static std::string GetHost(const OriginType& origin) {
    return origin->Host().Utf8();
  }

  static uint16_t GetPort(const OriginType& origin) { return origin->Port(); }

  static SchemeHostPort GetTupleOrPrecursorTupleIfOpaque(
      const OriginType& origin) {
    const blink::SecurityOrigin* precursor =
        origin->GetOriginOrPrecursorOriginIfOpaque();
    if (!precursor)
      return SchemeHostPort();
    return SchemeHostPort(precursor->Protocol().Utf8(),
                          precursor->Host().Utf8(), precursor->Port());
  }

  static bool IsSameOrigin(const OriginType& a, const OriginType& b) {
    return a->IsSameOriginWith(b.get());
  }

  static std::string Serialize(const OriginType& origin) {
    return origin->ToString().Utf8();
  }

  static bool IsValidUrl(std::string_view str) {
    return blink::KURL(String::FromUTF8(str)).IsValid();
  }

  static bool IsOriginPotentiallyTrustworthy(const OriginType& origin) {
    return origin->IsPotentiallyTrustworthy();
  }

  static bool IsUrlPotentiallyTrustworthy(std::string_view str) {
    // Note: intentionally avoid constructing GURL() directly from `str`, since
    // this is a test harness intended to exercise the behavior of `KURL` and
    // `SecurityOrigin`.
    return network::IsUrlPotentiallyTrustworthy(
        GURL(blink::KURL(String::FromUTF8(str))));
  }

  static bool IsOriginOfLocalhost(const OriginType& origin) {
    return origin->IsLocalhost();
  }

  // Only static members = no constructors are needed.
  BlinkSecurityOriginTestTraits() = delete;
};

INSTANTIATE_TYPED_TEST_SUITE_P(BlinkSecurityOrigin,
                               AbstractOriginTest,
                               BlinkSecurityOriginTestTraits);

}  // namespace url

// Apparently INSTANTIATE_TYPED_TEST_SUITE_P needs to be used in the same
// namespace as where the typed test suite was defined.
namespace network {
namespace test {

INSTANTIATE_TYPED_TEST_SUITE_P(BlinkSecurityOrigin,
                               AbstractTrustworthinessTest,
                               url::BlinkSecurityOriginTestTraits);

}  // namespace test
}  // namespace network
