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
    // port is 0 if it was not specified so the default port for a given scheme
    // will be used.
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
  CSPSource source(csp.Get(), "http", "example.com", 0, "/",
                   CSPSource::kHasWildcard, CSPSource::kHasWildcard);

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
  CSPSource source(csp.Get(), "http", "", 0, "/", CSPSource::kNoWildcard,
                   CSPSource::kHasWildcard);

  EXPECT_TRUE(source.Matches(KURL(base, "http://example.com:8000/")));
  EXPECT_TRUE(source.Matches(KURL(base, "https://example.com:8000/")));
  EXPECT_TRUE(source.Matches(KURL(base, "http://not-example.com:8000/")));
  EXPECT_TRUE(source.Matches(KURL(base, "https://not-example.com:8000/")));
  EXPECT_FALSE(source.Matches(KURL(base, "ftp://example.com:8000/")));
}

TEST_F(CSPSourceTest, InsecureHostSchemeMatchesSecureScheme) {
  KURL base;
  CSPSource source(csp.Get(), "http", "example.com", 0, "/",
                   CSPSource::kNoWildcard, CSPSource::kHasWildcard);

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
    CSPSource source(csp.Get(), "", "a.com", 0, "/", CSPSource::kNoWildcard,
                     CSPSource::kNoWildcard);
    EXPECT_TRUE(source.Matches(KURL(base, "http://a.com")));
    EXPECT_TRUE(source.Matches(KURL(base, "https://a.com")));
    EXPECT_FALSE(source.Matches(KURL(base, "ftp://a.com")));
  }

  // Self scheme is https.
  {
    Persistent<ContentSecurityPolicy> csp(
        MakeGarbageCollected<ContentSecurityPolicy>());
    csp->SetupSelf(*SecurityOrigin::CreateFromString("https://a.com/"));
    CSPSource source(csp.Get(), "", "a.com", 0, "/", CSPSource::kNoWildcard,
                     CSPSource::kNoWildcard);
    EXPECT_FALSE(source.Matches(KURL(base, "http://a.com")));
    EXPECT_TRUE(source.Matches(KURL(base, "https://a.com")));
    EXPECT_FALSE(source.Matches(KURL(base, "ftp://a.com")));
  }

  // Self scheme is not in the http familly.
  {
    Persistent<ContentSecurityPolicy> csp(
        MakeGarbageCollected<ContentSecurityPolicy>());
    csp->SetupSelf(*SecurityOrigin::CreateFromString("ftp://a.com/"));
    CSPSource source(csp.Get(), "", "a.com", 0, "/", CSPSource::kNoWildcard,
                     CSPSource::kNoWildcard);
    EXPECT_FALSE(source.Matches(KURL(base, "http://a.com")));
    EXPECT_TRUE(source.Matches(KURL(base, "ftp://a.com")));
  }

  // Self scheme is unique
  {
    Persistent<ContentSecurityPolicy> csp(
        MakeGarbageCollected<ContentSecurityPolicy>());
    csp->SetupSelf(
        *SecurityOrigin::CreateFromString("non-standard-scheme://a.com/"));
    CSPSource source(csp.Get(), "", "a.com", 0, "/", CSPSource::kNoWildcard,
                     CSPSource::kNoWildcard);
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
    CSPSource source(csp.Get(), "http", "example.com", 0, "/",
                     CSPSource::kNoWildcard, CSPSource::kNoWildcard);

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
    CSPSource source(csp.Get(), "http", "", 0, "", CSPSource::kHasWildcard,
                     CSPSource::kNoWildcard);
    EXPECT_TRUE(source.Matches(KURL(base, "http://a.com")));
    EXPECT_TRUE(source.Matches(KURL(base, "http://.")));
  }

  // Host is *.foo.bar
  {
    CSPSource source(csp.Get(), "", "foo.bar", 0, "", CSPSource::kHasWildcard,
                     CSPSource::kNoWildcard);
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
    CSPSource source(csp.Get(), "", "foo.bar", 0, "", CSPSource::kNoWildcard,
                     CSPSource::kNoWildcard);
    EXPECT_TRUE(source.Matches(KURL(base, "http://foo.bar")));
    EXPECT_FALSE(source.Matches(KURL(base, "http://sub.foo.bar")));
    EXPECT_FALSE(source.Matches(KURL(base, "http://bar")));
    // Please see http://crbug.com/692505
    EXPECT_FALSE(source.Matches(KURL(base, "http://.foo.bar")));
  }

  // Host matching is case-insensitive.
  {
    CSPSource source(csp.Get(), "", "FoO.BaR", 0, "", CSPSource::kNoWildcard,
                     CSPSource::kNoWildcard);
    EXPECT_TRUE(source.Matches(KURL(base, "http://foo.bar")));
    EXPECT_FALSE(source.Matches(KURL(base, "http://sub.foo.bar")));
  }

  // Wildcarded host matching is case-insensitive.
  {
    CSPSource source(csp.Get(), "", "FoO.BaR", 0, "", CSPSource::kHasWildcard,
                     CSPSource::kNoWildcard);
    EXPECT_TRUE(source.Matches(KURL(base, "http://sub.foo.bar")));
    EXPECT_FALSE(source.Matches(KURL(base, "http://foo.bar")));
  }
}

