// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_link_element.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
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
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<head>"
      "<link rel=\"icon\" type=\"image/ico\" href=\"\" />"
      "</head>");
  auto* link_element = To<HTMLLinkElement>(GetDocument().head()->firstChild());
  EXPECT_EQ(NullURL(), link_element->Href());
}

// This tests whether Web Monetization counter is properly triggered.
TEST_F(HTMLLinkElementTest, WebMonetizationCounter) {
  // A <link rel="icon"> is not counted.
  GetDocument().head()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <link rel="icon" type="image/ico" href="">
  )HTML");
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kHTMLLinkElementMonetization));

  // A <link rel="monetization"> is counted.
  GetDocument().head()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
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
  GetDocument().head()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <link rel="icon" type="image/ico" href="">
  )HTML");
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kLinkRelCanonical));

  // A <link rel="canonoical"> is counted.
  GetDocument().head()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
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
  GetDocument().head()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <link rel="not-privacy-policy" href="/">
  )HTML");
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kLinkRelPrivacyPolicy));

  // <link rel="privacy-policy"> is counted when present.
  GetDocument().head()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <link rel="privacy-policy" href="/">
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kLinkRelPrivacyPolicy));
}

// This tests whether `rel=terms-of-service` is properly counted.
TEST_F(HTMLLinkElementTest, TermsOfServiceCounter) {
  // <link rel="terms-of-service"> is not counted when absent
  GetDocument().head()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <link rel="not-terms-of-service" href="/">
  )HTML");
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kLinkRelTermsOfService));

  // <link rel="terms-of-service"> is counted when present.
  GetDocument().head()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <link rel="terms-of-service" href="/">
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kLinkRelTermsOfService));
}

// This tests whether `rel=facilitated-payment` is properly counted.
TEST_F(HTMLLinkElementTest, PaymentCounter) {
  // <link rel="facilitated-payment"> is not counted when absent.
  GetDocument().head()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <link rel="not-facilitated-payment" href="https://example.com/">
  )HTML");
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kLinkRelFacilitatedPayment));

  // <link rel="facilitated-payment"> is not counted when <link rel="payment">
  // is present.
  GetDocument().head()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <link rel="payment" href="https://example.com/">
  )HTML");
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kLinkRelFacilitatedPayment));

  // <link rel="facilitated-payment"> is counted when present.
  GetDocument().head()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <link rel="facilitated-payment" href="https://example.com/">
  )HTML");
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kLinkRelFacilitatedPayment));
}

#if BUILDFLAG(IS_ANDROID)
// Tests that the payment link is handled when the 'rel' and 'href' attributes
// are set by order before appending the element to the document.
TEST_F(HTMLLinkElementTest, PaymentLinkHandledWhenRelAndHrefSetBeforeAppend) {
  // Ensures Document::HandlePaymentLink is not invoked initially.
  EXPECT_FALSE(GetDocument().payment_link_handled_);

  auto* link_element = MakeGarbageCollected<HTMLLinkElement>(
      GetDocument(), CreateElementFlags());
  link_element->setAttribute(html_names::kRelAttr,
                             AtomicString("facilitated-payment"));
  link_element->setAttribute(html_names::kHrefAttr,
                             AtomicString("https://example.com/"));
  GetDocument().head()->appendChild(link_element);

  // Ensures Document::HandlePaymentLink is invoked.
  EXPECT_TRUE(GetDocument().payment_link_handled_);
}

// Tests that the payment link is handled when the 'href' and 'rel' attributes
// are set by order before appending the element to the document.
TEST_F(HTMLLinkElementTest, PaymentLinkHandledWhenHrefAndRelSetBeforeAppend) {
  // Ensures Document::HandlePaymentLink is not invoked initially.
  EXPECT_FALSE(GetDocument().payment_link_handled_);

  auto* link_element = MakeGarbageCollected<HTMLLinkElement>(
      GetDocument(), CreateElementFlags());
  link_element->setAttribute(html_names::kHrefAttr,
                             AtomicString("https://example.com/"));
  link_element->setAttribute(html_names::kRelAttr,
                             AtomicString("facilitated-payment"));
  GetDocument().head()->appendChild(link_element);

  // Ensures Document::HandlePaymentLink is invoked.
  EXPECT_TRUE(GetDocument().payment_link_handled_);
}

// Tests that the payment link is handled when the 'rel' and 'href' attributes
// are set by order after appending the element to the document.
TEST_F(HTMLLinkElementTest, PaymentLinkHandledWhenRelAndHrefSetAfterAppend) {
  // Ensures Document::HandlePaymentLink is not invoked initially.
  EXPECT_FALSE(GetDocument().payment_link_handled_);

  auto* link_element = MakeGarbageCollected<HTMLLinkElement>(
      GetDocument(), CreateElementFlags());
  GetDocument().head()->appendChild(link_element);
  link_element->setAttribute(html_names::kRelAttr,
                             AtomicString("facilitated-payment"));
  link_element->setAttribute(html_names::kHrefAttr,
                             AtomicString("https://example.com/"));

  // Ensures Document::HandlePaymentLink is invoked.
  EXPECT_TRUE(GetDocument().payment_link_handled_);
}

