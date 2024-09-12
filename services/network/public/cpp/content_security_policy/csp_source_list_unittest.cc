// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/content_security_policy/csp_source_list.h"

#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace network {

namespace {

// A CSPSource used in test not interested checking the interactions with
// 'self'. It doesn't match any URL.
static const network::mojom::CSPSource no_self;

// Allow() is an abbreviation of CheckCSPSourceList. Useful for writing
// test expectations on one line.
CSPCheckResult Allow(
    const mojom::CSPSourceListPtr& source_list,
    const GURL& url,
    const mojom::CSPSource& self,
    bool is_redirect = false,
    mojom::CSPDirectiveName directive_name = mojom::CSPDirectiveName::FrameSrc,
    bool is_opaque_fenced_frame = false) {
  return CheckCSPSourceList(directive_name, *source_list, url, self,
                            is_redirect, is_opaque_fenced_frame);
}

std::vector<mojom::ContentSecurityPolicyPtr> Parse(
    const std::vector<std::string>& policies) {
  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  for (const auto& policy : policies) {
    headers->AddHeader("Content-Security-Policy", policy);
  }
  std::vector<mojom::ContentSecurityPolicyPtr> parsed_policies;
  AddContentSecurityPolicyFromHeaders(*headers, GURL("https://example.com/"),
                                      &parsed_policies);
  return parsed_policies;
}

mojom::CSPSourceListPtr ParseToSourceList(mojom::CSPDirectiveName directive,
                                          const std::string& value) {
  return std::move(
      Parse({ToString(directive) + " " + value})[0]->directives[directive]);
}

std::vector<mojom::CSPSourceListPtr> ParseToVectorOfSourceLists(
    mojom::CSPDirectiveName directive,
    const std::vector<std::string>& values) {
  std::vector<std::string> csp_values(values.size());
  base::ranges::transform(values, csp_values.begin(),
                          [directive](const std::string& s) {
                            return ToString(directive) + " " + s;
                          });
  std::vector<mojom::ContentSecurityPolicyPtr> policies = Parse(csp_values);
  std::vector<mojom::CSPSourceListPtr> sources(policies.size());
  base::ranges::transform(
      policies, sources.begin(),
      [directive](mojom::ContentSecurityPolicyPtr& p) {
        return mojom::CSPSourceListPtr(std::move(p->directives[directive]));
      });
  return sources;
}

std::vector<const mojom::CSPSourceList*> ToRawPointers(
    const std::vector<mojom::CSPSourceListPtr>& list) {
  std::vector<const mojom::CSPSourceList*> out(list.size());
  base::ranges::transform(list, out.begin(), &mojom::CSPSourceListPtr::get);
  return out;
}

}  // namespace

TEST(CSPSourceList, MultipleSource) {
  auto self = network::mojom::CSPSource::New("http", "example.com", 80, "",
                                             false, false);
  std::vector<mojom::CSPSourcePtr> sources;
  sources.push_back(mojom::CSPSource::New("", "a.com", url::PORT_UNSPECIFIED,
                                          "", false, false));
  sources.push_back(mojom::CSPSource::New("", "b.com", url::PORT_UNSPECIFIED,
                                          "", false, false));
  auto source_list = mojom::CSPSourceList::New();
  source_list->sources = std::move(sources);
  EXPECT_TRUE(Allow(source_list, GURL("http://a.com"), *self));
  EXPECT_TRUE(Allow(source_list, GURL("http://b.com"), *self));
  EXPECT_FALSE(Allow(source_list, GURL("http://c.com"), *self));
}

TEST(CSPSourceList, AllowStar) {
  auto self = network::mojom::CSPSource::New("http", "example.com", 80, "",
                                             false, false);
  auto source_list = mojom::CSPSourceList::New();
  source_list->allow_star = true;
  EXPECT_EQ(Allow(source_list, GURL("http://not-example.com"), *self),
            network::CSPCheckResult::Allowed());
  EXPECT_EQ(Allow(source_list, GURL("https://not-example.com"), *self),
            network::CSPCheckResult::Allowed());
  EXPECT_EQ(Allow(source_list, GURL("ws://not-example.com"), *self),
            network::CSPCheckResult::AllowedOnlyIfWildcardMatchesWs());
  EXPECT_EQ(Allow(source_list, GURL("wss://not-example.com"), *self),
            network::CSPCheckResult::AllowedOnlyIfWildcardMatchesWs());
  EXPECT_EQ(Allow(source_list, GURL("ftp://not-example.com"), *self),
            base::FeatureList::IsEnabled(
                network::features::kCspStopMatchingWildcardDirectivesToFtp)
                ? network::CSPCheckResult::Blocked()
                : network::CSPCheckResult::AllowedOnlyIfWildcardMatchesFtp());
  EXPECT_EQ(Allow(source_list, GURL("file://not-example.com"), *self),
            network::CSPCheckResult::Blocked());
  EXPECT_EQ(Allow(source_list, GURL("applewebdata://a.test"), *self),
            network::CSPCheckResult::Blocked());

  {
    // With a protocol of 'file', '*' allow 'file:'
    auto file = network::mojom::CSPSource::New(
        "file", "example.com", url::PORT_UNSPECIFIED, "", false, false);
    EXPECT_TRUE(Allow(source_list, GURL("file://not-example.com"), *file));
    EXPECT_FALSE(Allow(source_list, GURL("applewebdata://a.test"), *file));
  }
}

