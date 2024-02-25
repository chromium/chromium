// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/source_list_directive.h"

#include "services/network/public/mojom/content_security_policy.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/csp/csp_source.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

namespace {

network::mojom::blink::CSPSourceListPtr ParseSourceList(
    const String& directive_name,
    const String& directive_value) {
  Vector<network::mojom::blink::ContentSecurityPolicyPtr> parsed =
      ParseContentSecurityPolicies(
          directive_name + " " + directive_value,
          network::mojom::blink::ContentSecurityPolicyType::kEnforce,
          network::mojom::blink::ContentSecurityPolicySource::kHTTP,
          KURL("https://example.test"));
  return std::move(
      parsed[0]
          ->directives
          .find(ContentSecurityPolicy::GetDirectiveType(directive_name))
          ->value);
}

}  // namespace

class SourceListDirectiveTest : public testing::Test {
 protected:
  void SetUp() override {
    self_source = network::mojom::blink::CSPSource::New("https", "example.test",
                                                        443, "", false, false);
  }

  network::mojom::blink::CSPSourcePtr self_source;
};

TEST_F(SourceListDirectiveTest, BasicMatchingNone) {
  KURL base;
  String sources = "'none'";
  network::mojom::blink::CSPSourceListPtr source_list =
      ParseSourceList("script-src", sources);
  ASSERT_TRUE(source_list);

  EXPECT_FALSE(CSPSourceListAllows(*source_list, *self_source,
                                   KURL(base, "http://example.com/")));
  EXPECT_FALSE(CSPSourceListAllows(*source_list, *self_source,
                                   KURL(base, "https://example.test/")));
}

TEST_F(SourceListDirectiveTest, BasicMatchingStrictDynamic) {
  String sources = "'strict-dynamic'";
  network::mojom::blink::CSPSourceListPtr source_list =
      ParseSourceList("script-src", sources);

  EXPECT_TRUE(source_list->allow_dynamic);
}

TEST_F(SourceListDirectiveTest, BasicMatchingUnsafeHashes) {
  String sources = "'unsafe-hashes'";
  network::mojom::blink::CSPSourceListPtr source_list =
      ParseSourceList("script-src", sources);

  EXPECT_TRUE(source_list->allow_unsafe_hashes);
}

TEST_F(SourceListDirectiveTest, BasicMatchingStar) {
  KURL base;
  String sources = "*";
  network::mojom::blink::CSPSourceListPtr source_list =
      ParseSourceList("script-src", sources);

  EXPECT_EQ(CSPSourceListAllows(*source_list, *self_source,
                                KURL(base, "http://example.com/")),
            CSPCheckResult::Allowed());
  EXPECT_EQ(CSPSourceListAllows(*source_list, *self_source,
                                KURL(base, "https://example.com/")),
            CSPCheckResult::Allowed());
  EXPECT_EQ(CSPSourceListAllows(*source_list, *self_source,
                                KURL(base, "http://example.com/bar")),
            CSPCheckResult::Allowed());
  EXPECT_EQ(CSPSourceListAllows(*source_list, *self_source,
                                KURL(base, "http://foo.example.com/")),
            CSPCheckResult::Allowed());
  EXPECT_EQ(CSPSourceListAllows(*source_list, *self_source,
                                KURL(base, "http://foo.example.com/bar")),
            CSPCheckResult::Allowed());
  EXPECT_EQ(CSPSourceListAllows(*source_list, *self_source,
                                KURL(base, "ftp://example.com/")),
            base::FeatureList::IsEnabled(
                network::features::kCspStopMatchingWildcardDirectivesToFtp)
                ? CSPCheckResult::Blocked()
                : CSPCheckResult::AllowedOnlyIfWildcardMatchesFtp());
  EXPECT_EQ(CSPSourceListAllows(*source_list, *self_source,
                                KURL(base, "ws://example.com/")),
            CSPCheckResult::AllowedOnlyIfWildcardMatchesWs());

  EXPECT_EQ(CSPSourceListAllows(*source_list, *self_source,
                                KURL(base, "data:https://example.test/")),
            CSPCheckResult::Blocked());
  EXPECT_EQ(CSPSourceListAllows(*source_list, *self_source,
                                KURL(base, "blob:https://example.test/")),
            CSPCheckResult::Blocked());
  EXPECT_EQ(CSPSourceListAllows(*source_list, *self_source,
                                KURL(base, "filesystem:https://example.test/")),
            CSPCheckResult::Blocked());
  EXPECT_EQ(CSPSourceListAllows(*source_list, *self_source,
                                KURL(base, "file:///etc/hosts")),
            CSPCheckResult::Blocked());
  EXPECT_EQ(CSPSourceListAllows(*source_list, *self_source,
                                KURL(base, "applewebdata://example.test/")),
            CSPCheckResult::Blocked());
}

