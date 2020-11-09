// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_meta_element.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/css/css_computed_style_declaration.h"
#include "third_party/blink/renderer/core/css/media_query_list.h"
#include "third_party/blink/renderer/core/css/media_query_matcher.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/color_scheme_helper.h"
#include "third_party/blink/renderer/core/testing/mock_policy_container_host.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class HTMLMetaElementTest : public PageTestBase,
                            private ScopedDisplayCutoutAPIForTest {
 public:
  HTMLMetaElementTest() : ScopedDisplayCutoutAPIForTest(true) {}
  void SetUp() override {
    PageTestBase::SetUp();
    GetDocument().GetSettings()->SetViewportMetaEnabled(true);
  }

  mojom::ViewportFit LoadTestPageAndReturnViewportFit(const String& value) {
    LoadTestPageWithViewportFitValue(value);
    return GetDocument()
        .GetViewportData()
        .GetViewportDescription()
        .GetViewportFit();
  }

 protected:
  HTMLMetaElement* CreateColorSchemeMeta(const AtomicString& content) {
    auto* meta = MakeGarbageCollected<HTMLMetaElement>(GetDocument());
    meta->setAttribute(html_names::kNameAttr, "color-scheme");
    meta->setAttribute(html_names::kContentAttr, content);
    return meta;
  }

  void SetColorScheme(const AtomicString& content) {
    auto* meta = To<HTMLMetaElement>(GetDocument().head()->firstChild());
    ASSERT_TRUE(meta);
    meta->setAttribute(html_names::kContentAttr, content);
  }

  void ExpectComputedColorScheme(const String& expected) const {
    auto* computed = MakeGarbageCollected<CSSComputedStyleDeclaration>(
        GetDocument().documentElement());
    EXPECT_EQ(expected,
              computed->GetPropertyValue(CSSPropertyID::kColorScheme));
  }

 private:
  void LoadTestPageWithViewportFitValue(const String& value) {
    GetDocument().documentElement()->setInnerHTML(
        "<head>"
        "<meta name='viewport' content='viewport-fit=" +
        value +
        "'>"
        "</head>");
  }
};
class HTMLMetaElementSimTest : public SimTest {};

TEST_F(HTMLMetaElementTest, ViewportFit_Auto) {
  EXPECT_EQ(mojom::ViewportFit::kAuto,
            LoadTestPageAndReturnViewportFit("auto"));
}

TEST_F(HTMLMetaElementTest, ViewportFit_Contain) {
  EXPECT_EQ(mojom::ViewportFit::kContain,
            LoadTestPageAndReturnViewportFit("contain"));
}

TEST_F(HTMLMetaElementTest, ViewportFit_Cover) {
  EXPECT_EQ(mojom::ViewportFit::kCover,
            LoadTestPageAndReturnViewportFit("cover"));
}

TEST_F(HTMLMetaElementTest, ViewportFit_Invalid) {
  EXPECT_EQ(mojom::ViewportFit::kAuto,
            LoadTestPageAndReturnViewportFit("invalid"));
}

TEST_F(HTMLMetaElementTest, ColorSchemeProcessing_FirstWins) {
  GetDocument().head()->setInnerHTML(R"HTML(
    <meta name="color-scheme" content="dark">
    <meta name="color-scheme" content="light">
  )HTML");

  ExpectComputedColorScheme("dark");
}

TEST_F(HTMLMetaElementTest, ColorSchemeProcessing_Remove) {
  GetDocument().head()->setInnerHTML(R"HTML(
    <meta id="first-meta" name="color-scheme" content="dark">
    <meta name="color-scheme" content="light">
  )HTML");

  ExpectComputedColorScheme("dark");

  GetDocument().getElementById("first-meta")->remove();

  ExpectComputedColorScheme("light");
}

TEST_F(HTMLMetaElementTest, ColorSchemeProcessing_InsertBefore) {
  GetDocument().head()->setInnerHTML(R"HTML(
    <meta name="color-scheme" content="dark">
  )HTML");

  ExpectComputedColorScheme("dark");

  Element* head = GetDocument().head();
  head->insertBefore(CreateColorSchemeMeta("light"), head->firstChild());

  ExpectComputedColorScheme("light");
}

TEST_F(HTMLMetaElementTest, ColorSchemeProcessing_AppendChild) {
  GetDocument().head()->setInnerHTML(R"HTML(
    <meta name="color-scheme" content="dark">
  )HTML");

  ExpectComputedColorScheme("dark");

  GetDocument().head()->AppendChild(CreateColorSchemeMeta("light"));

  ExpectComputedColorScheme("dark");
}

TEST_F(HTMLMetaElementTest, ColorSchemeProcessing_SetAttribute) {
  GetDocument().head()->setInnerHTML(R"HTML(
    <meta id="meta" name="color-scheme" content="dark">
  )HTML");

  ExpectComputedColorScheme("dark");

  GetDocument().getElementById("meta")->setAttribute(html_names::kContentAttr,
                                                     "light");

  ExpectComputedColorScheme("light");
}

