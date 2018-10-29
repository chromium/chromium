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

#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

#include <stdint.h>

#include "services/network/public/mojom/cors.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/blob/blob_url.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace blink {

const uint16_t kMaxAllowedPort = UINT16_MAX;

class SecurityOriginTest : public testing::Test {};

TEST_F(SecurityOriginTest, ValidPortsCreateTupleOrigins) {
  uint16_t ports[] = {0, 80, 443, 5000, kMaxAllowedPort};

  for (size_t i = 0; i < arraysize(ports); ++i) {
    scoped_refptr<const SecurityOrigin> origin =
        SecurityOrigin::Create("http", "example.com", ports[i]);
    EXPECT_FALSE(origin->IsOpaque())
        << "Port " << ports[i] << " should have generated a tuple origin.";
  }
}

TEST_F(SecurityOriginTest, LocalAccess) {
  scoped_refptr<SecurityOrigin> file1 =
      SecurityOrigin::CreateFromString("file:///etc/passwd");
  scoped_refptr<const SecurityOrigin> file2 =
      SecurityOrigin::CreateFromString("file:///etc/shadow");

  EXPECT_TRUE(file1->IsSameSchemeHostPort(file1.get()));
  EXPECT_TRUE(file1->IsSameSchemeHostPort(file2.get()));
  EXPECT_TRUE(file2->IsSameSchemeHostPort(file1.get()));

  EXPECT_TRUE(file1->CanAccess(file1.get()));
  EXPECT_TRUE(file1->CanAccess(file2.get()));
  EXPECT_TRUE(file2->CanAccess(file1.get()));

  // Block |file1|'s access to local origins. It should now be same-origin
  // with itself, but shouldn't have access to |file2|.
  file1->BlockLocalAccessFromLocalOrigin();
  EXPECT_TRUE(file1->IsSameSchemeHostPort(file1.get()));
  EXPECT_FALSE(file1->IsSameSchemeHostPort(file2.get()));
  EXPECT_FALSE(file2->IsSameSchemeHostPort(file1.get()));

  EXPECT_TRUE(file1->CanAccess(file1.get()));
  EXPECT_FALSE(file1->CanAccess(file2.get()));
  EXPECT_FALSE(file2->CanAccess(file1.get()));
}

TEST_F(SecurityOriginTest, IsPotentiallyTrustworthy) {
  struct TestCase {
    bool access_granted;
    const char* url;
  };

  TestCase inputs[] = {
      // Access is granted to webservers running on localhost.
      {true, "http://localhost"},
      {true, "http://LOCALHOST"},
      {true, "http://localhost:100"},
      {true, "http://a.localhost"},
      {true, "http://127.0.0.1"},
      {true, "http://127.0.0.2"},
      {true, "http://127.1.0.2"},
      {true, "http://0177.00.00.01"},
      {true, "http://[::1]"},
      {true, "http://[0:0::1]"},
      {true, "http://[0:0:0:0:0:0:0:1]"},
      {true, "http://[::1]:21"},
      {true, "http://127.0.0.1:8080"},
      {true, "ftp://127.0.0.1"},
      {true, "ftp://127.0.0.1:443"},
      {true, "ws://127.0.0.1"},

      // Access is denied to non-localhost over HTTP
      {false, "http://[1::]"},
      {false, "http://[::2]"},
      {false, "http://[1::1]"},
      {false, "http://[1:2::3]"},
      {false, "http://[::127.0.0.1]"},
      {false, "http://a.127.0.0.1"},
      {false, "http://127.0.0.1.b"},
      {false, "http://localhost.a"},

      // Access is granted to all secure transports.
      {true, "https://foobar.com"},
      {true, "wss://foobar.com"},

      // Access is denied to insecure transports.
      {false, "ftp://foobar.com"},
      {false, "http://foobar.com"},
      {false, "http://foobar.com:443"},
      {false, "ws://foobar.com"},

      // Access is granted to local files
      {true, "file:///home/foobar/index.html"},

      // blob: URLs must look to the inner URL's origin, and apply the same
      // rules as above. Spot check some of them
      {true, "blob:http://localhost:1000/578223a1-8c13-17b3-84d5-eca045ae384a"},
      {true, "blob:https://foopy:99/578223a1-8c13-17b3-84d5-eca045ae384a"},
      {false, "blob:http://baz:99/578223a1-8c13-17b3-84d5-eca045ae384a"},
      {false, "blob:ftp://evil:99/578223a1-8c13-17b3-84d5-eca045ae384a"},

      // filesystem: URLs work the same as blob: URLs, and look to the inner
      // URL for security origin.
      {true, "filesystem:http://localhost:1000/foo"},
      {true, "filesystem:https://foopy:99/foo"},
      {false, "filesystem:http://baz:99/foo"},
      {false, "filesystem:ftp://evil:99/foo"},
  };

  for (size_t i = 0; i < arraysize(inputs); ++i) {
    SCOPED_TRACE(inputs[i].url);
    scoped_refptr<const SecurityOrigin> origin =
        SecurityOrigin::CreateFromString(inputs[i].url);
    String error_message;
    EXPECT_EQ(inputs[i].access_granted, origin->IsPotentiallyTrustworthy());
  }

  // Anonymous opaque origins are not considered secure.
  scoped_refptr<SecurityOrigin> opaque_origin =
      SecurityOrigin::CreateUniqueOpaque();
  EXPECT_FALSE(opaque_origin->IsPotentiallyTrustworthy());
}