TEST_F(CSPSourceTest, DoesNotSubsume) {
  struct Source {
    const char* scheme;
    const char* host;
    const char* path;
    const int port;
  };
  struct TestCase {
    const Source a;
    const Source b;
  } cases[] = {
      {{"http", "example.com", "/", 0}, {"http", "another.com", "/", 0}},
      {{"wss", "example.com", "/", 0}, {"http", "example.com", "/", 0}},
      {{"wss", "example.com", "/", 0}, {"about", "example.com", "/", 0}},
      {{"http", "example.com", "/", 0}, {"about", "example.com", "/", 0}},
      {{"http", "example.com", "/1.html", 0},
       {"http", "example.com", "/2.html", 0}},
      {{"http", "example.com", "/", 443}, {"about", "example.com", "/", 800}},
  };
  for (const auto& test : cases) {
    CSPSource* returned = MakeGarbageCollected<CSPSource>(
        csp.Get(), test.a.scheme, test.a.host, test.a.port, test.a.path,
        CSPSource::kNoWildcard, CSPSource::kNoWildcard);

    CSPSource* required = MakeGarbageCollected<CSPSource>(
        csp.Get(), test.b.scheme, test.b.host, test.b.port, test.b.path,
        CSPSource::kNoWildcard, CSPSource::kNoWildcard);

    EXPECT_FALSE(required->Subsumes(returned));
    // Verify the same test with a and b swapped.
    EXPECT_FALSE(required->Subsumes(returned));
  }
}