// Tests that the payment link is handled when the 'href' and 'rel' attributes
// are set by order after appending the element to the document.
TEST_F(HTMLLinkElementTest, PaymentLinkHandledWhenHrefAndRelSetAfterAppend) {
  // Ensures Document::HandlePaymentLink is not invoked initially.
  EXPECT_FALSE(GetDocument().payment_link_handled_);

  auto* link_element = MakeGarbageCollected<HTMLLinkElement>(
      GetDocument(), CreateElementFlags());
  GetDocument().head()->appendChild(link_element);
  link_element->setAttribute(html_names::kHrefAttr,
                             AtomicString("https://example.com/"));
  link_element->setAttribute(html_names::kRelAttr,
                             AtomicString("facilitated-payment"));

  // Ensures Document::HandlePaymentLink is invoked.
  EXPECT_TRUE(GetDocument().payment_link_handled_);
}

// Tests that the payment link is not handled when the 'rel' attribute is not
// set.
TEST_F(HTMLLinkElementTest, PaymentLinkNotHandledWhenRelNotSet) {
  // Ensures Document::HandlePaymentLink is not invoked initially.
  EXPECT_FALSE(GetDocument().payment_link_handled_);

  auto* link_element = MakeGarbageCollected<HTMLLinkElement>(
      GetDocument(), CreateElementFlags());
  link_element->setAttribute(html_names::kHrefAttr,
                             AtomicString("https://example.com/"));
  GetDocument().head()->appendChild(link_element);

  // Ensures Document::HandlePaymentLink is not invoked.
  EXPECT_FALSE(GetDocument().payment_link_handled_);
}

// Tests that the payment link is not handled when the 'href' attribute is not
// set.
TEST_F(HTMLLinkElementTest, PaymentLinkNotHandledWhenHrefNotSet) {
  // Ensures Document::HandlePaymentLink is not invoked initially.
  EXPECT_FALSE(GetDocument().payment_link_handled_);

  auto* link_element = MakeGarbageCollected<HTMLLinkElement>(
      GetDocument(), CreateElementFlags());
  link_element->setAttribute(html_names::kRelAttr,
                             AtomicString("facilitated-payment"));
  GetDocument().head()->appendChild(link_element);

  // Ensures Document::HandlePaymentLink is not invoked.
  EXPECT_FALSE(GetDocument().payment_link_handled_);
}

// Tests that the payment link is not handled when the element is not appended
// to the document.
TEST_F(HTMLLinkElementTest, PaymentLinkNotHandledWhenNotAppended) {
  // Ensures Document::HandlePaymentLink is not invoked initially.
  EXPECT_FALSE(GetDocument().payment_link_handled_);

  auto* link_element = MakeGarbageCollected<HTMLLinkElement>(
      GetDocument(), CreateElementFlags());
  link_element->setAttribute(html_names::kRelAttr,
                             AtomicString("facilitated-payment"));
  link_element->setAttribute(html_names::kHrefAttr,
                             AtomicString("https://example.com/"));

  // Ensures Document::HandlePaymentLink is not invoked.
  EXPECT_FALSE(GetDocument().payment_link_handled_);
}

// Tests that the payment link is not handled when the element is in an iFrame.
TEST_F(HTMLLinkElementSimTest, PaymentLinkNotHandledWhenNotInTheMainFrame) {
  // Ensures Document::HandlePaymentLink is not invoked initially.
  EXPECT_FALSE(GetDocument().payment_link_handled_);

  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest child_frame_resource("https://example.com/subframe.html",
                                  "text/html");

  LoadURL("https://example.com/");

  main_resource.Complete(
      R"HTML(
        <body>
          <iframe src='https://example.com/subframe.html'></iframe>
        </body>)HTML");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  child_frame_resource.Complete(R"HTML(
    <link rel="facilitated-payment" href='https://paymentlinkexample.com/'>
  )HTML");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  auto* iframe_element = DynamicTo<HTMLIFrameElement>(
      GetDocument().QuerySelector(AtomicString("iframe")));
  ASSERT_NE(iframe_element, nullptr);
  auto* link_element = To<HTMLLinkElement>(
      iframe_element->contentDocument()->head()->firstChild());
  ASSERT_NE(link_element, nullptr);
  ASSERT_EQ(link_element->FastGetAttribute(html_names::kRelAttr),
            AtomicString("facilitated-payment"));

  // Ensures Document::HandlePaymentLink is not invoked.
  EXPECT_FALSE(GetDocument().payment_link_handled_);
}
#endif

}  // namespace blink
