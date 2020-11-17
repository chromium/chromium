// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/source_list_directive.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/csp/csp_source.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

class SourceListDirectiveTest : public testing::Test {
 public:
  SourceListDirectiveTest()
      : csp(MakeGarbageCollected<ContentSecurityPolicy>()) {}

 protected:
  struct Source {
    String scheme;
    String host;
    const int port;
    String path;
    CSPSource::WildcardDisposition host_wildcard;
    CSPSource::WildcardDisposition port_wildcard;
  };

  void SetUp() override {
    KURL secure_url("https://example.test/image.png");
    context = MakeGarbageCollected<NullExecutionContext>();
    context->GetSecurityContext().SetSecurityOrigin(
        SecurityOrigin::Create(secure_url));
    csp->BindToDelegate(context->GetContentSecurityPolicyDelegate());
  }

  ContentSecurityPolicy* SetUpWithOrigin(const String& origin) {
    KURL secure_url(origin);
    auto* context = MakeGarbageCollected<NullExecutionContext>();
    context->GetSecurityContext().SetSecurityOrigin(
        SecurityOrigin::Create(secure_url));
    auto* csp = MakeGarbageCollected<ContentSecurityPolicy>();
    csp->BindToDelegate(context->GetContentSecurityPolicyDelegate());
    return csp;
  }

  bool EqualSources(const Source& a, const Source& b) {
    return a.scheme == b.scheme && a.host == b.host && a.port == b.port &&
           a.path == b.path && a.host_wildcard == b.host_wildcard &&
           a.port_wildcard == b.port_wildcard;
  }

  Persistent<ContentSecurityPolicy> csp;
  Persistent<ExecutionContext> context;
};

TEST_F(SourceListDirectiveTest, BasicMatchingNone) {
  KURL base;
  String sources = "'none'";
  SourceListDirective source_list("script-src", sources, csp.Get());

  EXPECT_FALSE(source_list.Allows(KURL(base, "http://example.com/")));
  EXPECT_FALSE(source_list.Allows(KURL(base, "https://example.test/")));
}

TEST_F(SourceListDirectiveTest, BasicMatchingStrictDynamic) {
  String sources = "'strict-dynamic'";
  SourceListDirective source_list("script-src", sources, csp.Get());

  EXPECT_TRUE(source_list.AllowDynamic());
}

TEST_F(SourceListDirectiveTest, BasicMatchingUnsafeHashes) {
  String sources = "'unsafe-hashes'";
  SourceListDirective source_list("script-src", sources, csp.Get());

  EXPECT_TRUE(source_list.AllowUnsafeHashes());
}

TEST_F(SourceListDirectiveTest, BasicMatchingStar) {
  KURL base;
  String sources = "*";
  SourceListDirective source_list("script-src", sources, csp.Get());

  EXPECT_TRUE(source_list.Allows(KURL(base, "http://example.com/")));
  EXPECT_TRUE(source_list.Allows(KURL(base, "https://example.com/")));
  EXPECT_TRUE(source_list.Allows(KURL(base, "http://example.com/bar")));
  EXPECT_TRUE(source_list.Allows(KURL(base, "http://foo.example.com/")));
  EXPECT_TRUE(source_list.Allows(KURL(base, "http://foo.example.com/bar")));
  EXPECT_TRUE(source_list.Allows(KURL(base, "ftp://example.com/")));

  EXPECT_FALSE(source_list.Allows(KURL(base, "data:https://example.test/")));
  EXPECT_FALSE(source_list.Allows(KURL(base, "blob:https://example.test/")));
  EXPECT_FALSE(
      source_list.Allows(KURL(base, "filesystem:https://example.test/")));
  EXPECT_FALSE(source_list.Allows(KURL(base, "file:///etc/hosts")));
  EXPECT_FALSE(source_list.Allows(KURL(base, "applewebdata://example.test/")));
}

