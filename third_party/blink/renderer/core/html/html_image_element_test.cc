// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_image_element.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

class TestFrameClient : public EmptyLocalFrameClient {
 public:
  void OnMainFrameImageAdRectangleChanged(
      DOMNodeId element_id,
      const gfx::Rect& image_ad_rect) override {
    observed_image_ad_rects_.emplace_back(element_id, image_ad_rect);
  }

  const std::vector<std::pair<DOMNodeId, gfx::Rect>>& observed_image_ad_rects()
      const {
    return observed_image_ad_rects_;
  }

 private:
  std::vector<std::pair<DOMNodeId, gfx::Rect>> observed_image_ad_rects_;
};

}  // namespace

class HTMLImageElementTest : public PageTestBase {
 protected:
  static constexpr int kViewportWidth = 500;
  static constexpr int kViewportHeight = 600;

  void SetUp() override {
    test_frame_client_ = MakeGarbageCollected<TestFrameClient>();

    PageTestBase::SetupPageWithClients(
        nullptr, test_frame_client_.Get(), nullptr,
        gfx::Size(kViewportWidth, kViewportHeight));
  }

  Persistent<TestFrameClient> test_frame_client_;
};

// Instantiate class constants. Not needed after C++17.
constexpr int HTMLImageElementTest::kViewportWidth;
constexpr int HTMLImageElementTest::kViewportHeight;

TEST_F(HTMLImageElementTest, width) {
  auto* image = MakeGarbageCollected<HTMLImageElement>(GetDocument());
  image->setAttribute(html_names::kWidthAttr, AtomicString("400"));
  // TODO(yoav): `width` does not impact resourceWidth until we resolve
  // https://github.com/ResponsiveImagesCG/picture-element/issues/268
  EXPECT_EQ(std::nullopt, image->GetResourceWidth());
  image->setAttribute(html_names::kSizesAttr, AtomicString("100vw"));
  EXPECT_EQ(500, image->GetResourceWidth());
}

TEST_F(HTMLImageElementTest, sourceSize) {
  auto* image = MakeGarbageCollected<HTMLImageElement>(GetDocument());
  image->setAttribute(html_names::kWidthAttr, AtomicString("400"));
  EXPECT_EQ(kViewportWidth, image->SourceSize(*image));
  image->setAttribute(html_names::kSizesAttr, AtomicString("50vw"));
  EXPECT_EQ(250, image->SourceSize(*image));
}

TEST_F(HTMLImageElementTest, ImageAdRectangleUpdate) {
  GetDocument().GetSettings()->SetScriptEnabled(true);

  SetBodyInnerHTML(R"HTML(
    <img id="target"
         style="position:absolute;top:5px;left:5px;width:10px;height:10px;">
    </img>

    <p style="position:absolute;top:10000px;">abc</p>
  )HTML");

  HTMLImageElement* image = To<HTMLImageElement>(GetElementById("target"));
  image->SetIsAdRelated();

  EXPECT_TRUE(test_frame_client_->observed_image_ad_rects().empty());

  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(test_frame_client_->observed_image_ad_rects().size(), 1u);
  DOMNodeId id = test_frame_client_->observed_image_ad_rects()[0].first;
  EXPECT_EQ(test_frame_client_->observed_image_ad_rects()[0].second,
            gfx::Rect(5, 5, 10, 10));

  // Scrolling won't trigger another notification, as the rectangle hasn't
  // changed relative to the page.
  {
    auto* script = GetDocument().CreateRawElement(html_names::kScriptTag);
    script->setTextContent(R"JS(
      window.scroll(0, 100);
    )JS");
    GetDocument().body()->appendChild(script);
    UpdateAllLifecyclePhasesForTest();
  }

  EXPECT_EQ(test_frame_client_->observed_image_ad_rects().size(), 1u);

  // Update the size to 1x1. A new notification is expected to signal the
  // removal of the element.
  {
    auto* script = GetDocument().CreateRawElement(html_names::kScriptTag);
    script->setTextContent(R"JS(
      var image = document.getElementById('target');
      image.style.width = '1px';
      image.style.height = '1px';
    )JS");
    GetDocument().body()->appendChild(script);
    UpdateAllLifecyclePhasesForTest();
  }

  EXPECT_EQ(test_frame_client_->observed_image_ad_rects().size(), 2u);
  EXPECT_EQ(test_frame_client_->observed_image_ad_rects()[1].first, id);
  EXPECT_EQ(test_frame_client_->observed_image_ad_rects()[1].second,
            gfx::Rect());

  // Update the size to 30x30. A new notification is expected to signal the new
  // rectangle.
  {
    auto* script = GetDocument().CreateRawElement(html_names::kScriptTag);
    script->setTextContent(R"JS(
      var image = document.getElementById('target');
      image.style.width = '30px';
      image.style.height = '30px';
    )JS");
    GetDocument().body()->appendChild(script);
    UpdateAllLifecyclePhasesForTest();
  }

  EXPECT_EQ(test_frame_client_->observed_image_ad_rects().size(), 3u);
  EXPECT_EQ(test_frame_client_->observed_image_ad_rects()[2].first, id);
  EXPECT_EQ(test_frame_client_->observed_image_ad_rects()[2].second,
            gfx::Rect(5, 5, 30, 30));

  // Remove the element. A new notification is expected to signal the removal of
  // the element.
  {
    auto* script = GetDocument().CreateRawElement(html_names::kScriptTag);
    script->setTextContent(R"JS(
      var image = document.getElementById('target');
      image.remove()
    )JS");
    GetDocument().body()->appendChild(script);
    UpdateAllLifecyclePhasesForTest();
  }

  EXPECT_EQ(test_frame_client_->observed_image_ad_rects().size(), 4u);
  EXPECT_EQ(test_frame_client_->observed_image_ad_rects()[3].first, id);
  EXPECT_EQ(test_frame_client_->observed_image_ad_rects()[3].second,
            gfx::Rect());
}

