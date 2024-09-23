// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/content_security_policy/csp_source.h"

#include "base/test/scoped_feature_list.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"
#include "url/url_features.h"

namespace network {

namespace {

// A CSPSource used in test not interested checking the interactions with
// 'self'. It doesn't match any URL.
static const network::mojom::CSPSource no_self;

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

enum class NonSpecialUrlBehavior { Compliant, NonCompliant };

}  // namespace

class CSPSourceTest : public testing::TestWithParam<
                          std::tuple<CSPSourceContext, NonSpecialUrlBehavior>> {
 public:
  CSPSourceTest() {
    scoped_feature_list_.InitWithFeatureState(
        url::kStandardCompliantNonSpecialSchemeURLParsing,
        std::get<1>(GetParam()) == NonSpecialUrlBehavior::Compliant);
  }

  bool Allow(const network::mojom::CSPSource& source,
             const GURL& url,
             const network::mojom::CSPSource& self_source = no_self,
             bool is_redirect = false,
             bool is_opaque_fenced_frame = false) {
    return CheckCSPSource(source, url, self_source, std::get<0>(GetParam()),
                          is_redirect, is_opaque_fenced_frame);
  }

  bool IsPermissionsPolicyContext() {
    return std::get<0>(GetParam()) == CSPSourceContext::PermissionsPolicy;
  }

  bool UseStandardCompliantNonSpecialSchemeUrlParsing() {
    return std::get<1>(GetParam()) == NonSpecialUrlBehavior::Compliant;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    CSPSourceTest,
    ::testing::Combine(testing::Values(CSPSourceContext::ContentSecurityPolicy,
                                       CSPSourceContext::PermissionsPolicy),
                       testing::Values(NonSpecialUrlBehavior::Compliant,
                                       NonSpecialUrlBehavior::NonCompliant)));

TEST_P(CSPSourceTest, BasicMatching) {
  auto source = network::mojom::CSPSource::New("http", "example.com", 8000,
                                               "/foo/", false, false);

  EXPECT_TRUE(Allow(*source, GURL("http://example.com:8000/foo/")));
  EXPECT_TRUE(Allow(*source, GURL("http://example.com:8000/foo/bar")));
  EXPECT_TRUE(Allow(*source, GURL("HTTP://EXAMPLE.com:8000/foo/BAR")));
  EXPECT_FALSE(Allow(*source, GURL("http://example.com:8000/bar/")));
  EXPECT_FALSE(Allow(*source, GURL("https://example.com:8000/bar/")));
  EXPECT_FALSE(Allow(*source, GURL("http://example.com:9000/bar/")));
  EXPECT_FALSE(Allow(*source, GURL("HTTP://example.com:8000/FOO/bar")));
  EXPECT_FALSE(Allow(*source, GURL("HTTP://example.com:8000/FOO/BAR")));
}

TEST_P(CSPSourceTest, AllowScheme) {
  // http -> {http, https}.
  {
    auto source = network::mojom::CSPSource::New(
        "http", "", url::PORT_UNSPECIFIED, "", false, false);
    EXPECT_TRUE(Allow(*source, GURL("http://a.com")));
    EXPECT_TRUE(Allow(*source, GURL("https://a.com")));
    // This passes because the source is "scheme only" so the upgrade is
    // allowed.
    EXPECT_TRUE(Allow(*source, GURL("https://a.com:80")));
    EXPECT_FALSE(Allow(*source, GURL("ftp://a.com")));
    EXPECT_FALSE(Allow(*source, GURL("ws://a.com")));
    EXPECT_FALSE(Allow(*source, GURL("wss://a.com")));
  }

  // ws -> {ws, wss}.
  {
    auto source = network::mojom::CSPSource::New(
        "ws", "", url::PORT_UNSPECIFIED, "", false, false);
    EXPECT_FALSE(Allow(*source, GURL("http://a.com")));
    EXPECT_FALSE(Allow(*source, GURL("https://a.com")));
    EXPECT_FALSE(Allow(*source, GURL("ftp://a.com")));
    EXPECT_TRUE(Allow(*source, GURL("ws://a.com")));
    EXPECT_TRUE(Allow(*source, GURL("wss://a.com")));
  }

  // Exact matches required (ftp)
  {
    auto source = network::mojom::CSPSource::New(
        "ftp", "", url::PORT_UNSPECIFIED, "", false, false);
    EXPECT_TRUE(Allow(*source, GURL("ftp://a.com")));
    EXPECT_FALSE(Allow(*source, GURL("http://a.com")));
  }

  // Exact matches required (https)
  {
    auto source = network::mojom::CSPSource::New(
        "https", "", url::PORT_UNSPECIFIED, "", false, false);
    EXPECT_TRUE(Allow(*source, GURL("https://a.com")));
    EXPECT_FALSE(Allow(*source, GURL("http://a.com")));
  }

  // Exact matches required (wss)
  {
    auto source = network::mojom::CSPSource::New(
        "wss", "", url::PORT_UNSPECIFIED, "", false, false);
    EXPECT_TRUE(Allow(*source, GURL("wss://a.com")));
    EXPECT_FALSE(Allow(*source, GURL("ws://a.com")));
  }

  // Scheme is empty (ProtocolMatchesSelf).
  {
    auto source = network::mojom::CSPSource::New(
        "", "a.com", url::PORT_UNSPECIFIED, "", false, false);
    EXPECT_FALSE(Allow(*source, GURL("http://a.com")));

    {
      // Self's scheme is http.
      auto self_source =
          network::mojom::CSPSource::New("http", "a.com", 80, "", false, false);
      EXPECT_TRUE(Allow(*source, GURL("http://a.com"), *self_source));
      // Upgrades are disallowed for permission policies.
      EXPECT_EQ(Allow(*source, GURL("https://a.com"), *self_source),
                !IsPermissionsPolicyContext());
      EXPECT_FALSE(Allow(*source, GURL("ftp://a.com"), *self_source));
    }

    {
      // Self's is https.
      auto self_source = network::mojom::CSPSource::New("https", "a.com", 443,
                                                        "", false, false);
      EXPECT_FALSE(Allow(*source, GURL("http://a.com"), *self_source));
      EXPECT_TRUE(Allow(*source, GURL("https://a.com"), *self_source));
      EXPECT_FALSE(Allow(*source, GURL("ftp://a.com"), *self_source));
    }

    {
      // Self's scheme is not in the http family.
      auto self_source =
          network::mojom::CSPSource::New("ftp", "a.com", 21, "", false, false);
      EXPECT_FALSE(Allow(*source, GURL("http://a.com"), *self_source));
      EXPECT_TRUE(Allow(*source, GURL("ftp://a.com"), *self_source));
    }

    {
      // Self's scheme is unique (non standard scheme).
      auto self_source = network::mojom::CSPSource::New(
          "non-standard-scheme", "a.com", url::PORT_UNSPECIFIED, "", false,
          false);
      EXPECT_FALSE(Allow(*source, GURL("http://a.com"), *self_source));
      EXPECT_FALSE(
          Allow(*source, GURL("non-standard-scheme://a.com"), *self_source));
    }

    // Self's scheme is unique (e.g. data-url).
    EXPECT_FALSE(Allow(*source, GURL("http://a.com")));
    EXPECT_FALSE(Allow(*source, GURL("data:text/html,hello")));
  }
}

TEST_P(CSPSourceTest, AllowHost) {
  auto self_source = network::mojom::CSPSource::New("http", "example.com", 80,
                                                    "", false, false);
  // Host is * (source-expression = "http://*")
  {
    auto source = network::mojom::CSPSource::New(
        "http", "", url::PORT_UNSPECIFIED, "", true, false);
    EXPECT_TRUE(Allow(*source, GURL("http://a.com"), *self_source));
    EXPECT_TRUE(Allow(*source, GURL("http://."), *self_source));
  }

  // Host is *.foo.bar
  {
    auto source = network::mojom::CSPSource::New(
        "", "foo.bar", url::PORT_UNSPECIFIED, "", true, false);
    EXPECT_FALSE(Allow(*source, GURL("http://a.com"), *self_source));
    EXPECT_FALSE(Allow(*source, GURL("http://bar"), *self_source));
    EXPECT_FALSE(Allow(*source, GURL("http://foo.bar"), *self_source));
    EXPECT_FALSE(Allow(*source, GURL("http://o.bar"), *self_source));
    EXPECT_TRUE(Allow(*source, GURL("http://*.foo.bar"), *self_source));
    EXPECT_TRUE(Allow(*source, GURL("http://sub.foo.bar"), *self_source));
    EXPECT_TRUE(Allow(*source, GURL("http://sub.sub.foo.bar"), *self_source));
    // Please see http://crbug.com/692505
    EXPECT_TRUE(Allow(*source, GURL("http://.foo.bar"), *self_source));
  }

  // Host is exact.
  {
    auto source = network::mojom::CSPSource::New(
        "", "foo.bar", url::PORT_UNSPECIFIED, "", false, false);
    EXPECT_TRUE(Allow(*source, GURL("http://foo.bar"), *self_source));
    EXPECT_FALSE(Allow(*source, GURL("http://sub.foo.bar"), *self_source));
    EXPECT_FALSE(Allow(*source, GURL("http://bar"), *self_source));
    // Please see http://crbug.com/692505
    EXPECT_FALSE(Allow(*source, GURL("http://.foo.bar"), *self_source));
  }
}

TEST_P(CSPSourceTest, AllowPort) {
  auto self_source = network::mojom::CSPSource::New("http", "example.com", 80,
                                                    "", false, false);

  // Source's port unspecified.
  {
    auto source = network::mojom::CSPSource::New(
        "", "a.com", url::PORT_UNSPECIFIED, "", false, false);
    EXPECT_TRUE(Allow(*source, GURL("http://a.com:80"), *self_source));
    EXPECT_FALSE(Allow(*source, GURL("http://a.com:8080"), *self_source));
    EXPECT_FALSE(Allow(*source, GURL("http://a.com:443"), *self_source));
    EXPECT_FALSE(Allow(*source, GURL("https://a.com:80"), *self_source));
    EXPECT_FALSE(Allow(*source, GURL("https://a.com:8080"), *self_source));
    // Upgrades are disallowed for permission policies.
    EXPECT_EQ(Allow(*source, GURL("https://a.com:443"), *self_source),
              !IsPermissionsPolicyContext());
    EXPECT_FALSE(Allow(*source, GURL("unknown://a.com:80"), *self_source));
    EXPECT_TRUE(Allow(*source, GURL("http://a.com"), *self_source));
    EXPECT_TRUE(Allow(*source, GURL("http://a.com"), *self_source));
    // Upgrades are disallowed for permission policies.
    EXPECT_EQ(Allow(*source, GURL("https://a.com"), *self_source),
              !IsPermissionsPolicyContext());
  }

  // Source's port is "*".
  {
    auto source = network::mojom::CSPSource::New(
        "", "a.com", url::PORT_UNSPECIFIED, "", false, true);
    EXPECT_TRUE(Allow(*source, GURL("http://a.com"), *self_source));
    EXPECT_TRUE(Allow(*source, GURL("http://a.com:80"), *self_source));
    EXPECT_TRUE(Allow(*source, GURL("http://a.com:8080"), *self_source));
    // Upgrades are disallowed for permission policies.
    EXPECT_EQ(Allow(*source, GURL("https://a.com:8080"), *self_source),
              !IsPermissionsPolicyContext());
    EXPECT_EQ(Allow(*source, GURL("https://a.com:0"), *self_source),
              !IsPermissionsPolicyContext());
    EXPECT_EQ(Allow(*source, GURL("https://a.com"), *self_source),
              !IsPermissionsPolicyContext());
  }

  // Source has a port.
  {
    auto source =
        network::mojom::CSPSource::New("", "a.com", 80, "", false, false);
    EXPECT_TRUE(Allow(*source, GURL("http://a.com:80"), *self_source));
    EXPECT_TRUE(Allow(*source, GURL("http://a.com"), *self_source));
    EXPECT_FALSE(Allow(*source, GURL("http://a.com:8080"), *self_source));
    // Upgrades are disallowed for permission policies.
    EXPECT_EQ(Allow(*source, GURL("https://a.com"), *self_source),
              !IsPermissionsPolicyContext());
  }

  // Allow upgrade from :80 to :443
  {
    auto source =
        network::mojom::CSPSource::New("", "a.com", 80, "", false, false);
    // Upgrades are disallowed for permission policies.
    EXPECT_EQ(Allow(*source, GURL("https://a.com:443"), *self_source),
              !IsPermissionsPolicyContext());
    // Should not allow scheme upgrades unless both port and scheme are
    // upgraded.
    EXPECT_FALSE(Allow(*source, GURL("http://a.com:443"), *self_source));
  }

  // Host is * but port is specified
  {
    auto source =
        network::mojom::CSPSource::New("http", "", 111, "", true, false);
    EXPECT_TRUE(Allow(*source, GURL("http://a.com:111"), *self_source));
    EXPECT_FALSE(Allow(*source, GURL("http://a.com:222"), *self_source));
  }
}

TEST_P(CSPSourceTest, AllowPath) {
  auto self_source = network::mojom::CSPSource::New("http", "example.com", 80,
                                                    "", false, false);

  // Path to a file
  {
    auto source = network::mojom::CSPSource::New(
        "", "a.com", url::PORT_UNSPECIFIED, "/path/to/file", false, false);
    EXPECT_TRUE(
        Allow(*source, GURL("http://a.com/path/to/file"), *self_source));
    EXPECT_FALSE(Allow(*source, GURL("http://a.com/path/to/"), *self_source));
    EXPECT_FALSE(Allow(*source, GURL("http://a.com/path/to/file/subpath"),
                       *self_source));
    EXPECT_FALSE(
        Allow(*source, GURL("http://a.com/path/to/something"), *self_source));
    EXPECT_FALSE(Allow(*source, GURL("http://a.com/"), *self_source));
    EXPECT_FALSE(Allow(*source, GURL("http://a.com"), *self_source));
  }

  // Path to a directory
  {
    auto source = network::mojom::CSPSource::New(
        "", "a.com", url::PORT_UNSPECIFIED, "/path/to/", false, false);
    EXPECT_TRUE(
        Allow(*source, GURL("http://a.com/path/to/file"), *self_source));
    EXPECT_TRUE(Allow(*source, GURL("http://a.com/path/to/"), *self_source));
    EXPECT_FALSE(Allow(*source, GURL("http://a.com/path/"), *self_source));
    EXPECT_FALSE(Allow(*source, GURL("http://a.com/path/to"), *self_source));
    EXPECT_FALSE(Allow(*source, GURL("http://a.com/path/to"), *self_source));
    EXPECT_FALSE(Allow(*source, GURL("http://a.com/"), *self_source));
    EXPECT_FALSE(Allow(*source, GURL("http://a.com"), *self_source));
  }

  // Empty path
  {
    auto source = network::mojom::CSPSource::New(
        "", "a.com", url::PORT_UNSPECIFIED, "", false, false);
    EXPECT_TRUE(
        Allow(*source, GURL("http://a.com/path/to/file"), *self_source));
    EXPECT_TRUE(Allow(*source, GURL("http://a.com/path/to/"), *self_source));
    EXPECT_TRUE(Allow(*source, GURL("http://a.com/"), *self_source));
    EXPECT_TRUE(Allow(*source, GURL("http://a.com"), *self_source));
  }

  // Almost empty path
  {
    auto source = network::mojom::CSPSource::New(
        "", "a.com", url::PORT_UNSPECIFIED, "/", false, false);
    EXPECT_TRUE(
        Allow(*source, GURL("http://a.com/path/to/file"), *self_source));
    EXPECT_TRUE(Allow(*source, GURL("http://a.com/path/to/"), *self_source));
    EXPECT_TRUE(Allow(*source, GURL("http://a.com/"), *self_source));
    EXPECT_TRUE(Allow(*source, GURL("http://a.com"), *self_source));
  }

  // Path encoded.
  {
    auto source = network::mojom::CSPSource::New(
        "http", "a.com", url::PORT_UNSPECIFIED, "/Hello Günter", false, false);
    EXPECT_TRUE(
        Allow(*source, GURL("http://a.com/Hello%20G%C3%BCnter"), *self_source));
    EXPECT_TRUE(
        Allow(*source, GURL("http://a.com/Hello Günter"), *self_source));
  }

  // Host is * but path is specified.
  {
    auto source = network::mojom::CSPSource::New(
        "http", "", url::PORT_UNSPECIFIED, "/allowed-path", true, false);
    EXPECT_TRUE(
        Allow(*source, GURL("http://a.com/allowed-path"), *self_source));
    EXPECT_FALSE(
        Allow(*source, GURL("http://a.com/disallowed-path"), *self_source));
  }
}

TEST_P(CSPSourceTest, RedirectMatching) {
  auto source = network::mojom::CSPSource::New("http", "a.com", 8000, "/bar/",
                                               false, false);

  EXPECT_TRUE(Allow(*source, GURL("http://a.com:8000/"), no_self, true));
  EXPECT_TRUE(Allow(*source, GURL("http://a.com:8000/foo"), no_self, true));
  EXPECT_FALSE(Allow(*source, GURL("https://a.com:8000/foo"), no_self, true));
  EXPECT_FALSE(
      Allow(*source, GURL("http://not-a.com:8000/foo"), no_self, true));
  EXPECT_FALSE(Allow(*source, GURL("http://a.com:9000/foo/"), no_self, false));
}

TEST_P(CSPSourceTest, Intersect) {
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

    auto a_intersect_b = CSPSourcesIntersect(*a, *b);
    auto b_intersect_a = CSPSourcesIntersect(*b, *a);
    if (test.intersection) {
      EXPECT_EQ(test.intersection, ToString(*a_intersect_b))
          << "The intersection of " << test.a << " and " << test.b
          << " should be " << test.intersection;
      // Intersection should be symmetric.
      EXPECT_EQ(test.intersection, ToString(*b_intersect_a))
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

TEST_P(CSPSourceTest, DoesNotSubsume) {
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

    EXPECT_FALSE(CSPSourceSubsumes(*a, *b))
        << test.a << " should not subsume " << test.b;
    EXPECT_FALSE(CSPSourceSubsumes(*b, *a))
        << test.b << " should not subsume " << test.a;
  }
}

TEST_P(CSPSourceTest, Subsumes) {
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

    EXPECT_EQ(CSPSourceSubsumes(*a, *b), test.expected_a_subsumes_b)
        << test.a << " subsumes " << test.b << " should return "
        << test.expected_a_subsumes_b;
    EXPECT_EQ(CSPSourceSubsumes(*b, *a), test.expected_b_subsumes_a)
        << test.b << " subsumes " << test.a << " should return "
        << test.expected_b_subsumes_a;

    a->is_host_wildcard = true;
    EXPECT_FALSE(CSPSourceSubsumes(*b, *a))
        << test.b << " should not subsume " << ToString(*a);

    // If also |b| has a wildcard host, then the result should be the expected
    // one.
    b->is_host_wildcard = true;
    EXPECT_EQ(CSPSourceSubsumes(*b, *a), test.expected_b_subsumes_a)
        << ToString(*b) << " subsumes " << ToString(*a) << " should return "
        << test.expected_b_subsumes_a;
  }
}

TEST_P(CSPSourceTest, HostWildcardSubsumes) {
  const char* a = "http://*.example.org";
  const char* b = "http://www.example.org";
  const char* c = "http://example.org";
  const char* d = "https://*.example.org";

  auto source_a = CSPSource(a);
  auto source_b = CSPSource(b);
  auto source_c = CSPSource(c);
  auto source_d = CSPSource(d);

  // *.example.com subsumes www.example.com.
  EXPECT_TRUE(CSPSourceSubsumes(*source_a, *source_b))
      << a << " should subsume " << b;
  EXPECT_FALSE(CSPSourceSubsumes(*source_b, *source_a))
      << b << " should not subsume " << a;

  // *.example.com and example.com have no relations.
  EXPECT_FALSE(CSPSourceSubsumes(*source_a, *source_c))
      << a << " should not subsume " << c;
  EXPECT_FALSE(CSPSourceSubsumes(*source_c, *source_a))
      << c << " should not subsume " << a;

  // https://*.example.com and http://www.example.com have no relations.
  EXPECT_FALSE(CSPSourceSubsumes(*source_d, *source_b))
      << d << " should not subsume " << b;
  EXPECT_FALSE(CSPSourceSubsumes(*source_b, *source_d))
      << b << " should not subsume " << d;
}

TEST_P(CSPSourceTest, PortWildcardSubsumes) {
  const char* a = "http://example.org:*";
  const char* b = "http://example.org";
  const char* c = "https://example.org:*";

  auto source_a = CSPSource(a);
  auto source_b = CSPSource(b);
  auto source_c = CSPSource(c);

  EXPECT_TRUE(CSPSourceSubsumes(*source_a, *source_b))
      << a << " should subsume " << b;
  EXPECT_FALSE(CSPSourceSubsumes(*source_b, *source_a))
      << b << " should not subsume " << a;

  // https://example.com:* and http://example.com have no relations.
  EXPECT_FALSE(CSPSourceSubsumes(*source_b, *source_c))
      << b << " should not subsume " << c;
  EXPECT_FALSE(CSPSourceSubsumes(*source_c, *source_b))
      << c << " should not subsume " << b;
}

TEST_P(CSPSourceTest, SchemesOnlySubsumes) {
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
    EXPECT_EQ(CSPSourceSubsumes(*source_a, *source_b), test.expected)
        << test.a << " subsumes " << test.b << " should return "
        << test.expected;
  }
}

TEST_P(CSPSourceTest, ToString) {
  {
    auto source = network::mojom::CSPSource::New(
        "http", "", url::PORT_UNSPECIFIED, "", false, false);
    EXPECT_EQ("http:", ToString(*source));
  }
  {
    auto source = network::mojom::CSPSource::New(
        "http", "a.com", url::PORT_UNSPECIFIED, "", false, false);
    EXPECT_EQ("http://a.com", ToString(*source));
  }
  {
    auto source = network::mojom::CSPSource::New(
        "", "a.com", url::PORT_UNSPECIFIED, "", false, false);
    EXPECT_EQ("a.com", ToString(*source));
  }
  {
    auto source = network::mojom::CSPSource::New(
        "", "a.com", url::PORT_UNSPECIFIED, "", true, false);
    EXPECT_EQ("*.a.com", ToString(*source));
  }
  {
    auto source = network::mojom::CSPSource::New("", "", url::PORT_UNSPECIFIED,
                                                 "", true, false);
    EXPECT_EQ("*", ToString(*source));
  }
  {
    auto source =
        network::mojom::CSPSource::New("", "a.com", 80, "", false, false);
    EXPECT_EQ("a.com:80", ToString(*source));
  }
  {
    auto source = network::mojom::CSPSource::New(
        "", "a.com", url::PORT_UNSPECIFIED, "", false, true);
    EXPECT_EQ("a.com:*", ToString(*source));
  }
  {
    auto source = network::mojom::CSPSource::New(
        "", "a.com", url::PORT_UNSPECIFIED, "/path", false, false);
    EXPECT_EQ("a.com/path", ToString(*source));
  }
}

TEST_P(CSPSourceTest, UpgradeRequests) {
  auto source =
      network::mojom::CSPSource::New("http", "a.com", 80, "", false, false);

  EXPECT_TRUE(Allow(*source, GURL("http://a.com:80"), no_self, true));
  EXPECT_FALSE(Allow(*source, GURL("https://a.com:80"), no_self, true));
  EXPECT_FALSE(Allow(*source, GURL("http://a.com:443"), no_self, true));
  // Upgrades are disallowed for permission policies.
  EXPECT_EQ(Allow(*source, GURL("https://a.com:443"), no_self, true),
            !IsPermissionsPolicyContext());
  EXPECT_EQ(Allow(*source, GURL("https://a.com"), no_self, true),
            !IsPermissionsPolicyContext());
}

TEST_P(CSPSourceTest, CustomSchemeWithHost) {
  std::string uri = "custom-scheme://a/b";
  auto csp_source = CSPSource(uri);
  auto url = GURL(uri);

  EXPECT_EQ("a", csp_source->host);
  EXPECT_FALSE(Allow(*csp_source, url));

  // It is still possible for developers to use a scheme-only CSPSource.
  // See test: CSPSourceTest.CustomScheme
}

TEST_P(CSPSourceTest, CustomScheme) {
  auto csp_source = CSPSource("custom-scheme:");  // Scheme only CSP.
  EXPECT_TRUE(Allow(*csp_source, GURL("custom-scheme://a/b")));
  EXPECT_FALSE(Allow(*csp_source, GURL("other-scheme://a/b")));
}

TEST_P(CSPSourceTest, OpaqueURLMatchingAllowSchemeHttps) {
  auto source = CSPSource("https:");
  EXPECT_TRUE(Allow(*source, GURL("https://a.com"), no_self,
                    /*is_redirect=*/false, /*is_opaque_fenced_frame=*/true));
}

TEST_P(CSPSourceTest, OpaqueURLMatchingAllowSchemeNonHttps) {
  auto source = CSPSource("http:");
  EXPECT_FALSE(Allow(*source, GURL("https://a.com"), no_self,
                     /*is_redirect=*/false, /*is_opaque_fenced_frame=*/true));
}

TEST_P(CSPSourceTest, OpaqueURLMatchingAllowHostAndPort) {
  {
    auto source = CSPSource("https://*:*");
    EXPECT_TRUE(Allow(*source, GURL("https://a.com"), no_self,
                      /*is_redirect=*/false, /*is_opaque_fenced_frame=*/true));
  }

  {
    auto source = CSPSource("https://*");
    EXPECT_FALSE(Allow(*source, GURL("https://a.com"), no_self,
                       /*is_redirect=*/false, /*is_opaque_fenced_frame=*/true));
  }

  {
    auto source = CSPSource("https://*:443");
    EXPECT_FALSE(Allow(*source, GURL("https://a.com"), no_self,
                       /*is_redirect=*/false, /*is_opaque_fenced_frame=*/true));
  }

  {
    auto source = CSPSource("https://a.com:*");
    EXPECT_FALSE(Allow(*source, GURL("https://a.com"), no_self,
                       /*is_redirect=*/false, /*is_opaque_fenced_frame=*/true));
  }

  {
    auto source = CSPSource("https://a.com");
    EXPECT_FALSE(Allow(*source, GURL("https://a.com"), no_self,
                       /*is_redirect=*/false, /*is_opaque_fenced_frame=*/true));
  }
}

}  // namespace network
