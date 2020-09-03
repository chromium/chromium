// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/content_security_policy/csp_source.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/cpp/content_security_policy/csp_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace network {

namespace {

// Allow() is an abbreviation of CSPSource::Allow(). Useful for writing test
// expectations on one line.
bool Allow(const network::mojom::CSPSourcePtr& source,
           const GURL& url,
           CSPContext* context,
           bool is_redirect = false) {
  return CheckCSPSource(source, url, context, is_redirect);
}

network::mojom::CSPSourcePtr CSPSource(const std::string& raw) {
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
  headers->SetHeader("Content-Security-Policy", "script-src " + raw);
  std::vector<mojom::ContentSecurityPolicyPtr> policies;
  AddContentSecurityPolicyFromHeaders(*headers, GURL("https://example.com/"),
                                      &policies);
  return std::move(
      policies[0]->directives[mojom::CSPDirectiveName::ScriptSrc]->sources[0]);
}

}  // namespace

TEST(CSPSourceTest, BasicMatching) {
  CSPContext context;

  auto source = network::mojom::CSPSource::New("http", "example.com", 8000,
                                               "/foo/", false, false);

  EXPECT_TRUE(Allow(source, GURL("http://example.com:8000/foo/"), &context));
  EXPECT_TRUE(Allow(source, GURL("http://example.com:8000/foo/bar"), &context));
  EXPECT_TRUE(Allow(source, GURL("HTTP://EXAMPLE.com:8000/foo/BAR"), &context));

  EXPECT_FALSE(Allow(source, GURL("http://example.com:8000/bar/"), &context));
  EXPECT_FALSE(Allow(source, GURL("https://example.com:8000/bar/"), &context));
  EXPECT_FALSE(Allow(source, GURL("http://example.com:9000/bar/"), &context));
  EXPECT_FALSE(
      Allow(source, GURL("HTTP://example.com:8000/FOO/bar"), &context));
  EXPECT_FALSE(
      Allow(source, GURL("HTTP://example.com:8000/FOO/BAR"), &context));
}

