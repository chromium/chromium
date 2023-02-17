// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_image_cache.h"

#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/style/style_fetched_image.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"

namespace blink {

class StyleImageCacheTest : public PageTestBase {
 protected:
  void SetUp() override {
    PageTestBase::SetUp();
    GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  }
  const HeapHashMap<std::pair<String, float>, WeakMember<StyleFetchedImage>>&
  FetchedImageMap() {
    return GetDocument().GetStyleEngine().style_image_cache_.fetched_image_map_;
  }
};

TEST_F(StyleImageCacheTest, DuplicateBackgroundImageURLs) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .rule1 { background-image: url(url.png) }
      .rule2 { background-image: url(url.png) }
    </style>
    <div id="target"></div>
  )HTML");

  Element* target = GetDocument().getElementById("target");
  ASSERT_TRUE(target);
  ASSERT_FALSE(target->ComputedStyleRef().BackgroundLayers().GetImage());

  target->setAttribute(blink::html_names::kClassAttr, "rule1");
  UpdateAllLifecyclePhasesForTest();

  StyleImage* rule1_image =
      target->ComputedStyleRef().BackgroundLayers().GetImage();
  EXPECT_TRUE(rule1_image);

  target->setAttribute(blink::html_names::kClassAttr, "rule2");
  UpdateAllLifecyclePhasesForTest();

  StyleImage* rule2_image =
      target->ComputedStyleRef().BackgroundLayers().GetImage();
  EXPECT_EQ(rule1_image, rule2_image);
}

TEST_F(StyleImageCacheTest, CustomPropertyURL) {
  SetBodyInnerHTML(R"HTML(
    <style>
      :root { --bg: url(url.png) }
      #target { background-image: var(--bg) }
      .green { background-color: green }
    </style>
    <div id="target"></div>
  )HTML");

  Element* target = GetDocument().getElementById("target");

  StyleImage* initial_image =
      target->ComputedStyleRef().BackgroundLayers().GetImage();
  EXPECT_TRUE(initial_image);

  target->setAttribute(blink::html_names::kClassAttr, "green");
  UpdateAllLifecyclePhasesForTest();

  StyleImage* image_after_recalc =
      target->ComputedStyleRef().BackgroundLayers().GetImage();
  EXPECT_EQ(initial_image, image_after_recalc);
}

TEST_F(StyleImageCacheTest, ComputedValueRelativePath) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #target1 { background-image: url(http://test.com/url.png) }
      #target2 { background-image: url(url.png) }
    </style>
    <div id="target1"></div>
    <div id="target2"></div>
  )HTML");

  Element* target1 = GetDocument().getElementById("target1");
  Element* target2 = GetDocument().getElementById("target2");

  // Resolves to the same absolute url. Can share StyleFetchedImage since the
  // computed value is the absolute url.
  EXPECT_EQ(target1->ComputedStyleRef().BackgroundLayers().GetImage(),
            target2->ComputedStyleRef().BackgroundLayers().GetImage());

  const CSSProperty& property =
      CSSProperty::Get(CSSPropertyID::kBackgroundImage);
  EXPECT_EQ(property
                .CSSValueFromComputedStyle(target1->ComputedStyleRef(), nullptr,
                                           false)
                ->CssText(),
            "url(\"http://test.com/url.png\")");
  EXPECT_EQ(property
                .CSSValueFromComputedStyle(target2->ComputedStyleRef(), nullptr,
                                           false)
                ->CssText(),
            "url(\"http://test.com/url.png\")");
}

TEST_F(StyleImageCacheTest, WeakReferenceGC) {
  SetBodyInnerHTML(R"HTML(
    <style id="sheet">
      #target1 { background-image: url(url.png) }
      #target2 { background-image: url(url2.png) }
    </style>
    <div id="target1"></div>
    <div id="target2"></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(FetchedImageMap().Contains(
      std::pair<String, float>{"http://test.com/url.png", 0.0f}));
  EXPECT_TRUE(FetchedImageMap().Contains(
      std::pair<String, float>{"http://test.com/url2.png", 0.0f}));
  EXPECT_EQ(FetchedImageMap().size(), 2u);

  Element* sheet = GetDocument().getElementById("sheet");
  ASSERT_TRUE(sheet);
  sheet->remove();
  UpdateAllLifecyclePhasesForTest();
  ThreadState::Current()->CollectAllGarbageForTesting();

  // After the sheet has been removed, the lifecycle update and garbage
  // collection have been run, the weak references in the cache should have been
  // collected.
  EXPECT_FALSE(FetchedImageMap().Contains(
      std::pair<String, float>{"http://test.com/url.png", 0.0f}));
  EXPECT_FALSE(FetchedImageMap().Contains(
      std::pair<String, float>{"http://test.com/url2.png", 0.0f}));
  EXPECT_EQ(FetchedImageMap().size(), 0u);
}

}  // namespace blink