TEST_F(CSPSourceTest, Subsumes) {
  struct Source {
    const char* scheme;
    const char* path;
    const int port;
  };
  struct TestCase {
    const Source a;
    const Source b;
    bool expected;
    bool expected_when_swapped;
  } cases[] = {
      // Equal signals
      {{"http", "/", 0}, {"http", "/", 0}, true, true},
      {{"https", "/", 0}, {"https", "/", 0}, true, true},
      {{"https", "/page1.html", 0}, {"https", "/page1.html", 0}, true, true},
      {{"http", "/", 70}, {"http", "/", 70}, true, true},
      {{"https", "/", 70}, {"https", "/", 70}, true, true},
      {{"https", "/page1.html", 0}, {"https", "/page1.html", 0}, true, true},
      {{"http", "/page1.html", 70}, {"http", "/page1.html", 70}, true, true},
      {{"https", "/page1.html", 70}, {"https", "/page1.html", 70}, true, true},
      {{"http", "/", 0}, {"http", "", 0}, true, true},
      {{"http", "/", 80}, {"http", "", 80}, true, true},
      {{"http", "/", 80}, {"https", "", 443}, false, true},
      // One stronger signal in the first CSPSource
      {{"https", "/", 0}, {"http", "/", 0}, true, false},
      {{"http", "/page1.html", 0}, {"http", "/", 0}, true, false},
      {{"http", "/", 80}, {"http", "/", 0}, true, true},
      {{"http", "/", 700}, {"http", "/", 0}, false, false},
      // Two stronger signals in the first CSPSource
      {{"https", "/page1.html", 0}, {"http", "/", 0}, true, false},
      {{"https", "/", 80}, {"http", "/", 0}, false, false},
      {{"http", "/page1.html", 80}, {"http", "/", 0}, true, false},
      // Three stronger signals in the first CSPSource
      {{"https", "/page1.html", 70}, {"http", "/", 0}, false, false},
      // Mixed signals
      {{"https", "/", 0}, {"http", "/page1.html", 0}, false, false},
      {{"https", "/", 0}, {"http", "/", 70}, false, false},
      {{"http", "/page1.html", 0}, {"http", "/", 70}, false, false},
  };

  for (const auto& test : cases) {
    CSPSource* returned = MakeGarbageCollected<CSPSource>(
        csp.Get(), test.a.scheme, "example.com", test.a.port, test.a.path,
        CSPSource::kNoWildcard, CSPSource::kNoWildcard);

    CSPSource* required = MakeGarbageCollected<CSPSource>(
        csp.Get(), test.b.scheme, "example.com", test.b.port, test.b.path,
        CSPSource::kNoWildcard, CSPSource::kNoWildcard);

    EXPECT_EQ(required->Subsumes(returned), test.expected);
    // Verify the same test with a and b swapped.
    EXPECT_EQ(returned->Subsumes(required), test.expected_when_swapped);
  }

  // When returned CSP has a wildcard but the required csp doesn't, then it is
  // not subsumed.
  for (const auto& test : cases) {
    CSPSource* returned = MakeGarbageCollected<CSPSource>(
        csp.Get(), test.a.scheme, "example.com", test.a.port, test.a.path,
        CSPSource::kHasWildcard, CSPSource::kNoWildcard);
    CSPSource* required = MakeGarbageCollected<CSPSource>(
        csp.Get(), test.b.scheme, "example.com", test.b.port, test.b.path,
        CSPSource::kNoWildcard, CSPSource::kNoWildcard);

    EXPECT_FALSE(required->Subsumes(returned));

    // If required csp also allows a wildcard in host, then the answer should be
    // as expected.
    CSPSource* required2 = MakeGarbageCollected<CSPSource>(
        csp.Get(), test.b.scheme, "example.com", test.b.port, test.b.path,
        CSPSource::kHasWildcard, CSPSource::kNoWildcard);
    EXPECT_EQ(required2->Subsumes(returned), test.expected);
  }
}