TEST(CSPSourceTest, AllowScheme) {
  CSPContext context;

  // http -> {http, https}.
  {
    auto source = network::mojom::CSPSource::New(
        "http", "", url::PORT_UNSPECIFIED, "", false, false);
    EXPECT_TRUE(Allow(source, GURL("http://a.com"), &context));
    EXPECT_TRUE(Allow(source, GURL("https://a.com"), &context));
    // This passes because the source is "scheme only" so the upgrade is
    // allowed.
    EXPECT_TRUE(Allow(source, GURL("https://a.com:80"), &context));
    EXPECT_FALSE(Allow(source, GURL("ftp://a.com"), &context));
    EXPECT_FALSE(Allow(source, GURL("ws://a.com"), &context));
    EXPECT_FALSE(Allow(source, GURL("wss://a.com"), &context));
  }

  // ws -> {ws, wss}.
  {
    auto source = network::mojom::CSPSource::New(
        "ws", "", url::PORT_UNSPECIFIED, "", false, false);
    EXPECT_FALSE(Allow(source, GURL("http://a.com"), &context));
    EXPECT_FALSE(Allow(source, GURL("https://a.com"), &context));
    EXPECT_FALSE(Allow(source, GURL("ftp://a.com"), &context));
    EXPECT_TRUE(Allow(source, GURL("ws://a.com"), &context));
    EXPECT_TRUE(Allow(source, GURL("wss://a.com"), &context));
  }

  // Exact matches required (ftp)
  {
    auto source = network::mojom::CSPSource::New(
        "ftp", "", url::PORT_UNSPECIFIED, "", false, false);
    EXPECT_TRUE(Allow(source, GURL("ftp://a.com"), &context));
    EXPECT_FALSE(Allow(source, GURL("http://a.com"), &context));
  }

  // Exact matches required (https)
  {
    auto source = network::mojom::CSPSource::New(
        "https", "", url::PORT_UNSPECIFIED, "", false, false);
    EXPECT_TRUE(Allow(source, GURL("https://a.com"), &context));
    EXPECT_FALSE(Allow(source, GURL("http://a.com"), &context));
  }

  // Exact matches required (wss)
  {
    auto source = network::mojom::CSPSource::New(
        "wss", "", url::PORT_UNSPECIFIED, "", false, false);
    EXPECT_TRUE(Allow(source, GURL("wss://a.com"), &context));
    EXPECT_FALSE(Allow(source, GURL("ws://a.com"), &context));
  }

  // Scheme is empty (ProtocolMatchesSelf).
  {
    auto source = network::mojom::CSPSource::New(
        "", "a.com", url::PORT_UNSPECIFIED, "", false, false);
    EXPECT_FALSE(Allow(source, GURL("http://a.com"), &context));

    // Self's scheme is http.
    context.SetSelf(url::Origin::Create(GURL("http://a.com")));
    EXPECT_TRUE(Allow(source, GURL("http://a.com"), &context));
    EXPECT_TRUE(Allow(source, GURL("https://a.com"), &context));
    EXPECT_FALSE(Allow(source, GURL("ftp://a.com"), &context));

    // Self's is https.
    context.SetSelf(url::Origin::Create(GURL("https://a.com")));
    EXPECT_FALSE(Allow(source, GURL("http://a.com"), &context));
    EXPECT_TRUE(Allow(source, GURL("https://a.com"), &context));
    EXPECT_FALSE(Allow(source, GURL("ftp://a.com"), &context));

    // Self's scheme is not in the http familly.
    context.SetSelf(url::Origin::Create(GURL("ftp://a.com/")));
    EXPECT_FALSE(Allow(source, GURL("http://a.com"), &context));
    EXPECT_TRUE(Allow(source, GURL("ftp://a.com"), &context));

    // Self's scheme is unique (non standard scheme).
    context.SetSelf(url::Origin::Create(GURL("non-standard-scheme://a.com")));
    EXPECT_FALSE(Allow(source, GURL("http://a.com"), &context));
    EXPECT_FALSE(Allow(source, GURL("non-standard-scheme://a.com"), &context));

    // Self's scheme is unique (data-url).
    context.SetSelf(
        url::Origin::Create(GURL("data:text/html,<iframe src=[...]>")));
    EXPECT_FALSE(Allow(source, GURL("http://a.com"), &context));
    EXPECT_FALSE(Allow(source, GURL("data:text/html,hello"), &context));
  }
}

TEST(CSPSourceTest, AllowHost) {
  CSPContext context;
  context.SetSelf(url::Origin::Create(GURL("http://example.com")));

  // Host is * (source-expression = "http://*")
  {
    auto source = network::mojom::CSPSource::New(
        "http", "", url::PORT_UNSPECIFIED, "", true, false);
    EXPECT_TRUE(Allow(source, GURL("http://a.com"), &context));
    EXPECT_TRUE(Allow(source, GURL("http://."), &context));
  }

  // Host is *.foo.bar
  {
    auto source = network::mojom::CSPSource::New(
        "", "foo.bar", url::PORT_UNSPECIFIED, "", true, false);
    EXPECT_FALSE(Allow(source, GURL("http://a.com"), &context));
    EXPECT_FALSE(Allow(source, GURL("http://bar"), &context));
    EXPECT_FALSE(Allow(source, GURL("http://foo.bar"), &context));
    EXPECT_FALSE(Allow(source, GURL("http://o.bar"), &context));
    EXPECT_TRUE(Allow(source, GURL("http://*.foo.bar"), &context));
    EXPECT_TRUE(Allow(source, GURL("http://sub.foo.bar"), &context));
    EXPECT_TRUE(Allow(source, GURL("http://sub.sub.foo.bar"), &context));
    // Please see http://crbug.com/692505
    EXPECT_TRUE(Allow(source, GURL("http://.foo.bar"), &context));
  }

  // Host is exact.
  {
    auto source = network::mojom::CSPSource::New(
        "", "foo.bar", url::PORT_UNSPECIFIED, "", false, false);
    EXPECT_TRUE(Allow(source, GURL("http://foo.bar"), &context));
    EXPECT_FALSE(Allow(source, GURL("http://sub.foo.bar"), &context));
    EXPECT_FALSE(Allow(source, GURL("http://bar"), &context));
    // Please see http://crbug.com/692505
    EXPECT_FALSE(Allow(source, GURL("http://.foo.bar"), &context));
  }
}

