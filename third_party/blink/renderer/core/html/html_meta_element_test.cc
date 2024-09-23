// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_meta_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_computed_style_declaration.h"
#include "third_party/blink/renderer/core/css/media_query_list.h"
#include "third_party/blink/renderer/core/css/media_query_matcher.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
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
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
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
  HTMLMetaElement* CreateColorSchemeMeta(const char* content) {
    auto* meta = MakeGarbageCollected<HTMLMetaElement>(GetDocument(),
                                                       CreateElementFlags());
    meta->setAttribute(html_names::kNameAttr, keywords::kColorScheme);
    meta->setAttribute(html_names::kContentAttr, AtomicString(content));
    return meta;
  }

  void SetColorScheme(const char* content) {
    auto* meta = To<HTMLMetaElement>(GetDocument().head()->firstChild());
    ASSERT_TRUE(meta);
    meta->setAttribute(html_names::kContentAttr, AtomicString(content));
  }

  void ExpectPageColorSchemes(ColorSchemeFlags expected) const {
    EXPECT_EQ(expected, GetDocument().GetStyleEngine().GetPageColorSchemes());
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

// TODO(https://crbug.com/1430288) remove after data collected (end of '23)
TEST_F(HTMLMetaElementTest, ViewportFit_Auto_NotUseCounted) {
  EXPECT_EQ(mojom::ViewportFit::kAuto,
            LoadTestPageAndReturnViewportFit("auto"));
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kViewportFitContain));
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kViewportFitCover));
  // TODO(https://crbug.com/1430288) remove tracking this union of features
  // after data collected (end of '23)
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kViewportFitCoverOrSafeAreaInsetBottom));
}

TEST_F(HTMLMetaElementTest, ViewportFit_Contain_IsUseCounted) {
  EXPECT_EQ(mojom::ViewportFit::kContain,
            LoadTestPageAndReturnViewportFit("contain"));
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kViewportFitCover));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kViewportFitCoverOrSafeAreaInsetBottom));
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kViewportFitContain));
}

// TODO(https://crbug.com/1430288) remove after data collected (end of '23)
TEST_F(HTMLMetaElementTest, ViewportFit_Cover_IsUseCounted) {
  EXPECT_EQ(mojom::ViewportFit::kCover,
            LoadTestPageAndReturnViewportFit("cover"));
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kViewportFitCover));
  // TODO(https://crbug.com/1430288) remove tracking this union of features
  // after data collected (end of '23)
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kViewportFitCoverOrSafeAreaInsetBottom));
}

TEST_F(HTMLMetaElementTest, ColorSchemeProcessing_FirstWins) {
  GetDocument().head()->setInnerHTML(R"HTML(
    <meta name="color-scheme" content="dark">
    <meta name="color-scheme" content="light">
  )HTML");

  ExpectPageColorSchemes(static_cast<ColorSchemeFlags>(ColorSchemeFlag::kDark));
}

TEST_F(HTMLMetaElementTest, ColorSchemeProcessing_Remove) {
  GetDocument().head()->setInnerHTML(R"HTML(
    <meta id="first-meta" name="color-scheme" content="dark">
    <meta name="color-scheme" content="light">
  )HTML");

  ExpectPageColorSchemes(static_cast<ColorSchemeFlags>(ColorSchemeFlag::kDark));

  GetDocument().getElementById(AtomicString("first-meta"))->remove();

  ExpectPageColorSchemes(
      static_cast<ColorSchemeFlags>(ColorSchemeFlag::kLight));
}

TEST_F(HTMLMetaElementTest, ColorSchemeProcessing_InsertBefore) {
  GetDocument().head()->setInnerHTML(R"HTML(
    <meta name="color-scheme" content="dark">
  )HTML");

  ExpectPageColorSchemes(static_cast<ColorSchemeFlags>(ColorSchemeFlag::kDark));

  Element* head = GetDocument().head();
  head->insertBefore(CreateColorSchemeMeta("light"), head->firstChild());

  ExpectPageColorSchemes(
      static_cast<ColorSchemeFlags>(ColorSchemeFlag::kLight));
}

TEST_F(HTMLMetaElementTest, ColorSchemeProcessing_AppendChild) {
  GetDocument().head()->setInnerHTML(R"HTML(
    <meta name="color-scheme" content="dark">
  )HTML");

  ExpectPageColorSchemes(static_cast<ColorSchemeFlags>(ColorSchemeFlag::kDark));

  GetDocument().head()->AppendChild(CreateColorSchemeMeta("light"));

  ExpectPageColorSchemes(static_cast<ColorSchemeFlags>(ColorSchemeFlag::kDark));
}

TEST_F(HTMLMetaElementTest, ColorSchemeProcessing_SetAttribute) {
  GetDocument().head()->setInnerHTML(R"HTML(
    <meta id="meta" name="color-scheme" content="dark">
  )HTML");

  ExpectPageColorSchemes(static_cast<ColorSchemeFlags>(ColorSchemeFlag::kDark));

  GetDocument()
      .getElementById(AtomicString("meta"))
      ->setAttribute(html_names::kContentAttr, AtomicString("light"));

  ExpectPageColorSchemes(
      static_cast<ColorSchemeFlags>(ColorSchemeFlag::kLight));
}