TEST_F(SecurityOriginTest, IsSecure) {
  struct TestCase {
    bool is_secure;
    const char* url;
  } inputs[] = {
      {false, "blob:ftp://evil:99/578223a1-8c13-17b3-84d5-eca045ae384a"},
      {false, "blob:http://example.com/578223a1-8c13-17b3-84d5-eca045ae384a"},
      {false, "file:///etc/passwd"},
      {false, "ftp://example.com/"},
      {false, "http://example.com/"},
      {false, "ws://example.com/"},
      {true, "blob:https://example.com/578223a1-8c13-17b3-84d5-eca045ae384a"},
      {true, "https://example.com/"},
      {true, "wss://example.com/"},

      {true, "about:blank"},
      {false, ""},
      {false, "\0"},
  };

  for (auto test : inputs)
    EXPECT_EQ(test.is_secure, SecurityOrigin::IsSecure(KURL(test.url)))
        << "URL: '" << test.url << "'";

  EXPECT_FALSE(SecurityOrigin::IsSecure(NullURL()));
}

TEST_F(SecurityOriginTest, IsSecureViaTrustworthy) {
  const char* urls[] = {"http://localhost/", "http://localhost:8080/",
                        "http://127.0.0.1/", "http://127.0.0.1:8080/",
                        "http://[::1]/"};

  for (const char* test : urls) {
    KURL url(test);
    EXPECT_FALSE(SecurityOrigin::IsSecure(url));
    SecurityPolicy::AddOriginTrustworthyWhiteList(
        SecurityOrigin::CreateFromString(url)->ToRawString());
    EXPECT_TRUE(SecurityOrigin::IsSecure(url));
  }
}

TEST_F(SecurityOriginTest, IsSecureViaTrustworthyHostnamePattern) {
  KURL url("http://bar.foo.com");
  EXPECT_FALSE(SecurityOrigin::IsSecure(url));
  SecurityPolicy::AddOriginTrustworthyWhiteList("*.foo.com");
  EXPECT_TRUE(SecurityOrigin::IsSecure(url));
}

