// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_anchor_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {
namespace {

using HTMLAnchorElementTest = PageTestBase;

TEST_F(HTMLAnchorElementTest, UnchangedHrefDoesNotInvalidateStyle) {
  SetBodyInnerHTML("<a href=\"https://www.chromium.org/\">Chromium</a>");
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());

  auto* anchor =
      To<HTMLAnchorElement>(GetDocument().QuerySelector(AtomicString("a")));
  anchor->setAttribute(html_names::kHrefAttr,
                       AtomicString("https://www.chromium.org/"));
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());
}

// This tests whether `rel=privacy-policy` is properly counted.
TEST_F(HTMLAnchorElementTest, PrivacyPolicyCounter) {
  // <a rel="privacy-policy"> is not counted when absent
  SetBodyInnerHTML(R"HTML(
    <a rel="not-privacy-policy" href="/">Test</a>
  )HTML");
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kLinkRelPrivacyPolicy));

  // <a rel="privacy-policy"> is counted when present.
  SetBodyInnerHTML(R"HTML(
    <a rel="privacy-policy" href="/">Test</a>
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kLinkRelPrivacyPolicy));
}

// This tests whether `rel=terms-of-service` is properly counted.
TEST_F(HTMLAnchorElementTest, TermsOfServiceCounter) {
  // <a rel="terms-of-service"> is not counted when absent
  SetBodyInnerHTML(R"HTML(
    <a rel="not-terms-of-service" href="/">Test</a>
  )HTML");
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kLinkRelTermsOfService));

  // <a rel="terms-of-service"> is counted when present.
  SetBodyInnerHTML(R"HTML(
    <a rel="terms-of-service" href="/">Test</a>
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kLinkRelTermsOfService));
}

}  // namespace
}  // namespace blink