TEST(CSPSourceTest, AllowPort) {
  CSPContext context;
  context.SetSelf(url::Origin::Create(GURL("http://example.com")));

  // Source's port unspecified.
  {
    auto source = network::mojom::CSPSource::New(
        "", "a.com", url::PORT_UNSPECIFIED, "", false, false);
    EXPECT_TRUE(Allow(source, GURL("http://a.com:80"), &context));
    EXPECT_FALSE(Allow(source, GURL("http://a.com:8080"), &context));
    EXPECT_FALSE(Allow(source, GURL("http://a.com:443"), &context));
    EXPECT_FALSE(Allow(source, GURL("https://a.com:80"), &context));
    EXPECT_FALSE(Allow(source, GURL("https://a.com:8080"), &context));
    EXPECT_TRUE(Allow(source, GURL("https://a.com:443"), &context));
    EXPECT_FALSE(Allow(source, GURL("unknown://a.com:80"), &context));
    EXPECT_TRUE(Allow(source, GURL("http://a.com"), &context));
    EXPECT_TRUE(Allow(source, GURL("http://a.com"), &context));
    EXPECT_TRUE(Allow(source, GURL("https://a.com"), &context));
  }

  // Source's port is "*".
  {
    auto source = network::mojom::CSPSource::New(
        "", "a.com", url::PORT_UNSPECIFIED, "", false, true);
    EXPECT_TRUE(Allow(source, GURL("http://a.com"), &context));
    EXPECT_TRUE(Allow(source, GURL("http://a.com:80"), &context));
    EXPECT_TRUE(Allow(source, GURL("http://a.com:8080"), &context));
    EXPECT_TRUE(Allow(source, GURL("https://a.com:8080"), &context));
    EXPECT_TRUE(Allow(source, GURL("https://a.com:0"), &context));
    EXPECT_TRUE(Allow(source, GURL("https://a.com"), &context));
  }

  // Source has a port.
  {
    auto source =
        network::mojom::CSPSource::New("", "a.com", 80, "", false, false);
    EXPECT_TRUE(Allow(source, GURL("http://a.com:80"), &context));
    EXPECT_TRUE(Allow(source, GURL("http://a.com"), &context));
    EXPECT_FALSE(Allow(source, GURL("http://a.com:8080"), &context));
    EXPECT_TRUE(Allow(source, GURL("https://a.com"), &context));
  }

  // Allow upgrade from :80 to :443
  {
    auto source =
        network::mojom::CSPSource::New("", "a.com", 80, "", false, false);
    EXPECT_TRUE(Allow(source, GURL("https://a.com:443"), &context));
    // Should not allow scheme upgrades unless both port and scheme are
    // upgraded.
    EXPECT_FALSE(Allow(source, GURL("http://a.com:443"), &context));
  }

  // Host is * but port is specified
  {
    auto source =
        network::mojom::CSPSource::New("http", "", 111, "", true, false);
    EXPECT_TRUE(Allow(source, GURL("http://a.com:111"), &context));
    EXPECT_FALSE(Allow(source, GURL("http://a.com:222"), &context));
  }
}