TEST_F(CSPSourceTest, WildcardsSubsumes) {
  struct Wildcards {
    CSPSource::WildcardDisposition host_dispotion;
    CSPSource::WildcardDisposition port_dispotion;
  };
  struct TestCase {
    const Wildcards a;
    const Wildcards b;
    bool expected;
  } cases[] = {
      // One out of four possible wildcards.
      {{CSPSource::kHasWildcard, CSPSource::kNoWildcard},
       {CSPSource::kNoWildcard, CSPSource::kNoWildcard},
       false},
      {{CSPSource::kNoWildcard, CSPSource::kHasWildcard},
       {CSPSource::kNoWildcard, CSPSource::kNoWildcard},
       false},
      {{CSPSource::kNoWildcard, CSPSource::kNoWildcard},
       {CSPSource::kNoWildcard, CSPSource::kHasWildcard},
       true},
      {{CSPSource::kNoWildcard, CSPSource::kNoWildcard},
       {CSPSource::kHasWildcard, CSPSource::kNoWildcard},
       true},
      // Two out of four possible wildcards.
      {{CSPSource::kHasWildcard, CSPSource::kHasWildcard},
       {CSPSource::kNoWildcard, CSPSource::kNoWildcard},
       false},
      {{CSPSource::kHasWildcard, CSPSource::kNoWildcard},
       {CSPSource::kHasWildcard, CSPSource::kNoWildcard},
       true},
      {{CSPSource::kHasWildcard, CSPSource::kNoWildcard},
       {CSPSource::kNoWildcard, CSPSource::kHasWildcard},
       false},
      {{CSPSource::kNoWildcard, CSPSource::kHasWildcard},
       {CSPSource::kHasWildcard, CSPSource::kNoWildcard},
       false},
      {{CSPSource::kNoWildcard, CSPSource::kHasWildcard},
       {CSPSource::kNoWildcard, CSPSource::kHasWildcard},
       true},
      {{CSPSource::kNoWildcard, CSPSource::kNoWildcard},
       {CSPSource::kHasWildcard, CSPSource::kHasWildcard},
       true},
      // Three out of four possible wildcards.
      {{CSPSource::kHasWildcard, CSPSource::kHasWildcard},
       {CSPSource::kHasWildcard, CSPSource::kNoWildcard},
       false},
      {{CSPSource::kHasWildcard, CSPSource::kHasWildcard},
       {CSPSource::kNoWildcard, CSPSource::kHasWildcard},
       false},
      {{CSPSource::kHasWildcard, CSPSource::kNoWildcard},
       {CSPSource::kHasWildcard, CSPSource::kHasWildcard},
       true},
      {{CSPSource::kNoWildcard, CSPSource::kHasWildcard},
       {CSPSource::kHasWildcard, CSPSource::kHasWildcard},
       true},
      // Four out of four possible wildcards.
      {{CSPSource::kHasWildcard, CSPSource::kHasWildcard},
       {CSPSource::kHasWildcard, CSPSource::kHasWildcard},
       true},
  };

  // There are different cases for wildcards but now also the second CSPSource
  // has a more specific path.
  for (const auto& test : cases) {
    CSPSource* returned = MakeGarbageCollected<CSPSource>(
        csp.Get(), "http", "example.com", 0, "/", test.a.host_dispotion,
        test.a.port_dispotion);
    CSPSource* required = MakeGarbageCollected<CSPSource>(
        csp.Get(), "http", "example.com", 0, "/", test.b.host_dispotion,
        test.b.port_dispotion);
    EXPECT_EQ(required->Subsumes(returned), test.expected);

    // Wildcards should not matter when required csp is stricter than returned
    // csp.
    CSPSource* required2 = MakeGarbageCollected<CSPSource>(
        csp.Get(), "https", "example.com", 0, "/", test.b.host_dispotion,
        test.b.port_dispotion);
    EXPECT_FALSE(required2->Subsumes(returned));
  }
}

TEST_F(CSPSourceTest, SchemesOnlySubsumes) {
  struct TestCase {
    String a_scheme;
    String b_scheme;
    bool expected;
  } cases[] = {
      // HTTP
      {"http", "http", true},
      {"http", "https", false},
      {"https", "http", true},
      {"https", "https", true},
      // WSS
      {"ws", "ws", true},
      {"ws", "wss", false},
      {"wss", "ws", true},
      {"wss", "wss", true},
      // Unequal
      {"ws", "http", false},
      {"http", "ws", false},
      {"http", "about", false},
  };

  for (const auto& test : cases) {
    CSPSource* returned = MakeGarbageCollected<CSPSource>(
        csp.Get(), test.a_scheme, "example.com", 0, "/", CSPSource::kNoWildcard,
        CSPSource::kNoWildcard);
    CSPSource* required = MakeGarbageCollected<CSPSource>(
        csp.Get(), test.b_scheme, "example.com", 0, "/", CSPSource::kNoWildcard,
        CSPSource::kNoWildcard);
    EXPECT_EQ(required->Subsumes(returned), test.expected);
  }
}