TEST_F(SourceListDirectiveTest, BasicMatchingStarPlusExplicitFtpWs) {
  network::mojom::blink::CSPSourceListPtr source_list =
      ParseSourceList("script-src", "* ftp: ws:");

  EXPECT_EQ(CSPSourceListAllows(*source_list, *self_source,
                                KURL("ftp://example.com/")),
            CSPCheckResult::Allowed());
  EXPECT_EQ(CSPSourceListAllows(*source_list, *self_source,
                                KURL("ws://example.com/")),
            CSPCheckResult::Allowed());
}

TEST_F(SourceListDirectiveTest, StarallowsSelf) {
  KURL base;
  String sources = "*";
  network::mojom::blink::CSPSourceListPtr source_list =
      ParseSourceList("script-src", sources);

  auto self_origin =
      network::mojom::blink::CSPSource::New("file", "", -1, "", false, false);
  EXPECT_TRUE(CSPSourceListAllows(*source_list, *self_origin,
                                  KURL(base, "file:///etc/hosts")));

  // The other results are the same as above:
  EXPECT_TRUE(CSPSourceListAllows(*source_list, *self_origin,
                                  KURL(base, "http://example.com/")));
  EXPECT_TRUE(CSPSourceListAllows(*source_list, *self_origin,
                                  KURL(base, "https://example.com/")));
  EXPECT_TRUE(CSPSourceListAllows(*source_list, *self_origin,
                                  KURL(base, "http://example.com/bar")));
  EXPECT_TRUE(CSPSourceListAllows(*source_list, *self_origin,
                                  KURL(base, "http://foo.example.com/")));
  EXPECT_TRUE(CSPSourceListAllows(*source_list, *self_origin,
                                  KURL(base, "http://foo.example.com/bar")));

  EXPECT_FALSE(CSPSourceListAllows(*source_list, *self_origin,
                                   KURL(base, "data:https://example.test/")));
  EXPECT_FALSE(CSPSourceListAllows(*source_list, *self_origin,
                                   KURL(base, "blob:https://example.test/")));
  EXPECT_FALSE(
      CSPSourceListAllows(*source_list, *self_origin,
                          KURL(base, "filesystem:https://example.test/")));
  EXPECT_FALSE(CSPSourceListAllows(*source_list, *self_origin,
                                   KURL(base, "applewebdata://example.test/")));
}

TEST_F(SourceListDirectiveTest, BasicMatchingSelf) {
  KURL base;
  String sources = "'self'";
  network::mojom::blink::CSPSourceListPtr source_list =
      ParseSourceList("script-src", sources);

  EXPECT_FALSE(CSPSourceListAllows(*source_list, *self_source,
                                   KURL(base, "http://example.com/")));
  EXPECT_FALSE(CSPSourceListAllows(*source_list, *self_source,
                                   KURL(base, "https://not-example.com/")));
  EXPECT_TRUE(CSPSourceListAllows(*source_list, *self_source,
                                  KURL(base, "https://example.test/")));
}

TEST_F(SourceListDirectiveTest, BlobMatchingBlob) {
  KURL base;
  String sources = "blob:";
  network::mojom::blink::CSPSourceListPtr source_list =
      ParseSourceList("script-src", sources);

  EXPECT_FALSE(CSPSourceListAllows(*source_list, *self_source,
                                   KURL(base, "https://example.test/")));
  EXPECT_TRUE(CSPSourceListAllows(*source_list, *self_source,
                                  KURL(base, "blob:https://example.test/")));
}