// Tests that a URL with no host does not match a hostname pattern.
TEST_F(SecurityOriginTest, IsSecureViaTrustworthyHostnamePatternEmptyHostname) {
  KURL url("file://foo");
  EXPECT_FALSE(SecurityOrigin::IsSecure(url));
  SecurityPolicy::AddOriginTrustworthyWhiteList("*.foo.com");
  EXPECT_FALSE(SecurityOrigin::IsSecure(url));
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

  for (size_t i = 0; i < arraysize(tests); ++i) {
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

  for (size_t i = 0; i < arraysize(tests); ++i) {
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
      *origin, "https", "example.com", false,
      network::mojom::CORSOriginAccessMatchPriority::kMediumPriority);
  EXPECT_TRUE(origin->CanRequest(url));
}

TEST_F(SecurityOriginTest, CannotRequestWithBlockListedAccess) {
  scoped_refptr<const SecurityOrigin> origin =
      SecurityOrigin::CreateFromString("https://chromium.org");
  const blink::KURL allowed_url("https://test.example.com");
  const blink::KURL blocked_url("https://example.com");

  // BlockList that is more or same specificity wins.
  SecurityPolicy::AddOriginAccessAllowListEntry(
      *origin, "https", "example.com", true,
      network::mojom::CORSOriginAccessMatchPriority::kDefaultPriority);
  SecurityPolicy::AddOriginAccessBlockListEntry(
      *origin, "https", "example.com", false,
      network::mojom::CORSOriginAccessMatchPriority::kLowPriority);
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
      *origin, "https", "test.example.com", true,
      network::mojom::CORSOriginAccessMatchPriority::kMediumPriority);
  SecurityPolicy::AddOriginAccessBlockListEntry(
      *origin, "https", "example.com", true,
      network::mojom::CORSOriginAccessMatchPriority::kLowPriority);
  // Allow since test.example.com (allowlist) has a higher priority than
  // *.example.com (blocklist).
  EXPECT_TRUE(origin->CanRequest(allowed_url));
  // Block since example.com isn't on the allowlist.
  EXPECT_FALSE(origin->CanRequest(blocked_url));
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
      *origin, "https", "☃.net", true,
      network::mojom::CORSOriginAccessMatchPriority::kMediumPriority);
  EXPECT_FALSE(origin->CanRequest(punycode_url));
  EXPECT_FALSE(origin->CanRequest(unicode_url));

  // Verify punycode allowlist only affects punycode URLs.
  SecurityPolicy::AddOriginAccessAllowListEntry(
      *origin, "https", "xn--n3h.net", true,
      network::mojom::CORSOriginAccessMatchPriority::kMediumPriority);
  EXPECT_TRUE(origin->CanRequest(punycode_url));
  EXPECT_FALSE(origin->CanRequest(unicode_url));

  // Clear enterprise policy allowlist.
  SecurityPolicy::ClearOriginAccessAllowListForOrigin(*origin);

  EXPECT_FALSE(origin->CanRequest(punycode_url));
  EXPECT_FALSE(origin->CanRequest(unicode_url));

  // Simulate <all_urls> being in the extension permissions.
  SecurityPolicy::AddOriginAccessAllowListEntry(
      *origin, "https", "", true,
      network::mojom::CORSOriginAccessMatchPriority::kDefaultPriority);

  EXPECT_TRUE(origin->CanRequest(punycode_url));
  EXPECT_FALSE(origin->CanRequest(unicode_url));

  // Verify unicode origin can not be blocklisted.
  SecurityPolicy::AddOriginAccessBlockListEntry(
      *origin, "https", "☃.net", true,
      network::mojom::CORSOriginAccessMatchPriority::kLowPriority);
  EXPECT_TRUE(origin->CanRequest(punycode_url));
  EXPECT_FALSE(origin->CanRequest(unicode_url));

  // Verify punycode blocklist only affects punycode URLs.
  SecurityPolicy::AddOriginAccessBlockListEntry(
      *origin, "https", "xn--n3h.net", true,
      network::mojom::CORSOriginAccessMatchPriority::kLowPriority);
  EXPECT_FALSE(origin->CanRequest(punycode_url));
  EXPECT_FALSE(origin->CanRequest(unicode_url));
}

TEST_F(SecurityOriginTest, PortAndEffectivePortMethod) {
  struct TestCase {
    unsigned short port;
    unsigned short effective_port;
    const char* origin;
  } cases[] = {
      {0, 80, "http://example.com"},
      {0, 80, "http://example.com:80"},
      {81, 81, "http://example.com:81"},
      {0, 443, "https://example.com"},
      {0, 443, "https://example.com:443"},
      {444, 444, "https://example.com:444"},
  };

  for (const auto& test : cases) {
    scoped_refptr<const SecurityOrigin> origin =
        SecurityOrigin::CreateFromString(test.origin);
    EXPECT_EQ(test.port, origin->Port());
    EXPECT_EQ(test.effective_port, origin->EffectivePort());
  }
}