TEST_F(CSPSourceTest, IsSimilar) {
  struct Source {
    const char* scheme;
    const char* host;
    const char* path;
    const int port;
  };
  struct TestCase {
    const Source a;
    const Source b;
    bool is_similar;
  } cases[] = {
      // Similar
      {{"http", "example.com", "/", 0}, {"http", "example.com", "/", 0}, true},
      // Schemes
      {{"https", "example.com", "/", 0},
       {"https", "example.com", "/", 0},
       true},
      {{"https", "example.com", "/", 0}, {"http", "example.com", "/", 0}, true},
      {{"ws", "example.com", "/", 0}, {"wss", "example.com", "/", 0}, true},
      // Ports
      {{"http", "example.com", "/", 90},
       {"http", "example.com", "/", 90},
       true},
      {{"wss", "example.com", "/", 0},
       {"wss", "example.com", "/", 0},
       true},  // use default port
      {{"http", "example.com", "/", 80}, {"http", "example.com", "/", 0}, true},
      {{"http", "example.com", "/", 80},
       {"https", "example.com", "/", 443},
       true},
      {{"http", "example.com", "/", 80},
       {"https", "example.com", "/", 444},
       false},
      // Paths
      {{"http", "example.com", "/", 0},
       {"http", "example.com", "/1.html", 0},
       true},
      {{"http", "example.com", "/", 0}, {"http", "example.com", "", 0}, true},
      {{"http", "example.com", "/", 0},
       {"http", "example.com", "/a/b/", 0},
       true},
      {{"http", "example.com", "/a/", 0},
       {"http", "example.com", "/a/", 0},
       true},
      {{"http", "example.com", "/a/", 0},
       {"http", "example.com", "/a/b/", 0},
       true},
      {{"http", "example.com", "/a/", 0},
       {"http", "example.com", "/a/b/1.html", 0},
       true},
      {{"http", "example.com", "/1.html", 0},
       {"http", "example.com", "/1.html", 0},
       true},
      // Mixed
      {{"http", "example.com", "/1.html", 90},
       {"http", "example.com", "/", 90},
       true},
      {{"https", "example.com", "/", 0}, {"http", "example.com", "/", 0}, true},
      {{"http", "example.com", "/a/", 90},
       {"https", "example.com", "", 90},
       true},
      {{"wss", "example.com", "/a/", 90},
       {"ws", "example.com", "/a/b/", 90},
       true},
      {{"https", "example.com", "/a/", 90},
       {"https", "example.com", "/a/b/", 90},
       true},
      // Not Similar
      {{"http", "example.com", "/a/", 0},
       {"https", "example.com", "", 90},
       false},
      {{"https", "example.com", "/", 0},
       {"https", "example.com", "/", 90},
       false},
      {{"http", "example.com", "/", 0}, {"http", "another.com", "/", 0}, false},
      {{"wss", "example.com", "/", 0}, {"http", "example.com", "/", 0}, false},
      {{"wss", "example.com", "/", 0}, {"about", "example.com", "/", 0}, false},
      {{"http", "example.com", "/", 0},
       {"about", "example.com", "/", 0},
       false},
      {{"http", "example.com", "/1.html", 0},
       {"http", "example.com", "/2.html", 0},
       false},
      {{"http", "example.com", "/a/1.html", 0},
       {"http", "example.com", "/a/b/", 0},
       false},
      {{"http", "example.com", "/", 443},
       {"about", "example.com", "/", 800},
       false},
  };

  for (const auto& test : cases) {
    CSPSource* returned = MakeGarbageCollected<CSPSource>(
        csp.Get(), test.a.scheme, test.a.host, test.a.port, test.a.path,
        CSPSource::kNoWildcard, CSPSource::kNoWildcard);

    CSPSource* required = MakeGarbageCollected<CSPSource>(
        csp.Get(), test.b.scheme, test.b.host, test.b.port, test.b.path,
        CSPSource::kNoWildcard, CSPSource::kNoWildcard);

    EXPECT_EQ(returned->IsSimilar(required), test.is_similar);
    // Verify the same test with a and b swapped.
    EXPECT_EQ(required->IsSimilar(returned), test.is_similar);
  }
}