TEST_F(SourceListDirectiveTest, BasicMatching) {
  KURL base;
  String sources = "http://example1.com:8000/foo/ https://example2.com/";
  network::mojom::blink::CSPSourceListPtr source_list =
      ParseSourceList("script-src", sources);

  EXPECT_TRUE(CSPSourceListAllows(*source_list, *self_source,
                                  KURL(base, "http://example1.com:8000/foo/")));
  EXPECT_TRUE(
      CSPSourceListAllows(*source_list, *self_source,
                          KURL(base, "http://example1.com:8000/foo/bar")));
  EXPECT_TRUE(CSPSourceListAllows(*source_list, *self_source,
                                  KURL(base, "https://example2.com/")));
  EXPECT_TRUE(CSPSourceListAllows(*source_list, *self_source,
                                  KURL(base, "https://example2.com/foo/")));

  EXPECT_FALSE(CSPSourceListAllows(*source_list, *self_source,
                                   KURL(base, "https://not-example.com/")));
  EXPECT_FALSE(CSPSourceListAllows(*source_list, *self_source,
                                   KURL(base, "http://example1.com/")));
  EXPECT_FALSE(CSPSourceListAllows(*source_list, *self_source,
                                   KURL(base, "https://example1.com/foo")));
  EXPECT_FALSE(CSPSourceListAllows(
      *source_list, *self_source, KURL(base, "http://example1.com:9000/foo/")));
  EXPECT_FALSE(CSPSourceListAllows(
      *source_list, *self_source, KURL(base, "http://example1.com:8000/FOO/")));
}

TEST_F(SourceListDirectiveTest, WildcardMatching) {
  KURL base;
  String sources =
      "http://example1.com:*/foo/ https://*.example2.com/bar/ http://*.test/";
  network::mojom::blink::CSPSourceListPtr source_list =
      ParseSourceList("script-src", sources);

  EXPECT_TRUE(CSPSourceListAllows(*source_list, *self_source,
                                  KURL(base, "http://example1.com/foo/")));
  EXPECT_TRUE(CSPSourceListAllows(*source_list, *self_source,
                                  KURL(base, "http://example1.com:8000/foo/")));
  EXPECT_TRUE(CSPSourceListAllows(*source_list, *self_source,
                                  KURL(base, "http://example1.com:9000/foo/")));
  EXPECT_TRUE(CSPSourceListAllows(*source_list, *self_source,
                                  KURL(base, "https://foo.example2.com/bar/")));
  EXPECT_TRUE(CSPSourceListAllows(*source_list, *self_source,
                                  KURL(base, "http://foo.test/")));
  EXPECT_TRUE(CSPSourceListAllows(*source_list, *self_source,
                                  KURL(base, "http://foo.bar.test/")));
  EXPECT_TRUE(CSPSourceListAllows(*source_list, *self_source,
                                  KURL(base, "https://example1.com/foo/")));
  EXPECT_TRUE(
      CSPSourceListAllows(*source_list, *self_source,
                          KURL(base, "https://example1.com:8000/foo/")));
  EXPECT_TRUE(
      CSPSourceListAllows(*source_list, *self_source,
                          KURL(base, "https://example1.com:9000/foo/")));
  EXPECT_TRUE(CSPSourceListAllows(*source_list, *self_source,
                                  KURL(base, "https://foo.test/")));
  EXPECT_TRUE(CSPSourceListAllows(*source_list, *self_source,
                                  KURL(base, "https://foo.bar.test/")));

  EXPECT_FALSE(CSPSourceListAllows(
      *source_list, *self_source, KURL(base, "https://example1.com:8000/foo")));
  EXPECT_FALSE(CSPSourceListAllows(
      *source_list, *self_source, KURL(base, "https://example2.com:8000/bar")));
  EXPECT_FALSE(
      CSPSourceListAllows(*source_list, *self_source,
                          KURL(base, "https://foo.example2.com:8000/bar")));
  EXPECT_FALSE(CSPSourceListAllows(*source_list, *self_source,
                                   KURL(base, "https://example2.foo.com/bar")));
  EXPECT_FALSE(CSPSourceListAllows(*source_list, *self_source,
                                   KURL(base, "http://foo.test.bar/")));
  EXPECT_FALSE(CSPSourceListAllows(*source_list, *self_source,
                                   KURL(base, "https://example2.com/bar/")));
  EXPECT_FALSE(CSPSourceListAllows(*source_list, *self_source,
                                   KURL(base, "http://test/")));
}

