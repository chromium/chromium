// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef SERVICES_NETWORK_PUBLIC_CPP_IS_POTENTIALLY_TRUSTWORTHY_UNITTEST_H_
#define SERVICES_NETWORK_PUBLIC_CPP_IS_POTENTIALLY_TRUSTWORTHY_UNITTEST_H_

#include <string_view>

#include "base/containers/contains.h"
#include "base/test/scoped_command_line.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/network_switches.h"
#include "url/origin_abstract_tests.h"

namespace network {
namespace test {

// AbstractTrustworthinessTest below abstracts away differences between
// network::IsOriginPotentiallyTrustworthy and
// blink::SecurityOrigin::IsPotentiallyTrustworthy by parametrizing the tests
// with a class that has to expose the same members as url::UrlOriginTestTraits
// and the following extra members:
//   static bool IsOriginPotentiallyTrustworthy(const OriginType& origin);
//   static bool IsUrlPotentiallyTrustworthy(std::string_view str);
//   static bool IsOriginOfLocalhost(const OriginType& origin);
template <typename TTrustworthinessTraits>
class AbstractTrustworthinessTest
    : public url::AbstractOriginTest<TTrustworthinessTraits> {
 protected:
  // Wrappers that help ellide away TTrustworthinessTraits.
  //
  // Note that calling the wrappers needs to be prefixed with `this->...` to
  // avoid hitting: explicit qualification required to use member 'FooBar'
  // from dependent base class.
  using OriginType = typename TTrustworthinessTraits::OriginType;
  bool IsOriginPotentiallyTrustworthy(const OriginType& origin) {
    return TTrustworthinessTraits::IsOriginPotentiallyTrustworthy(origin);
  }
  bool IsOriginPotentiallyTrustworthy(std::string_view str) {
    auto origin = this->CreateOriginFromString(str);
    return TTrustworthinessTraits::IsOriginPotentiallyTrustworthy(origin);
  }
  bool IsUrlPotentiallyTrustworthy(std::string_view str) {
    return TTrustworthinessTraits::IsUrlPotentiallyTrustworthy(str);
  }
  bool IsOriginOfLocalhost(const OriginType& origin) {
    return TTrustworthinessTraits::IsOriginOfLocalhost(origin);
  }
};

TYPED_TEST_SUITE_P(AbstractTrustworthinessTest);

TYPED_TEST_P(AbstractTrustworthinessTest, OpaqueOrigins) {
  auto unique_origin = this->CreateUniqueOpaqueOrigin();
  EXPECT_FALSE(this->IsOriginPotentiallyTrustworthy(unique_origin));

  auto example_origin = this->CreateOriginFromString("https://www.example.com");
  auto opaque_origin = this->DeriveNewOpaqueOrigin(example_origin);
  EXPECT_FALSE(this->IsOriginPotentiallyTrustworthy(opaque_origin));
}

TYPED_TEST_P(AbstractTrustworthinessTest, OriginFromString) {
  EXPECT_FALSE(this->IsOriginPotentiallyTrustworthy("about:blank"));
  EXPECT_FALSE(this->IsOriginPotentiallyTrustworthy("about:blank#ref"));
  EXPECT_FALSE(this->IsOriginPotentiallyTrustworthy("about:srcdoc"));
  EXPECT_FALSE(
      this->IsOriginPotentiallyTrustworthy("javascript:alert('blah')"));
  EXPECT_FALSE(this->IsOriginPotentiallyTrustworthy("data:test/plain;blah"));
}

TYPED_TEST_P(AbstractTrustworthinessTest, CustomSchemes) {
  // Custom testing schemes are registered in url::AbstractOriginTest::SetUp.
  // Let's double-check that schemes we test with have the expected properties.
  EXPECT_TRUE(base::Contains(url::GetSecureSchemes(), "sec"));
  EXPECT_TRUE(base::Contains(url::GetSecureSchemes(), "sec-std-with-host"));
  EXPECT_TRUE(base::Contains(url::GetSecureSchemes(), "sec-noaccess"));
  EXPECT_TRUE(base::Contains(url::GetNoAccessSchemes(), "sec-noaccess"));
  EXPECT_TRUE(base::Contains(url::GetNoAccessSchemes(), "noaccess"));
  EXPECT_TRUE(GURL("sec-std-with-host://blah/x.js").IsStandard());

  // Unrecognized / unknown schemes are not trustworthy.
  EXPECT_FALSE(
      this->IsOriginPotentiallyTrustworthy("unknown-scheme://example.com"));
  EXPECT_FALSE(
      this->IsUrlPotentiallyTrustworthy("unknown-scheme://example.com"));

  // Secure URLs are trustworthy, even if their scheme is also marked as
  // no-access, or are not marked as standard.  See also //chrome-layer
  // ChromeContentClientTest.AdditionalSchemes test and
  // https://crbug.com/734581.
  EXPECT_TRUE(this->IsUrlPotentiallyTrustworthy("sec://blah/x.js"));
  EXPECT_TRUE(
      this->IsUrlPotentiallyTrustworthy("sec-std-with-host://blah/x.js"));
  EXPECT_TRUE(this->IsUrlPotentiallyTrustworthy("sec-noaccess://blah/x.js"));
  EXPECT_TRUE(
      this->IsOriginPotentiallyTrustworthy("sec-std-with-host://blah/x.js"));
  // No-access and non-standard/non-local schemes translate into an
  // untrustworthy, opaque origin.
  // TODO(lukasza): Maybe if the spec had a notion of an origin *precursor*,
  // then it could inspect the scheme of the precursor.  After this, it may be
  // possible to EXPECT_TRUE below...
  EXPECT_FALSE(this->IsOriginPotentiallyTrustworthy("sec://blah/x.js"));
  EXPECT_FALSE(
      this->IsOriginPotentiallyTrustworthy("sec-noaccess://blah/x.js"));

  // No-access, non-secure schemes are untrustworthy.
  EXPECT_FALSE(this->IsUrlPotentiallyTrustworthy("noaccess:blah"));
  EXPECT_FALSE(this->IsOriginPotentiallyTrustworthy("noaccess:blah"));

  // Standard, but non-secure schemes are untrustworthy.
  EXPECT_FALSE(this->IsUrlPotentiallyTrustworthy("std-with-host://blah/x.js"));
  EXPECT_FALSE(
      this->IsOriginPotentiallyTrustworthy("std-with-host://blah/x.js"));
}

TYPED_TEST_P(AbstractTrustworthinessTest, UrlFromString) {
  EXPECT_TRUE(this->IsUrlPotentiallyTrustworthy("about:blank"));
  EXPECT_TRUE(this->IsUrlPotentiallyTrustworthy("about:blank?x=2"));
  EXPECT_TRUE(this->IsUrlPotentiallyTrustworthy("about:blank#ref"));
  EXPECT_TRUE(this->IsUrlPotentiallyTrustworthy("about:blank?x=2#ref"));

  EXPECT_TRUE(this->IsUrlPotentiallyTrustworthy("about:srcdoc"));
  EXPECT_TRUE(this->IsUrlPotentiallyTrustworthy("about:srcdoc?x=2"));
  EXPECT_TRUE(this->IsUrlPotentiallyTrustworthy("about:srcdoc#ref"));
  EXPECT_TRUE(this->IsUrlPotentiallyTrustworthy("about:srcdoc?x=2#ref"));

  // The test expectations below document the current behavior, that "emerges"
  // from how out implementation of IsUrlPotentiallyTrustworthy treats scenarios
  // that are not covered by the spec.  The current behavior might or might not
  // be the desirable long-term behavior (it just accidentally emerged from the
  // current implentattion).  In particular, not how
  // https://www.w3.org/TR/secure-contexts/#is-url-trustworthy only mentions how
  // to deal with "about:srcdoc" and "about:blank", and how
  // https://github.com/w3c/webappsec-secure-contexts/issues/85 discusses
  // general treatment of "secure" / "authenticated" schemes (see also
  // IsSchemeConsideredAuthenticated in product code).
  EXPECT_TRUE(this->IsUrlPotentiallyTrustworthy("about:about"));
  EXPECT_TRUE(this->IsUrlPotentiallyTrustworthy("about:mumble"));

  EXPECT_TRUE(this->IsUrlPotentiallyTrustworthy("data:test/plain;blah"));
  EXPECT_TRUE(this->IsUrlPotentiallyTrustworthy("data:text/html,Hello"));
  EXPECT_FALSE(this->IsUrlPotentiallyTrustworthy("javascript:alert('blah')"));

  EXPECT_TRUE(
      this->IsUrlPotentiallyTrustworthy("https://example.com/fun.html"));
  EXPECT_FALSE(
      this->IsUrlPotentiallyTrustworthy("http://example.com/fun.html"));

  EXPECT_FALSE(this->IsUrlPotentiallyTrustworthy("ftp://example.com/"));

  EXPECT_TRUE(this->IsUrlPotentiallyTrustworthy("wss://example.com/fun.html"));
  EXPECT_FALSE(this->IsUrlPotentiallyTrustworthy("ws://example.com/fun.html"));

  EXPECT_FALSE(
      this->IsUrlPotentiallyTrustworthy("http://localhost.com/fun.html"));
  EXPECT_TRUE(
      this->IsUrlPotentiallyTrustworthy("https://localhost.com/fun.html"));

  EXPECT_FALSE(
      this->IsUrlPotentiallyTrustworthy("http://127.example.com/fun.html"));
  EXPECT_TRUE(
      this->IsUrlPotentiallyTrustworthy("https://127.example.com/fun.html"));

  EXPECT_FALSE(this->IsUrlPotentiallyTrustworthy("http://[::2]/fun.html"));
  EXPECT_FALSE(
      this->IsUrlPotentiallyTrustworthy("http://[::1].example.com/fun.html"));

  // IPv4 mapped IPv6 literals for loopback.
  EXPECT_FALSE(this->IsUrlPotentiallyTrustworthy("http://[::ffff:127.0.0.1]/"));
  EXPECT_FALSE(this->IsUrlPotentiallyTrustworthy("http://[::ffff:7f00:1]"));

  // IPv4 compatible IPv6 literal for loopback.
  EXPECT_FALSE(this->IsUrlPotentiallyTrustworthy("http://[::127.0.0.1]"));

  EXPECT_FALSE(this->IsUrlPotentiallyTrustworthy("http://loopback"));

  // Legacy localhost names.
  EXPECT_FALSE(
      this->IsUrlPotentiallyTrustworthy("http://localhost.localdomain"));
  EXPECT_FALSE(this->IsUrlPotentiallyTrustworthy("http://localhost6"));
  EXPECT_FALSE(
      this->IsUrlPotentiallyTrustworthy("ftp://localhost6.localdomain6"));

  EXPECT_FALSE(this->IsUrlPotentiallyTrustworthy(
      "filesystem:http://www.example.com/temporary/"));
  EXPECT_FALSE(this->IsUrlPotentiallyTrustworthy(
      "filesystem:ftp://www.example.com/temporary/"));
  EXPECT_TRUE(this->IsUrlPotentiallyTrustworthy(
      "filesystem:https://www.example.com/temporary/"));

  EXPECT_FALSE(this->IsUrlPotentiallyTrustworthy(
      "blob:http://www.example.com/guid-goes-here"));
  EXPECT_FALSE(this->IsUrlPotentiallyTrustworthy(
      "blob:ftp://www.example.com/guid-goes-here"));
  EXPECT_TRUE(this->IsUrlPotentiallyTrustworthy(
      "blob:https://www.example.com/guid-goes-here"));

  EXPECT_FALSE(
      this->IsUrlPotentiallyTrustworthy("filesystem:data:text/html,Hello"));
  EXPECT_FALSE(this->IsUrlPotentiallyTrustworthy("filesystem:about:blank"));
  EXPECT_FALSE(this->IsUrlPotentiallyTrustworthy(
      "blob:blob:https://example.com/578223a1-8c13-17b3-84d5-eca045ae384a"));
  EXPECT_FALSE(this->IsUrlPotentiallyTrustworthy(
      "filesystem:blob:https://example.com/"
      "578223a1-8c13-17b3-84d5-eca045ae384a"));

  // These tests are imported from IsPotentiallyTrustworthy.Url.
  EXPECT_TRUE(this->IsUrlPotentiallyTrustworthy("file:///test/fun.html"));
  EXPECT_TRUE(this->IsUrlPotentiallyTrustworthy("file:///test/"));
  EXPECT_TRUE(this->IsUrlPotentiallyTrustworthy("file://localhost/test/"));
  EXPECT_TRUE(this->IsUrlPotentiallyTrustworthy("file://otherhost/test/"));

  EXPECT_TRUE(this->IsUrlPotentiallyTrustworthy("http://localhost/fun.html"));
  EXPECT_TRUE(this->IsUrlPotentiallyTrustworthy("http://localhost./fun.html"));
  EXPECT_TRUE(
      this->IsUrlPotentiallyTrustworthy("http://pumpkin.localhost/fun.html"));
  EXPECT_TRUE(this->IsUrlPotentiallyTrustworthy(
      "http://crumpet.pumpkin.localhost/fun.html"));
  EXPECT_TRUE(this->IsUrlPotentiallyTrustworthy(
      "http://pumpkin.localhost:8080/fun.html"));
  EXPECT_TRUE(this->IsUrlPotentiallyTrustworthy(
      "http://crumpet.pumpkin.localhost:3000/fun.html"));

  EXPECT_TRUE(this->IsUrlPotentiallyTrustworthy("http://127.0.0.1/fun.html"));
  EXPECT_TRUE(this->IsUrlPotentiallyTrustworthy("ftp://127.0.0.1/fun.html"));
  EXPECT_TRUE(this->IsUrlPotentiallyTrustworthy("http://127.3.0.1/fun.html"));

  EXPECT_TRUE(this->IsUrlPotentiallyTrustworthy("http://[::1]/fun.html"));

  EXPECT_TRUE(this->IsUrlPotentiallyTrustworthy(
      "filesystem:ftp://127.0.0.1/temporary/"));
  EXPECT_TRUE(
      this->IsUrlPotentiallyTrustworthy("blob:ftp://127.0.0.1/guid-goes-here"));

  EXPECT_FALSE(this->IsUrlPotentiallyTrustworthy("blob:data:text/html,Hello"));
  EXPECT_FALSE(this->IsUrlPotentiallyTrustworthy("blob:about:blank"));

  // These tests are imported from SecurityOriginTest.IsSecure.
  EXPECT_TRUE(this->IsUrlPotentiallyTrustworthy("file:///etc/passwd"));
  EXPECT_FALSE(this->IsUrlPotentiallyTrustworthy("blob:data:text/html,Hello"));
  EXPECT_FALSE(this->IsUrlPotentiallyTrustworthy("blob:about:blank"));
  EXPECT_FALSE(
      this->IsUrlPotentiallyTrustworthy("filesystem:data:text/html,Hello"));
  EXPECT_FALSE(this->IsUrlPotentiallyTrustworthy("filesystem:about:blank"));
  EXPECT_FALSE(this->IsUrlPotentiallyTrustworthy(""));
  EXPECT_FALSE(this->IsUrlPotentiallyTrustworthy("\0"));

  // These tests are imported from SecurityOriginTest.IsSecureForLocalServers.
  EXPECT_TRUE(this->IsUrlPotentiallyTrustworthy("http://localhost/"));
  EXPECT_TRUE(this->IsUrlPotentiallyTrustworthy("http://localhost:8080/"));
  EXPECT_TRUE(this->IsUrlPotentiallyTrustworthy("http://127.0.0.1/"));
  EXPECT_TRUE(this->IsUrlPotentiallyTrustworthy("http://127.0.0.1:8080/"));
  EXPECT_TRUE(this->IsUrlPotentiallyTrustworthy("http://[::1]/"));
  EXPECT_TRUE(this->IsUrlPotentiallyTrustworthy("http://vhost.localhost/"));
}

TYPED_TEST_P(AbstractTrustworthinessTest, TestcasesInheritedFromBlink) {
  struct TestCase {
    bool is_potentially_trustworthy;
    bool is_localhost;
    const char* url;
  };

  TestCase inputs[] = {
      // Access is granted to webservers running on localhost.
      {true, true, "http://localhost"},
      {true, true, "http://localhost."},
      {true, true, "http://LOCALHOST"},
      {true, true, "http://localhost:100"},
      {true, true, "http://a.localhost"},
      {true, true, "http://a.b.localhost"},
      {true, true, "http://127.0.0.1"},
      {true, true, "http://127.0.0.2"},
      {true, true, "http://127.1.0.2"},
      {true, true, "http://0177.00.00.01"},
      {true, true, "http://[::1]"},
      {true, true, "http://[0:0::1]"},
      {true, true, "http://[0:0:0:0:0:0:0:1]"},
      {true, true, "http://[::1]:21"},
      {true, true, "http://127.0.0.1:8080"},
      {true, true, "ftp://127.0.0.1"},
      {true, true, "ftp://127.0.0.1:443"},
      {true, true, "ws://127.0.0.1"},

      // Non-localhost over HTTP
      {false, false, "http://[1::]"},
      {false, false, "http://[::2]"},
      {false, false, "http://[1::1]"},
      {false, false, "http://[1:2::3]"},
      {false, false, "http://[::127.0.0.1]"},
      {false, false, "http://a.127.0.0.1"},
      {false, false, "http://127.0.0.1.b"},
      {false, false, "http://localhost.a"},

      // loopback resolves to localhost on Windows, but not
      // recognized generically here.
      {false, false, "http://loopback"},

      // IPv4 mapped IPv6 literals for 127.0.0.1.
      {false, false, "http://[::ffff:127.0.0.1]"},
      {false, false, "http://[::ffff:7f00:1]"},

      // IPv4 compatible IPv6 literal for 127.0.0.1.
      {false, false, "http://[::127.0.0.1]"},

      // Legacy localhost names.
      {false, false, "http://localhost.localdomain"},
      {false, false, "http://localhost6"},
      {false, false, "ftp://localhost6.localdomain6"},

      // Secure transports are considered trustworthy.
      {true, false, "https://foobar.com"},
      {true, false, "wss://foobar.com"},

      // Insecure transports are not considered trustworthy.
      {false, false, "ftp://foobar.com"},
      {false, false, "http://foobar.com"},
      {false, false, "http://foobar.com:443"},
      {false, false, "ws://foobar.com"},
      {false, false, "custom-scheme://example.com"},

      // Local files are considered trustworthy.
      {true, false, "file:///home/foobar/index.html"},

      // blob: URLs must look to the inner URL's origin, and apply the same
      // rules as above. Spot check some of them
      {true, true,
       "blob:http://localhost:1000/578223a1-8c13-17b3-84d5-eca045ae384a"},
      {true, false,
       "blob:https://foopy:99/578223a1-8c13-17b3-84d5-eca045ae384a"},
      {false, false, "blob:http://baz:99/578223a1-8c13-17b3-84d5-eca045ae384a"},
      {false, false, "blob:ftp://evil:99/578223a1-8c13-17b3-84d5-eca045ae384a"},
      {false, false, "blob:data:text/html,Hello"},
      {false, false, "blob:about:blank"},
      {false, false,
       "blob:blob:https://example.com/578223a1-8c13-17b3-84d5-eca045ae384a"},

      // filesystem: URLs work the same as blob: URLs, and look to the inner
      // URL for security origin.
      {true, true, "filesystem:http://localhost:1000/foo"},
      {true, false, "filesystem:https://foopy:99/foo"},
      {false, false, "filesystem:http://baz:99/foo"},
      {false, false, "filesystem:ftp://evil:99/foo"},
      {false, false, "filesystem:data:text/html,Hello"},
      {false, false, "filesystem:about:blank"},
      {false, false,
       "filesystem:blob:https://example.com/"
       "578223a1-8c13-17b3-84d5-eca045ae384a"},

      // about: and data: URLs.
      {false, false, "about:blank"},
      {false, false, "about:srcdoc"},
      {false, false, "data:text/html,Hello"},
  };

  for (size_t i = 0; i < std::size(inputs); ++i) {
    SCOPED_TRACE(inputs[i].url);
    auto origin = this->CreateOriginFromString(inputs[i].url);
    EXPECT_EQ(inputs[i].is_potentially_trustworthy,
              this->IsOriginPotentiallyTrustworthy(origin));
    EXPECT_EQ(inputs[i].is_localhost, this->IsOriginOfLocalhost(origin));

    GURL test_gurl(inputs[i].url);
    if (!(test_gurl.SchemeIsBlob() || test_gurl.SchemeIsFileSystem())) {
      // Check that the origin's notion of localhost matches //net's notion of
      // localhost. This is skipped for blob: and filesystem: URLs since
      // blink::SecurityOrigin uses their inner URL's origin.
      EXPECT_EQ(net::IsLocalhost(GURL(inputs[i].url)),
                this->IsOriginOfLocalhost(origin));
    }
  }
}

TYPED_TEST_P(AbstractTrustworthinessTest, IsTrustworthyWithPattern) {
  const struct HostnamePatternCase {
    const char* pattern;
    const char* url;
    bool expected_secure;
  } kTestCases[] = {
      {"http://bar.foo.com", "http://bar.foo.com", true},
      {"*.baz.com", "http://bar.baz.com", true},
      {"http://foo", "*.foo.com", false},
  };

  for (const auto& test : kTestCases) {
    EXPECT_FALSE(this->IsUrlPotentiallyTrustworthy(test.url));
    base::test::ScopedCommandLine scoped_command_line;
    base::CommandLine* command_line =
        scoped_command_line.GetProcessCommandLine();
    command_line->AppendSwitchASCII(
        network::switches::kUnsafelyTreatInsecureOriginAsSecure, test.pattern);
    network::SecureOriginAllowlist::GetInstance().ResetForTesting();
    EXPECT_EQ(this->IsUrlPotentiallyTrustworthy(test.url),
              test.expected_secure);
  }
}

REGISTER_TYPED_TEST_SUITE_P(AbstractTrustworthinessTest,
                            OpaqueOrigins,
                            OriginFromString,
                            CustomSchemes,
                            UrlFromString,
                            TestcasesInheritedFromBlink,
                            IsTrustworthyWithPattern);

}  // namespace test
}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_IS_POTENTIALLY_TRUSTWORTHY_UNITTEST_H_
