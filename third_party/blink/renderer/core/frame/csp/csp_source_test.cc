// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/csp_source.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

class CSPSourceTest : public testing::Test {
 public:
  CSPSourceTest() : csp(MakeGarbageCollected<ContentSecurityPolicy>()) {}

 protected:
  Persistent<ContentSecurityPolicy> csp;
  struct Source {
    String scheme;
    String host;
    String path;
    // port is CSPSource::kPortUnspecified if it was not specified so the
    // default port for a given scheme will be used.
    const int port;
    CSPSource::WildcardDisposition host_wildcard;
    CSPSource::WildcardDisposition port_wildcard;
  };

  bool EqualSources(const Source& a, const Source& b) {
    return a.scheme == b.scheme && a.host == b.host && a.port == b.port &&
           a.path == b.path && a.host_wildcard == b.host_wildcard &&
           a.port_wildcard == b.port_wildcard;
  }
};

TEST_F(CSPSourceTest, BasicMatching) {
  KURL base;
  CSPSource source(csp.Get(), "http", "example.com", 8000, "/foo/",
                   CSPSource::kNoWildcard, CSPSource::kNoWildcard);

  EXPECT_TRUE(source.Matches(KURL(base, "http://example.com:8000/foo/")));
  EXPECT_TRUE(source.Matches(KURL(base, "http://example.com:8000/foo/bar")));
  EXPECT_TRUE(source.Matches(KURL(base, "HTTP://EXAMPLE.com:8000/foo/BAR")));

  EXPECT_FALSE(source.Matches(KURL(base, "http://example.com:8000/bar/")));
  EXPECT_FALSE(source.Matches(KURL(base, "https://example.com:8000/bar/")));
  EXPECT_FALSE(source.Matches(KURL(base, "http://example.com:9000/bar/")));
  EXPECT_FALSE(source.Matches(KURL(base, "HTTP://example.com:8000/FOO/bar")));
  EXPECT_FALSE(source.Matches(KURL(base, "HTTP://example.com:8000/FOO/BAR")));
}

TEST_F(CSPSourceTest, BasicPathMatching) {
  KURL base;
  CSPSource a(csp.Get(), "http", "example.com", 8000, "/",
              CSPSource::kNoWildcard, CSPSource::kNoWildcard);

  EXPECT_TRUE(a.Matches(KURL(base, "http://example.com:8000")));
  EXPECT_TRUE(a.Matches(KURL(base, "http://example.com:8000/")));
  EXPECT_TRUE(a.Matches(KURL(base, "http://example.com:8000/foo/bar")));

  EXPECT_FALSE(a.Matches(KURL(base, "http://example.com:8000path")));
  EXPECT_FALSE(a.Matches(KURL(base, "http://example.com:9000/")));

  CSPSource b(csp.Get(), "http", "example.com", 8000, "",
              CSPSource::kNoWildcard, CSPSource::kNoWildcard);
  EXPECT_TRUE(b.Matches(KURL(base, "http://example.com:8000")));
  EXPECT_TRUE(b.Matches(KURL(base, "http://example.com:8000/")));
  EXPECT_TRUE(a.Matches(KURL(base, "http://example.com:8000/foo/bar")));

  EXPECT_FALSE(b.Matches(KURL(base, "http://example.com:8000path")));
  EXPECT_FALSE(b.Matches(KURL(base, "http://example.com:9000/")));
}