TEST(CSPSourceList, AllowSelf) {
  auto self = network::mojom::CSPSource::New("http", "example.com", 80, "",
                                             false, false);
  auto source_list = mojom::CSPSourceList::New();
  source_list->allow_self = true;
  EXPECT_TRUE(Allow(source_list, GURL("http://example.com"), *self));
  EXPECT_FALSE(Allow(source_list, GURL("http://not-example.com"), *self));
  EXPECT_TRUE(Allow(source_list, GURL("https://example.com"), *self));
  EXPECT_FALSE(Allow(source_list, GURL("ws://example.com"), *self));
}

TEST(CSPSourceList, AllowStarAndSelf) {
  auto self =
      network::mojom::CSPSource::New("https", "a.com", 443, "", false, false);
  auto source_list = mojom::CSPSourceList::New();

  // If the request is by {*} and not by {'self'} then it should be
  // by the union {*,'self'}.
  source_list->allow_self = true;
  source_list->allow_star = false;
  EXPECT_FALSE(Allow(source_list, GURL("http://b.com"), *self));
  source_list->allow_self = false;
  source_list->allow_star = true;
  EXPECT_TRUE(Allow(source_list, GURL("http://b.com"), *self));
  source_list->allow_self = true;
  source_list->allow_star = true;
  EXPECT_TRUE(Allow(source_list, GURL("http://b.com"), *self));
}

TEST(CSPSourceList, AllowSelfWithUnspecifiedPort) {
  auto self = network::mojom::CSPSource::New("https", "example.com", 443, "",
                                             false, false);
  auto source_list = mojom::CSPSourceList::New();
  source_list->allow_self = true;

  EXPECT_TRUE(Allow(source_list, GURL("https://example.com/print.pdf"), *self));
}

TEST(CSPSourceList, AllowNone) {
  auto self = network::mojom::CSPSource::New("http", "example.com", 80, "",
                                             false, false);
  auto source_list = mojom::CSPSourceList::New();
  EXPECT_FALSE(Allow(source_list, GURL("http://example.com"), *self));
  EXPECT_FALSE(Allow(source_list, GURL("https://example.test/"), *self));
}

TEST(CSPSourceTest, SelfIsUnique) {
  // Policy: 'self'
  auto source_list = mojom::CSPSourceList::New();
  source_list->allow_self = true;

  auto self =
      network::mojom::CSPSource::New("http", "a.com", 80, "", false, false);
  EXPECT_TRUE(Allow(source_list, GURL("http://a.com"), *self));
  EXPECT_FALSE(Allow(source_list, GURL("data:text/html,hello"), *self));

  // Self doesn't match anything.
  auto no_self_source = network::mojom::CSPSource::New(
      "", "", url::PORT_UNSPECIFIED, "", false, false);
  EXPECT_FALSE(Allow(source_list, GURL("http://a.com"), *no_self_source));
  EXPECT_FALSE(
      Allow(source_list, GURL("data:text/html,hello"), *no_self_source));
}

TEST(CSPSourceList, Subsume) {
  std::string required =
      "http://example1.com/foo/ https://*.example2.com/bar/ "
      "http://*.example3.com:*/bar/";
  mojom::CSPSourceListPtr required_sources =
      ParseToSourceList(mojom::CSPDirectiveName::ScriptSrc, required);

  struct TestCase {
    std::vector<std::string> response_csp;
    bool expected;
  } cases[] = {
      // Non-intersecting source lists give an effective policy of 'none', which
      // is always subsumed.
      {{"http://example1.com/bar/", "http://*.example3.com:*/bar/"}, true},
      {{"http://example1.com/bar/",
        "http://*.example3.com:*/bar/ https://*.example2.com/bar/"},
       true},
      // Lists that intersect into one of the required sources are subsumed.
      {{"http://example1.com/foo/"}, true},
      {{"https://*.example2.com/bar/"}, true},
      {{"http://*.example3.com:*/bar/"}, true},
      {{"https://example1.com/foo/",
        "http://*.example1.com/foo/ https://*.example2.com/bar/"},
       true},
      {{"https://example2.com/bar/",
        "http://*.example3.com:*/bar/ https://*.example2.com/bar/"},
       true},
      {{"http://example3.com:100/bar/",
        "http://*.example3.com:*/bar/ https://*.example2.com/bar/"},
       true},
      // Lists that intersect into two of the required sources are subsumed.
      {{"http://example1.com/foo/ https://*.example2.com/bar/"}, true},
      {{"http://example1.com/foo/ https://a.example2.com/bar/",
        "https://a.example2.com/bar/ http://example1.com/foo/"},
       true},
      {{"http://example1.com/foo/ https://a.example2.com/bar/",
        "http://*.example2.com/bar/ http://example1.com/foo/"},
       true},
      // Ordering should not matter.
      {{"https://example1.com/foo/ https://a.example2.com/bar/",
        "http://a.example2.com/bar/ http://example1.com/foo/"},
       true},
      // Lists that intersect into a policy identical to the required list are
      // subsumed.
      {{"http://example1.com/foo/ https://*.example2.com/bar/ "
        "http://*.example3.com:*/bar/ http://example1.com/foo/"},
       true},
      {{"http://example1.com/foo/ https://*.example2.com/bar/ "
        "http://*.example3.com:*/bar/"},
       true},
      {{"http://example1.com/foo/ https://*.example2.com/bar/ "
        "http://*.example3.com:*/bar/",
        "http://example1.com/foo/ https://*.example2.com/bar/ "
        "http://*.example3.com:*/bar/ http://example4.com/foo/"},
       true},
      {{"http://example1.com/foo/ https://*.example2.com/bar/ "
        "http://*.example3.com:*/bar/",
        "http://example1.com/foo/ https://*.example2.com/bar/ "
        "http://*.example3.com:*/bar/ http://example1.com/foo/"},
       true},
      // Lists that include sources which are not subsumed by the required list
      // are not subsumed.
      {{"http://example1.com/foo/ https://*.example2.com/bar/ "
        "http://*.example3.com:*/bar/ http://*.example4.com:*/bar/"},
       false},
      {{"http://example1.com/foo/ http://example2.com/foo/"}, false},
      {{"http://*.com/bar/", "http://example1.com/bar/"}, false},
      {{"http://*.example1.com/bar/"}, false},
      {{"http://example1.com/bar/"}, false},
      {{"http://*.example1.com/foo/"}, false},
      {{"wss://example2.com/bar/"}, false},
      {{"http://*.non-example3.com:*/bar/"}, false},
      {{"http://example3.com/foo/"}, false},
      {{"http://not-example1.com", "http://not-example1.com"}, false},
      // Lists that intersect into sources which are not subsumed by the
      // required
      // list are not subsumed.
      {{"http://not-example1.com/foo/", "https:"}, false},
      {{"http://not-example1.com/foo/ http://example1.com/foo/", "https:"},
       false},
      {{"http://*", "http://*.com http://*.example3.com:*/bar/"}, false},
  };

  auto origin_b =
      mojom::CSPSource::New("https", "frame.test", 443, "", false, false);
  for (const auto& test : cases) {
    auto response_sources = ParseToVectorOfSourceLists(
        mojom::CSPDirectiveName::ScriptSrc, test.response_csp);

    EXPECT_EQ(test.expected,
              CSPSourceListSubsumes(
                  *required_sources, ToRawPointers(response_sources),
                  mojom::CSPDirectiveName::ScriptSrc, origin_b.get()))
        << required << " should " << (test.expected ? "" : "not ") << "subsume "
        << base::JoinString(test.response_csp, ", ");
  }
}