TEST_F(SourceListDirectiveTest, StarallowsSelf) {
  KURL base;
  String sources = "*";
  SourceListDirective source_list("script-src", sources, csp.Get());

  // With a protocol of 'file', '*' allows 'file:':
  scoped_refptr<const SecurityOrigin> origin =
      SecurityOrigin::CreateFromValidTuple("file", "", 0);
  csp->SetupSelf(*origin);
  EXPECT_TRUE(source_list.Allows(KURL(base, "file:///etc/hosts")));

  // The other results are the same as above:
  EXPECT_TRUE(source_list.Allows(KURL(base, "http://example.com/")));
  EXPECT_TRUE(source_list.Allows(KURL(base, "https://example.com/")));
  EXPECT_TRUE(source_list.Allows(KURL(base, "http://example.com/bar")));
  EXPECT_TRUE(source_list.Allows(KURL(base, "http://foo.example.com/")));
  EXPECT_TRUE(source_list.Allows(KURL(base, "http://foo.example.com/bar")));

  EXPECT_FALSE(source_list.Allows(KURL(base, "data:https://example.test/")));
  EXPECT_FALSE(source_list.Allows(KURL(base, "blob:https://example.test/")));
  EXPECT_FALSE(
      source_list.Allows(KURL(base, "filesystem:https://example.test/")));
  EXPECT_FALSE(source_list.Allows(KURL(base, "applewebdata://example.test/")));
}

TEST_F(SourceListDirectiveTest, BasicMatchingSelf) {
  KURL base;
  String sources = "'self'";
  SourceListDirective source_list("script-src", sources, csp.Get());

  EXPECT_FALSE(source_list.Allows(KURL(base, "http://example.com/")));
  EXPECT_FALSE(source_list.Allows(KURL(base, "https://not-example.com/")));
  EXPECT_TRUE(source_list.Allows(KURL(base, "https://example.test/")));
}

TEST_F(SourceListDirectiveTest, BlobMatchingBlob) {
  KURL base;
  String sources = "blob:";
  SourceListDirective source_list("script-src", sources, csp.Get());

  EXPECT_FALSE(source_list.Allows(KURL(base, "https://example.test/")));
  EXPECT_TRUE(source_list.Allows(KURL(base, "blob:https://example.test/")));
}

TEST_F(SourceListDirectiveTest, BasicMatching) {
  KURL base;
  String sources = "http://example1.com:8000/foo/ https://example2.com/";
  SourceListDirective source_list("script-src", sources, csp.Get());

  EXPECT_TRUE(source_list.Allows(KURL(base, "http://example1.com:8000/foo/")));
  EXPECT_TRUE(
      source_list.Allows(KURL(base, "http://example1.com:8000/foo/bar")));
  EXPECT_TRUE(source_list.Allows(KURL(base, "https://example2.com/")));
  EXPECT_TRUE(source_list.Allows(KURL(base, "https://example2.com/foo/")));

  EXPECT_FALSE(source_list.Allows(KURL(base, "https://not-example.com/")));
  EXPECT_FALSE(source_list.Allows(KURL(base, "http://example1.com/")));
  EXPECT_FALSE(source_list.Allows(KURL(base, "https://example1.com/foo")));
  EXPECT_FALSE(source_list.Allows(KURL(base, "http://example1.com:9000/foo/")));
  EXPECT_FALSE(source_list.Allows(KURL(base, "http://example1.com:8000/FOO/")));
}

