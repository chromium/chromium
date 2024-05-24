// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_link_element.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class HTMLLinkElementTest : public PageTestBase {};
class HTMLLinkElementSimTest : public SimTest {};

// This tests that we should ignore empty string value
// in href attribute value of the link element.
TEST_F(HTMLLinkElementTest, EmptyHrefAttribute) {
  GetDocument().documentElement()->setInnerHTML(
      "<head>"
      "<link rel=\"icon\" type=\"image/ico\" href=\"\" />"
      "</head>");
  auto* link_element = To<HTMLLinkElement>(GetDocument().head()->firstChild());
  EXPECT_EQ(NullURL(), link_element->Href());
}

// This tests whether Web Monetization counter is properly triggered.
TEST_F(HTMLLinkElementTest, WebMonetizationCounter) {
  // A <link rel="icon"> is not counted.
  GetDocument().head()->setInnerHTML(R"HTML(
    <link rel="icon" type="image/ico" href="">
  )HTML");
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kHTMLLinkElementMonetization));

  // A <link rel="monetization"> is counted.
  GetDocument().head()->setInnerHTML(R"HTML(
    <link rel="monetization">
  )HTML");
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kHTMLLinkElementMonetization));

  // However, it does not affect the counter for <meta name="monetization">.
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kHTMLMetaElementMonetization));
}

TEST_F(HTMLLinkElementSimTest, WebMonetizationNotCountedInSubFrame) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest child_frame_resource("https://example.com/subframe.html",
                                  "text/html");

  LoadURL("https://example.com/");

  main_resource.Complete(
      R"HTML(
        <body onload='console.log("main body onload");'>
          <iframe src='https://example.com/subframe.html'
                  onload='console.log("child frame element onload");'></iframe>
        </body>)HTML");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  child_frame_resource.Complete(R"HTML(
    <link rel="monetization">
  )HTML");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // Ensure that main frame and subframe are loaded before checking the counter.
  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("child frame element onload"));

  // <link rel="monetization"> is not counted in subframes.
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kHTMLLinkElementMonetization));
}

// This tests whether the Canonical counter is properly triggered.
TEST_F(HTMLLinkElementTest, CanonicalCounter) {
  // A <link rel="icon"> is not counted.
  GetDocument().head()->setInnerHTML(R"HTML(
    <link rel="icon" type="image/ico" href="">
  )HTML");
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kLinkRelCanonical));

  // A <link rel="canonoical"> is counted.
  GetDocument().head()->setInnerHTML(R"HTML(
    <link rel="canonical" href="">
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kLinkRelCanonical));
}

TEST_F(HTMLLinkElementSimTest, CanonicalNotCountedInSubFrame) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest child_frame_resource("https://example.com/subframe.html",
                                  "text/html");

  LoadURL("https://example.com/");

  main_resource.Complete(
      R"HTML(
        <body onload='console.log("main body onload");'>
          <iframe src='https://example.com/subframe.html'
                  onload='console.log("child frame element onload");'></iframe>
        </body>)HTML");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  child_frame_resource.Complete(R"HTML(
    <link rel="canonical" href="">
  )HTML");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // Ensure that main frame and subframe are loaded before checking the counter.
  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("child frame element onload"));

  // <link rel="canonical"> is not counted in subframes.
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kLinkRelCanonical));
}

// This tests whether `rel=privacy-policy` is properly counted.
TEST_F(HTMLLinkElementTest, PrivacyPolicyCounter) {
  // <link rel="privacy-policy"> is not counted when absent
  GetDocument().head()->setInnerHTML(R"HTML(
    <link rel="not-privacy-policy" href="/">
  )HTML");
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kLinkRelPrivacyPolicy));

  // <link rel="privacy-policy"> is counted when present.
  GetDocument().head()->setInnerHTML(R"HTML(
    <link rel="privacy-policy" href="/">
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kLinkRelPrivacyPolicy));
}

// This tests whether `rel=terms-of-service` is properly counted.
TEST_F(HTMLLinkElementTest, TermsOfServiceCounter) {
  // <link rel="terms-of-service"> is not counted when absent
  GetDocument().head()->setInnerHTML(R"HTML(
    <link rel="not-terms-of-service" href="/">
  )HTML");
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kLinkRelTermsOfService));

  // <link rel="terms-of-service"> is counted when present.
  GetDocument().head()->setInnerHTML(R"HTML(
    <link rel="terms-of-service" href="/">
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kLinkRelTermsOfService));
}

// This tests whether `rel=payment` is properly counted.
TEST_F(HTMLLinkElementTest, PaymentCounter) {
  // <link rel="payment"> is not counted when absent.
  GetDocument().head()->setInnerHTML(R"HTML(
    <link rel="not-payment" href="/">
  )HTML");
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kLinkRelPayment));

  // <link rel="payment"> is counted when present.
  GetDocument().head()->setInnerHTML(R"HTML(
    <link rel="payment" href="/">
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kLinkRelPayment));
}

}  // namespace blink