TEST(CSPSourceTest, AllowPath) {
  CSPContext context;
  context.SetSelf(url::Origin::Create(GURL("http://example.com")));

  // Path to a file
  {
    auto source = network::mojom::CSPSource::New(
        "", "a.com", url::PORT_UNSPECIFIED, "/path/to/file", false, false);
    EXPECT_TRUE(Allow(source, GURL("http://a.com/path/to/file"), &context));
    EXPECT_FALSE(Allow(source, GURL("http://a.com/path/to/"), &context));
    EXPECT_FALSE(
        Allow(source, GURL("http://a.com/path/to/file/subpath"), &context));
    EXPECT_FALSE(
        Allow(source, GURL("http://a.com/path/to/something"), &context));
    EXPECT_FALSE(Allow(source, GURL("http://a.com/"), &context));
    EXPECT_FALSE(Allow(source, GURL("http://a.com"), &context));
  }

  // Path to a directory
  {
    auto source = network::mojom::CSPSource::New(
        "", "a.com", url::PORT_UNSPECIFIED, "/path/to/", false, false);
    EXPECT_TRUE(Allow(source, GURL("http://a.com/path/to/file"), &context));
    EXPECT_TRUE(Allow(source, GURL("http://a.com/path/to/"), &context));
    EXPECT_FALSE(Allow(source, GURL("http://a.com/path/"), &context));
    EXPECT_FALSE(Allow(source, GURL("http://a.com/path/to"), &context));
    EXPECT_FALSE(Allow(source, GURL("http://a.com/path/to"), &context));
    EXPECT_FALSE(Allow(source, GURL("http://a.com/"), &context));
    EXPECT_FALSE(Allow(source, GURL("http://a.com"), &context));
  }

  // Empty path
  {
    auto source = network::mojom::CSPSource::New(
        "", "a.com", url::PORT_UNSPECIFIED, "", false, false);
    EXPECT_TRUE(Allow(source, GURL("http://a.com/path/to/file"), &context));
    EXPECT_TRUE(Allow(source, GURL("http://a.com/path/to/"), &context));
    EXPECT_TRUE(Allow(source, GURL("http://a.com/"), &context));
    EXPECT_TRUE(Allow(source, GURL("http://a.com"), &context));
  }

  // Almost empty path
  {
    auto source = network::mojom::CSPSource::New(
        "", "a.com", url::PORT_UNSPECIFIED, "/", false, false);
    EXPECT_TRUE(Allow(source, GURL("http://a.com/path/to/file"), &context));
    EXPECT_TRUE(Allow(source, GURL("http://a.com/path/to/"), &context));
    EXPECT_TRUE(Allow(source, GURL("http://a.com/"), &context));
    EXPECT_TRUE(Allow(source, GURL("http://a.com"), &context));
  }

  // Path encoded.
  {
    auto source = network::mojom::CSPSource::New(
        "http", "a.com", url::PORT_UNSPECIFIED, "/Hello Günter", false, false);
    EXPECT_TRUE(
        Allow(source, GURL("http://a.com/Hello%20G%C3%BCnter"), &context));
    EXPECT_TRUE(Allow(source, GURL("http://a.com/Hello Günter"), &context));
  }

  // Host is * but path is specified.
  {
    auto source = network::mojom::CSPSource::New(
        "http", "", url::PORT_UNSPECIFIED, "/allowed-path", true, false);
    EXPECT_TRUE(Allow(source, GURL("http://a.com/allowed-path"), &context));
    EXPECT_FALSE(Allow(source, GURL("http://a.com/disallowed-path"), &context));
  }
}

TEST(CSPSourceTest, RedirectMatching) {
  CSPContext context;
  auto source = network::mojom::CSPSource::New("http", "a.com", 8000, "/bar/",
                                               false, false);
  EXPECT_TRUE(Allow(source, GURL("http://a.com:8000/"), &context, true));
  EXPECT_TRUE(Allow(source, GURL("http://a.com:8000/foo"), &context, true));
  EXPECT_FALSE(Allow(source, GURL("https://a.com:8000/foo"), &context, true));
  EXPECT_FALSE(
      Allow(source, GURL("http://not-a.com:8000/foo"), &context, true));
  EXPECT_FALSE(Allow(source, GURL("http://a.com:9000/foo/"), &context, false));
}