TEST_F(SecurityOriginTest, CreateFromTuple) {
  struct TestCase {
    const char* scheme;
    const char* host;
    uint16_t port;
    const char* origin;
  } cases[] = {
      {"http", "example.com", 80, "http://example.com"},
      {"http", "example.com", 81, "http://example.com:81"},
      {"https", "example.com", 443, "https://example.com"},
      {"https", "example.com", 444, "https://example.com:444"},
      {"file", "", 0, "file://"},
      {"file", "example.com", 0, "file://"},
      {"gopher", "Foo.com", 70, "gopher://foo.com"},
  };

  for (const auto& test : cases) {
    scoped_refptr<const SecurityOrigin> origin =
        SecurityOrigin::Create(test.scheme, test.host, test.port);
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

TEST_F(SecurityOriginTest, OpaqueOriginIsSameSchemeHostPort) {
  scoped_refptr<const SecurityOrigin> opaque_origin =
      SecurityOrigin::CreateUniqueOpaque();
  scoped_refptr<const SecurityOrigin> tuple_origin =
      SecurityOrigin::CreateFromString("http://example.com");

  EXPECT_TRUE(opaque_origin->IsSameSchemeHostPort(opaque_origin.get()));
  EXPECT_FALSE(SecurityOrigin::CreateUniqueOpaque()->IsSameSchemeHostPort(
      opaque_origin.get()));
  EXPECT_FALSE(tuple_origin->IsSameSchemeHostPort(opaque_origin.get()));
  EXPECT_FALSE(opaque_origin->IsSameSchemeHostPort(tuple_origin.get()));
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
      {",", "%2C", true},
      {"💩", "xn--ls8h", true},
      {"[]", "[]", false},
      {"%yo", "%25yo", false},
  };

  for (const TestCase& test : cases) {
    SCOPED_TRACE(testing::Message() << "raw host: '" << test.host << "'");
    String host = String::FromUTF8(test.host);
    bool success = false;
    String canonical_host = SecurityOrigin::CanonicalizeHost(host, &success);
    EXPECT_EQ(test.canonical_output, canonical_host);
    EXPECT_EQ(test.expected_success, success);
  }
}

TEST_F(SecurityOriginTest, UrlOriginConversions) {
  url::AddLocalScheme("nonstandard-but-local");
  SchemeRegistry::RegisterURLSchemeAsLocal("nonstandard-but-local");
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

      // Nonstandard schemes.
      {"unrecognized-scheme://localhost/", "", "", 0, true},
      {"mailto:localhost/", "", "", 0, true},
      {"about:blank", "", "", 0, true},

      // Registered URLs
      {"ftp://example.com/", "ftp", "example.com", 21},
      // crbug.com/781342
      // Conversion doesn't work for gopher.
      // {"gopher://example.com/", "gopher", "example.com", 70},
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

      // Gopher:
      {"gopher://8u.9.Vx6/", "gopher", "8u.9.vx6", 70},
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
    EXPECT_EQ(security_origin_via_gurl->Port(),
              security_origin_via_kurl->Port());
    EXPECT_EQ(test_case.port, security_origin_via_gurl->EffectivePort());
    EXPECT_EQ(test_case.port, security_origin_via_kurl->EffectivePort());
    EXPECT_EQ(test_case.opaque, security_origin_via_gurl->IsOpaque());
    EXPECT_EQ(test_case.opaque, security_origin_via_kurl->IsOpaque());
    EXPECT_EQ(!test_case.opaque, security_origin_via_kurl->IsSameSchemeHostPort(
                                     security_origin_via_gurl.get()));
    EXPECT_EQ(!test_case.opaque, security_origin_via_gurl->IsSameSchemeHostPort(
                                     security_origin_via_kurl.get()));

    if (!test_case.opaque) {
      scoped_refptr<const SecurityOrigin> security_origin =
          SecurityOrigin::Create(test_case.scheme, test_case.host,
                                 test_case.port);
      EXPECT_TRUE(security_origin->IsSameSchemeHostPort(
          security_origin_via_gurl.get()));
      EXPECT_TRUE(security_origin->IsSameSchemeHostPort(
          security_origin_via_kurl.get()));
      EXPECT_TRUE(security_origin_via_gurl->IsSameSchemeHostPort(
          security_origin.get()));
      EXPECT_TRUE(security_origin_via_kurl->IsSameSchemeHostPort(
          security_origin.get()));
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
      EXPECT_TRUE(origin->Domain().IsEmpty());
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
    scoped_refptr<const SecurityOrigin> origin =
        SecurityOrigin::CreateFromString(test.url);
    EXPECT_EQ(test.token, origin->ToTokenForFastCheck()) << test.token;
  }
}

}  // namespace blink