TEST(CSPSourceList, SubsumeWithSelf) {
  std::string required =
      "http://example1.com/foo/ http://*.example2.com/bar/ "
      "http://*.example3.com:*/bar/ 'self'";
  mojom::CSPSourceListPtr required_sources =
      ParseToSourceList(mojom::CSPDirectiveName::ScriptSrc, required);

  struct TestCase {
    std::vector<std::string> response_csp;
    const char* origin;
    bool expected;
  } cases[] = {
      // "https://example.test/" is a secure origin for both `required` and
      // `response_csp`.
      {{"'self'"}, "https://example.test/", true},
      {{"https://example.test"}, "https://example.test/", true},
      {{"https://example.test/"}, "https://example.test/", true},
      {{"'self' 'self' 'self'"}, "https://example.test/", true},
      {{"'self'", "'self'", "'self'"}, "https://example.test/", true},
      {{"'self'", "'self'", "https://*.example.test/"},
       "https://example.test/",
       true},
      {{"'self'", "'self'", "https://*.example.test/bar/"},
       "https://example.test/",
       true},
      {{"'self' https://another.test/bar", "'self' http://*.example.test/bar",
        "https://*.example.test/bar/"},
       "https://example.test/",
       true},
      {{"http://example1.com/foo/ 'self'"}, "https://example.test/", true},
      {{"http://example1.com/foo/ https://example.test/"},
       "https://example.test/",
       true},
      {{"http://example1.com/foo/ http://*.example2.com/bar/"},
       "https://example.test/",
       true},
      {{"http://example1.com/foo/ http://*.example2.com/bar/ "
        "http://*.example3.com:*/bar/ https://example.test/"},
       "https://example.test/",
       true},
      {{"http://example1.com/foo/ http://*.example2.com/bar/ "
        "http://*.example3.com:*/bar/ 'self'"},
       "https://example.test/",
       true},
      {{"'self'", "'self'", "https://example.test/"},
       "https://example.test/",
       true},
      {{"'self'", "https://example.test/folder/"},
       "https://example.test/",
       true},
      {{"'self'", "http://example.test/folder/"},
       "https://example.test/",
       true},
      {{"'self' https://example.com/", "https://example.com/"},
       "https://example.test/",
       false},
      {{"http://example1.com/foo/ http://*.example2.com/bar/",
        "http://example1.com/foo/ http://*.example2.com/bar/ 'self'"},
       "https://example.test/",
       true},
      {{"http://*.example1.com/foo/", "http://*.example1.com/foo/ 'self'"},
       "https://example.test/",
       false},
      {{"https://*.example.test/", "https://*.example.test/ 'self'"},
       "https://example.test/",
       false},
      {{"http://example.test/"}, "https://example.test/", false},
      {{"https://example.test/"}, "https://example.test/", true},
      // Origins of `required` and `response_csp` do not match.
      {{"https://example.test/"}, "https://other-origin.test/", false},
      {{"'self'"}, "https://other-origin.test/", true},
      {{"http://example1.com/foo/ http://*.example2.com/bar/ "
        "http://*.example3.com:*/bar/ 'self'"},
       "https://other-origin.test/",
       true},
      {{"http://example1.com/foo/ http://*.example2.com/bar/ "
        "http://*.example3.com:*/bar/ https://other-origin.test/"},
       "https://other-origin.test/",
       true},
      {{"http://example1.com/foo/ 'self'"}, "https://other-origin.test/", true},
      {{"'self'", "https://example.test/"}, "https://other-origin.test/", true},
      {{"'self' https://example.test/", "https://example.test/"},
       "https://other-origin.test/",
       false},
      {{"https://example.test/", "http://example.test/"},
       "https://other-origin.test/",
       false},
      {{"'self'", "http://other-origin.test/"},
       "https://other-origin.test/",
       true},
      {{"'self'", "https://non-example.test/"},
       "https://other-origin.test/",
       true},
      // `response_csp`'s origin matches one of the sources in the source list
      // of `required`.
      {{"'self'", "http://*.example1.com/foo/"}, "http://example1.com/", true},
      {{"http://*.example2.com/bar/", "'self'"},
       "http://example2.com/bar/",
       true},
      {{"'self' http://*.example1.com/foo/", "http://*.example1.com/foo/"},
       "http://example1.com/",
       false},
      {{"http://*.example2.com/bar/ http://example1.com/",
        "'self' http://example1.com/"},
       "http://example2.com/bar/",
       false},
  };

  for (const auto& test : cases) {
    auto response_sources = ParseToVectorOfSourceLists(
        mojom::CSPDirectiveName::ScriptSrc, test.response_csp);

    GURL parsed_test_origin(test.origin);
    auto origin_b = mojom::CSPSource::New(
        parsed_test_origin.scheme(), parsed_test_origin.host(),
        parsed_test_origin.EffectiveIntPort(), "", false, false);
    EXPECT_EQ(test.expected,
              CSPSourceListSubsumes(
                  *required_sources, ToRawPointers(response_sources),
                  mojom::CSPDirectiveName::ScriptSrc, origin_b.get()))
        << required << "from origin " << test.origin << " should "
        << (test.expected ? "" : "not ") << "subsume "
        << base::JoinString(test.response_csp, ", ");
  }
}