TEST_F(SourceListDirectiveTest, RedirectMatching) {
  KURL base;
  String sources = "http://example1.com/foo/ http://example2.com/bar/";
  network::mojom::blink::CSPSourceListPtr source_list =
      ParseSourceList("script-src", sources);

  EXPECT_TRUE(CSPSourceListAllows(
      *source_list, *self_source, KURL(base, "http://example1.com/foo/"),
      ResourceRequest::RedirectStatus::kFollowedRedirect));
  EXPECT_TRUE(CSPSourceListAllows(
      *source_list, *self_source, KURL(base, "http://example1.com/bar/"),
      ResourceRequest::RedirectStatus::kFollowedRedirect));
  EXPECT_TRUE(CSPSourceListAllows(
      *source_list, *self_source, KURL(base, "http://example2.com/bar/"),
      ResourceRequest::RedirectStatus::kFollowedRedirect));
  EXPECT_TRUE(CSPSourceListAllows(
      *source_list, *self_source, KURL(base, "http://example2.com/foo/"),
      ResourceRequest::RedirectStatus::kFollowedRedirect));
  EXPECT_TRUE(CSPSourceListAllows(
      *source_list, *self_source, KURL(base, "https://example1.com/foo/"),
      ResourceRequest::RedirectStatus::kFollowedRedirect));
  EXPECT_TRUE(CSPSourceListAllows(
      *source_list, *self_source, KURL(base, "https://example1.com/bar/"),
      ResourceRequest::RedirectStatus::kFollowedRedirect));

  EXPECT_FALSE(CSPSourceListAllows(
      *source_list, *self_source, KURL(base, "http://example3.com/foo/"),
      ResourceRequest::RedirectStatus::kFollowedRedirect));
}

TEST_F(SourceListDirectiveTest, AllowAllInline) {
  struct TestCase {
    String sources;
    bool expected;
  } cases[] = {
      // List does not contain 'unsafe-inline'.
      {"http://example1.com/foo/", false},
      {"'sha512-321cba'", false},
      {"'nonce-yay'", false},
      {"'strict-dynamic'", false},
      {"'sha512-321cba' http://example1.com/foo/", false},
      {"http://example1.com/foo/ 'sha512-321cba'", false},
      {"http://example1.com/foo/ 'nonce-yay'", false},
      {"'sha512-321cba' 'nonce-yay'", false},
      {"http://example1.com/foo/ 'sha512-321cba' 'nonce-yay'", false},
      {"http://example1.com/foo/ 'sha512-321cba' 'nonce-yay'", false},
      {" 'sha512-321cba' 'nonce-yay' 'strict-dynamic'", false},
      // List contains 'unsafe-inline'.
      {"'unsafe-inline'", true},
      {"'self' 'unsafe-inline'", true},
      {"'unsafe-inline' http://example1.com/foo/", true},
      {"'sha512-321cba' 'unsafe-inline'", false},
      {"'nonce-yay' 'unsafe-inline'", false},
      {"'strict-dynamic' 'unsafe-inline' 'nonce-yay'", false},
      {"'sha512-321cba' http://example1.com/foo/ 'unsafe-inline'", false},
      {"http://example1.com/foo/ 'sha512-321cba' 'unsafe-inline'", false},
      {"http://example1.com/foo/ 'nonce-yay' 'unsafe-inline'", false},
      {"'sha512-321cba' 'nonce-yay' 'unsafe-inline'", false},
      {"http://example1.com/foo/ 'sha512-321cba' 'unsafe-inline' 'nonce-yay'",
       false},
      {"http://example1.com/foo/ 'sha512-321cba' 'nonce-yay' 'unsafe-inline'",
       false},
      {" 'sha512-321cba' 'unsafe-inline' 'nonce-yay' 'strict-dynamic'", false},
  };

  using network::mojom::blink::CSPDirectiveName;

  // Script-src and style-src differently handle presence of 'strict-dynamic'.
  network::mojom::blink::CSPSourceListPtr script_src =
      ParseSourceList("script-src", "'strict-dynamic' 'unsafe-inline'");
  EXPECT_FALSE(CSPSourceListAllowAllInline(
      CSPDirectiveName::ScriptSrc, ContentSecurityPolicy::InlineType::kScript,
      *script_src));

  network::mojom::blink::CSPSourceListPtr style_src =
      ParseSourceList("style-src", "'strict-dynamic' 'unsafe-inline'");
  EXPECT_TRUE(CSPSourceListAllowAllInline(
      CSPDirectiveName::StyleSrc, ContentSecurityPolicy::InlineType::kStyle,
      *style_src));

  for (const auto& test : cases) {
    script_src = ParseSourceList("script-src", test.sources);
    EXPECT_EQ(CSPSourceListAllowAllInline(
                  CSPDirectiveName::ScriptSrc,
                  ContentSecurityPolicy::InlineType::kScript, *script_src),
              test.expected);

    style_src = ParseSourceList("style-src", test.sources);
    EXPECT_EQ(CSPSourceListAllowAllInline(
                  CSPDirectiveName::StyleSrc,
                  ContentSecurityPolicy::InlineType::kStyle, *style_src),
              test.expected);

    // If source list doesn't have a valid type, it must not allow all inline.
    network::mojom::blink::CSPSourceListPtr img_src =
        ParseSourceList("img-src", test.sources);
    EXPECT_FALSE(CSPSourceListAllowAllInline(
        CSPDirectiveName::ImgSrc, ContentSecurityPolicy::InlineType::kScript,
        *img_src));
  }
}

