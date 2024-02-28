// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_image_cache.h"

#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/style/style_fetched_image.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

namespace {
constexpr char kTestResourceFilename[] = "background_image.png";
constexpr char kTestResourceMimeType[] = "image/png";
}  // namespace

class StyleImageCacheTest : public PageTestBase {
 protected:
  void SetUp() override {
    PageTestBase::SetUp();
    GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  }
  const HeapHashMap<String, WeakMember<ImageResourceContent>>&
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

  Element* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);
  ASSERT_FALSE(target->ComputedStyleRef().BackgroundLayers().GetImage());

  target->setAttribute(html_names::kClassAttr, AtomicString("rule1"));
  UpdateAllLifecyclePhasesForTest();

  StyleImage* rule1_image =
      target->ComputedStyleRef().BackgroundLayers().GetImage();
  EXPECT_TRUE(rule1_image);

  target->setAttribute(html_names::kClassAttr, AtomicString("rule2"));
  UpdateAllLifecyclePhasesForTest();

  StyleImage* rule2_image =
      target->ComputedStyleRef().BackgroundLayers().GetImage();
  EXPECT_EQ(*rule1_image, *rule2_image);
}

TEST_F(StyleImageCacheTest, DifferingFragmentsBackgroundImageURLs) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .rule1 { background-image: url(url.svg#a) }
      .rule2 { background-image: url(url.svg#b) }
    </style>
    <div id="target"></div>
  )HTML");

  Element* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);
  ASSERT_FALSE(target->ComputedStyleRef().BackgroundLayers().GetImage());

  target->setAttribute(html_names::kClassAttr, AtomicString("rule1"));
  UpdateAllLifecyclePhasesForTest();

  StyleImage* rule1_image =
      target->ComputedStyleRef().BackgroundLayers().GetImage();
  EXPECT_TRUE(rule1_image);

  target->setAttribute(html_names::kClassAttr, AtomicString("rule2"));
  UpdateAllLifecyclePhasesForTest();

  StyleImage* rule2_image =
      target->ComputedStyleRef().BackgroundLayers().GetImage();
  EXPECT_NE(*rule1_image, *rule2_image);
  EXPECT_EQ(rule1_image->CachedImage(), rule2_image->CachedImage());
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

  Element* target = GetDocument().getElementById(AtomicString("target"));

  StyleImage* initial_image =
      target->ComputedStyleRef().BackgroundLayers().GetImage();
  EXPECT_TRUE(initial_image);

  target->setAttribute(html_names::kClassAttr, AtomicString("green"));
  UpdateAllLifecyclePhasesForTest();

  StyleImage* image_after_recalc =
      target->ComputedStyleRef().BackgroundLayers().GetImage();
  EXPECT_EQ(*initial_image, *image_after_recalc);
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

  Element* target1 = GetDocument().getElementById(AtomicString("target1"));
  Element* target2 = GetDocument().getElementById(AtomicString("target2"));

  // Resolves to the same absolute url. Can share the underlying
  // ImageResourceContent since the computed value is the absolute url.
  EXPECT_EQ(*target1->ComputedStyleRef().BackgroundLayers().GetImage(),
            *target2->ComputedStyleRef().BackgroundLayers().GetImage());

  const CSSProperty& property =
      CSSProperty::Get(CSSPropertyID::kBackgroundImage);
  EXPECT_EQ(property
                .CSSValueFromComputedStyle(target1->ComputedStyleRef(), nullptr,
                                           false, CSSValuePhase::kComputedValue)
                ->CssText(),
            "url(\"http://test.com/url.png\")");
  EXPECT_EQ(property
                .CSSValueFromComputedStyle(target2->ComputedStyleRef(), nullptr,
                                           false, CSSValuePhase::kComputedValue)
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

  EXPECT_TRUE(FetchedImageMap().Contains("http://test.com/url.png"));
  EXPECT_TRUE(FetchedImageMap().Contains("http://test.com/url2.png"));
  EXPECT_EQ(FetchedImageMap().size(), 2u);

  Element* sheet = GetDocument().getElementById(AtomicString("sheet"));
  ASSERT_TRUE(sheet);
  sheet->remove();
  UpdateAllLifecyclePhasesForTest();
  ThreadState::Current()->CollectAllGarbageForTesting();

  // After the sheet has been removed, the lifecycle update and garbage
  // collection have been run, the weak references in the cache should have been
  // collected.
  EXPECT_FALSE(FetchedImageMap().Contains("http://test.com/url.png"));
  EXPECT_FALSE(FetchedImageMap().Contains("http://test.com/url2.png"));
  EXPECT_EQ(FetchedImageMap().size(), 0u);
}

class StyleImageCacheFrameClientTest : public EmptyLocalFrameClient {
 public:
  std::unique_ptr<URLLoader> CreateURLLoaderForTesting() override {
    return URLLoaderMockFactory::GetSingletonInstance()->CreateURLLoader();
  }
};

class StyleImageCacheWithLoadingTest : public StyleImageCacheTest {
 public:
  StyleImageCacheWithLoadingTest() = default;
  ~StyleImageCacheWithLoadingTest() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

 protected:
  void SetUp() override {
    auto setting_overrider = [](Settings& settings) {
      settings.SetLoadsImagesAutomatically(true);
    };
    PageTestBase::SetupPageWithClients(
        nullptr, MakeGarbageCollected<StyleImageCacheFrameClientTest>(),
        setting_overrider);
  }
};

TEST_F(StyleImageCacheWithLoadingTest, DuplicateBackgroundImageURLs) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .rule1 { background-image: url(http://test.com/background_image.png) }
      .rule2 { background-image: url(http://test.com/background_image.png) }
    </style>
    <div id="target"></div>
  )HTML");
  url_test_helpers::RegisterMockedURLLoad(
      url_test_helpers::ToKURL("http://test.com/background_image.png"),
      test::CoreTestDataPath(kTestResourceFilename), kTestResourceMimeType);
  Element* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);
  ASSERT_FALSE(target->ComputedStyleRef().BackgroundLayers().GetImage());

  target->setAttribute(html_names::kClassAttr, AtomicString("rule1"));
  UpdateAllLifecyclePhasesForTest();
  url_test_helpers::ServeAsynchronousRequests();
  StyleImage* rule1_image =
      target->ComputedStyleRef().BackgroundLayers().GetImage();
  EXPECT_TRUE(rule1_image);
  EXPECT_FALSE(rule1_image->ErrorOccurred());

  target->setAttribute(html_names::kClassAttr, AtomicString("rule2"));
  UpdateAllLifecyclePhasesForTest();
  url_test_helpers::ServeAsynchronousRequests();
  StyleImage* rule2_image =
      target->ComputedStyleRef().BackgroundLayers().GetImage();
  EXPECT_EQ(*rule1_image, *rule2_image);
  EXPECT_FALSE(rule2_image->ErrorOccurred());
}

TEST_F(StyleImageCacheWithLoadingTest, LoadFailedBackgroundImageURL) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .rule1 { background-image: url(http://test.com/background_image.png) }
      .rule2 { background-image: url(http://test.com/background_image.png) }
    </style>
    <div id="target"></div>
  )HTML");
  const auto image_url =
      url_test_helpers::ToKURL("http://test.com/background_image.png");
  url_test_helpers::RegisterMockedErrorURLLoad(image_url);
  Element* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);
  ASSERT_FALSE(target->ComputedStyleRef().BackgroundLayers().GetImage());
  target->setAttribute(html_names::kClassAttr, AtomicString("rule1"));
  UpdateAllLifecyclePhasesForTest();
  url_test_helpers::ServeAsynchronousRequests();
  StyleImage* rule1_image1 =
      target->ComputedStyleRef().BackgroundLayers().GetImage();
  EXPECT_TRUE(rule1_image1->ErrorOccurred());
  url_test_helpers::RegisterMockedURLUnregister(image_url);
  url_test_helpers::RegisterMockedURLLoad(
      image_url, test::CoreTestDataPath(kTestResourceFilename),
      kTestResourceMimeType);
  target->setAttribute(html_names::kClassAttr, AtomicString("rule2"));
  UpdateAllLifecyclePhasesForTest();
  url_test_helpers::ServeAsynchronousRequests();
  StyleImage* rule1_image2 =
      target->ComputedStyleRef().BackgroundLayers().GetImage();
  EXPECT_NE(*rule1_image1, *rule1_image2);
  EXPECT_FALSE(rule1_image2->ErrorOccurred());
  EXPECT_TRUE(FetchedImageMap().Contains(image_url.GetString()));
  EXPECT_EQ(FetchedImageMap().size(), 1u);
}

}  // namespace blink