TEST_F(HTMLImageElementTest, ResourceWidthWithPicture) {
  SetBodyInnerHTML(R"HTML(
    <picture>
      <source srcset="a.png" sizes="auto"/>
      <img id="i" width="5" height="5" src="b.png" loading="lazy" sizes="auto"/>
    </picture>
  )HTML");

  HTMLImageElement* image = To<HTMLImageElement>(GetElementById("i"));
  ASSERT_NE(image, nullptr);
  EXPECT_EQ(*image->GetResourceWidth(), 5);
}

TEST_F(HTMLImageElementTest, ResourceWidthWithPictureContainingScripts) {
  SetBodyInnerHTML(R"HTML(
    <picture>
      <source srcset="a.png" sizes="auto"/>
      <script></script>
      <img id="i" width="5" height="5" src="b.png" loading="lazy" sizes="auto"/>
      <script></script>
    </picture>
  )HTML");

  HTMLImageElement* image = To<HTMLImageElement>(GetElementById("i"));
  ASSERT_NE(image, nullptr);
  EXPECT_EQ(*image->GetResourceWidth(), 5);
}

using HTMLImageElementSimTest = SimTest;

TEST_F(HTMLImageElementSimTest, Sharedstoragewritable_SecureContext_Allowed) {
  WebRuntimeFeaturesBase::EnableSharedStorageAPI(true);
  WebRuntimeFeaturesBase::EnableSharedStorageAPIM118(true);
  SimRequest main_resource("https://example.com/index.html", "text/html");
  SimSubresourceRequest image_resource("https://example.com/foo.png",
                                       "image/png");
  LoadURL("https://example.com/index.html");
  main_resource.Complete(R"(
    <img src="foo.png" id="target"
      allow="shared-storage"
      sharedstoragewritable></img>
  )");

  image_resource.Complete("image data");
  EXPECT_TRUE(ConsoleMessages().empty());
}

TEST_F(HTMLImageElementSimTest,
       Sharedstoragewritable_InsecureContext_NotAllowed) {
  WebRuntimeFeaturesBase::EnableSharedStorageAPI(true);
  WebRuntimeFeaturesBase::EnableSharedStorageAPIM118(true);
  SimRequest main_resource("http://example.com/index.html", "text/html");
  SimSubresourceRequest image_resource("http://example.com/foo.png",
                                       "image/png");
  LoadURL("http://example.com/index.html");
  main_resource.Complete(R"(
    <img src="foo.png" id="target"
      allow="shared-storage"
      sharedstoragewritable></img>
  )");

  image_resource.Complete("image data");
  EXPECT_EQ(ConsoleMessages().size(), 1u);
  EXPECT_TRUE(ConsoleMessages().front().StartsWith(
      "sharedStorageWritable: sharedStorage operations are only available in "
      "secure contexts."))
      << "Expect error that Shared Storage operations are not allowed in "
         "insecure contexts but got: "
      << ConsoleMessages().front();
}