TEST_F(SourceListDirectiveTest, IsNone) {
  struct TestCase {
    String sources;
    bool expected;
  } cases[] = {
      // Source list is 'none'.
      {"'none'", true},
      {"", true},
      {"   ", true},
      // Source list is not 'none'.
      {"http://example1.com/foo/", false},
      {"'sha512-321cba'", false},
      {"'nonce-yay'", false},
      {"'strict-dynamic'", false},
      {"'sha512-321cba' http://example1.com/foo/", false},
      {"http://example1.com/foo/ 'sha512-321cba'", false},
      {"http://example1.com/foo/ 'nonce-yay'", false},
      {"'none' 'sha512-321cba' http://example1.com/foo/", false},
      {"'none' http://example1.com/foo/ 'sha512-321cba'", false},
      {"'none' http://example1.com/foo/ 'nonce-yay'", false},
      {"'sha512-321cba' 'nonce-yay'", false},
      {"http://example1.com/foo/ 'sha512-321cba' 'nonce-yay'", false},
      {"http://example1.com/foo/ 'sha512-321cba' 'nonce-yay'", false},
      {" 'sha512-321cba' 'nonce-yay' 'strict-dynamic'", false},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(test.sources);
    network::mojom::blink::CSPSourceListPtr script_src =
        ParseSourceList("script-src", test.sources);
    EXPECT_EQ(CSPSourceListIsNone(*script_src), test.expected);

    network::mojom::blink::CSPSourceListPtr form_action =
        ParseSourceList("form-action", test.sources);
    EXPECT_EQ(CSPSourceListIsNone(*form_action), test.expected);

    network::mojom::blink::CSPSourceListPtr frame_src =
        ParseSourceList("frame-src", test.sources);
    EXPECT_EQ(CSPSourceListIsNone(*frame_src), test.expected);
  }
}