TEST(CSPSourceTest, Intersect) {
  struct TestCase {
    const char* a;
    const char* b;
    const char* intersection;
  } cases[]{
      // Scheme only.
      {"http:", "https:", "https:"},
      {"http:", "http:", "http:"},
      // b is stronger than a.
      {"http:", "http://example.org/page.html", "http://example.org/page.html"},
      {"http://example.org", "http://example.org/page.html",
       "http://example.org/page.html"},
      {"http://example.org", "http://example.org/page.html",
       "http://example.org/page.html"},
      {"http://example.org/page.html", "http://example.org/page.html",
       "http://example.org/page.html"},
      {"http:", "https://example.org/page.html",
       "https://example.org/page.html"},
      {"http://example.org:80", "http://example.org", "http://example.org:80"},
      {"http://example.org:80", "http://example.org:*",
       "http://example.org:80"},
      {"http://example.org:90", "http://example.org:*",
       "http://example.org:90"},
      // Nontrivial intersection.
      {"https:", "http://example.org/page.html",
       "https://example.org/page.html"},
      {"https://*.org/page.html", "https://example.org/",
       "https://example.org/page.html"},
      {"http://*.org/page.html", "https://example.org/",
       "https://example.org/page.html"},
      {"http://example.org:*/page.html", "https://example.org/",
       "https://example.org/page.html"},
      {"http://example.org:*/page.html", "https://example.org/",
       "https://example.org/page.html"},
      {"http://*.example.com:*", "http://*.com", "http://*.example.com"},
      // Empty intersection
      {"data:", "http:", nullptr},
      {"data:", "http://example.org", nullptr},
      {"data://example.org", "http://example.org", nullptr},
      {"http://example.com", "http://example.org", nullptr},
      {"http://example.org:90", "http://example.org", nullptr},
      {"http://example.org/page.html", "http://example.org/about.html",
       nullptr},
  };

  for (const auto& test : cases) {
    auto a = CSPSource(test.a);
    auto b = CSPSource(test.b);

    auto a_intersect_b = CSPSourcesIntersect(a, b);
    auto b_intersect_a = CSPSourcesIntersect(b, a);
    if (test.intersection) {
      EXPECT_EQ(test.intersection, ToString(a_intersect_b))
          << "The intersection of " << test.a << " and " << test.b
          << " should be " << test.intersection;
      // Intersection should be symmetric.
      EXPECT_EQ(test.intersection, ToString(b_intersect_a))
          << "The intersection of " << test.b << " and " << test.a
          << " should be " << test.intersection;
    } else {
      EXPECT_FALSE(a_intersect_b) << "The intersection of " << test.a << " and "
                                  << test.b << " should be empty.";
      EXPECT_FALSE(b_intersect_a) << "The intersection of " << test.b << " and "
                                  << test.a << " should be empty.";
    }
  }
}

TEST(CSPSourceTest, DoesNotSubsume) {
  struct TestCase {
    const char* a;
    const char* b;
  } cases[] = {
      // In the following test cases, neither |a| subsumes |b| nor |b| subsumes
      // |a|.
      // Different hosts.
      {"http://example.com", "http://another.com"},
      // Different schemes (wss -> http).
      {"wss://example.com", "http://example.com"},
      // Different schemes (wss -> about).
      {"wss://example.com/", "about://example.com/"},
      // Different schemes (wss -> about).
      {"http://example.com/", "about://example.com/"},
      // Different paths.
      {"http://example.com/1.html", "http://example.com/2.html"},
      // Different ports.
      {"http://example.com:443/", "http://example.com:800/"},
  };
  for (const auto& test : cases) {
    auto a = CSPSource(test.a);
    auto b = CSPSource(test.b);

    EXPECT_FALSE(CSPSourceSubsumes(a, b))
        << test.a << " should not subsume " << test.b;
    EXPECT_FALSE(CSPSourceSubsumes(b, a))
        << test.b << " should not subsume " << test.a;
  }
}