TEST_F(SourceListDirectiveTest, WildcardMatching) {
  KURL base;
  String sources =
      "http://example1.com:*/foo/ https://*.example2.com/bar/ http://*.test/";
  SourceListDirective source_list("script-src", sources, csp.Get());

  EXPECT_TRUE(source_list.Allows(KURL(base, "http://example1.com/foo/")));
  EXPECT_TRUE(source_list.Allows(KURL(base, "http://example1.com:8000/foo/")));
  EXPECT_TRUE(source_list.Allows(KURL(base, "http://example1.com:9000/foo/")));
  EXPECT_TRUE(source_list.Allows(KURL(base, "https://foo.example2.com/bar/")));
  EXPECT_TRUE(source_list.Allows(KURL(base, "http://foo.test/")));
  EXPECT_TRUE(source_list.Allows(KURL(base, "http://foo.bar.test/")));
  EXPECT_TRUE(source_list.Allows(KURL(base, "https://example1.com/foo/")));
  EXPECT_TRUE(source_list.Allows(KURL(base, "https://example1.com:8000/foo/")));
  EXPECT_TRUE(source_list.Allows(KURL(base, "https://example1.com:9000/foo/")));
  EXPECT_TRUE(source_list.Allows(KURL(base, "https://foo.test/")));
  EXPECT_TRUE(source_list.Allows(KURL(base, "https://foo.bar.test/")));

  EXPECT_FALSE(source_list.Allows(KURL(base, "https://example1.com:8000/foo")));
  EXPECT_FALSE(source_list.Allows(KURL(base, "https://example2.com:8000/bar")));
  EXPECT_FALSE(
      source_list.Allows(KURL(base, "https://foo.example2.com:8000/bar")));
  EXPECT_FALSE(source_list.Allows(KURL(base, "https://example2.foo.com/bar")));
  EXPECT_FALSE(source_list.Allows(KURL(base, "http://foo.test.bar/")));
  EXPECT_FALSE(source_list.Allows(KURL(base, "https://example2.com/bar/")));
  EXPECT_FALSE(source_list.Allows(KURL(base, "http://test/")));
}