TEST_F(CSPSourceTest, FirstSubsumesSecond) {
  struct Source {
    const char* scheme;
    const char* host;
    const int port;
    const char* path;
  };
  struct TestCase {
    const Source source_b;
    String scheme_a;
    bool expected;
  } cases[] = {
      // Subsumed.
      {{"http", "example.com", 0, "/"}, "http", true},
      {{"http", "example.com", 0, "/page.html"}, "http", true},
      {{"http", "second-example.com", 80, "/"}, "http", true},
      {{"https", "second-example.com", 0, "/"}, "http", true},
      {{"http", "second-example.com", 0, "/page.html"}, "http", true},
      {{"https", "second-example.com", 80, "/page.html"}, "http", true},
      {{"https", "second-example.com", 0, "/"}, "https", true},
      {{"https", "second-example.com", 0, "/page.html"}, "https", true},
      {{"http", "example.com", 900, "/"}, "http", true},
      // NOT subsumed.
      {{"http", "second-example.com", 0, "/"}, "wss", false},
      {{"http", "non-example.com", 900, "/"}, "http", false},
      {{"http", "second-example.com", 0, "/"}, "https", false},
  };

  CSPSource* no_wildcards = MakeGarbageCollected<CSPSource>(
      csp.Get(), "http", "example.com", 0, "/", CSPSource::kNoWildcard,
      CSPSource::kNoWildcard);
  CSPSource* host_wildcard = MakeGarbageCollected<CSPSource>(
      csp.Get(), "http", "third-example.com", 0, "/", CSPSource::kHasWildcard,
      CSPSource::kNoWildcard);
  CSPSource* port_wildcard = MakeGarbageCollected<CSPSource>(
      csp.Get(), "http", "third-example.com", 0, "/", CSPSource::kNoWildcard,
      CSPSource::kHasWildcard);
  CSPSource* both_wildcards = MakeGarbageCollected<CSPSource>(
      csp.Get(), "http", "third-example.com", 0, "/", CSPSource::kHasWildcard,
      CSPSource::kHasWildcard);
  CSPSource* http_only = MakeGarbageCollected<CSPSource>(
      csp.Get(), "http", "", 0, "", CSPSource::kNoWildcard,
      CSPSource::kNoWildcard);
  CSPSource* https_only = MakeGarbageCollected<CSPSource>(
      csp.Get(), "https", "", 0, "", CSPSource::kNoWildcard,
      CSPSource::kNoWildcard);

  for (const auto& test : cases) {
    // Setup default vectors.
    HeapVector<Member<CSPSource>> list_a;
    HeapVector<Member<CSPSource>> list_b;
    list_b.push_back(no_wildcards);
    // Empty `listA` implies `none` is allowed.
    EXPECT_FALSE(CSPSource::FirstSubsumesSecond(list_a, list_b));

    list_a.push_back(no_wildcards);
    // Add CSPSources based on the current test.
    list_b.push_back(MakeGarbageCollected<CSPSource>(
        csp.Get(), test.source_b.scheme, test.source_b.host, 0,
        test.source_b.path, CSPSource::kNoWildcard, CSPSource::kNoWildcard));
    list_a.push_back(MakeGarbageCollected<CSPSource>(
        csp.Get(), test.scheme_a, "second-example.com", 0, "/",
        CSPSource::kNoWildcard, CSPSource::kNoWildcard));
    // listB contains: ["http://example.com/", test.listB]
    // listA contains: ["http://example.com/",
    // test.schemeA + "://second-example.com/"]
    EXPECT_EQ(test.expected, CSPSource::FirstSubsumesSecond(list_a, list_b));

    // If we add another source to `listB` with a host wildcard,
    // then the result should definitely be false.
    list_b.push_back(host_wildcard);

    // If we add another source to `listA` with a port wildcard,
    // it does not make `listB` to be subsumed under `listA`.
    list_b.push_back(port_wildcard);
    EXPECT_FALSE(CSPSource::FirstSubsumesSecond(list_a, list_b));

    // If however we add another source to `listA` with both wildcards,
    // that CSPSource is subsumed, so the answer should be as expected
    // before.
    list_a.push_back(both_wildcards);
    EXPECT_EQ(test.expected, CSPSource::FirstSubsumesSecond(list_a, list_b));

    // If we add a scheme-source expression of 'https' to `listB`, then it
    // should not be subsumed.
    list_b.push_back(https_only);
    EXPECT_FALSE(CSPSource::FirstSubsumesSecond(list_a, list_b));

    // If we add a scheme-source expression of 'http' to `listA`, then it should
    // subsume all current epxression in `listB`.
    list_a.push_back(http_only);
    EXPECT_TRUE(CSPSource::FirstSubsumesSecond(list_a, list_b));
  }
}