TEST(CSPSourceTest, Subsumes) {
  struct TestCase {
    const char* a;
    const char* b;
    bool expected_a_subsumes_b;
    bool expected_b_subsumes_a;
  } cases[] = {
      // Equal signals.
      {"http://a.org/", "http://a.org/", true, true},
      {"https://a.org/", "https://a.org/", true, true},
      {"https://a.org/page.html", "https://a.org/page.html", true, true},
      {"http://a.org:70", "http://a.org:70", true, true},
      {"https://a.org:70", "https://a.org:70", true, true},
      {"https://a.org/page.html", "https://a.org/page.html", true, true},
      {"http://a.org:70/page.html", "http://a.org:70/page.html", true, true},
      {"https://a.org:70/page.html", "https://a.org:70/page.html", true, true},
      {"http://a.org/", "http://a.org", true, true},
      {"http://a.org:80", "http://a.org:80", true, true},
      {"http://a.org:80", "https://a.org:443", true, false},
      {"http://a.org", "https://a.org:443", true, false},
      {"http://a.org:80", "https://a.org", true, false},
      // One stronger signal in the first CSPSource.
      {"http://a.org/", "https://a.org/", true, false},
      {"http://a.org/page.html", "http://a.org/", false, true},
      {"http://a.org:80/page.html", "http://a.org:80/", false, true},
      {"http://a.org:80", "http://a.org/", true, true},
      {"http://a.org:700", "http://a.org/", false, false},
      // Two stronger signals in the first CSPSource.
      {"https://a.org/page.html", "http://a.org/", false, true},
      {"https://a.org:80", "http://a.org/", false, false},
      {"http://a.org:80/page.html", "http://a.org/", false, true},
      // Three stronger signals in the first CSPSource.
      {"https://a.org:70/page.html", "http://a.org/", false, false},
      // Mixed signals.
      {"https://a.org/", "http://a.org/page.html", false, false},
      {"https://a.org", "http://a.org:70/", false, false},
      {"http://a.org/page.html", "http://a.org:70/", false, false},
  };

  for (const auto& test : cases) {
    auto a = CSPSource(test.a);
    auto b = CSPSource(test.b);

    EXPECT_EQ(CSPSourceSubsumes(a, b), test.expected_a_subsumes_b)
        << test.a << " subsumes " << test.b << " should return "
        << test.expected_a_subsumes_b;
    EXPECT_EQ(CSPSourceSubsumes(b, a), test.expected_b_subsumes_a)
        << test.b << " subsumes " << test.a << " should return "
        << test.expected_b_subsumes_a;

    a->is_host_wildcard = true;
    EXPECT_FALSE(CSPSourceSubsumes(b, a))
        << test.b << " should not subsume " << ToString(a);

    // If also |b| has a wildcard host, then the result should be the expected
    // one.
    b->is_host_wildcard = true;
    EXPECT_EQ(CSPSourceSubsumes(b, a), test.expected_b_subsumes_a)
        << ToString(b) << " subsumes " << ToString(a) << " should return "
        << test.expected_b_subsumes_a;
  }
}

TEST(CSPSourceTest, HostWildcardSubsumes) {
  const char* a = "http://*.example.org";
  const char* b = "http://www.example.org";
  const char* c = "http://example.org";
  const char* d = "https://*.example.org";

  auto source_a = CSPSource(a);
  auto source_b = CSPSource(b);
  auto source_c = CSPSource(c);
  auto source_d = CSPSource(d);

  // *.example.com subsumes www.example.com.
  EXPECT_TRUE(CSPSourceSubsumes(source_a, source_b))
      << a << " should subsume " << b;
  EXPECT_FALSE(CSPSourceSubsumes(source_b, source_a))
      << b << " should not subsume " << a;

  // *.example.com and example.com have no relations.
  EXPECT_FALSE(CSPSourceSubsumes(source_a, source_c))
      << a << " should not subsume " << c;
  EXPECT_FALSE(CSPSourceSubsumes(source_c, source_a))
      << c << " should not subsume " << a;

  // https://*.example.com and http://www.example.com have no relations.
  EXPECT_FALSE(CSPSourceSubsumes(source_d, source_b))
      << d << " should not subsume " << b;
  EXPECT_FALSE(CSPSourceSubsumes(source_b, source_d))
      << b << " should not subsume " << d;
}

