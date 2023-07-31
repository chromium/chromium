// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_urlpatterninit_usvstring.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_url_pattern_init.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_script_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {
namespace {

class URLPatternHistogramTest : public PageTestBase {
 protected:
  URLPattern* MakeURLPattern(const String& pattern) {
    return URLPattern::Create(MakeGarbageCollected<V8URLPatternInput>(pattern),
                              ASSERT_NO_EXCEPTION);
  }

  URLPattern* MakeURLPattern(const String& pattern, const String& base) {
    return URLPattern::Create(MakeGarbageCollected<V8URLPatternInput>(pattern),
                              base, ASSERT_NO_EXCEPTION);
  }

  struct URLPatternInitStruct {
    String protocol, username, password, hostname, port, pathname, search, hash,
        baseURL;

    URLPatternInit* ToDictionary() const {
      auto* result = URLPatternInit::Create();
      if (!protocol.IsNull()) {
        result->setProtocol(protocol);
      }
      if (!username.IsNull()) {
        result->setUsername(username);
      }
      if (!password.IsNull()) {
        result->setPassword(password);
      }
      if (!hostname.IsNull()) {
        result->setHostname(hostname);
      }
      if (!port.IsNull()) {
        result->setPort(port);
      }
      if (!pathname.IsNull()) {
        result->setPathname(pathname);
      }
      if (!search.IsNull()) {
        result->setSearch(search);
      }
      if (!hash.IsNull()) {
        result->setHash(hash);
      }
      if (!baseURL.IsNull()) {
        result->setBaseURL(baseURL);
      }
      return result;
    }
  };

  URLPattern* MakeURLPattern(const URLPatternInitStruct& init) {
    return URLPattern::Create(
        MakeGarbageCollected<V8URLPatternInput>(init.ToDictionary()),
        ASSERT_NO_EXCEPTION);
  }