TEST(CSPSourceList, SubsumeAllowAllInline) {
  struct TestCase {
    mojom::CSPDirectiveName directive;
    std::string required;
    std::vector<std::string> response_csp;
    bool expected;
  } cases[] = {
      // `required` allows all inline behavior.
      {mojom::CSPDirectiveName::StyleSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'strict-dynamic'",
       {"'unsafe-inline' http://example1.com/foo/bar.html"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline'",
       {"http://example1.com/foo/ 'unsafe-inline'"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline'",
       {"'unsafe-inline' 'nonce-yay'", "'unsafe-inline'"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline'",
       {"'unsafe-inline' 'nonce-yay'", "'unsafe-inline'", "'strict-dynamic'"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline'",
       {"'unsafe-inline' 'nonce-yay'", "'unsafe-inline'",
        "'strict-dynamic' 'nonce-yay'"},
       true},
      // `required` does not allow all inline behavior.
      {mojom::CSPDirectiveName::StyleSrc,
       "http://example1.com/foo/ 'self' 'strict-dynamic'",
       {"'unsafe-inline' http://example1.com/foo/bar.html"},
       false},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self'",
       {"'unsafe-inline'"},
       false},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline'",
       {"'unsafe-inline' 'nonce-yay'", "'nonce-abc'"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self'",
       {"'unsafe-inline' https://example.test/"},
       false},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'strict-dynamic'",
       {"'unsafe-inline' https://example.test/"},
       false},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'strict-dynamic'",
       {"'unsafe-inline' 'strict-dynamic'"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'nonce-yay'",
       {"'unsafe-inline' 'nonce-yay'"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'strict-dynamic' "
       "'nonce-yay'",
       {"'unsafe-inline' 'nonce-yay'"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'strict-dynamic' "
       "'nonce-yay'",
       {"http://example1.com/foo/ 'unsafe-inline' 'strict-dynamic'"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'sha512-321cba' "
       "'strict-dynamic'",
       {"'unsafe-inline' 'sha512-321cba'"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'strict-dynamic' "
       "'sha512-321cba'",
       {"http://example1.com/foo/ 'unsafe-inline' 'strict-dynamic'"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'sha512-321cba'",
       {"http://example1.com/foo/ 'unsafe-inline'",
        "http://example1.com/foo/ 'sha512-321cba'"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'sha512-321cba'",
       {"http://example1.com/foo/ 'unsafe-inline'",
        "http://example1.com/foo/ 'unsafe-inline' 'sha512-321cba'"},
       false},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'sha512-321cba'",
       {"http://example1.com/foo/ 'unsafe-inline' 'nonce-yay'",
        "http://example1.com/foo/ 'unsafe-inline' 'sha512-321cba'"},
       true},
      {mojom::CSPDirectiveName::DefaultSrc,
       "http://example1.com/foo/ 'unsafe-inline' 'strict-dynamic'",
       {"http://example1.com/foo/ 'unsafe-inline'"},
       false},
  };

  auto origin_b =
      mojom::CSPSource::New("https", "frame.test", 443, "", false, false);
  for (const auto& test : cases) {
    mojom::CSPSourceListPtr required_sources =
        ParseToSourceList(test.directive, test.required);
    auto response_sources =
        ParseToVectorOfSourceLists(test.directive, test.response_csp);

    EXPECT_EQ(test.expected,
              CSPSourceListSubsumes(*required_sources,
                                    ToRawPointers(response_sources),
                                    test.directive, origin_b.get()))
        << test.required << " should " << (test.expected ? "" : "not ")
        << "subsume " << base::JoinString(test.response_csp, ", ");
  }
}

TEST(CSPSourceList, SubsumeUnsafeAttributes) {
  struct TestCase {
    mojom::CSPDirectiveName directive;
    std::string required;
    std::vector<std::string> response_csp;
    bool expected;
  } cases[] = {
      // `required` or `response_csp` contain `unsafe-eval`.
      {mojom::CSPDirectiveName::StyleSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'strict-dynamic' "
       "'unsafe-eval'",
       {"http://example1.com/foo/bar.html 'unsafe-eval'"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-eval'",
       {"http://example1.com/foo/ 'unsafe-inline'"},
       false},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-eval'",
       {"http://example1.com/foo/ 'unsafe-inline' 'unsafe-eval'"},
       false},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-eval'",
       {"http://example1.com/foo/ 'unsafe-eval'",
        "http://example1.com/foo/bar 'self' unsafe-eval'",
        "http://non-example.com/foo/ 'unsafe-eval' 'self'"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self'",
       {"http://example1.com/foo/ 'unsafe-eval'"},
       false},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline'",
       {"http://example1.com/foo/ 'unsafe-eval'",
        "http://example1.com/foo/bar 'self' 'unsafe-eval'",
        "http://non-example.com/foo/ 'unsafe-eval' 'self'"},
       false},
      // `required` or `response_csp` contain `unsafe-hashes`.
      {mojom::CSPDirectiveName::StyleSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'unsafe-eval' "
       "'strict-dynamic' "
       "'unsafe-hashes'",
       {"http://example1.com/foo/bar.html 'unsafe-hashes'"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-hashes'",
       {"http://example1.com/foo/ 'unsafe-inline'"},
       false},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-hashes'",
       {"http://example1.com/foo/ 'unsafe-inline' 'unsafe-hashes'"},
       false},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-eval' "
       "'unsafe-hashes'",
       {"http://example1.com/foo/ 'unsafe-eval' 'unsafe-hashes'",
        "http://example1.com/foo/bar 'self' 'unsafe-hashes'",
        "http://non-example.com/foo/ 'unsafe-hashes' 'self'"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self'",
       {"http://example1.com/foo/ 'unsafe-hashes'"},
       false},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline'",
       {"http://example1.com/foo/ 'unsafe-hashes'",
        "http://example1.com/foo/bar 'self' 'unsafe-hashes'",
        "https://example1.com/foo/bar 'unsafe-hashes' 'self'"},
       false},
  };

  auto origin_b =
      mojom::CSPSource::New("https", "frame.test", 443, "", false, false);
  for (const auto& test : cases) {
    mojom::CSPSourceListPtr required_sources =
        ParseToSourceList(test.directive, test.required);
    auto response_sources =
        ParseToVectorOfSourceLists(test.directive, test.response_csp);

    EXPECT_EQ(test.expected,
              CSPSourceListSubsumes(*required_sources,
                                    ToRawPointers(response_sources),
                                    test.directive, origin_b.get()))
        << test.required << " should " << (test.expected ? "" : "not ")
        << "subsume " << base::JoinString(test.response_csp, ", ");
  }
}

TEST(CSPSourceList, SubsumeNoncesAndHashes) {
  // For |required| to subsume |response_csp|:
  // - If |response_csp| enforces some nonce, then |required| must contain some
  // nonce, but they do not need to match.
  // - On the other side, all hashes enforced by |response_csp| must be
  // contained in |required|.
  struct TestCase {
    mojom::CSPDirectiveName directive;
    std::string required;
    std::vector<std::string> response_csp;
    bool expected;
  } cases[] = {
      // Check nonces.
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'unsafe-inline' 'nonce-abc'",
       {"'unsafe-inline'"},
       false},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'nonce-abc'",
       {"'nonce-abc'"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline'",
       {"'unsafe-inline' 'nonce-yay'", "'nonce-yay'"},
       false},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'nonce-yay'",
       {"'unsafe-inline' 'nonce-yay'", "'nonce-yay'"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'nonce-abc' 'nonce-yay'",
       {"'unsafe-inline' https://example.test/"},
       false},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'nonce-abc' 'nonce-yay'",
       {"'nonce-abc' https://example1.com/foo/"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'nonce-yay' "
       "'strict-dynamic'",
       {"https://example.test/ 'nonce-yay'"},
       false},
      {mojom::CSPDirectiveName::StyleSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'nonce-yay' "
       "'strict-dynamic'",
       {"'nonce-yay' https://example1.com/foo/"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'nonce-abc'",
       {"http://example1.com/foo/ 'nonce-xyz'"},
       true},
      {mojom::CSPDirectiveName::StyleSrc,
       "http://example1.com/foo/ 'nonce-abc'",
       {"http://example1.com/foo/ 'nonce-xyz'"},
       true},
      // Check hashes.
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'sha512-321cba'",
       {"http://example1.com/foo/page.html 'strict-dynamic'",
        "https://example1.com/foo/ 'sha512-321cba'"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'sha512-321cba'",
       {"http://some-other.com/ 'strict-dynamic' 'sha512-321cba'",
        "http://example1.com/foo/ 'unsafe-inline' 'sha512-321cba'"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'sha512-321cba'",
       {"http://example1.com/foo/ 'sha512-321abc' 'sha512-321cba'",
        "http://example1.com/foo/ 'sha512-321abc' 'sha512-321cba'"},
       false},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'sha512-321cba'",
       {"http://example1.com/foo/ 'unsafe-inline'",
        "http://example1.com/foo/ 'sha512-321cba'"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'sha512-321abc'",
       {"http://example1.com/foo/ 'unsafe-inline' 'sha512-321abc'",
        "http://example1.com/foo/ 'sha512-321abc'"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'sha512-321abc'",
       {"'unsafe-inline' 'sha512-321abc'",
        "http://example1.com/foo/ 'sha512-321abc'"},
       true},
      // Nonces and hashes together.
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'sha512-321abc' "
       "'nonce-abc'",
       {"'unsafe-inline' 'sha512-321abc' 'self'",
        "'unsafe-inline''sha512-321abc' https://example.test/"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'sha512-321abc' "
       "'nonce-abc'",
       {"'unsafe-inline' 'sha512-321abc' 'self' 'nonce-abc'",
        "'sha512-321abc' https://example.test/"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'sha512-321abc' "
       "'nonce-abc'",
       {"'unsafe-inline' 'sha512-321abc' 'self'",
        " 'sha512-321abc' https://example.test/ 'nonce-abc'"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'sha512-321abc' "
       "'nonce-abc'",
       {"'unsafe-inline' 'sha512-321abc' 'self' 'nonce-xyz'",
        "unsafe-inline' 'sha512-321abc' https://example.test/ 'nonce-xyz'"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'sha512-321abc' "
       "'nonce-abc'",
       {"'unsafe-inline' 'sha512-321abc' 'self' 'sha512-xyz'",
        "unsafe-inline' 'sha512-321abc' https://example.test/ 'sha512-xyz'"},
       false},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'nonce-abc' 'sha512-321abc'",
       {"http://example1.com/foo/ 'nonce-xyz' 'sha512-321abc'"},
       true},
      {mojom::CSPDirectiveName::StyleSrc,
       "http://example1.com/foo/ 'nonce-abc' 'sha512-321abc'",
       {"http://example1.com/foo/ 'nonce-xyz' 'sha512-321abc'"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'nonce-abc' 'sha512-321abc'",
       {"http://example1.com/foo/ 'nonce-xyz' 'sha512-xyz'"},
       false},
      {mojom::CSPDirectiveName::StyleSrc,
       "http://example1.com/foo/ 'nonce-abc' 'sha512-321abc'",
       {"http://example1.com/foo/ 'nonce-xyz' 'sha512-xyz'",
        "http://example1.com/foo/ 'nonce-zyx' 'nonce-xyz' 'sha512-xyz'"},
       false},
  };

  auto origin_b =
      mojom::CSPSource::New("https", "frame.test", 443, "", false, false);
  for (const auto& test : cases) {
    mojom::CSPSourceListPtr required_sources =
        ParseToSourceList(test.directive, test.required);
    auto response_sources =
        ParseToVectorOfSourceLists(test.directive, test.response_csp);

    EXPECT_EQ(test.expected,
              CSPSourceListSubsumes(*required_sources,
                                    ToRawPointers(response_sources),
                                    test.directive, origin_b.get()))
        << test.required << " should " << (test.expected ? "" : "not ")
        << "subsume " << base::JoinString(test.response_csp, ", ");
  }
}

TEST(CSPSourceList, SubsumeStrictDynamic) {
  struct TestCase {
    mojom::CSPDirectiveName directive;
    std::string required;
    std::vector<std::string> response_csp;
    bool expected;
  } cases[] = {
      // Neither `required` nor effective policy of `response_csp` has
      // `strict-dynamic`.
      {mojom::CSPDirectiveName::StyleSrc,
       "http://example1.com/foo/ 'self' 'nonce-yay' 'strict-dynamic'",
       {"'strict-dynamic' 'nonce-yay'", "'nonce-yay' 'strict-dynamic'"},
       true},
      {mojom::CSPDirectiveName::StyleSrc,
       "http://example1.com/foo/ 'self' 'nonce-yay' 'strict-dynamic'",
       {"'strict-dynamic' 'nonce-yay'", "'nonce-abc' 'strict-dynamic'"},
       true},
      {mojom::CSPDirectiveName::StyleSrc,
       "http://example1.com/foo/ 'self' 'sha512-321abc'",
       {"'strict-dynamic' 'nonce-yay' 'sha512-321abc'",
        "'sha512-321abc' 'strict-dynamic'"},
       true},
      {mojom::CSPDirectiveName::StyleSrc,
       "http://example1.com/foo/ 'self' 'sha512-321abc'",
       {"'strict-dynamic' 'nonce-yay' 'sha512-321abc'",
        "'sha512-321abc' 'strict-dynamic'", "'strict-dynamic'"},
       true},
      {mojom::CSPDirectiveName::StyleSrc,
       "http://example1.com/foo/ 'self' 'sha512-321abc'",
       {"'strict-dynamic' 'nonce-yay' http://example1.com/",
        "http://example1.com/ 'strict-dynamic'"},
       false},
      // `required` has `strict-dynamic`, effective policy of `response_csp`
      // does not.
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'nonce-yay' 'strict-dynamic'",
       {"'strict-dynamic' 'nonce-yay'", "'nonce-yay'"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'sha512-321abc' 'strict-dynamic'",
       {"'strict-dynamic' 'sha512-321abc'", "'unsafe-inline' 'sha512-321abc'"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'sha512-321abc' 'strict-dynamic'",
       {"'sha512-321abc'"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'sha512-321abc' 'strict-dynamic'",
       {"http://example1.com/foo/ 'sha512-321abc'"},
       false},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'sha512-321abc' 'strict-dynamic'",
       {"'self' 'sha512-321abc'"},
       false},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'strict-dynamic'",
       {"'strict-dynamic' 'nonce-yay'", "'nonce-yay'"},
       false},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'strict-dynamic'",
       {"http://example1.com/ 'sha512-321abc'",
        "http://example1.com/ 'sha512-321abc'"},
       false},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'sha512-321abc' 'strict-dynamic'",
       {"https://example1.com/foo/ 'sha512-321abc'",
        "http://example1.com/foo/ 'sha512-321abc'"},
       false},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'nonce-yay' 'strict-dynamic'",
       {"'strict-dynamic' 'nonce-yay'", "'nonce-yay'", "'sha512-321abc'"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-hashes' "
       "'strict-dynamic'",
       {"'strict-dynamic' 'unsafe-hashes'"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'nonce-yay' 'strict-dynamic'",
       {"'strict-dynamic' 'nonce-yay' 'unsafe-hashes'"},
       false},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-eval' 'strict-dynamic'",
       {"'strict-dynamic' 'unsafe-eval'"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'nonce-yay' 'strict-dynamic'",
       {"'strict-dynamic' 'nonce-yay' 'unsafe-eval'"},
       false},
      // `required` does not have `strict-dynamic`, but effective policy of
      // `response_csp` does.
      // Note that any subsumption in this set-up should be `false`.
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'nonce-yay'",
       {"'strict-dynamic' 'nonce-yay'", "'sha512-321abc' 'strict-dynamic'"},
       false},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'sha512-321abc'",
       {"'strict-dynamic' 'sha512-321abc'", "'strict-dynamic' 'sha512-321abc'"},
       false},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline'",
       {"'strict-dynamic'"},
       false},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'sha512-321abc'",
       {"'strict-dynamic'"},
       false},
      // Both `required` and effective policy of `response_csp` has
      // `strict-dynamic`.
      {mojom::CSPDirectiveName::ScriptSrc,
       "'strict-dynamic'",
       {"'strict-dynamic'", "'strict-dynamic'", "'strict-dynamic'"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "'strict-dynamic'",
       {"'strict-dynamic'", "'strict-dynamic' 'nonce-yay'",
        "'strict-dynamic' 'nonce-yay'"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "'strict-dynamic' 'nonce-yay'",
       {"http://example.com 'strict-dynamic' 'nonce-yay'"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'strict-dynamic'",
       {"'strict-dynamic' 'nonce-yay'", "'strict-dynamic'"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'strict-dynamic'",
       {"'strict-dynamic' http://another.com/",
        "http://another.com/ 'strict-dynamic'"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'strict-dynamic'",
       {"'self' 'sha512-321abc' 'strict-dynamic'",
        "'self' 'strict-dynamic' 'sha512-321abc'"},
       false},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'sha512-321abc' 'strict-dynamic'",
       {"'self' 'sha512-321abc' 'strict-dynamic'"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'sha512-321abc' 'strict-dynamic'",
       {"'unsafe-inline' 'strict-dynamic'"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'sha512-321abc' 'strict-dynamic'",
       {"'unsafe-inline' 'sha512-123xyz' 'strict-dynamic'"},
       false},
      {mojom::CSPDirectiveName::ScriptSrc,
       "'unsafe-eval' 'self' 'sha512-321abc' 'strict-dynamic'",
       {"'unsafe-eval' 'strict-dynamic'"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'sha512-321abc' 'strict-dynamic'",
       {"'unsafe-eval' 'strict-dynamic'"},
       false},
      {mojom::CSPDirectiveName::ScriptSrc,
       "'unsafe-hashes' 'self' 'sha512-321abc' 'strict-dynamic'",
       {"'unsafe-hashes' 'strict-dynamic'"},
       true},
      {mojom::CSPDirectiveName::ScriptSrc,
       "http://example1.com/foo/ 'self' 'sha512-321abc' 'strict-dynamic'",
       {"'unsafe-hashes' 'strict-dynamic'"},
       false},
  };

  auto origin_b =
      mojom::CSPSource::New("https", "frame.test", 443, "", false, false);
  for (const auto& test : cases) {
    mojom::CSPSourceListPtr required_sources =
        ParseToSourceList(test.directive, test.required);
    auto response_sources =
        ParseToVectorOfSourceLists(test.directive, test.response_csp);

    EXPECT_EQ(test.expected,
              CSPSourceListSubsumes(*required_sources,
                                    ToRawPointers(response_sources),
                                    test.directive, origin_b.get()))
        << test.required << " should " << (test.expected ? "" : "not ")
        << "subsume " << base::JoinString(test.response_csp, ", ");
  }
}

TEST(CSPSourceList, SubsumeListWildcard) {
  struct TestCase {
    std::string required;
    std::vector<std::string> response_csp;
    bool expected;
  } cases[] = {
      // `required` subsumes `response_csp`.
      {"*", {""}, true},
      {"*", {"'none'"}, true},
      {"*", {"*"}, true},
      {"*", {"*", "*", "*"}, true},
      {"*", {"*", "* https: http: ftp: ws: wss:"}, true},
      {"*", {"*", "https: http: ftp: ws: wss:"}, true},
      {"https: http: ftp: ws: wss:", {"*", "https: http: ftp: ws: wss:"}, true},
      {"http: ftp: ws:", {"*", "https: http: ftp: ws: wss:"}, true},
      {"http: ftp: ws:", {"*", "https: 'strict-dynamic'"}, true},
      {"http://another.test", {"*", "'self'"}, true},
      {"http://another.test/", {"*", "'self'"}, true},
      {"http://another.test", {"https:", "'self'"}, true},
      {"'self'", {"*", "'self'"}, true},
      {"'unsafe-eval' * ", {"'unsafe-eval'"}, true},
      {"'unsafe-hashes' * ", {"'unsafe-hashes'"}, true},
      {"'unsafe-inline' * ", {"'unsafe-inline'"}, true},
      {"*", {"*", "http://a.com ws://b.com ftp://c.com"}, true},
      {"*", {"* data: blob:", "http://a.com ws://b.com ftp://c.com"}, true},
      {"*", {"data: blob:", "http://a.com ws://b.com ftp://c.com"}, true},
      {"*", {"*", "data://a.com ws://b.com ftp://c.com"}, true},
      {"* data:",
       {"data: blob: *", "data://a.com ws://b.com ftp://c.com"},
       true},
      {"http://a.com ws://b.com ftp://c.com",
       {"*", "http://a.com ws://b.com ftp://c.com"},
       true},
      // `required` does not subsume `response_csp`.
      {"*", std::vector<std::string>(), false},
      {"", {"*"}, false},
      {"'none'", {"*"}, false},
      {"*", {"data:"}, false},
      {"*", {"blob:"}, false},
      {"http: ftp: ws:",
       {"* 'strict-dynamic'", "https: 'strict-dynamic'"},
       false},
      {"https://another.test", {"*"}, false},
      {"*", {"* 'unsafe-eval'"}, false},
      {"*", {"* 'unsafe-hashes'"}, false},
      {"*", {"* 'unsafe-inline'"}, false},
      {"'unsafe-eval'", {"* 'unsafe-eval'"}, false},
      {"'unsafe-hashes'", {"* 'unsafe-hashes'"}, false},
      {"'unsafe-inline'", {"* 'unsafe-inline'"}, false},
      {"*", {"data: blob:", "data://a.com ws://b.com ftp://c.com"}, false},
      {"* data:",
       {"data: blob:", "blob://a.com ws://b.com ftp://c.com"},
       false},
  };

  auto origin_b =
      mojom::CSPSource::New("https", "another.test", 443, "", false, false);
  for (const auto& test : cases) {
    mojom::CSPSourceListPtr required_sources =
        ParseToSourceList(mojom::CSPDirectiveName::ScriptSrc, test.required);
    auto response_sources = ParseToVectorOfSourceLists(
        mojom::CSPDirectiveName::ScriptSrc, test.response_csp);

    EXPECT_EQ(test.expected,
              CSPSourceListSubsumes(
                  *required_sources, ToRawPointers(response_sources),
                  mojom::CSPDirectiveName::ScriptSrc, origin_b.get()))
        << test.required << " should " << (test.expected ? "" : "not ")
        << "subsume " << base::JoinString(test.response_csp, ", ");
  }
}

TEST(CSPSourceList, SubsumeListNoScheme) {
  auto origin_http =
      mojom::CSPSource::New("http", "example.org", 80, "", false, false);
  auto origin_https =
      mojom::CSPSource::New("https", "example.org", 443, "", false, false);
  struct TestCase {
    std::string required;
    std::vector<std::string> response_csp;
    raw_ptr<mojom::CSPSource> origin;
    bool expected;
  } cases[] = {
      {"http://a.com", {"a.com"}, origin_https.get(), true},
      {"https://a.com", {"a.com"}, origin_https.get(), true},
      {"https://a.com", {"a.com"}, origin_http.get(), false},
      {"data://a.com", {"a.com"}, origin_https.get(), false},
      {"a.com", {"a.com"}, origin_https.get(), true},
      {"a.com", {"a.com"}, origin_http.get(), true},
      {"a.com", {"https://a.com"}, origin_http.get(), true},
      {"a.com", {"https://a.com"}, origin_https.get(), true},
      {"a.com", {"http://a.com"}, origin_https.get(), false},
      {"https:", {"a.com"}, origin_http.get(), false},
      {"http:", {"a.com"}, origin_http.get(), true},
      {"https:", {"a.com", "https:"}, origin_http.get(), true},
      {"https:", {"a.com"}, origin_https.get(), true},
  };

  for (const auto& test : cases) {
    mojom::CSPSourceListPtr required_sources =
        ParseToSourceList(mojom::CSPDirectiveName::ScriptSrc, test.required);
    auto response_sources = ParseToVectorOfSourceLists(
        mojom::CSPDirectiveName::ScriptSrc, test.response_csp);

    EXPECT_EQ(test.expected,
              CSPSourceListSubsumes(
                  *required_sources, ToRawPointers(response_sources),
                  mojom::CSPDirectiveName::ScriptSrc, test.origin))
        << test.required << " on origin with scheme " << test.origin->scheme
        << " should " << (test.expected ? "" : "not ") << "subsume "
        << base::JoinString(test.response_csp, ", ");
  }
}

TEST(CSPSourceList, OpaqueURLMatchingAllowStar) {
  auto source_list = mojom::CSPSourceList::New();
  source_list->allow_star = true;
  EXPECT_TRUE(Allow(source_list, GURL("https://not-example.com"), no_self,
                    /*is_redirect=*/false,
                    mojom::CSPDirectiveName::FencedFrameSrc,
                    /*is_opaque_fenced_frame=*/true));
}

TEST(CSPSourceList, OpaqueURLMatchingAllowSelf) {
  auto self = network::mojom::CSPSource::New("https", "example.com", 443, "",
                                             false, false);

  auto source_list = mojom::CSPSourceList::New();
  source_list->allow_self = true;
  EXPECT_FALSE(Allow(source_list, GURL("https://example.com"), *self,
                     /*is_redirect=*/false,
                     mojom::CSPDirectiveName::FencedFrameSrc,
                     /*is_opaque_fenced_frame=*/true));
}

}  // namespace network