TEST_F(CSPSourceTest, WildcardMatching) {
  KURL base;
  CSPSource source(csp.Get(), "http", "example.com",
                   CSPSource::kPortUnspecified, "/", CSPSource::kHasWildcard,
                   CSPSource::kHasWildcard);

  EXPECT_TRUE(source.Matches(KURL(base, "http://foo.example.com:8000/")));
  EXPECT_TRUE(source.Matches(KURL(base, "http://foo.example.com:8000/foo")));
  EXPECT_TRUE(source.Matches(KURL(base, "http://foo.example.com:9000/foo/")));
  EXPECT_TRUE(
      source.Matches(KURL(base, "HTTP://FOO.EXAMPLE.com:8000/foo/BAR")));

  EXPECT_FALSE(source.Matches(KURL(base, "http://example.com:8000/")));
  EXPECT_FALSE(source.Matches(KURL(base, "http://example.com:8000/foo")));
  EXPECT_FALSE(source.Matches(KURL(base, "http://example.com:9000/foo/")));
  EXPECT_FALSE(source.Matches(KURL(base, "http://example.foo.com:8000/")));
  EXPECT_FALSE(source.Matches(KURL(base, "https://example.foo.com:8000/")));
  EXPECT_FALSE(source.Matches(KURL(base, "https://example.com:8000/bar/")));
}

TEST_F(CSPSourceTest, RedirectMatching) {
  KURL base;
  CSPSource source(csp.Get(), "http", "example.com", 8000, "/bar/",
                   CSPSource::kNoWildcard, CSPSource::kNoWildcard);

  EXPECT_TRUE(
      source.Matches(KURL(base, "http://example.com:8000/"),
                     ResourceRequest::RedirectStatus::kFollowedRedirect));
  EXPECT_TRUE(
      source.Matches(KURL(base, "http://example.com:8000/foo"),
                     ResourceRequest::RedirectStatus::kFollowedRedirect));
  // Should not allow upgrade of port or scheme without upgrading both
  EXPECT_FALSE(
      source.Matches(KURL(base, "https://example.com:8000/foo"),
                     ResourceRequest::RedirectStatus::kFollowedRedirect));
  EXPECT_FALSE(
      source.Matches(KURL(base, "http://not-example.com:8000/foo"),
                     ResourceRequest::RedirectStatus::kFollowedRedirect));
  EXPECT_FALSE(source.Matches(KURL(base, "http://example.com:9000/foo/"),
                              ResourceRequest::RedirectStatus::kNoRedirect));
}

TEST_F(CSPSourceTest, InsecureSchemeMatchesSecureScheme) {
  KURL base;
  CSPSource source(csp.Get(), "http", "", CSPSource::kPortUnspecified, "/",
                   CSPSource::kNoWildcard, CSPSource::kHasWildcard);

  EXPECT_TRUE(source.Matches(KURL(base, "http://example.com:8000/")));
  EXPECT_TRUE(source.Matches(KURL(base, "https://example.com:8000/")));
  EXPECT_TRUE(source.Matches(KURL(base, "http://not-example.com:8000/")));
  EXPECT_TRUE(source.Matches(KURL(base, "https://not-example.com:8000/")));
  EXPECT_FALSE(source.Matches(KURL(base, "ftp://example.com:8000/")));
}

TEST_F(CSPSourceTest, InsecureHostSchemeMatchesSecureScheme) {
  KURL base;
  CSPSource source(csp.Get(), "http", "example.com",
                   CSPSource::kPortUnspecified, "/", CSPSource::kNoWildcard,
                   CSPSource::kHasWildcard);

  EXPECT_TRUE(source.Matches(KURL(base, "http://example.com:8000/")));
  EXPECT_FALSE(source.Matches(KURL(base, "http://not-example.com:8000/")));
  EXPECT_TRUE(source.Matches(KURL(base, "https://example.com:8000/")));
  EXPECT_FALSE(source.Matches(KURL(base, "https://not-example.com:8000/")));
}