  ::testing::AssertionResult URLPatternTestRecordsHistogram(
      URLPattern* pattern,
      const KURL& url,
      WebFeature feature) {
    GetDocument().ClearUseCounterForTesting(feature);
    pattern->test(ToScriptStateForMainWorld(&GetFrame()),
                  MakeGarbageCollected<V8URLPatternInput>(url.GetString()),
                  ASSERT_NO_EXCEPTION);
    if (GetDocument().IsUseCounted(feature)) {
      return ::testing::AssertionSuccess()
             << "test with pattern " << pattern->ToString() << " on URL " << url
             << " logged feature " << feature;
    } else {
      return ::testing::AssertionFailure()
             << "test with pattern " << pattern->ToString() << " on URL " << url
             << " did not log feature " << feature;
    }
  }
};

TEST_F(URLPatternHistogramTest, OmittedSearchAndHashInString) {
  {
    // This pattern would start matching any search or hash.
    URLPattern* pattern = MakeURLPattern("https://example.com/login");
    EXPECT_FALSE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/login"),
        WebFeature::kURLPatternReliantOnImplicitURLComponentsInString));
    EXPECT_FALSE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/login?#"),
        WebFeature::kURLPatternReliantOnImplicitURLComponentsInString));
    EXPECT_TRUE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/login?a=1"),
        WebFeature::kURLPatternReliantOnImplicitURLComponentsInString));
    EXPECT_TRUE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/login#privacy"),
        WebFeature::kURLPatternReliantOnImplicitURLComponentsInString));
    EXPECT_TRUE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/login?a=1#privacy"),
        WebFeature::kURLPatternReliantOnImplicitURLComponentsInString));
  }
  {
    // This pattern would start matching any hash.
    URLPattern* pattern = MakeURLPattern("https://example.com/login?a=1");
    EXPECT_FALSE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/login"),
        WebFeature::kURLPatternReliantOnImplicitURLComponentsInString));
    EXPECT_FALSE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/login?#"),
        WebFeature::kURLPatternReliantOnImplicitURLComponentsInString));
    EXPECT_FALSE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/login?a=1"),
        WebFeature::kURLPatternReliantOnImplicitURLComponentsInString));
    EXPECT_FALSE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/login#privacy"),
        WebFeature::kURLPatternReliantOnImplicitURLComponentsInString));
    EXPECT_TRUE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/login?a=1#privacy"),
        WebFeature::kURLPatternReliantOnImplicitURLComponentsInString));
  }
  {
    // No behavior change in this case, since the presence of a hash means we're
    // specifying the exact pathname, search, etc.
    URLPattern* pattern = MakeURLPattern("https://example.com/login#privacy");
    EXPECT_FALSE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/login"),
        WebFeature::kURLPatternReliantOnImplicitURLComponentsInString));
    EXPECT_FALSE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/login?#"),
        WebFeature::kURLPatternReliantOnImplicitURLComponentsInString));
    EXPECT_FALSE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/login?a=1"),
        WebFeature::kURLPatternReliantOnImplicitURLComponentsInString));
    EXPECT_FALSE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/login#privacy"),
        WebFeature::kURLPatternReliantOnImplicitURLComponentsInString));
    EXPECT_FALSE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/login?a=1#privacy"),
        WebFeature::kURLPatternReliantOnImplicitURLComponentsInString));
  }
  {
    // This behavior applies to patterns from base URLs, too.
    URLPattern* pattern = MakeURLPattern("/login", "https://example.com/a/b");
    EXPECT_FALSE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/login"),
        WebFeature::kURLPatternReliantOnImplicitURLComponentsInString));
    EXPECT_FALSE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/login?#"),
        WebFeature::kURLPatternReliantOnImplicitURLComponentsInString));
    EXPECT_TRUE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/login?a=1"),
        WebFeature::kURLPatternReliantOnImplicitURLComponentsInString));
    EXPECT_TRUE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/login#privacy"),
        WebFeature::kURLPatternReliantOnImplicitURLComponentsInString));
    EXPECT_TRUE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/login?a=1#privacy"),
        WebFeature::kURLPatternReliantOnImplicitURLComponentsInString));
  }
  {
    // When it does, specified components still supersede those in the base URL.
    URLPattern* pattern =
        MakeURLPattern("/login", "https://example.com/?q=1#h");
    EXPECT_FALSE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/login"),
        WebFeature::kURLPatternReliantOnImplicitURLComponentsInString));
    EXPECT_TRUE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/login?q=2#z"),
        WebFeature::kURLPatternReliantOnImplicitURLComponentsInString));
  }
  {
    // If username and password are specified, then you can't really have not
    // specified a hostname (even an empty one), since an @ must occur in the
    // authority section. But nonetheless, for test coverage's sake...
    URLPattern* pattern = MakeURLPattern("https://user:pass@example.com/*");
    EXPECT_FALSE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://user:pass@example.com/"),
        WebFeature::kURLPatternReliantOnImplicitURLComponentsInString));
    EXPECT_FALSE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://user:pass@example.com/foo?bar=1#baz"),
        WebFeature::kURLPatternReliantOnImplicitURLComponentsInString));
  }
  {
    // Debatable special cases!
    //
    // This is subtle, but it's most natural in the current algorithm to
    // interpret this as having an empty hostname component, similar to how "?"
    // has a search component which is empty. If we wanted to define this to
    // mean "any URL with the https scheme" (i.e., https://*/*) then we would
    // have to add a special case.
    EXPECT_FALSE(URLPatternTestRecordsHistogram(
        MakeURLPattern("https:"), KURL("https://example.com/login"),
        WebFeature::kURLPatternReliantOnImplicitURLComponentsInString));
    // Previously this meant exactly /, but now means any path. We might want to
    // exempt this and just force you to write /*.
    EXPECT_TRUE(URLPatternTestRecordsHistogram(
        MakeURLPattern("https://example.com"),
        KURL("https://example.com/login"),
        WebFeature::kURLPatternReliantOnImplicitURLComponentsInString));
  }
  {
    // This behavior is irrelevant for dictionary-constructed patterns.
    // They already use wildcards for unspecified components.
    URLPattern* pattern = MakeURLPattern({.pathname = "/login"});
    EXPECT_FALSE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/login?a=1"),
        WebFeature::kURLPatternReliantOnImplicitURLComponentsInString));
  }
}