TEST_F(SourceListDirectiveTest, IsSelf) {
  struct TestCase {
    String sources;
    bool expected;
  } cases[] = {
      // Source list is 'self'.
      {"'self'", true},
      {"'self' 'none'", true},

      // Source list is not 'self'.
      {"'none'", false},
      {"http://example1.com/foo/", false},
      {"'sha512-321cba'", false},
      {"'nonce-yay'", false},
      {"'strict-dynamic'", false},
      {"'sha512-321cba' http://example1.com/foo/", false},
      {"http://example1.com/foo/ 'sha512-321cba'", false},
      {"http://example1.com/foo/ 'nonce-yay'", false},
      {"'self' 'sha512-321cba' http://example1.com/foo/", false},
      {"'self' http://example1.com/foo/ 'sha512-321cba'", false},
      {"'self' http://example1.com/foo/ 'nonce-yay'", false},
      {"'sha512-321cba' 'nonce-yay'", false},
      {"http://example1.com/foo/ 'sha512-321cba' 'nonce-yay'", false},
      {"http://example1.com/foo/ 'sha512-321cba' 'nonce-yay'", false},
      {" 'sha512-321cba' 'nonce-yay' 'strict-dynamic'", false},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(test.sources);
    network::mojom::blink::CSPSourceListPtr script_src =
        ParseSourceList("script-src", test.sources);
    EXPECT_EQ(CSPSourceListIsSelf(*script_src), test.expected);

    network::mojom::blink::CSPSourceListPtr form_action =
        ParseSourceList("form-action", test.sources);
    EXPECT_EQ(CSPSourceListIsSelf(*form_action), test.expected);

    network::mojom::blink::CSPSourceListPtr frame_src =
        ParseSourceList("frame-src", test.sources);
    EXPECT_EQ(CSPSourceListIsSelf(*frame_src), test.expected);
  }
}

TEST_F(SourceListDirectiveTest, AllowsURLBasedMatching) {
  struct TestCase {
    String sources;
    bool expected;
  } cases[] = {
      // No URL-based matching.
      {"'none'", false},
      {"'sha256-abcdefg'", false},
      {"'nonce-abc'", false},
      {"'nonce-abce' 'sha256-abcdefg'", false},

      // Strict-dynamic.
      {"'sha256-abcdefg' 'strict-dynamic'", false},
      {"'nonce-abce' 'strict-dynamic'", false},
      {"'nonce-abce' 'sha256-abcdefg' 'strict-dynamic'", false},
      {"'sha256-abcdefg' 'strict-dynamic' https:", false},
      {"'nonce-abce' 'strict-dynamic' http://example.test", false},
      {"'nonce-abce' 'sha256-abcdefg' 'strict-dynamic' *://example.test",
       false},

      // URL-based.
      {"*", true},
      {"'self'", true},
      {"http:", true},
      {"http: https:", true},
      {"http: 'none'", true},
      {"http: https: 'none'", true},
      {"'sha256-abcdefg' https://example.test", true},
      {"'nonce-abc' https://example.test", true},
      {"'nonce-abce' 'sha256-abcdefg' https://example.test", true},
      {"'sha256-abcdefg' https://example.test 'none'", true},
      {"'nonce-abc' https://example.test 'none'", true},
      {"'nonce-abce' 'sha256-abcdefg' https://example.test 'none'", true},

  };

  for (const auto& test : cases) {
    SCOPED_TRACE(test.sources);
    network::mojom::blink::CSPSourceListPtr script_src =
        ParseSourceList("script-src", test.sources);
    EXPECT_EQ(CSPSourceListAllowsURLBasedMatching(*script_src), test.expected);

    network::mojom::blink::CSPSourceListPtr form_action =
        ParseSourceList("form-action", test.sources);
    EXPECT_EQ(CSPSourceListAllowsURLBasedMatching(*form_action), test.expected);

    network::mojom::blink::CSPSourceListPtr frame_src =
        ParseSourceList("frame-src", test.sources);
    EXPECT_EQ(CSPSourceListAllowsURLBasedMatching(*frame_src), test.expected);
  }
}

TEST_F(SourceListDirectiveTest, ParseSourceListHost) {
  struct TestCase {
    String sources;
    bool expected;
  } cases[] = {
      // Wildcard.
      {"*", true},
      {"*.", false},
      {"*.a", true},
      {"a.*.a", false},
      {"a.*", false},

      // Dots.
      {"a.b.c", true},
      {"a.b.", true},
      {".b.c", false},
      {"a..c", false},

      // Valid/Invalid characters.
      {"az09-", true},
      {"+", false},
  };

  for (const auto& test : cases) {
    network::mojom::blink::CSPSourceListPtr parsed =
        ParseSourceList("default-src", test.sources);
    EXPECT_EQ(CSPSourceListIsNone(*parsed), !test.expected)
        << "ParseSourceList failed to parse: " << test.sources;
  }
}