TEST_F(CSPSourceTest, Intersect) {
  struct TestCase {
    const Source a;
    const Source b;
    const Source normalized;
  } cases[] = {
      {{"http", "example.com", "/", 0, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       {"http", "example.com", "/", 0, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       {"http", "example.com", "/", 0, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard}},
      {{"ws", "example.com", "/", 0, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       {"wss", "example.com", "/", 0, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       {"wss", "example.com", "/", 0, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard}},
      // Wildcards
      {{"http", "example.com", "/", 0, CSPSource::kHasWildcard,
        CSPSource::kNoWildcard},
       {"http", "example.com", "/", 0, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       {"http", "example.com", "/", 0, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard}},
      {{"http", "example.com", "/", 0, CSPSource::kHasWildcard,
        CSPSource::kHasWildcard},
       {"http", "example.com", "/", 0, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       {"http", "example.com", "/", 0, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard}},
      {{"http", "example.com", "/", 0, CSPSource::kHasWildcard,
        CSPSource::kNoWildcard},
       {"http", "example.com", "/", 0, CSPSource::kNoWildcard,
        CSPSource::kHasWildcard},
       {"http", "example.com", "/", 0, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard}},
      // Ports
      {{"http", "example.com", "/", 80, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       {"http", "example.com", "/", 0, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       {"http", "example.com", "/", 80, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard}},
      {{"http", "example.com", "/", 80, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       {"https", "example.com", "/", 443, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       {"https", "example.com", "/", 443, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard}},
      {{"https", "example.com", "/", 443, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       {"http", "example.com", "/", 80, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       {"https", "example.com", "/", 443, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard}},
      // Paths
      {{"http", "example.com", "/", 0, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       {"http", "example.com", "/1.html", 0, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       {"http", "example.com", "/1.html", 0, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard}},
      {{"http", "example.com", "/", 0, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       {"http", "example.com", "", 0, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       {"http", "example.com", "/", 0, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard}},
      {{"http", "example.com", "/", 0, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       {"http", "example.com", "/a/b/", 0, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       {"http", "example.com", "/a/b/", 0, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard}},
      {{"http", "example.com", "/a/", 0, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       {"http", "example.com", "/a/b/", 0, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       {"http", "example.com", "/a/b/", 0, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard}},
      {{"http", "example.com", "/a/", 0, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       {"http", "example.com", "/a/b/1.html", 0, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       {"http", "example.com", "/a/b/1.html", 0, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard}},
      // Mixed
      {{"http", "example.com", "/1.html", 0, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       {"http", "example.com", "/", 80, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       {"http", "example.com", "/1.html", 80, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard}},
  };

  for (const auto& test : cases) {
    CSPSource* a = MakeGarbageCollected<CSPSource>(
        csp.Get(), test.a.scheme, test.a.host, test.a.port, test.a.path,
        test.a.host_wildcard, test.a.port_wildcard);
    CSPSource* b = MakeGarbageCollected<CSPSource>(
        csp.Get(), test.b.scheme, test.b.host, test.b.port, test.b.path,
        test.b.host_wildcard, test.b.port_wildcard);

    CSPSource* normalized = a->Intersect(b);
    Source intersect_ab = {
        normalized->scheme_,        normalized->host_,
        normalized->path_,          normalized->port_,
        normalized->host_wildcard_, normalized->port_wildcard_};
    EXPECT_TRUE(EqualSources(intersect_ab, test.normalized));

    // Verify the same test with A and B swapped. The result should be
    // identical.
    normalized = b->Intersect(a);
    Source intersect_ba = {
        normalized->scheme_,        normalized->host_,
        normalized->path_,          normalized->port_,
        normalized->host_wildcard_, normalized->port_wildcard_};
    EXPECT_TRUE(EqualSources(intersect_ba, test.normalized));
  }
}

TEST_F(CSPSourceTest, IntersectSchemesOnly) {
  struct TestCase {
    const Source a;
    const Source b;
    const Source normalized;
  } cases[] = {
      // Both sources are schemes only.
      {{"http", "", "", 0, CSPSource::kNoWildcard, CSPSource::kNoWildcard},
       {"http", "", "", 0, CSPSource::kNoWildcard, CSPSource::kNoWildcard},
       {"http", "", "", 0, CSPSource::kNoWildcard, CSPSource::kNoWildcard}},
      {{"http", "", "", 0, CSPSource::kNoWildcard, CSPSource::kNoWildcard},
       {"https", "", "", 0, CSPSource::kNoWildcard, CSPSource::kNoWildcard},
       {"https", "", "", 0, CSPSource::kNoWildcard, CSPSource::kNoWildcard}},
      {{"ws", "", "", 0, CSPSource::kNoWildcard, CSPSource::kNoWildcard},
       {"wss", "", "", 0, CSPSource::kNoWildcard, CSPSource::kNoWildcard},
       {"wss", "", "", 0, CSPSource::kNoWildcard, CSPSource::kNoWildcard}},
      // One source is a scheme only and the other one has no wildcards.
      {{"http", "", "", 0, CSPSource::kNoWildcard, CSPSource::kNoWildcard},
       {"http", "example.com", "/", 0, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       {"http", "example.com", "/", 0, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard}},
      {{"http", "", "", 0, CSPSource::kNoWildcard, CSPSource::kNoWildcard},
       {"https", "example.com", "/", 80, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       {"https", "example.com", "/", 80, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard}},
      {{"https", "", "", 0, CSPSource::kNoWildcard, CSPSource::kNoWildcard},
       {"http", "example.com", "/page.html", 80, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
       {"https", "example.com", "/page.html", 80, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard}},
      // One source is a scheme only and the other has one or two wildcards.
      {{"https", "", "", 0, CSPSource::kNoWildcard, CSPSource::kNoWildcard},
       {"http", "example.com", "/page.html", 80, CSPSource::kHasWildcard,
        CSPSource::kNoWildcard},
       {"https", "example.com", "/page.html", 80, CSPSource::kHasWildcard,
        CSPSource::kNoWildcard}},
      {{"https", "", "", 0, CSPSource::kNoWildcard, CSPSource::kNoWildcard},
       {"http", "example.com", "/page.html", 80, CSPSource::kNoWildcard,
        CSPSource::kHasWildcard},
       {"https", "example.com", "/page.html", 80, CSPSource::kNoWildcard,
        CSPSource::kHasWildcard}},
      {{"https", "", "", 0, CSPSource::kNoWildcard, CSPSource::kNoWildcard},
       {"http", "example.com", "/page.html", 80, CSPSource::kHasWildcard,
        CSPSource::kHasWildcard},
       {"https", "example.com", "/page.html", 80, CSPSource::kHasWildcard,
        CSPSource::kHasWildcard}},
  };

  for (const auto& test : cases) {
    CSPSource* a = MakeGarbageCollected<CSPSource>(
        csp.Get(), test.a.scheme, test.a.host, test.a.port, test.a.path,
        test.a.host_wildcard, test.a.port_wildcard);

    CSPSource* b = MakeGarbageCollected<CSPSource>(
        csp.Get(), test.b.scheme, test.b.host, test.b.port, test.b.path,
        test.b.host_wildcard, test.b.port_wildcard);

    CSPSource* normalized = a->Intersect(b);
    Source intersect_ab = {
        normalized->scheme_,        normalized->host_,
        normalized->path_,          normalized->port_,
        normalized->host_wildcard_, normalized->port_wildcard_};
    EXPECT_TRUE(EqualSources(intersect_ab, test.normalized));

    // Verify the same test with A and B swapped. The result should be
    // identical.
    normalized = b->Intersect(a);
    Source intersect_ba = {
        normalized->scheme_,        normalized->host_,
        normalized->path_,          normalized->port_,
        normalized->host_wildcard_, normalized->port_wildcard_};
    EXPECT_TRUE(EqualSources(intersect_ba, test.normalized));
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
      {{"https", "example.com", "", 0, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
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
      {{"http", "example.com", "", 0, CSPSource::kNoWildcard,
        CSPSource::kNoWildcard},
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