class TransparentPlaceholderImageSimTest
    : public SimTest,
      public ::testing::WithParamInterface<bool> {
 protected:
  void SetUp() override {
    SimTest::SetUp();
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(
          features::kSimplifyLoadingTransparentPlaceholderImage);
    } else {
      feature_list_.InitAndDisableFeature(
          features::kSimplifyLoadingTransparentPlaceholderImage);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(TransparentPlaceholderImageSimTest,
                         TransparentPlaceholderImageSimTest,
                         testing::Bool());

TEST_P(TransparentPlaceholderImageSimTest, OnloadTransparentPlaceholderImage) {
  SimRequest main_resource("http://example.com/index.html", "text/html");
  LoadURL("http://example.com/index.html");
  main_resource.Complete(R"(
    <body onload='console.log("main body onload");'>
      <img src="data:image/gif;base64,R0lGODlhAQABAIAAAP///////yH5BAEKAAEALAAAAAABAAEAAAICTAEAOw=="
           onload='console.log("image element onload");'>
    </body>)");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // Ensure that both body and image are successfully loaded.
  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("image element onload"));
}

TEST_P(TransparentPlaceholderImageSimTest,
       CurrentSrcForTransparentPlaceholderImage) {
  const String image_source =
      "data:image/gif;base64,R0lGODlhAQABAIAAAP///////"
      "yH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==";

  SimRequest main_resource("http://example.com/index.html", "text/html");
  LoadURL("http://example.com/index.html");
  main_resource.Complete(R"(
    <img id="myimg" src=)" +
                         image_source + R"(>
    <script>
      console.log(myimg.currentSrc);
    </script>)");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // Ensure that currentSrc is correctly set as the image source.
  EXPECT_TRUE(ConsoleMessages().Contains(image_source));
}

class HTMLImageElementUseCounterTest : public HTMLImageElementTest {
 protected:
  bool IsCounted(WebFeature feature) {
    return GetDocument().IsUseCounted(feature);
  }
};

TEST_F(HTMLImageElementUseCounterTest, AutoSizesUseCountersNoSizes) {
  SetBodyInnerHTML(R"HTML(
    <img id="target"
         loading="lazy">
    </img>
  )HTML");

  HTMLImageElement* image = To<HTMLImageElement>(GetElementById("target"));
  ASSERT_NE(image, nullptr);

  EXPECT_FALSE(IsCounted(WebFeature::kAutoSizesLazy));
  EXPECT_FALSE(IsCounted(WebFeature::kAutoSizesNonLazy));
}

TEST_F(HTMLImageElementUseCounterTest, AutoSizesUseCountersNonAutoSizes) {
  SetBodyInnerHTML(R"HTML(
    <img id="target"
         sizes = "33px"
         loading="lazy">
    </img>
  )HTML");

  HTMLImageElement* image = To<HTMLImageElement>(GetElementById("target"));
  ASSERT_NE(image, nullptr);

  EXPECT_FALSE(IsCounted(WebFeature::kAutoSizesLazy));
  EXPECT_FALSE(IsCounted(WebFeature::kAutoSizesNonLazy));
}

TEST_F(HTMLImageElementUseCounterTest, AutoSizesNonLazyUseCounter) {
  SetBodyInnerHTML(R"HTML(
    <img id="target"
         sizes="auto">
    </img>
  )HTML");

  HTMLImageElement* image = To<HTMLImageElement>(GetElementById("target"));
  ASSERT_NE(image, nullptr);

  EXPECT_FALSE(IsCounted(WebFeature::kAutoSizesLazy));
  EXPECT_TRUE(IsCounted(WebFeature::kAutoSizesNonLazy));
}

TEST_F(HTMLImageElementUseCounterTest, AutoSizesLazyUseCounter) {
  SetBodyInnerHTML(R"HTML(
    <img id="target"
         sizes="auto"
         loading="lazy">
    </img>
  )HTML");

  HTMLImageElement* image = To<HTMLImageElement>(GetElementById("target"));
  ASSERT_NE(image, nullptr);

  EXPECT_TRUE(IsCounted(WebFeature::kAutoSizesLazy));
  EXPECT_FALSE(IsCounted(WebFeature::kAutoSizesNonLazy));
}

}  // namespace blink
