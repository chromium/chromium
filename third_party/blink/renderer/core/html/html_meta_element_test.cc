// Copyright 2018 The Chromium Authors. All rights reserved.
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
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/color_scheme_helper.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class HTMLMetaElementTest : public PageTestBase,
                            private ScopedDisplayCutoutAPIForTest,
                            private ScopedMetaColorSchemeForTest,
                            private ScopedMediaQueryPrefersColorSchemeForTest,
                            private ScopedCSSColorSchemeForTest {
 public:
  HTMLMetaElementTest()
      : ScopedDisplayCutoutAPIForTest(true),
        ScopedMetaColorSchemeForTest(true),
        ScopedMediaQueryPrefersColorSchemeForTest(true),
        ScopedCSSColorSchemeForTest(true) {}
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
    GetDocument().documentElement()->SetInnerHTMLFromString(
        "<head>"
        "<meta name='viewport' content='viewport-fit=" +
        value +
        "'>"
        "</head>");
  }
};

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
  GetDocument().head()->SetInnerHTMLFromString(R"HTML(
    <meta name="color-scheme" content="dark">
    <meta name="color-scheme" content="light">
  )HTML");

  ExpectComputedColorScheme("dark");
}

TEST_F(HTMLMetaElementTest, ColorSchemeProcessing_Remove) {
  GetDocument().head()->SetInnerHTMLFromString(R"HTML(
    <meta id="first-meta" name="color-scheme" content="dark">
    <meta name="color-scheme" content="light">
  )HTML");

  ExpectComputedColorScheme("dark");

  GetDocument().getElementById("first-meta")->remove();

  ExpectComputedColorScheme("light");
}

TEST_F(HTMLMetaElementTest, ColorSchemeProcessing_InsertBefore) {
  GetDocument().head()->SetInnerHTMLFromString(R"HTML(
    <meta name="color-scheme" content="dark">
  )HTML");

  ExpectComputedColorScheme("dark");

  Element* head = GetDocument().head();
  head->insertBefore(CreateColorSchemeMeta("light"), head->firstChild());

  ExpectComputedColorScheme("light");
}

TEST_F(HTMLMetaElementTest, ColorSchemeProcessing_AppendChild) {
  GetDocument().head()->SetInnerHTMLFromString(R"HTML(
    <meta name="color-scheme" content="dark">
  )HTML");

  ExpectComputedColorScheme("dark");

  GetDocument().head()->AppendChild(CreateColorSchemeMeta("light"));

  ExpectComputedColorScheme("dark");
}

TEST_F(HTMLMetaElementTest, ColorSchemeProcessing_SetAttribute) {
  GetDocument().head()->SetInnerHTMLFromString(R"HTML(
    <meta id="meta" name="color-scheme" content="dark">
  )HTML");

  ExpectComputedColorScheme("dark");

  GetDocument().getElementById("meta")->setAttribute(html_names::kContentAttr,
                                                     "light");

  ExpectComputedColorScheme("light");
}

TEST_F(HTMLMetaElementTest, ColorSchemeProcessing_RemoveContentAttribute) {
  GetDocument().head()->SetInnerHTMLFromString(R"HTML(
    <meta id="meta" name="color-scheme" content="dark">
  )HTML");

  ExpectComputedColorScheme("dark");

  GetDocument().getElementById("meta")->removeAttribute(
      html_names::kContentAttr);

  ExpectComputedColorScheme("normal");
}

TEST_F(HTMLMetaElementTest, ColorSchemeProcessing_RemoveNameAttribute) {
  GetDocument().head()->SetInnerHTMLFromString(R"HTML(
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
  ColorSchemeHelper color_scheme_helper;
  color_scheme_helper.SetPreferredColorScheme(GetDocument(),
                                              PreferredColorScheme::kDark);

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
  GetDocument().head()->SetInnerHTMLFromString(R"HTML(
    <meta name="referrer" content="strict-origin">
    <meta name="referrer" >
  )HTML");
  EXPECT_EQ(network::mojom::ReferrerPolicy::kStrictOrigin,
            GetDocument().GetReferrerPolicy());
}

}  // namespace blink