TEST_F(HTMLMetaElementTest, ColorSchemeProcessing_RemoveContentAttribute) {
  GetDocument().head()->setInnerHTML(R"HTML(
    <meta id="meta" name="color-scheme" content="dark">
  )HTML");

  ExpectComputedColorScheme("dark");

  GetDocument().getElementById("meta")->removeAttribute(
      html_names::kContentAttr);

  ExpectComputedColorScheme("normal");
}

TEST_F(HTMLMetaElementTest, ColorSchemeProcessing_RemoveNameAttribute) {
  GetDocument().head()->setInnerHTML(R"HTML(
    <meta id="meta" name="color-scheme" content="dark">
  )HTML");

  ExpectComputedColorScheme("dark");

  GetDocument().getElementById("meta")->removeAttribute(html_names::kNameAttr);

  ExpectComputedColorScheme("normal");
}

TEST_F(HTMLMetaElementTest, ColorSchemeParsing) {
  GetDocument().head()->AppendChild(CreateColorSchemeMeta(""));

  SetColorScheme("");
  ExpectComputedColorScheme("normal");

  SetColorScheme("normal");
  ExpectComputedColorScheme("normal");

  SetColorScheme("light");
  ExpectComputedColorScheme("light");

  SetColorScheme("dark");
  ExpectComputedColorScheme("dark");

  SetColorScheme("light dark");
  ExpectComputedColorScheme("light dark");

  SetColorScheme(" BLUE  light   ");
  ExpectComputedColorScheme("BLUE light");

  SetColorScheme("light,dark");
  ExpectComputedColorScheme("normal");

  SetColorScheme("light,");
  ExpectComputedColorScheme("normal");

  SetColorScheme(",light");
  ExpectComputedColorScheme("normal");

  SetColorScheme(", light");
  ExpectComputedColorScheme("normal");

  SetColorScheme("light, dark");
  ExpectComputedColorScheme("normal");
}

TEST_F(HTMLMetaElementTest, ColorSchemeForcedDarkeningAndMQ) {
  ColorSchemeHelper color_scheme_helper(GetDocument());
  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kDark);

  auto* media_query = GetDocument().GetMediaQueryMatcher().MatchMedia(
      "(prefers-color-scheme: dark)");
  EXPECT_TRUE(media_query->matches());
  GetDocument().GetSettings()->SetForceDarkModeEnabled(true);
  EXPECT_FALSE(media_query->matches());

  GetDocument().head()->AppendChild(CreateColorSchemeMeta("light"));
  EXPECT_FALSE(media_query->matches());

  SetColorScheme("dark");
  EXPECT_TRUE(media_query->matches());

  SetColorScheme("light dark");
  EXPECT_TRUE(media_query->matches());
}

TEST_F(HTMLMetaElementTest, ReferrerPolicyWithoutContent) {
  GetDocument().head()->setInnerHTML(R"HTML(
    <meta name="referrer" content="strict-origin">
    <meta name="referrer" >
  )HTML");
  EXPECT_EQ(network::mojom::ReferrerPolicy::kStrictOrigin,
            GetDocument().GetReferrerPolicy());
}

TEST_F(HTMLMetaElementTest, ReferrerPolicyUpdatesPolicyContainer) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(blink::features::kPolicyContainer);

  MockPolicyContainerHost policy_container_host;
  mojo::PendingAssociatedRemote<mojom::blink::PolicyContainerHost>
      stub_policy_container_remote =
          policy_container_host.BindNewEndpointAndPassDedicatedRemote();
  auto policy_container = std::make_unique<PolicyContainer>(
      std::move(stub_policy_container_remote),
      mojom::blink::PolicyContainerDocumentPolicies::New());

  GetFrame().SetPolicyContainer(std::move(policy_container));
  EXPECT_CALL(policy_container_host,
              SetReferrerPolicy(network::mojom::ReferrerPolicy::kStrictOrigin));
  GetDocument().head()->setInnerHTML(R"HTML(
    <meta name="referrer" content="strict-origin">
  )HTML");
  EXPECT_EQ(network::mojom::ReferrerPolicy::kStrictOrigin,
            GetFrame().GetPolicyContainer()->GetReferrerPolicy());

  // Wait for mojo messages to be received.
  policy_container_host.FlushForTesting();
}

// This tests whether Web Monetization counter is properly triggered.
TEST_F(HTMLMetaElementTest, WebMonetizationCounter) {
  // <meta> elements that don't have name equal to "monetization" or that lack
  // a content attribute are not counted.
  GetDocument().head()->setInnerHTML(R"HTML(
    <meta name="color-scheme" content="dark">
    <meta name="monetization">
  )HTML");
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kHTMLMetaElementMonetization));

  // A <link rel="monetization"> with a content attribute is counted.
  GetDocument().head()->setInnerHTML(R"HTML(
    <meta name="monetization" content="$payment.pointer.url">
  )HTML");
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kHTMLMetaElementMonetization));

  // However, it does not affect the counter for <link rel="monetization">.
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kHTMLLinkElementMonetization));
}

TEST_F(HTMLMetaElementSimTest, WebMonetizationNotCountedInSubFrame) {
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
    <meta name="monetization" content="$payment.pointer.url">
  )HTML");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // Ensure that main frame and subframe are loaded before checking the counter.
  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("child frame element onload"));

  // <meta name="monetization"> is not counted in subframes.
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kHTMLMetaElementMonetization));
}

}  // namespace blink