TEST(CSPSourceTest, PortWildcardSubsumes) {
  const char* a = "http://example.org:*";
  const char* b = "http://example.org";
  const char* c = "https://example.org:*";

  auto source_a = CSPSource(a);
  auto source_b = CSPSource(b);
  auto source_c = CSPSource(c);

  EXPECT_TRUE(CSPSourceSubsumes(source_a, source_b))
      << a << " should subsume " << b;
  EXPECT_FALSE(CSPSourceSubsumes(source_b, source_a))
      << b << " should not subsume " << a;

  // https://example.com:* and http://example.com have no relations.
  EXPECT_FALSE(CSPSourceSubsumes(source_b, source_c))
      << b << " should not subsume " << c;
  EXPECT_FALSE(CSPSourceSubsumes(source_c, source_b))
      << c << " should not subsume " << b;
}

TEST(CSPSourceTest, SchemesOnlySubsumes) {
  struct TestCase {
    const char* a;
    const char* b;
    bool expected;
  } cases[] = {
      // HTTP.
      {"http:", "http:", true},
      {"http:", "https:", true},
      {"https:", "http:", false},
      {"https:", "https:", true},
      // WSS.
      {"ws:", "ws:", true},
      {"ws:", "wss:", true},
      {"wss:", "ws:", false},
      {"wss:", "wss:", true},
      // Unequal.
      {"ws:", "http:", false},
      {"http:", "ws:", false},
      {"http:", "about:", false},
      {"wss:", "https:", false},
      {"https:", "wss:", false},
  };

  for (const auto& test : cases) {
    auto source_a = CSPSource(test.a);
    auto source_b = CSPSource(test.b);
    EXPECT_EQ(CSPSourceSubsumes(source_a, source_b), test.expected)
        << test.a << " subsumes " << test.b << " should return "
        << test.expected;
  }
}

TEST(CSPSourceTest, ToString) {
  {
    auto source = network::mojom::CSPSource::New(
        "http", "", url::PORT_UNSPECIFIED, "", false, false);
    EXPECT_EQ("http:", ToString(source));
  }
  {
    auto source = network::mojom::CSPSource::New(
        "http", "a.com", url::PORT_UNSPECIFIED, "", false, false);
    EXPECT_EQ("http://a.com", ToString(source));
  }
  {
    auto source = network::mojom::CSPSource::New(
        "", "a.com", url::PORT_UNSPECIFIED, "", false, false);
    EXPECT_EQ("a.com", ToString(source));
  }
  {
    auto source = network::mojom::CSPSource::New(
        "", "a.com", url::PORT_UNSPECIFIED, "", true, false);
    EXPECT_EQ("*.a.com", ToString(source));
  }
  {
    auto source = network::mojom::CSPSource::New("", "", url::PORT_UNSPECIFIED,
                                                 "", true, false);
    EXPECT_EQ("*", ToString(source));
  }
  {
    auto source =
        network::mojom::CSPSource::New("", "a.com", 80, "", false, false);
    EXPECT_EQ("a.com:80", ToString(source));
  }
  {
    auto source = network::mojom::CSPSource::New(
        "", "a.com", url::PORT_UNSPECIFIED, "", false, true);
    EXPECT_EQ("a.com:*", ToString(source));
  }
  {
    auto source = network::mojom::CSPSource::New(
        "", "a.com", url::PORT_UNSPECIFIED, "/path", false, false);
    EXPECT_EQ("a.com/path", ToString(source));
  }
}

TEST(CSPSourceTest, UpgradeRequests) {
  CSPContext context;
  auto source =
      network::mojom::CSPSource::New("http", "a.com", 80, "", false, false);
  EXPECT_TRUE(Allow(source, GURL("http://a.com:80"), &context, true));
  EXPECT_FALSE(Allow(source, GURL("https://a.com:80"), &context, true));
  EXPECT_FALSE(Allow(source, GURL("http://a.com:443"), &context, true));
  EXPECT_TRUE(Allow(source, GURL("https://a.com:443"), &context, true));
  EXPECT_TRUE(Allow(source, GURL("https://a.com"), &context, true));
}

}  // namespace content