TEST_F(URLPatternHistogramTest, BaseURLInheritance) {
  {
    URLPattern* pattern = MakeURLPattern(
        {.protocol = "https", .baseURL = "https://example.com/"});
    EXPECT_FALSE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/"),
        WebFeature::kURLPatternReliantOnLaterComponentFromBaseURL));
    EXPECT_TRUE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://another.test/"),
        WebFeature::kURLPatternReliantOnLaterComponentFromBaseURL));
    EXPECT_TRUE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/hello"),
        WebFeature::kURLPatternReliantOnLaterComponentFromBaseURL));
    EXPECT_TRUE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/?a=1"),
        WebFeature::kURLPatternReliantOnLaterComponentFromBaseURL));
    EXPECT_TRUE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/#hash"),
        WebFeature::kURLPatternReliantOnLaterComponentFromBaseURL));
    EXPECT_TRUE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://username@example.com/"),
        WebFeature::kURLPatternReliantOnLaterComponentFromBaseURL));
  }
  {
    URLPattern* pattern = MakeURLPattern(
        {.hostname = "example.com", .baseURL = "https://another.test/"});
    EXPECT_FALSE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/"),
        WebFeature::kURLPatternReliantOnLaterComponentFromBaseURL));
    EXPECT_FALSE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://another.test/"),
        WebFeature::kURLPatternReliantOnLaterComponentFromBaseURL));
    EXPECT_TRUE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/hello"),
        WebFeature::kURLPatternReliantOnLaterComponentFromBaseURL));
    EXPECT_TRUE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/?a=1"),
        WebFeature::kURLPatternReliantOnLaterComponentFromBaseURL));
    EXPECT_TRUE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/#hash"),
        WebFeature::kURLPatternReliantOnLaterComponentFromBaseURL));
    EXPECT_TRUE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://username@example.com/"),
        WebFeature::kURLPatternReliantOnLaterComponentFromBaseURL));
  }
  {
    // Interestingly, this case actually doesn't change, even if a password is
    // supplied, because the password already results in a wildcard if we never
    // encounter the : in the username:password pair.
    URLPattern* pattern = MakeURLPattern(
        {.username = "username", .baseURL = "https://example.com/"});
    EXPECT_FALSE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/"),
        WebFeature::kURLPatternReliantOnLaterComponentFromBaseURL));
    EXPECT_FALSE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://another.test/"),
        WebFeature::kURLPatternReliantOnLaterComponentFromBaseURL));
    EXPECT_FALSE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://username@example.com/"),
        WebFeature::kURLPatternReliantOnLaterComponentFromBaseURL));
    EXPECT_FALSE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://username:password@example.com/"),
        WebFeature::kURLPatternReliantOnLaterComponentFromBaseURL));
    EXPECT_FALSE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://root@example.com/"),
        WebFeature::kURLPatternReliantOnLaterComponentFromBaseURL));
  }
  {
    // Here, we would start matching even though the base URL contains search
    // not found in the URL.
    URLPattern* pattern = MakeURLPattern(
        {.pathname = "/login", .baseURL = "https://example.com/a/b?a=1"});
    EXPECT_FALSE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/"),
        WebFeature::kURLPatternReliantOnLaterComponentFromBaseURL));
    EXPECT_FALSE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://another.test/login"),
        WebFeature::kURLPatternReliantOnLaterComponentFromBaseURL));
    EXPECT_TRUE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/login"),
        WebFeature::kURLPatternReliantOnLaterComponentFromBaseURL));
    EXPECT_FALSE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/login?a=1"),
        WebFeature::kURLPatternReliantOnLaterComponentFromBaseURL));
    EXPECT_TRUE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/login?b=47"),
        WebFeature::kURLPatternReliantOnLaterComponentFromBaseURL));
    EXPECT_TRUE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/login?a=1#privacy"),
        WebFeature::kURLPatternReliantOnLaterComponentFromBaseURL));
  }
  {
    // Here, we would start matching even though the base URL contains hash
    // not found in the URL.
    URLPattern* pattern = MakeURLPattern(
        {.pathname = "/login", .baseURL = "https://example.com/a/b#privacy"});
    EXPECT_FALSE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/"),
        WebFeature::kURLPatternReliantOnLaterComponentFromBaseURL));
    EXPECT_FALSE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://another.test/login"),
        WebFeature::kURLPatternReliantOnLaterComponentFromBaseURL));
    EXPECT_TRUE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/login"),
        WebFeature::kURLPatternReliantOnLaterComponentFromBaseURL));
    EXPECT_FALSE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/login#privacy"),
        WebFeature::kURLPatternReliantOnLaterComponentFromBaseURL));
    EXPECT_TRUE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/login#notprivacy"),
        WebFeature::kURLPatternReliantOnLaterComponentFromBaseURL));
    EXPECT_TRUE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/login?a=1#privacy"),
        WebFeature::kURLPatternReliantOnLaterComponentFromBaseURL));
  }
  {
    // Here, we would start matching even though the base URL contains hash
    // not found in the URL.
    URLPattern* pattern = MakeURLPattern(
        {.search = "a=1", .baseURL = "https://example.com/login#privacy"});
    EXPECT_TRUE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/login?a=1"),
        WebFeature::kURLPatternReliantOnLaterComponentFromBaseURL));
    EXPECT_TRUE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/login?a=1#notprivacy"),
        WebFeature::kURLPatternReliantOnLaterComponentFromBaseURL));
    EXPECT_FALSE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/?a=1#privacy"),
        WebFeature::kURLPatternReliantOnLaterComponentFromBaseURL));
    EXPECT_FALSE(URLPatternTestRecordsHistogram(
        pattern, KURL("https://example.com/login?a=1#privacy"),
        WebFeature::kURLPatternReliantOnLaterComponentFromBaseURL));
  }
}

TEST_F(URLPatternHistogramTest, ChangedPatternInDocumentRule) {
  ScopedSpeculationRulesDocumentRulesForTest enable_document_rules(true);
  GetFrame().GetSettings()->SetScriptEnabled(true);
  SetBodyInnerHTML(R"(<a href="https://example.com/?a=1">example</a>)");
  auto* script = MakeGarbageCollected<HTMLScriptElement>(GetDocument(),
                                                         CreateElementFlags());
  script->setAttribute(html_names::kTypeAttr, AtomicString("speculationrules"));
  script->setTextContent(
      R"({"prefetch":[{"source":"document","where":{"href_matches": "https://example.com/*"}}]})");
  GetDocument().body()->appendChild(script);
  UpdateAllLifecyclePhasesForTest();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kURLPatternReliantOnImplicitURLComponentsInString));
}

}  // namespace
}  // namespace blink