TEST_F(SourceListDirectiveTest, RedirectMatching) {
  KURL base;
  String sources = "http://example1.com/foo/ http://example2.com/bar/";
  SourceListDirective source_list("script-src", sources, csp.Get());

  EXPECT_TRUE(
      source_list.Allows(KURL(base, "http://example1.com/foo/"),
                         ResourceRequest::RedirectStatus::kFollowedRedirect));
  EXPECT_TRUE(
      source_list.Allows(KURL(base, "http://example1.com/bar/"),
                         ResourceRequest::RedirectStatus::kFollowedRedirect));
  EXPECT_TRUE(
      source_list.Allows(KURL(base, "http://example2.com/bar/"),
                         ResourceRequest::RedirectStatus::kFollowedRedirect));
  EXPECT_TRUE(
      source_list.Allows(KURL(base, "http://example2.com/foo/"),
                         ResourceRequest::RedirectStatus::kFollowedRedirect));
  EXPECT_TRUE(
      source_list.Allows(KURL(base, "https://example1.com/foo/"),
                         ResourceRequest::RedirectStatus::kFollowedRedirect));
  EXPECT_TRUE(
      source_list.Allows(KURL(base, "https://example1.com/bar/"),
                         ResourceRequest::RedirectStatus::kFollowedRedirect));

  EXPECT_FALSE(
      source_list.Allows(KURL(base, "http://example3.com/foo/"),
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

  // Script-src and style-src differently handle presence of 'strict-dynamic'.
  SourceListDirective script_src("script-src",
                                 "'strict-dynamic' 'unsafe-inline'", csp.Get());
  EXPECT_FALSE(script_src.AllowAllInline());

  SourceListDirective style_src("style-src", "'strict-dynamic' 'unsafe-inline'",
                                csp.Get());
  EXPECT_TRUE(style_src.AllowAllInline());

  for (const auto& test : cases) {
    SourceListDirective script_src("script-src", test.sources, csp.Get());
    EXPECT_EQ(script_src.AllowAllInline(), test.expected);

    SourceListDirective style_src("style-src", test.sources, csp.Get());
    EXPECT_EQ(style_src.AllowAllInline(), test.expected);

    // If source list doesn't have a valid type, it must not allow all inline.
    SourceListDirective img_src("img-src", test.sources, csp.Get());
    EXPECT_FALSE(img_src.AllowAllInline());
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
    SourceListDirective script_src("script-src", test.sources, csp.Get());
    EXPECT_EQ(script_src.IsNone(), test.expected);

    SourceListDirective form_action("form-action", test.sources, csp.Get());
    EXPECT_EQ(form_action.IsNone(), test.expected);

    SourceListDirective frame_src("frame-src", test.sources, csp.Get());
    EXPECT_EQ(frame_src.IsNone(), test.expected);
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
    SourceListDirective script_src("script-src", test.sources, csp.Get());
    EXPECT_EQ(script_src.IsSelf(), test.expected);

    SourceListDirective form_action("form-action", test.sources, csp.Get());
    EXPECT_EQ(form_action.IsSelf(), test.expected);

    SourceListDirective frame_src("frame-src", test.sources, csp.Get());
    EXPECT_EQ(frame_src.IsSelf(), test.expected);
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
    SourceListDirective script_src("script-src", test.sources, csp.Get());
    EXPECT_EQ(script_src.AllowsURLBasedMatching(), test.expected);

    SourceListDirective form_action("form-action", test.sources, csp.Get());
    EXPECT_EQ(form_action.AllowsURLBasedMatching(), test.expected);

    SourceListDirective frame_src("frame-src", test.sources, csp.Get());
    EXPECT_EQ(frame_src.AllowsURLBasedMatching(), test.expected);
  }
}

TEST_F(SourceListDirectiveTest, ParseHost) {
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
      {"a.b.", false},
      {".b.c", false},
      {"a..c", false},

      // Valid/Invalid characters.
      {"az09-", true},
      {"+", false},
  };

  for (const auto& test : cases) {
    String host;
    CSPSource::WildcardDisposition disposition = CSPSource::kNoWildcard;
    Vector<UChar> characters;
    test.sources.AppendTo(characters);
    const UChar* start = characters.data();
    const UChar* end = start + characters.size();
    EXPECT_EQ(test.expected,
              SourceListDirective::ParseHost(start, end, &host, &disposition))
        << "SourceListDirective::parseHost fail to parse: " << test.sources;
  }
}

TEST_F(SourceListDirectiveTest, AllowHostWildcard) {
  KURL base;
  // When the host-part is "*", the port must still be checked.
  // See crbug.com/682673.
  {
    String sources = "http://*:111";
    SourceListDirective source_list("default-src", sources, csp.Get());
    EXPECT_TRUE(source_list.Allows(KURL(base, "http://a.com:111")));
    EXPECT_FALSE(source_list.Allows(KURL(base, "http://a.com:222")));
  }
  // When the host-part is "*", the path must still be checked.
  // See crbug.com/682673.
  {
    String sources = "http://*/welcome.html";
    SourceListDirective source_list("default-src", sources, csp.Get());
    EXPECT_TRUE(source_list.Allows(KURL(base, "http://a.com/welcome.html")));
    EXPECT_FALSE(source_list.Allows(KURL(base, "http://a.com/passwords.txt")));
  }
  // When the host-part is "*" and the expression-source is not "*", then every
  // host are allowed. See crbug.com/682673.
  {
    String sources = "http://*";
    SourceListDirective source_list("default-src", sources, csp.Get());
    EXPECT_TRUE(source_list.Allows(KURL(base, "http://a.com")));
  }
}

TEST_F(SourceListDirectiveTest, AllowHostMixedCase) {
  KURL base;
  // Non-wildcard sources should match hosts case-insensitively.
  {
    String sources = "http://ExAmPle.com";
    SourceListDirective source_list("default-src", sources, csp.Get());
    EXPECT_TRUE(source_list.Allows(KURL(base, "http://example.com")));
  }
  // Wildcard sources should match hosts case-insensitively.
  {
    String sources = "http://*.ExAmPle.com";
    SourceListDirective source_list("default-src", sources, csp.Get());
    EXPECT_TRUE(source_list.Allows(KURL(base, "http://www.example.com")));
  }
}

}  // namespace blink