TEST_F(CSPSourceTest, SchemeIsEmpty) {
  KURL base;

  // Self scheme is http.
  {
    Persistent<ContentSecurityPolicy> csp(
        MakeGarbageCollected<ContentSecurityPolicy>());
    csp->SetupSelf(*SecurityOrigin::CreateFromString("http://a.com/"));
    CSPSource source(csp.Get(), "", "a.com", CSPSource::kPortUnspecified, "/",
                     CSPSource::kNoWildcard, CSPSource::kNoWildcard);
    EXPECT_TRUE(source.Matches(KURL(base, "http://a.com")));
    EXPECT_TRUE(source.Matches(KURL(base, "https://a.com")));
    EXPECT_FALSE(source.Matches(KURL(base, "ftp://a.com")));
  }

  // Self scheme is https.
  {
    Persistent<ContentSecurityPolicy> csp(
        MakeGarbageCollected<ContentSecurityPolicy>());
    csp->SetupSelf(*SecurityOrigin::CreateFromString("https://a.com/"));
    CSPSource source(csp.Get(), "", "a.com", CSPSource::kPortUnspecified, "/",
                     CSPSource::kNoWildcard, CSPSource::kNoWildcard);
    EXPECT_FALSE(source.Matches(KURL(base, "http://a.com")));
    EXPECT_TRUE(source.Matches(KURL(base, "https://a.com")));
    EXPECT_FALSE(source.Matches(KURL(base, "ftp://a.com")));
  }

  // Self scheme is not in the http familly.
  {
    Persistent<ContentSecurityPolicy> csp(
        MakeGarbageCollected<ContentSecurityPolicy>());
    csp->SetupSelf(*SecurityOrigin::CreateFromString("ftp://a.com/"));
    CSPSource source(csp.Get(), "", "a.com", CSPSource::kPortUnspecified, "/",
                     CSPSource::kNoWildcard, CSPSource::kNoWildcard);
    EXPECT_FALSE(source.Matches(KURL(base, "http://a.com")));
    EXPECT_TRUE(source.Matches(KURL(base, "ftp://a.com")));
  }

  // Self scheme is unique
  {
    Persistent<ContentSecurityPolicy> csp(
        MakeGarbageCollected<ContentSecurityPolicy>());
    csp->SetupSelf(
        *SecurityOrigin::CreateFromString("non-standard-scheme://a.com/"));
    CSPSource source(csp.Get(), "", "a.com", CSPSource::kPortUnspecified, "/",
                     CSPSource::kNoWildcard, CSPSource::kNoWildcard);
    EXPECT_FALSE(source.Matches(KURL(base, "http://a.com")));

    // The reason matching fails is because the host is parsed as "" when
    // using a non standard scheme even though it should be parsed as "a.com"
    // After adding it to the list of standard schemes it now gets parsed
    // correctly. This does not matter in practice though because there is
    // no way to render/load anything like "non-standard-scheme://a.com"
    EXPECT_FALSE(source.Matches(KURL(base, "non-standard-scheme://a.com")));
  }
}

