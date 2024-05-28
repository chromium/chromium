// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/csp_source.h"

#include "base/test/with_feature_override.h"
#include "services/network/public/mojom/content_security_policy.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "url/url_features.h"

namespace blink {

TEST(CSPSourceTest, BasicMatching) {
  KURL base;
  auto source = network::mojom::blink::CSPSource::New(
      "http", "example.com", 8000, "/foo/", false, false);

  EXPECT_TRUE(CSPSourceMatches(*source, "",
                               KURL(base, "http://example.com:8000/foo/")));
  EXPECT_TRUE(CSPSourceMatches(*source, "",
                               KURL(base, "http://example.com:8000/foo/bar")));
  EXPECT_TRUE(CSPSourceMatches(*source, "",
                               KURL(base, "HTTP://EXAMPLE.com:8000/foo/BAR")));
  EXPECT_FALSE(CSPSourceMatches(*source, "",
                                KURL(base, "http://example.com:8000/bar/")));
  EXPECT_FALSE(CSPSourceMatches(*source, "",
                                KURL(base, "https://example.com:8000/bar/")));
  EXPECT_FALSE(CSPSourceMatches(*source, "",
                                KURL(base, "http://example.com:9000/bar/")));
  EXPECT_FALSE(CSPSourceMatches(*source, "",
                                KURL(base, "HTTP://example.com:8000/FOO/bar")));
  EXPECT_FALSE(CSPSourceMatches(*source, "",
                                KURL(base, "HTTP://example.com:8000/FOO/BAR")));
}

TEST(CSPSourceTest, BasicPathMatching) {
  KURL base;
  auto a = network::mojom::blink::CSPSource::New("http", "example.com", 8000,
                                                 "/", false, false);

  EXPECT_TRUE(CSPSourceMatches(*a, "", KURL(base, "http://example.com:8000")));
  EXPECT_TRUE(CSPSourceMatches(*a, "", KURL(base, "http://example.com:8000/")));
  EXPECT_TRUE(
      CSPSourceMatches(*a, "", KURL(base, "http://example.com:8000/foo/bar")));

  EXPECT_FALSE(
      CSPSourceMatches(*a, "", KURL(base, "http://example.com:8000path")));
  EXPECT_FALSE(
      CSPSourceMatches(*a, "", KURL(base, "http://example.com:9000/")));

  auto b = network::mojom::blink::CSPSource::New("http", "example.com", 8000,
                                                 "", false, false);
  EXPECT_TRUE(CSPSourceMatches(*b, "", KURL(base, "http://example.com:8000")));
  EXPECT_TRUE(CSPSourceMatches(*b, "", KURL(base, "http://example.com:8000/")));
  EXPECT_TRUE(
      CSPSourceMatches(*a, "", KURL(base, "http://example.com:8000/foo/bar")));

  EXPECT_FALSE(
      CSPSourceMatches(*b, "", KURL(base, "http://example.com:8000path")));
  EXPECT_FALSE(
      CSPSourceMatches(*b, "", KURL(base, "http://example.com:9000/")));
}

TEST(CSPSourceTest, WildcardMatching) {
  KURL base;
  auto source = network::mojom::blink::CSPSource::New(
      "http", "example.com", url::PORT_UNSPECIFIED, "/", true, true);

  EXPECT_TRUE(CSPSourceMatches(*source, "",
                               KURL(base, "http://foo.example.com:8000/")));
  EXPECT_TRUE(CSPSourceMatches(*source, "",
                               KURL(base, "http://foo.example.com:8000/foo")));
  EXPECT_TRUE(CSPSourceMatches(*source, "",
                               KURL(base, "http://foo.example.com:9000/foo/")));
  EXPECT_TRUE(CSPSourceMatches(
      *source, "", KURL(base, "HTTP://FOO.EXAMPLE.com:8000/foo/BAR")));

  EXPECT_FALSE(
      CSPSourceMatches(*source, "", KURL(base, "http://example.com:8000/")));
  EXPECT_FALSE(
      CSPSourceMatches(*source, "", KURL(base, "http://example.com:8000/foo")));
  EXPECT_FALSE(CSPSourceMatches(*source, "",
                                KURL(base, "http://example.com:9000/foo/")));
  EXPECT_FALSE(CSPSourceMatches(*source, "",
                                KURL(base, "http://example.foo.com:8000/")));
  EXPECT_FALSE(CSPSourceMatches(*source, "",
                                KURL(base, "https://example.foo.com:8000/")));
  EXPECT_FALSE(CSPSourceMatches(*source, "",
                                KURL(base, "https://example.com:8000/bar/")));
}

TEST(CSPSourceTest, RedirectMatching) {
  KURL base;
  auto source = network::mojom::blink::CSPSource::New(
      "http", "example.com", 8000, "/bar/", false, false);

  EXPECT_TRUE(
      CSPSourceMatches(*source, "", KURL(base, "http://example.com:8000/"),
                       ResourceRequest::RedirectStatus::kFollowedRedirect));
  EXPECT_TRUE(
      CSPSourceMatches(*source, "", KURL(base, "http://example.com:8000/foo"),
                       ResourceRequest::RedirectStatus::kFollowedRedirect));
  // Should not allow upgrade of port or scheme without upgrading both
  EXPECT_FALSE(
      CSPSourceMatches(*source, "", KURL(base, "https://example.com:8000/foo"),
                       ResourceRequest::RedirectStatus::kFollowedRedirect));
  EXPECT_FALSE(CSPSourceMatches(
      *source, "", KURL(base, "http://not-example.com:8000/foo"),
      ResourceRequest::RedirectStatus::kFollowedRedirect));
  EXPECT_FALSE(CSPSourceMatches(*source, "",
                                KURL(base, "http://example.com:9000/foo/"),
                                ResourceRequest::RedirectStatus::kNoRedirect));
}

TEST(CSPSourceTest, InsecureSchemeMatchesSecureScheme) {
  KURL base;
  auto source = network::mojom::blink::CSPSource::New(
      "http", "", url::PORT_UNSPECIFIED, "/", false, true);

  EXPECT_TRUE(
      CSPSourceMatches(*source, "", KURL(base, "http://example.com:8000/")));
  EXPECT_TRUE(
      CSPSourceMatches(*source, "", KURL(base, "https://example.com:8000/")));
  EXPECT_TRUE(CSPSourceMatches(*source, "",
                               KURL(base, "http://not-example.com:8000/")));
  EXPECT_TRUE(CSPSourceMatches(*source, "",
                               KURL(base, "https://not-example.com:8000/")));
  EXPECT_FALSE(
      CSPSourceMatches(*source, "", KURL(base, "ftp://example.com:8000/")));
}

TEST(CSPSourceTest, InsecureHostSchemeMatchesSecureScheme) {
  KURL base;
  auto source = network::mojom::blink::CSPSource::New(
      "http", "example.com", url::PORT_UNSPECIFIED, "/", false, true);

  EXPECT_TRUE(
      CSPSourceMatches(*source, "", KURL(base, "http://example.com:8000/")));
  EXPECT_FALSE(CSPSourceMatches(*source, "",
                                KURL(base, "http://not-example.com:8000/")));
  EXPECT_TRUE(
      CSPSourceMatches(*source, "", KURL(base, "https://example.com:8000/")));
  EXPECT_FALSE(CSPSourceMatches(*source, "",
                                KURL(base, "https://not-example.com:8000/")));
}

class CSPSourceParamTest : public base::test::WithFeatureOverride,
                           public ::testing::Test {
 public:
  CSPSourceParamTest()
      : WithFeatureOverride(url::kStandardCompliantNonSpecialSchemeURLParsing) {
  }
};

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(CSPSourceParamTest);

TEST_P(CSPSourceParamTest, SchemeIsEmpty) {
  KURL base;

  // Self scheme is http.
  {
    auto source = network::mojom::blink::CSPSource::New(
        "", "a.com", url::PORT_UNSPECIFIED, "/", false, false);
    EXPECT_TRUE(CSPSourceMatches(*source, "http", KURL(base, "http://a.com")));
    EXPECT_TRUE(CSPSourceMatches(*source, "http", KURL(base, "https://a.com")));
    EXPECT_FALSE(CSPSourceMatches(*source, "http", KURL(base, "ftp://a.com")));
  }

  // Self scheme is https.
  {
    auto source = network::mojom::blink::CSPSource::New(
        "", "a.com", url::PORT_UNSPECIFIED, "/", false, false);
    EXPECT_FALSE(
        CSPSourceMatches(*source, "https", KURL(base, "http://a.com")));
    EXPECT_TRUE(
        CSPSourceMatches(*source, "https", KURL(base, "https://a.com")));
    EXPECT_FALSE(CSPSourceMatches(*source, "https", KURL(base, "ftp://a.com")));
  }

  // Self scheme is not in the http familly.
  {
    auto source = network::mojom::blink::CSPSource::New(
        "", "a.com", url::PORT_UNSPECIFIED, "/", false, false);
    EXPECT_FALSE(CSPSourceMatches(*source, "ftp", KURL(base, "http://a.com")));
    EXPECT_TRUE(CSPSourceMatches(*source, "ftp", KURL(base, "ftp://a.com")));
  }

  // Self scheme is unique
  {
    auto source = network::mojom::blink::CSPSource::New(
        "", "a.com", url::PORT_UNSPECIFIED, "/", false, false);
    EXPECT_FALSE(CSPSourceMatches(*source, "non-standard-scheme",
                                  KURL(base, "http://a.com")));

    // The reason matching fails is because the host is parsed as "" when
    // using a non standard scheme even though it should be parsed as "a.com"
    // After adding it to the list of standard schemes it now gets parsed
    // correctly. This does not matter in practice though because there is
    // no way to render/load anything like "non-standard-scheme://a.com"
    EXPECT_FALSE(CSPSourceMatches(*source, "non-standard-scheme",
                                  KURL(base, "non-standard-scheme://a.com")));
  }
}

TEST(CSPSourceTest, InsecureHostSchemePortMatchesSecurePort) {
  KURL base;

  // source scheme is "http", source port is 80
  {
    auto source = network::mojom::blink::CSPSource::New("http", "example.com",
                                                        80, "/", false, false);
    EXPECT_TRUE(
        CSPSourceMatches(*source, "", KURL(base, "http://example.com/")));
    EXPECT_TRUE(
        CSPSourceMatches(*source, "", KURL(base, "http://example.com:80/")));

    // Should not allow scheme upgrades unless both port and scheme are upgraded
    EXPECT_FALSE(
        CSPSourceMatches(*source, "", KURL(base, "http://example.com:443/")));
    EXPECT_TRUE(
        CSPSourceMatches(*source, "", KURL(base, "https://example.com/")));
    EXPECT_FALSE(
        CSPSourceMatches(*source, "", KURL(base, "https://example.com:80/")));

    EXPECT_TRUE(
        CSPSourceMatches(*source, "", KURL(base, "https://example.com:443/")));

    EXPECT_FALSE(
        CSPSourceMatches(*source, "", KURL(base, "http://example.com:8443/")));
    EXPECT_FALSE(
        CSPSourceMatches(*source, "", KURL(base, "https://example.com:8443/")));

    EXPECT_FALSE(
        CSPSourceMatches(*source, "", KURL(base, "http://not-example.com/")));
    EXPECT_FALSE(CSPSourceMatches(*source, "",
                                  KURL(base, "http://not-example.com:80/")));
    EXPECT_FALSE(CSPSourceMatches(*source, "",
                                  KURL(base, "http://not-example.com:443/")));
    EXPECT_FALSE(
        CSPSourceMatches(*source, "", KURL(base, "https://not-example.com/")));
    EXPECT_FALSE(CSPSourceMatches(*source, "",
                                  KURL(base, "https://not-example.com:80/")));
    EXPECT_FALSE(CSPSourceMatches(*source, "",
                                  KURL(base, "https://not-example.com:443/")));
  }

  // source scheme is "http", source port is 443
  {
    auto source = network::mojom::blink::CSPSource::New("http", "example.com",
                                                        443, "/", false, false);
    EXPECT_TRUE(
        CSPSourceMatches(*source, "", KURL(base, "https://example.com/")));
  }

  // source scheme is empty
  {
    auto source = network::mojom::blink::CSPSource::New("", "example.com", 80,
                                                        "/", false, false);
    EXPECT_TRUE(
        CSPSourceMatches(*source, "http", KURL(base, "http://example.com/")));
    EXPECT_TRUE(CSPSourceMatches(*source, "http",
                                 KURL(base, "https://example.com:443")));
    // Should not allow upgrade of port or scheme without upgrading both
    EXPECT_FALSE(CSPSourceMatches(*source, "http",
                                  KURL(base, "http://example.com:443")));
  }

  // source port is empty
  {
    auto source = network::mojom::blink::CSPSource::New(
        "http", "example.com", url::PORT_UNSPECIFIED, "/", false, false);

    EXPECT_TRUE(
        CSPSourceMatches(*source, "", KURL(base, "http://example.com")));
    EXPECT_TRUE(
        CSPSourceMatches(*source, "", KURL(base, "https://example.com")));
    EXPECT_TRUE(
        CSPSourceMatches(*source, "", KURL(base, "https://example.com:443")));
    // Should not allow upgrade of port or scheme without upgrading both
    EXPECT_FALSE(
        CSPSourceMatches(*source, "", KURL(base, "https://example.com:80")));
    EXPECT_FALSE(
        CSPSourceMatches(*source, "", KURL(base, "http://example.com:443")));
  }
}

TEST(CSPSourceTest, HostMatches) {
  KURL base;

  // Host is * (source-expression = "http://*")
  {
    auto source = network::mojom::blink::CSPSource::New(
        "http", "", url::PORT_UNSPECIFIED, "", true, false);
    EXPECT_TRUE(CSPSourceMatches(*source, "http", KURL(base, "http://a.com")));
    EXPECT_TRUE(CSPSourceMatches(*source, "http", KURL(base, "http://.")));
  }

  // Host is *.foo.bar
  {
    auto source = network::mojom::blink::CSPSource::New(
        "", "foo.bar", url::PORT_UNSPECIFIED, "", true, false);
    EXPECT_FALSE(CSPSourceMatches(*source, "http", KURL(base, "http://a.com")));
    EXPECT_FALSE(CSPSourceMatches(*source, "http", KURL(base, "http://bar")));
    EXPECT_FALSE(
        CSPSourceMatches(*source, "http", KURL(base, "http://foo.bar")));
    EXPECT_FALSE(CSPSourceMatches(*source, "http", KURL(base, "http://o.bar")));
    EXPECT_TRUE(
        CSPSourceMatches(*source, "http", KURL(base, "http://*.foo.bar")));
    EXPECT_TRUE(
        CSPSourceMatches(*source, "http", KURL(base, "http://sub.foo.bar")));
    EXPECT_TRUE(CSPSourceMatches(*source, "http",
                                 KURL(base, "http://sub.sub.foo.bar")));
    // Please see http://crbug.com/692505
    EXPECT_TRUE(
        CSPSourceMatches(*source, "http", KURL(base, "http://.foo.bar")));
  }

  // Host is exact.
  {
    auto source = network::mojom::blink::CSPSource::New(
        "", "foo.bar", url::PORT_UNSPECIFIED, "", false, false);
    EXPECT_TRUE(
        CSPSourceMatches(*source, "http", KURL(base, "http://foo.bar")));
    EXPECT_FALSE(
        CSPSourceMatches(*source, "http", KURL(base, "http://sub.foo.bar")));
    EXPECT_FALSE(CSPSourceMatches(*source, "http", KURL(base, "http://bar")));
    // Please see http://crbug.com/692505
    EXPECT_FALSE(
        CSPSourceMatches(*source, "http", KURL(base, "http://.foo.bar")));
  }
}

TEST_P(CSPSourceParamTest, MatchingAsSelf) {
  // Testing Step 4 of
  // https://w3c.github.io/webappsec-csp/#match-url-to-source-expression
  struct Source {
    String scheme;
    String host;
    String path;
    int port;
    bool host_wildcard;
    bool port_wildcard;
  };
  struct TestCase {
    const Source self_source;
    const String& url;
    bool expected;
  } cases[] = {
      // Same origin
      {{"http", "example.com", "", 80, false, false},
       "http://example.com:80/",
       true},
      {{"https", "example.com", "", 443, false, false},
       "https://example.com:443/",
       true},
      {{"https", "example.com", "", 4545, false, false},
       "https://example.com:4545/",
       true},  // Mismatching origin
      // Mismatching host
      {{"http", "example.com", "", 80, false, false},
       "http://example2.com:80/",
       false},
      // Ports not matching default schemes
      {{"http", "example.com", "", 8080, false, false},
       "https://example.com:443/",
       false},
      {{"http", "example.com", "", 80, false, false},
       "wss://example.com:8443/",
       false},
      // Allowed different scheme combinations (4.2.1 and 4.2.2)
      {{"http", "example.com", "", 80, false, false},
       "https://example.com:443/",
       true},
      {{"http", "example.com", "", 80, false, false},
       "ws://example.com:80/",
       true},
      {{"http", "example.com", "", 80, false, false},
       "wss://example.com:443/",
       true},
      {{"ws", "example.com", "", 80, false, false},
       "https://example.com:443/",
       true},
      {{"wss", "example.com", "", 443, false, false},
       "https://example.com:443/",
       true},
      {{"https", "example.com", "", 443, false, false},
       "wss://example.com:443/",
       true},
      // Ports not set (aka default)
      {{"https", "example.com", "", url::PORT_UNSPECIFIED, false, false},
       "wss://example.com:443/",
       true},
      {{"https", "example.com", "", 443, false, false},
       "wss://example.com/",
       true},

      // Paths are ignored
      {{"http", "example.com", "", 80, false, false},
       "https://example.com:443/some-path-here",
       true},
      {{"http", "example.com", "", 80, false, false},
       "ws://example.com:80/some-other-path-here",
       true},

      // Custom schemes
      {{"http", "example.com", "", 80, false, false},
       "custom-scheme://example.com/",
       false},
      {{"http", "example.com", "", 80, false, false},
       "custom-scheme://example.com:80/",
       false},
      {{"https", "example.com", "", 443, false, false},
       "custom-scheme://example.com/",
       false},
      {{"https", "example.com", "", 443, false, false},
       "custom-scheme://example.com:443/",
       false},
      {{"https", "example.com", "", 443, false, false},
       "custom-scheme://example.com/some-path",
       false},
      {{"http", "example.com", "", url::PORT_UNSPECIFIED, false, false},
       "custom-scheme://example.com/some-path",
       false},

      // If 'self' is file://, the host always matches.
      {{"file", "", "", url::PORT_UNSPECIFIED, false, false},
       "file:///info.txt",
       true},
      {{"file", "", "", url::PORT_UNSPECIFIED, false, false},
       "file://localhost/info.txt",
       true},
      {{"file", "localhost", "", url::PORT_UNSPECIFIED, false, false},
       "file:///info.txt",
       true},
      {{"file", "localhost", "", url::PORT_UNSPECIFIED, false, false},
       "file://localhost/info.txt",
       true},
  };

  KURL base;
  for (const auto& test : cases) {
    auto self_source = network::mojom::blink::CSPSource::New(
        test.self_source.scheme, test.self_source.host, test.self_source.port,
        test.self_source.path, test.self_source.host_wildcard,
        test.self_source.port_wildcard);
    EXPECT_EQ(test.expected,
              CSPSourceMatchesAsSelf(*self_source, KURL(base, test.url)));
  }
}

}  // namespace blink