TEST_F(HTMLMetaElementTest, ColorSchemeProcessing_RemoveContentAttribute) {
  GetDocument().head()->setInnerHTML(R"HTML(
    <meta id="meta" name="color-scheme" content="dark">
  )HTML");

  ExpectPageColorSchemes(static_cast<ColorSchemeFlags>(ColorSchemeFlag::kDark));

  GetDocument()
      .getElementById(AtomicString("meta"))
      ->removeAttribute(html_names::kContentAttr);

  ExpectPageColorSchemes(
      static_cast<ColorSchemeFlags>(ColorSchemeFlag::kNormal));
}

TEST_F(HTMLMetaElementTest, ColorSchemeProcessing_RemoveNameAttribute) {
  GetDocument().head()->setInnerHTML(R"HTML(
    <meta id="meta" name="color-scheme" content="dark">
  )HTML");

  ExpectPageColorSchemes(static_cast<ColorSchemeFlags>(ColorSchemeFlag::kDark));

  GetDocument()
      .getElementById(AtomicString("meta"))
      ->removeAttribute(html_names::kNameAttr);

  ExpectPageColorSchemes(
      static_cast<ColorSchemeFlags>(ColorSchemeFlag::kNormal));
}

TEST_F(HTMLMetaElementTest, ColorSchemeParsing) {
  GetDocument().head()->AppendChild(CreateColorSchemeMeta(""));

  SetColorScheme("");
  ExpectPageColorSchemes(
      static_cast<ColorSchemeFlags>(ColorSchemeFlag::kNormal));

  SetColorScheme("normal");
  ExpectPageColorSchemes(
      static_cast<ColorSchemeFlags>(ColorSchemeFlag::kNormal));

  SetColorScheme("light");
  ExpectPageColorSchemes(
      static_cast<ColorSchemeFlags>(ColorSchemeFlag::kLight));

  SetColorScheme("dark");
  ExpectPageColorSchemes(static_cast<ColorSchemeFlags>(ColorSchemeFlag::kDark));

  SetColorScheme("light dark");
  ExpectPageColorSchemes(
      static_cast<ColorSchemeFlags>(ColorSchemeFlag::kLight) |
      static_cast<ColorSchemeFlags>(ColorSchemeFlag::kDark));

  SetColorScheme(" BLUE  light   ");
  ExpectPageColorSchemes(
      static_cast<ColorSchemeFlags>(ColorSchemeFlag::kLight));

  SetColorScheme("light,dark");
  ExpectPageColorSchemes(
      static_cast<ColorSchemeFlags>(ColorSchemeFlag::kNormal));

  SetColorScheme("light,");
  ExpectPageColorSchemes(
      static_cast<ColorSchemeFlags>(ColorSchemeFlag::kNormal));

  SetColorScheme(",light");
  ExpectPageColorSchemes(
      static_cast<ColorSchemeFlags>(ColorSchemeFlag::kNormal));

  SetColorScheme(", light");
  ExpectPageColorSchemes(
      static_cast<ColorSchemeFlags>(ColorSchemeFlag::kNormal));

  SetColorScheme("light, dark");
  ExpectPageColorSchemes(
      static_cast<ColorSchemeFlags>(ColorSchemeFlag::kNormal));

  SetColorScheme("only");
  ExpectPageColorSchemes(
      static_cast<ColorSchemeFlags>(ColorSchemeFlag::kNormal));

  SetColorScheme("only light");
  ExpectPageColorSchemes(
      static_cast<ColorSchemeFlags>(ColorSchemeFlag::kOnly) |
      static_cast<ColorSchemeFlags>(ColorSchemeFlag::kLight));
}

TEST_F(HTMLMetaElementTest, ColorSchemeForcedDarkeningAndMQ) {
  ColorSchemeHelper color_scheme_helper(GetDocument());
  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kDark);

  auto* media_query = GetDocument().GetMediaQueryMatcher().MatchMedia(
      "(prefers-color-scheme: dark)");
  EXPECT_TRUE(media_query->matches());
  GetDocument().GetSettings()->SetForceDarkModeEnabled(true);
  EXPECT_TRUE(media_query->matches());

  GetDocument().head()->AppendChild(CreateColorSchemeMeta("light"));
  EXPECT_TRUE(media_query->matches());

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
            GetFrame().DomWindow()->GetReferrerPolicy());
  EXPECT_EQ(network::mojom::ReferrerPolicy::kStrictOrigin,
            GetFrame().DomWindow()->GetPolicyContainer()->GetReferrerPolicy());
}

TEST_F(HTMLMetaElementTest, ReferrerPolicyUpdatesPolicyContainer) {
  GetDocument().head()->setInnerHTML(R"HTML(
    <meta name="referrer" content="strict-origin">
  )HTML");
  EXPECT_EQ(network::mojom::ReferrerPolicy::kStrictOrigin,
            GetFrame().DomWindow()->GetReferrerPolicy());
  EXPECT_EQ(network::mojom::ReferrerPolicy::kStrictOrigin,
            GetFrame().DomWindow()->GetPolicyContainer()->GetReferrerPolicy());
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