TEST_F(SourceListDirectiveTest, ParsePort) {
  struct TestCase {
    String sources;
    bool valid;
    int expected_port;
  } cases[] = {
      {"example.com", true, url::PORT_UNSPECIFIED},
      {"example.com:80", true, 80},
      {"http://example.com:80", true, 80},
      {"https://example.com:80", true, 80},
      {"https://example.com:90/path", true, 90},

      {"http://example.com:", false},
      {"https://example.com:/", false},
      {"http://example.com:/path", false},
  };

  for (const auto& test : cases) {
    network::mojom::blink::CSPSourceListPtr parsed =
        ParseSourceList("default-src", test.sources);
    EXPECT_EQ(CSPSourceListIsNone(*parsed), !test.valid)
        << "ParseSourceList failed to parse: " << test.sources;
    if (test.valid) {
      ASSERT_EQ(1u, parsed->sources.size());
      EXPECT_EQ(test.expected_port, parsed->sources[0]->port);
    }
  }
}

TEST_F(SourceListDirectiveTest, AllowHostWildcard) {
  KURL base;
  // When the host-part is "*", the port must still be checked.
  // See crbug.com/682673.
  {
    String sources = "http://*:111";
    network::mojom::blink::CSPSourceListPtr source_list =
        ParseSourceList("default-src", sources);
    EXPECT_TRUE(CSPSourceListAllows(*source_list, *self_source,
                                    KURL(base, "http://a.com:111")));
    EXPECT_FALSE(CSPSourceListAllows(*source_list, *self_source,
                                     KURL(base, "http://a.com:222")));
  }
  // When the host-part is "*", the path must still be checked.
  // See crbug.com/682673.
  {
    String sources = "http://*/welcome.html";
    network::mojom::blink::CSPSourceListPtr source_list =
        ParseSourceList("default-src", sources);
    EXPECT_TRUE(CSPSourceListAllows(*source_list, *self_source,
                                    KURL(base, "http://a.com/welcome.html")));
    EXPECT_FALSE(CSPSourceListAllows(*source_list, *self_source,
                                     KURL(base, "http://a.com/passwords.txt")));
  }
  // When the host-part is "*" and the expression-source is not "*", then every
  // host are allowed. See crbug.com/682673.
  {
    String sources = "http://*";
    network::mojom::blink::CSPSourceListPtr source_list =
        ParseSourceList("default-src", sources);
    EXPECT_TRUE(CSPSourceListAllows(*source_list, *self_source,
                                    KURL(base, "http://a.com")));
  }
}

TEST_F(SourceListDirectiveTest, AllowHostMixedCase) {
  KURL base;
  // Non-wildcard sources should match hosts case-insensitively.
  {
    String sources = "http://ExAmPle.com";
    network::mojom::blink::CSPSourceListPtr source_list =
        ParseSourceList("default-src", sources);
    EXPECT_TRUE(CSPSourceListAllows(*source_list, *self_source,
                                    KURL(base, "http://example.com")));
  }
  // Wildcard sources should match hosts case-insensitively.
  {
    String sources = "http://*.ExAmPle.com";
    network::mojom::blink::CSPSourceListPtr source_list =
        ParseSourceList("default-src", sources);
    EXPECT_TRUE(CSPSourceListAllows(*source_list, *self_source,
                                    KURL(base, "http://www.example.com")));
  }
}

TEST_F(SourceListDirectiveTest, AllowNonce) {
  struct TestCase {
    const char* directive_value;
    const char* nonce;
    bool expected;
  } cases[] = {
      {"'self'", "yay", false},
      {"'self'", "boo", false},
      {"'nonce-yay'", "yay", true},
      {"'nonce-yay'", "boo", false},
      {"'nonce-yay' 'nonce-boo'", "yay", true},
      {"'nonce-yay' 'nonce-boo'", "boo", true},
  };

  for (const auto& test : cases) {
    network::mojom::blink::CSPSourceListPtr source_list =
        ParseSourceList("script-src", test.directive_value);
    EXPECT_EQ(test.expected, CSPSourceListAllowNonce(*source_list, test.nonce));
    // Empty/null strings are always not present.
    EXPECT_FALSE(CSPSourceListAllowNonce(*source_list, ""));
    EXPECT_FALSE(CSPSourceListAllowNonce(*source_list, String()));
  }
}

}  // namespace blink