TEST_F(CSPSourceTest, InsecureHostSchemePortMatchesSecurePort) {
  KURL base;

  // source scheme is "http", source port is 80
  {
    CSPSource source(csp.Get(), "http", "example.com", 80, "/",
                     CSPSource::kNoWildcard, CSPSource::kNoWildcard);
    EXPECT_TRUE(source.Matches(KURL(base, "http://example.com/")));
    EXPECT_TRUE(source.Matches(KURL(base, "http://example.com:80/")));

    // Should not allow scheme upgrades unless both port and scheme are upgraded
    EXPECT_FALSE(source.Matches(KURL(base, "http://example.com:443/")));
    EXPECT_TRUE(source.Matches(KURL(base, "https://example.com/")));
    EXPECT_FALSE(source.Matches(KURL(base, "https://example.com:80/")));

    EXPECT_TRUE(source.Matches(KURL(base, "https://example.com:443/")));

    EXPECT_FALSE(source.Matches(KURL(base, "http://example.com:8443/")));
    EXPECT_FALSE(source.Matches(KURL(base, "https://example.com:8443/")));

    EXPECT_FALSE(source.Matches(KURL(base, "http://not-example.com/")));
    EXPECT_FALSE(source.Matches(KURL(base, "http://not-example.com:80/")));
    EXPECT_FALSE(source.Matches(KURL(base, "http://not-example.com:443/")));
    EXPECT_FALSE(source.Matches(KURL(base, "https://not-example.com/")));
    EXPECT_FALSE(source.Matches(KURL(base, "https://not-example.com:80/")));
    EXPECT_FALSE(source.Matches(KURL(base, "https://not-example.com:443/")));
  }

  // source scheme is "http", source port is 443
  {
    CSPSource source(csp.Get(), "http", "example.com", 443, "/",
                     CSPSource::kNoWildcard, CSPSource::kNoWildcard);
    EXPECT_TRUE(source.Matches(KURL(base, "https://example.com/")));
  }

  // source scheme is empty
  {
    Persistent<ContentSecurityPolicy> csp(
        MakeGarbageCollected<ContentSecurityPolicy>());
    csp->SetupSelf(*SecurityOrigin::CreateFromString("http://example.com"));
    CSPSource source(csp.Get(), "", "example.com", 80, "/",
                     CSPSource::kNoWildcard, CSPSource::kNoWildcard);
    EXPECT_TRUE(source.Matches(KURL(base, "http://example.com/")));
    EXPECT_TRUE(source.Matches(KURL(base, "https://example.com:443")));
    // Should not allow upgrade of port or scheme without upgrading both
    EXPECT_FALSE(source.Matches(KURL(base, "http://example.com:443")));
  }

  // source port is empty
  {
    CSPSource source(csp.Get(), "http", "example.com",
                     CSPSource::kPortUnspecified, "/", CSPSource::kNoWildcard,
                     CSPSource::kNoWildcard);

    EXPECT_TRUE(source.Matches(KURL(base, "http://example.com")));
    EXPECT_TRUE(source.Matches(KURL(base, "https://example.com")));
    EXPECT_TRUE(source.Matches(KURL(base, "https://example.com:443")));
    // Should not allow upgrade of port or scheme without upgrading both
    EXPECT_FALSE(source.Matches(KURL(base, "https://example.com:80")));
    EXPECT_FALSE(source.Matches(KURL(base, "http://example.com:443")));
  }
}

TEST_F(CSPSourceTest, HostMatches) {
  KURL base;
  Persistent<ContentSecurityPolicy> csp(
      MakeGarbageCollected<ContentSecurityPolicy>());
  csp->SetupSelf(*SecurityOrigin::CreateFromString("http://a.com"));

  // Host is * (source-expression = "http://*")
  {
    CSPSource source(csp.Get(), "http", "", CSPSource::kPortUnspecified, "",
                     CSPSource::kHasWildcard, CSPSource::kNoWildcard);
    EXPECT_TRUE(source.Matches(KURL(base, "http://a.com")));
    EXPECT_TRUE(source.Matches(KURL(base, "http://.")));
  }

  // Host is *.foo.bar
  {
    CSPSource source(csp.Get(), "", "foo.bar", CSPSource::kPortUnspecified, "",
                     CSPSource::kHasWildcard, CSPSource::kNoWildcard);
    EXPECT_FALSE(source.Matches(KURL(base, "http://a.com")));
    EXPECT_FALSE(source.Matches(KURL(base, "http://bar")));
    EXPECT_FALSE(source.Matches(KURL(base, "http://foo.bar")));
    EXPECT_FALSE(source.Matches(KURL(base, "http://o.bar")));
    EXPECT_TRUE(source.Matches(KURL(base, "http://*.foo.bar")));
    EXPECT_TRUE(source.Matches(KURL(base, "http://sub.foo.bar")));
    EXPECT_TRUE(source.Matches(KURL(base, "http://sub.sub.foo.bar")));
    // Please see http://crbug.com/692505
    EXPECT_TRUE(source.Matches(KURL(base, "http://.foo.bar")));
  }

  // Host is exact.
  {
    CSPSource source(csp.Get(), "", "foo.bar", CSPSource::kPortUnspecified, "",
                     CSPSource::kNoWildcard, CSPSource::kNoWildcard);
    EXPECT_TRUE(source.Matches(KURL(base, "http://foo.bar")));
    EXPECT_FALSE(source.Matches(KURL(base, "http://sub.foo.bar")));
    EXPECT_FALSE(source.Matches(KURL(base, "http://bar")));
    // Please see http://crbug.com/692505
    EXPECT_FALSE(source.Matches(KURL(base, "http://.foo.bar")));
  }

  // Host matching is case-insensitive.
  {
    CSPSource source(csp.Get(), "", "FoO.BaR", CSPSource::kPortUnspecified, "",
                     CSPSource::kNoWildcard, CSPSource::kNoWildcard);
    EXPECT_TRUE(source.Matches(KURL(base, "http://foo.bar")));
    EXPECT_FALSE(source.Matches(KURL(base, "http://sub.foo.bar")));
  }

  // Wildcarded host matching is case-insensitive.
  {
    CSPSource source(csp.Get(), "", "FoO.BaR", CSPSource::kPortUnspecified, "",
                     CSPSource::kHasWildcard, CSPSource::kNoWildcard);
    EXPECT_TRUE(source.Matches(KURL(base, "http://sub.foo.bar")));
    EXPECT_FALSE(source.Matches(KURL(base, "http://foo.bar")));
  }
}

TEST_F(CSPSourceTest, MatchingAsSelf) {
  // Testing Step 4 of
  // https://w3c.github.io/webappsec-csp/#match-url-to-source-expression
  struct TestCase {
    const Source self_source;
    const String& url;
    bool expected;
  } cases[] = {
      // Same origin
      {{"http", "example.com", "", 80, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       "http://example.com:80/",
       true},
      {{"https", "example.com", "", 443, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       "https://example.com:443/",
       true},
      {{"https", "example.com", "", 4545, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       "https://example.com:4545/",
       true},  // Mismatching origin
      // Mismatching host
      {{"http", "example.com", "", 80, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       "http://example2.com:80/",
       false},
      // Ports not matching default schemes
      {{"http", "example.com", "", 8080, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       "https://example.com:443/",
       false},
      {{"http", "example.com", "", 80, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       "wss://example.com:8443/",
       false},
      // Allowed different scheme combinations (4.2.1 and 4.2.2)
      {{"http", "example.com", "", 80, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       "https://example.com:443/",
       true},
      {{"http", "example.com", "", 80, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       "ws://example.com:80/",
       true},
      {{"http", "example.com", "", 80, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       "wss://example.com:443/",
       true},
      {{"ws", "example.com", "", 80, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       "https://example.com:443/",
       true},
      {{"wss", "example.com", "", 443, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       "https://example.com:443/",
       true},
      {{"https", "example.com", "", 443, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       "wss://example.com:443/",
       true},
      // Ports not set (aka default)
      {{"https", "example.com", "", CSPSource::kPortUnspecified,
        CSPSource::kNoWildcard, CSPSource::kNoWildcard},
       "wss://example.com:443/",
       true},
      {{"https", "example.com", "", 443, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       "wss://example.com/",
       true},

      // Paths are ignored
      {{"http", "example.com", "", 80, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       "https://example.com:443/some-path-here",
       true},
      {{"http", "example.com", "", 80, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       "ws://example.com:80/some-other-path-here",
       true},

      // Custom schemes
      {{"http", "example.com", "", 80, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       "custom-scheme://example.com/",
       false},
      {{"http", "example.com", "", 80, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       "custom-scheme://example.com:80/",
       false},
      {{"https", "example.com", "", 443, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       "custom-scheme://example.com/",
       false},
      {{"https", "example.com", "", 443, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       "custom-scheme://example.com:443/",
       false},
      {{"https", "example.com", "", 443, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       "custom-scheme://example.com/some-path",
       false},
      {{"http", "example.com", "", CSPSource::kPortUnspecified,
        CSPSource::kNoWildcard, CSPSource::kNoWildcard},
       "custom-scheme://example.com/some-path",
       false},
  };

  KURL base;
  for (const auto& test : cases) {
    CSPSource* self_source = MakeGarbageCollected<CSPSource>(
        csp.Get(), test.self_source.scheme, test.self_source.host,
        test.self_source.port, test.self_source.path,
        test.self_source.host_wildcard, test.self_source.port_wildcard);
    EXPECT_EQ(self_source->MatchesAsSelf(KURL(base, test.url)), test.expected);
  }
}

}  // namespace blink
