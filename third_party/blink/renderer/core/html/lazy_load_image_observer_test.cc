// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/lazy_load_image_observer.h"

#include <tuple>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/style_image.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/network/http_header_map.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/testing/histogram_tester.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

Vector<char> ReadTestImage() {
  return test::ReadFromFile(test::CoreTestDataPath("notifications/500x500.png"))
      ->CopyAs<Vector<char>>();
}

class LazyLoadImagesSimTest : public ::testing::WithParamInterface<bool>,
                              public SimTest {
 protected:
  LazyLoadImagesSimTest() : scoped_lazy_image_loading_for_test_(GetParam()) {}

  void SetLazyLoadEnabled(bool enabled) {
    WebView().GetPage()->GetSettings().SetLazyLoadEnabled(enabled);
  }

  void LoadMainResource(const String& html_body) {
    SimRequest main_resource("https://example.com/", "text/html");
    LoadURL("https://example.com/");

    main_resource.Complete(html_body);
    GetDocument().UpdateStyleAndLayoutTree();
  }

  const ComputedStyle* GetElementComputedStyle(const Element& element,
                                               PseudoId pseudo_id) {
    if (pseudo_id == kPseudoIdNone)
      return element.GetComputedStyle();
    return element.GetPseudoElement(pseudo_id)->GetComputedStyle();
  }

  void ExpectCSSBackgroundImageDeferredState(const char* element_id,
                                             PseudoId pseudo_id,
                                             bool deferred) {
    const ComputedStyle* deferred_image_style = GetElementComputedStyle(
        *GetDocument().getElementById(element_id), pseudo_id);
    EXPECT_TRUE(deferred_image_style->HasBackgroundImage());
    bool is_background_image_found = false;
    for (const FillLayer* background_layer =
             &deferred_image_style->BackgroundLayers();
         background_layer; background_layer = background_layer->Next()) {
      if (StyleImage* deferred_image = background_layer->GetImage()) {
        EXPECT_TRUE(deferred_image->IsImageResource());
        EXPECT_EQ(deferred, deferred_image->IsLazyloadPossiblyDeferred());
        EXPECT_NE(deferred, deferred_image->IsLoaded());
        is_background_image_found = true;
      }
    }
    EXPECT_TRUE(is_background_image_found);
    EXPECT_FALSE(GetDocument().IsUseCounted(
        WebFeature::kLazyLoadImageMissingDimensionsForLazy));
    EXPECT_FALSE(GetDocument().IsUseCounted(
        WebFeature::kLazyLoadImageLoadingAttributeLazy));
    EXPECT_FALSE(GetDocument().IsUseCounted(
        WebFeature::kLazyLoadImageLoadingAttributeEager));
  }

  void VerifyImageElementWithDimensionDeferred(const char* img_attribute) {
    bool is_lazyload_image_enabled = GetParam();
    SimRequest image_resource("https://example.com/img.png", "image/png");

    if (is_lazyload_image_enabled) {
      LoadMainResource(String::Format(R"HTML(
        <body onload='console.log("main body onload");'>
          <div style='height:10000px;'></div>
          <img src="img.png" loading="lazy" %s
               onload= 'console.log("deferred_image onload");'>
        </body>)HTML",
                                      img_attribute));
    } else {
      LoadMainResource(String::Format(R"HTML(
        <body onload='console.log("main body onload");'>
          <div style='height:10000px;'></div>
          <img src="img.png" %s
               onload= 'console.log("deferred_image onload");'>
        </body>)HTML",
                                      img_attribute));
    }

    if (!is_lazyload_image_enabled)
      image_resource.Complete(ReadTestImage());

    Compositor().BeginFrame();
    test::RunPendingTasks();

    EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
    if (!is_lazyload_image_enabled)
      EXPECT_TRUE(ConsoleMessages().Contains("deferred_image onload"));

    if (is_lazyload_image_enabled) {
      // Scroll down until the image element is visible.
      GetDocument().View()->LayoutViewport()->SetScrollOffset(
          ScrollOffset(0, 10000), mojom::blink::ScrollType::kProgrammatic);
      Compositor().BeginFrame();
      test::RunPendingTasks();
      image_resource.Complete(ReadTestImage());
      test::RunPendingTasks();
      EXPECT_TRUE(ConsoleMessages().Contains("deferred_image onload"));
    }
    EXPECT_FALSE(GetDocument().IsUseCounted(
        WebFeature::kLazyLoadImageMissingDimensionsForLazy));
    EXPECT_EQ(is_lazyload_image_enabled,
              GetDocument().IsUseCounted(
                  WebFeature::kLazyLoadImageLoadingAttributeLazy));
    EXPECT_FALSE(GetDocument().IsUseCounted(
        WebFeature::kLazyLoadImageLoadingAttributeEager));
  }

 private:
  ScopedLazyImageLoadingForTest scoped_lazy_image_loading_for_test_;
};

TEST_P(LazyLoadImagesSimTest, LargeImageHeight100Width100) {
  VerifyImageElementWithDimensionDeferred("height='100px' width='100px'");
}

TEST_P(LazyLoadImagesSimTest, LargeImageHeight1Width100) {
  VerifyImageElementWithDimensionDeferred("height='1px' width='100px'");
}

TEST_P(LazyLoadImagesSimTest, LargeImageHeight100Width1) {
  VerifyImageElementWithDimensionDeferred("height='100px' width='1px'");
}

TEST_P(LazyLoadImagesSimTest, LargeImageStyleHeight100Width100) {
  VerifyImageElementWithDimensionDeferred(
      "style='height: 100px; width: 100px;'");
}

TEST_P(LazyLoadImagesSimTest, LargeImageStyleHeight100Width1) {
  VerifyImageElementWithDimensionDeferred("style='height: 100px; width: 1px;'");
}

TEST_P(LazyLoadImagesSimTest, LargeImageStyleHeight1Width100) {
  VerifyImageElementWithDimensionDeferred("style='height: 1px; width: 100px;'");
}

TEST_P(LazyLoadImagesSimTest, ImgSrcset) {
  if (!GetParam())  // Only test when LazyImage is enabled.
    return;
  SetLazyLoadEnabled(true);
  WebView().Resize(gfx::Size(100, 1));
  LoadMainResource(R"HTML(
        <body onload='console.log("main body onload");'>
          <div style='height:10000px;'></div>
          <img src="img.png" srcset="img.png?100w 100w, img.png?200w 200w"
           loading="lazy" onload= 'console.log("deferred_image onload");'>
        </body>)HTML");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("deferred_image onload"));

  // Resizing should not load the image.
  WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 1));
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_FALSE(ConsoleMessages().Contains("deferred_image onload"));

  // Scrolling down should load the larger image.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 10000), mojom::blink::ScrollType::kProgrammatic);
  SimRequest image_resource("https://example.com/img.png?200w", "image/png");
  Compositor().BeginFrame();
  test::RunPendingTasks();
  image_resource.Complete(ReadTestImage());
  test::RunPendingTasks();
  EXPECT_TRUE(ConsoleMessages().Contains("deferred_image onload"));

  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadImageLoadingAttributeLazy));
}

TEST_P(LazyLoadImagesSimTest, LazyLoadedImageSizeHistograms) {
  if (!GetParam()) {  // Only test when LazyImage is enabled.
    return;
  }

  HistogramTester histogram_tester;
  SimRequest lazy_a_resource("https://example.com/lazy_a.png", "image/png");
  SimRequest eager_resource("https://example.com/eager.png", "image/png");
  SimRequest lazy_b_resource("https://example.com/lazy_b.png", "image/png");
  LoadMainResource(R"HTML(
      <img src="lazy_a.png" loading="lazy">
      <img src="eager.png" loading="eager">
      <img src="lazy_b.png" loading="lazy">
    )HTML");
  Compositor().BeginFrame();
  test::RunPendingTasks();

  // Initially, no lazy images should have loaded.
  histogram_tester.ExpectTotalCount("Blink.LazyLoadedImage.Size", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.LazyLoadedImageBeforeDocumentOnLoad.Size", 0);

  // Load the first lazy loaded image.
  lazy_a_resource.Complete(ReadTestImage());
  Compositor().BeginFrame();
  test::RunPendingTasks();

  // We should have one lazy load sample, and one before-load lazy load sample.
  histogram_tester.ExpectTotalCount("Blink.LazyLoadedImage.Size", 1);
  int size_kb = ReadTestImage().size() / 1024;
  histogram_tester.ExpectUniqueSample("Blink.LazyLoadedImage.Size", size_kb, 1);
  ASSERT_FALSE(GetDocument().LoadEventFinished());
  histogram_tester.ExpectTotalCount(
      "Blink.LazyLoadedImageBeforeDocumentOnLoad.Size", 1);
  histogram_tester.ExpectUniqueSample(
      "Blink.LazyLoadedImageBeforeDocumentOnLoad.Size", size_kb, 1);

  // Load the eager image which completes the document load.
  eager_resource.Complete(ReadTestImage());
  Compositor().BeginFrame();
  test::RunPendingTasks();

  // Load should finish, but because the eager image is not lazy, the lazy load
  // metrics should not change.
  ASSERT_TRUE(GetDocument().LoadEventFinished());
  histogram_tester.ExpectTotalCount("Blink.LazyLoadedImage.Size", 1);
  histogram_tester.ExpectTotalCount(
      "Blink.LazyLoadedImageBeforeDocumentOnLoad.Size", 1);

  // Load the second lazy loaded image.
  lazy_b_resource.Complete(ReadTestImage());
  Compositor().BeginFrame();
  test::RunPendingTasks();

  // We should still only have one before-load sample, but we should have two
  // lazy load samples overall.
  histogram_tester.ExpectTotalCount("Blink.LazyLoadedImage.Size", 2);
  histogram_tester.ExpectUniqueSample("Blink.LazyLoadedImage.Size", size_kb, 2);
  histogram_tester.ExpectTotalCount(
      "Blink.LazyLoadedImageBeforeDocumentOnLoad.Size", 1);
}

INSTANTIATE_TEST_SUITE_P(All,
                         LazyLoadImagesSimTest,
                         ::testing::Bool() /*is_lazyload_image_enabled*/);

enum class LazyLoadImagesParams {
  kDisabled = 0,
  // LazyImageLoading enabled, DelayOutOfViewportLazyImages disabled.
  kEnabled,
  // LazyImageLoading enabled, DelayOutOfViewportLazyImages enabled.
  kEnabledWithDelayOutOfViewportLazyImages
};

class LazyLoadImagesParamsTest
    : public SimTest,
      public ::testing::WithParamInterface<
          std::tuple<LazyLoadImagesParams, WebEffectiveConnectionType>> {
 public:
  static constexpr int kViewportWidth = 800;
  static constexpr int kViewportHeight = 600;

  LazyLoadImagesParamsTest()
      : lazy_image_loading_(std::get<LazyLoadImagesParams>(GetParam()) !=
                            LazyLoadImagesParams::kDisabled),
        delay_out_of_viewport_lazy_images_(
            std::get<LazyLoadImagesParams>(GetParam()) ==
            LazyLoadImagesParams::kEnabledWithDelayOutOfViewportLazyImages) {
    // Ensure DelayOutOfViewportLazyImages is not enabled with just
    // `LazyLoadImagesParams::kEnabled`. This DCHECK ensures we remove
    // `LazyLoadImagesParams::kEnabledWithDelayOutOfViewportLazyImages` when
    // DelayOutOfViewportLazyImages is enabled by default.
    if (RuntimeEnabledFeatures::DelayOutOfViewportLazyImagesEnabled()) {
      DCHECK(std::get<LazyLoadImagesParams>(GetParam()) !=
             LazyLoadImagesParams::kEnabled);
    }
  }

  void SetUp() override {
    GetNetworkStateNotifier().SetNetworkConnectionInfoOverride(
        true /*on_line*/, kWebConnectionTypeWifi,
        std::get<WebEffectiveConnectionType>(GetParam()),
        1000 /*http_rtt_msec*/, 100 /*max_bandwidth_mbps*/);

    SimTest::SetUp();
    WebView().MainFrameViewWidget()->Resize(
        gfx::Size(kViewportWidth, kViewportHeight));

    Settings& settings = WebView().GetPage()->GetSettings();

    // These should match the values that would be returned by
    // GetLoadingDistanceThreshold().
    settings.SetLazyImageLoadingDistanceThresholdPxUnknown(200);
    settings.SetLazyImageLoadingDistanceThresholdPxOffline(300);
    settings.SetLazyImageLoadingDistanceThresholdPxSlow2G(400);
    settings.SetLazyImageLoadingDistanceThresholdPx2G(500);
    settings.SetLazyImageLoadingDistanceThresholdPx3G(600);
    settings.SetLazyImageLoadingDistanceThresholdPx4G(700);
    settings.SetLazyLoadEnabled(
        RuntimeEnabledFeatures::LazyImageLoadingEnabled());
  }

  // When DelayOutOfViewportLazyImages is enabled, this returns the threshold
  // that will be used after the document has finished loading, as a threshold
  // of zero is used during loading.
  int GetLoadingDistanceThreshold() const {
    static constexpr int kDistanceThresholdByEffectiveConnectionType[] = {
        200, 300, 400, 500, 600, 700};
    return kDistanceThresholdByEffectiveConnectionType[static_cast<int>(
        std::get<WebEffectiveConnectionType>(GetParam()))];
  }

 private:
  ScopedLazyImageLoadingForTest lazy_image_loading_;
  ScopedDelayOutOfViewportLazyImagesForTest delay_out_of_viewport_lazy_images_;
};

TEST_P(LazyLoadImagesParamsTest, NearViewport) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimSubresourceRequest css_resource("https://example.com/style.css",
                                     "text/css");

  SimSubresourceRequest eager_resource("https://example.com/eager.png",
                                       "image/png");
  absl::optional<SimSubresourceRequest> lazy_resource, auto_resource,
      unset_resource;
  lazy_resource.emplace("https://example.com/lazy.png", "image/png");
  auto_resource.emplace("https://example.com/auto.png", "image/png");
  unset_resource.emplace("https://example.com/unset.png", "image/png");
  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
        <head>
          <link rel='stylesheet' href='https://example.com/style.css' />
        </head>
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <img src='https://example.com/eager.png' loading='eager'
             onload='console.log("eager onload");' />
        <img src='https://example.com/lazy.png' loading='lazy'
             onload='console.log("lazy onload");' />
        <img src='https://example.com/auto.png' loading='auto'
             onload='console.log("auto onload");' />
        <img src='https://example.com/unset.png'
             onload='console.log("unset onload");' />
        </body>)HTML",
      kViewportHeight + GetLoadingDistanceThreshold() - 100));

  css_resource.Complete("img { width: 50px; height: 50px; }");
  test::RunPendingTasks();

  Vector<char> full_image = ReadTestImage();
  ASSERT_LT(2048U, full_image.size());
  Vector<char> partial_image;
  partial_image.Append(full_image.data(), 2048U);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_FALSE(ConsoleMessages().Contains("main body onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("eager onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("lazy onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("auto onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("unset onload"));

  eager_resource.Complete(full_image);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_FALSE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("eager onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("lazy onload"));
  // When automatic lazy image loading is enabled, images that are not
  // explicitly `loading=lazy` will still block the window load event.
  // Therefore, the following two images are either:
  //   a.) Fetched eagerly, when automatic lazy image loading is disabled
  //       - And therefore block the window load event
  //   b.) Fetched lazily, when automatic lazy image loading is enabled
  //       - And still block the window load event, if fetched before it fires.
  EXPECT_FALSE(ConsoleMessages().Contains("auto onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("unset onload"));

  auto_resource->Complete(full_image);
  unset_resource->Complete(full_image);

  // Run pending tasks to process load events from `auto_resource` and
  // `unset_resource`.
  test::RunPendingTasks();

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // The explicitly `loading=lazy` image never blocks the window load event.
  if (RuntimeEnabledFeatures::LazyImageLoadingEnabled())
    EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  else
    EXPECT_FALSE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("eager onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("lazy onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("auto onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("unset onload"));

  lazy_resource->Complete(full_image);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("eager onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("lazy onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("auto onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("unset onload"));
}

TEST_P(LazyLoadImagesParamsTest, FarFromViewport) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimSubresourceRequest css_resource("https://example.com/style.css",
                                     "text/css");

  SimSubresourceRequest eager_resource("https://example.com/eager.png",
                                       "image/png");
  absl::optional<SimSubresourceRequest> lazy_resource, auto_resource,
      unset_resource;
  lazy_resource.emplace("https://example.com/lazy.png", "image/png");
  auto_resource.emplace("https://example.com/auto.png", "image/png");
  unset_resource.emplace("https://example.com/unset.png", "image/png");

  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
        <head>
          <link rel='stylesheet' href='https://example.com/style.css' />
        </head>
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <img src='https://example.com/eager.png' loading='eager'
             onload='console.log("eager onload");' />
        <img src='https://example.com/lazy.png' loading='lazy'
             onload='console.log("lazy onload");' />
        <img src='https://example.com/auto.png' loading='auto'
             onload='console.log("auto onload");' />
        <img src='https://example.com/unset.png'
             onload='console.log("unset onload");' />
        </body>)HTML",
      kViewportHeight + GetLoadingDistanceThreshold() + 100));

  css_resource.Complete("img { width: 50px; height: 50px; }");
  test::RunPendingTasks();

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_FALSE(ConsoleMessages().Contains("main body onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("eager onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("lazy onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("auto onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("unset onload"));

  Vector<char> full_image = ReadTestImage();
  ASSERT_LT(2048U, full_image.size());
  Vector<char> partial_image;
  partial_image.Append(full_image.data(), 2048U);

  eager_resource.Complete(full_image);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_FALSE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("eager onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("lazy onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("auto onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("unset onload"));

  if (!RuntimeEnabledFeatures::LazyImageLoadingEnabled()) {
    lazy_resource->Complete(full_image);
  }

  auto_resource->Complete(full_image);
  unset_resource->Complete(full_image);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("eager onload"));

  if (RuntimeEnabledFeatures::LazyImageLoadingEnabled()) {
    // Scroll down so that the images are near the viewport.
    GetDocument().View()->LayoutViewport()->SetScrollOffset(
        ScrollOffset(0, 150), mojom::blink::ScrollType::kProgrammatic);

    Compositor().BeginFrame();
    test::RunPendingTasks();

    EXPECT_FALSE(ConsoleMessages().Contains("lazy onload"));

    lazy_resource->Complete(full_image);

    Compositor().BeginFrame();
    test::RunPendingTasks();
  }

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("eager onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("lazy onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("auto onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("unset onload"));
}

INSTANTIATE_TEST_SUITE_P(
    LazyImageLoading,
    LazyLoadImagesParamsTest,
    ::testing::Combine(
        ::testing::Values(
            LazyLoadImagesParams::kDisabled,
            LazyLoadImagesParams::kEnabled,
            LazyLoadImagesParams::kEnabledWithDelayOutOfViewportLazyImages),
        ::testing::Values(WebEffectiveConnectionType::kTypeUnknown,
                          WebEffectiveConnectionType::kTypeOffline,
                          WebEffectiveConnectionType::kTypeSlow2G,
                          WebEffectiveConnectionType::kType2G,
                          WebEffectiveConnectionType::kType3G,
                          WebEffectiveConnectionType::kType4G)));

class LazyLoadAutomaticImagesTest : public SimTest {
 public:
  static constexpr int kViewportWidth = 800;
  static constexpr int kViewportHeight = 600;
  static constexpr int kLoadingDistanceThreshold = 300;

  LazyLoadAutomaticImagesTest() : scoped_lazy_image_loading_for_test_(true) {}

  void SetUp() override {
    GetNetworkStateNotifier().SetNetworkConnectionInfoOverride(
        true /*on_line*/, kWebConnectionTypeWifi,
        WebEffectiveConnectionType::kType4G, 1000 /*http_rtt_msec*/,
        100 /*max_bandwidth_mbps*/);
    SimTest::SetUp();
    WebView().MainFrameViewWidget()->Resize(
        gfx::Size(kViewportWidth, kViewportHeight));

    Settings& settings = WebView().GetPage()->GetSettings();
    settings.SetLazyImageLoadingDistanceThresholdPx4G(
        kLoadingDistanceThreshold);
    settings.SetLazyFrameLoadingDistanceThresholdPx4G(
        kLoadingDistanceThreshold);
    settings.SetLazyLoadEnabled(
        RuntimeEnabledFeatures::LazyImageLoadingEnabled());
  }

  void LoadMainResourceWithImageFarFromViewport(const char* image_attributes) {
    SimRequest main_resource("https://example.com/", "text/html");
    LoadURL("https://example.com/");

    main_resource.Complete(String::Format(
        R"HTML(
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <img src='https://example.com/image.png' %s
             onload='console.log("image onload");' />
        </body>)HTML",
        kViewportHeight + kLoadingDistanceThreshold + 100, image_attributes));

    Compositor().BeginFrame();
    test::RunPendingTasks();
  }

  void TestLoadImageExpectingLazyLoad(const char* image_attributes) {
    LoadMainResourceWithImageFarFromViewport(image_attributes);
    EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
    EXPECT_FALSE(ConsoleMessages().Contains("image onload"));
  }

  void TestLoadImageExpectingLazyLoadWithoutPlaceholder(
      const char* image_attributes) {
    SimSubresourceRequest full_resource("https://example.com/image.png",
                                        "image/png");

    LoadMainResourceWithImageFarFromViewport(image_attributes);

    EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
    EXPECT_FALSE(ConsoleMessages().Contains("image onload"));

    // Scrolling down should trigger the fetch of the image.
    GetDocument().View()->LayoutViewport()->SetScrollOffset(
        ScrollOffset(0, kLoadingDistanceThreshold + kViewportHeight),
        mojom::blink::ScrollType::kProgrammatic);
    Compositor().BeginFrame();
    test::RunPendingTasks();
    full_resource.Complete(ReadTestImage());
    test::RunPendingTasks();

    EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
    EXPECT_TRUE(ConsoleMessages().Contains("image onload"));
    EXPECT_FALSE(GetDocument().IsUseCounted(
        WebFeature::kLazyLoadImageMissingDimensionsForLazy));
    EXPECT_TRUE(GetDocument().IsUseCounted(
        WebFeature::kLazyLoadImageLoadingAttributeLazy));
    EXPECT_FALSE(GetDocument().IsUseCounted(
        WebFeature::kLazyLoadImageLoadingAttributeEager));
  }

  void TestLoadImageExpectingFullImageLoad(const char* image_attributes) {
    SimSubresourceRequest full_resource("https://example.com/image.png",
                                        "image/png");

    LoadMainResourceWithImageFarFromViewport(image_attributes);

    EXPECT_FALSE(ConsoleMessages().Contains("main body onload"));
    EXPECT_FALSE(ConsoleMessages().Contains("image onload"));

    full_resource.Complete(ReadTestImage());

    Compositor().BeginFrame();
    test::RunPendingTasks();

    EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
    EXPECT_TRUE(ConsoleMessages().Contains("image onload"));
    EXPECT_FALSE(GetDocument().IsUseCounted(
        WebFeature::kLazyLoadImageMissingDimensionsForLazy));
    EXPECT_FALSE(GetDocument().IsUseCounted(
        WebFeature::kLazyLoadImageLoadingAttributeLazy));
    EXPECT_FALSE(GetDocument().IsUseCounted(
        WebFeature::kLazyLoadImageLoadingAttributeEager));
  }

 private:
  ScopedLazyImageLoadingForTest scoped_lazy_image_loading_for_test_;
  ScopedLazyImageVisibleLoadTimeMetricsForTest
      scoped_lazy_image_visible_load_time_metrics_for_test_ = true;
};

TEST_F(LazyLoadAutomaticImagesTest, LoadAllImagesIfPrinting) {
  TestLoadImageExpectingLazyLoad("id='my_image' loading='lazy'");

  // The body's load event should have already fired.
  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("child frame element onload"));

  Element* img = GetDocument().getElementById("my_image");
  ASSERT_TRUE(img);

  test::RunPendingTasks();

  EXPECT_FALSE(ConsoleMessages().Contains("image onload"));

  SimSubresourceRequest img_resource("https://example.com/image.png",
                                     "image/png");

  EXPECT_EQ(0, GetDocument().Fetcher()->BlockingRequestCount());

  EXPECT_TRUE(GetDocument().WillPrintSoon());

  // The loads in this case are blocking the load event.
  EXPECT_EQ(1, GetDocument().Fetcher()->BlockingRequestCount());

  img_resource.Complete(ReadTestImage());

  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_TRUE(ConsoleMessages().Contains("image onload"));
}

TEST_F(LazyLoadAutomaticImagesTest, AttributeChangedFromLazyToEager) {
  TestLoadImageExpectingLazyLoad("id='my_image' loading='lazy'");

  SimSubresourceRequest full_resource("https://example.com/image.png",
                                      "image/png");
  GetDocument()
      .getElementById("my_image")
      ->setAttribute(html_names::kLoadingAttr, "eager");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  full_resource.Complete(ReadTestImage());

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("image onload"));
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadImageMissingDimensionsForLazy));
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadImageLoadingAttributeLazy));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadImageLoadingAttributeEager));
}

TEST_F(LazyLoadAutomaticImagesTest, AttributeChangedFromAutoToEager) {
  TestLoadImageExpectingFullImageLoad("id='my_image' loading='auto'");

  SimSubresourceRequest full_resource("https://example.com/image.png",
                                      "image/png");
  GetDocument()
      .getElementById("my_image")
      ->setAttribute(html_names::kLoadingAttr, "eager");

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("image onload"));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadImageMissingDimensionsForLazy));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadImageLoadingAttributeLazy));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadImageLoadingAttributeEager));
}

TEST_F(LazyLoadAutomaticImagesTest, AttributeChangedFromUnsetToEager) {
  TestLoadImageExpectingFullImageLoad("id='my_image'");

  SimSubresourceRequest full_resource("https://example.com/image.png",
                                      "image/png");
  GetDocument()
      .getElementById("my_image")
      ->setAttribute(html_names::kLoadingAttr, "eager");

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("image onload"));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadImageMissingDimensionsForLazy));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadImageLoadingAttributeLazy));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadImageLoadingAttributeEager));
}

TEST_F(LazyLoadAutomaticImagesTest, TinyImageWithLazyAttr) {
  TestLoadImageExpectingLazyLoad("loading='lazy' width='1px' height='1px'");
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadImageLoadingAttributeLazy));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadImageLoadingAttributeEager));
}

TEST_F(LazyLoadAutomaticImagesTest, TinyImageViaStyleWithLazyAttr) {
  TestLoadImageExpectingLazyLoad(
      "loading='lazy' style='width:1px;height:1px;'");
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadImageLoadingAttributeLazy));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadImageLoadingAttributeEager));
}

TEST_F(LazyLoadAutomaticImagesTest, TinyImageWidth1Height1) {
  TestLoadImageExpectingFullImageLoad("width='1px' height='1px'");
}

TEST_F(LazyLoadAutomaticImagesTest, TinyImageWidth10Height10) {
  TestLoadImageExpectingFullImageLoad("width='10px' height='10px'");
}

TEST_F(LazyLoadAutomaticImagesTest, TinyImageWidth1Height11) {
  TestLoadImageExpectingLazyLoadWithoutPlaceholder(
      "width='1px' height='11px' loading='lazy'");
}

TEST_F(LazyLoadAutomaticImagesTest, TinyImageViaStyleWidth1Height1) {
  TestLoadImageExpectingFullImageLoad("style='width:1px;height:1px;'");
}

TEST_F(LazyLoadAutomaticImagesTest, TinyImageViaStyleWidth10Height10) {
  TestLoadImageExpectingFullImageLoad("style='width:10px;height:10px;'");
}

TEST_F(LazyLoadAutomaticImagesTest, TinyImageViaStyleWidth11Height1) {
  TestLoadImageExpectingLazyLoadWithoutPlaceholder(
      "style='width:11px;height:1px;'  loading='lazy'");
}

TEST_F(LazyLoadAutomaticImagesTest, JavascriptCreatedImageFarFromViewport) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimSubresourceRequest image_resource("https://example.com/image.png",
                                       "image/png");

  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <script>
          var my_image = new Image(50, 50);
          my_image.onload = function() { console.log('my_image onload'); };
          my_image.src = 'https://example.com/image.png';
          document.body.appendChild(my_image);
        </script>
        </body>)HTML",
      kViewportHeight + kLoadingDistanceThreshold + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_FALSE(ConsoleMessages().Contains("main body onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("my_image onload"));

  image_resource.Complete(ReadTestImage());

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("my_image onload"));
}

TEST_F(LazyLoadAutomaticImagesTest, JavascriptCreatedImageAddedAfterLoad) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimSubresourceRequest image_resource("https://example.com/image.png",
                                       "image/png");

  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <script>
          var my_image = new Image(50, 50);
          my_image.onload = function() {
            console.log('my_image onload');
            document.body.appendChild(this);
          };
          my_image.src = 'https://example.com/image.png';
        </script>
        </body>)HTML",
      kViewportHeight + kLoadingDistanceThreshold + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_FALSE(ConsoleMessages().Contains("main body onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("my_image onload"));

  image_resource.Complete(ReadTestImage());

  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("my_image onload"));
}

TEST_F(LazyLoadAutomaticImagesTest, ImageInsideLazyLoadedFrame) {
  ScopedLazyFrameLoadingForTest scoped_lazy_frame_loading_for_test(true);

  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(String::Format(
      R"HTML(
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <iframe src='https://example.com/child_frame.html' loading='lazy'
                id='child_frame' width='300px' height='300px'
                onload='console.log("child frame onload");'></iframe>
        </body>)HTML",
      kViewportHeight + kLoadingDistanceThreshold + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("child frame onload"));

  SimRequest child_frame_resource("https://example.com/child_frame.html",
                                  "text/html");

  // Scroll down so that the iframe is near the viewport, but the images within
  // it aren't near the viewport yet.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 150), mojom::blink::ScrollType::kProgrammatic);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  SimSubresourceRequest css_resource("https://example.com/style.css",
                                     "text/css");
  SimSubresourceRequest eager_resource("https://example.com/eager.png",
                                       "image/png");
  SimSubresourceRequest auto_resource("https://example.com/auto.png",
                                      "image/png");
  SimSubresourceRequest unset_resource("https://example.com/unset.png",
                                       "image/png");

  child_frame_resource.Complete(R"HTML(
      <head>
        <link rel='stylesheet' href='https://example.com/style.css' />
      </head>
      <body onload='window.parent.console.log("child body onload");'>
      <div style='height: 100px;'></div>
      <img src='https://example.com/eager.png' loading='eager'
           onload='window.parent.console.log("eager onload");' />
      <img src='https://example.com/lazy.png' loading='lazy'
           onload='window.parent.console.log("lazy onload");' />
      <img src='https://example.com/auto.png' loading='auto'
           onload='window.parent.console.log("auto onload");' />
      <img src='https://example.com/unset.png'
           onload='window.parent.console.log("unset onload");' />
      </body>)HTML");

  css_resource.Complete("img { width: 50px; height: 50px; }");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("child frame onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("child body onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("eager onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("lazy onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("auto onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("unset onload"));

  Vector<char> full_image = ReadTestImage();
  ASSERT_LT(2048U, full_image.size());
  Vector<char> partial_image;
  partial_image.Append(full_image.data(), 2048U);

  eager_resource.Complete(full_image);
  auto_resource.Complete(full_image);
  unset_resource.Complete(full_image);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("child frame onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("child body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("eager onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("lazy onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("auto onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("unset onload"));

  SimSubresourceRequest lazy_resource("https://example.com/lazy.png",
                                      "image/png");

  // Scroll down so that all the images in the iframe are near the viewport.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 250), mojom::blink::ScrollType::kProgrammatic);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  lazy_resource.Complete(full_image);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("child frame onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("child body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("eager onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("lazy onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("auto onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("unset onload"));
}

TEST_F(LazyLoadAutomaticImagesTest, AboveTheFoldImageLoadedBeforeVisible) {
  HistogramTester histogram_tester;

  SimRequest main_resource("https://example.com/", "text/html");
  SimSubresourceRequest image_resource("https://example.com/image.png",
                                       "image/png");

  LoadURL("https://example.com/");
  main_resource.Complete(
      "<body><img src='https://example.com/image.png' /></body>");
  image_resource.Complete(ReadTestImage());

  // VisibleLoadTime should not have been recorded yet, since the image is not
  // visible yet.
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleLoadTime.LazyLoadImages.AboveTheFold.4G", 0);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  histogram_tester.ExpectTotalCount(
      "Blink.VisibleBeforeLoaded.LazyLoadImages.AboveTheFold", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleBeforeLoaded.LazyLoadImages.BelowTheFold", 0);
  histogram_tester.ExpectUniqueSample(
      "Blink.VisibleLoadTime.LazyLoadImages.AboveTheFold.4G", 0, 1);
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleLoadTime.LazyLoadImages.BelowTheFold.4G", 0);
}

TEST_F(LazyLoadAutomaticImagesTest, AboveTheFoldImageVisibleBeforeLoaded) {
  HistogramTester histogram_tester;

  SimRequest main_resource("https://example.com/", "text/html");
  SimSubresourceRequest image_resource("https://example.com/image.png",
                                       "image/png");

  LoadURL("https://example.com/");
  main_resource.Complete(
      "<body><img src='https://example.com/image.png' loading='lazy'/></body>");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // VisibleBeforeLoaded should have been recorded immediately when the image
  // became visible.
  histogram_tester.ExpectUniqueSample(
      "Blink.VisibleBeforeLoaded.LazyLoadImages.AboveTheFold",
      static_cast<int>(WebEffectiveConnectionType::kType4G), 1);

  // VisibleLoadTime should not have been recorded yet, since the image is not
  // finished loading yet.
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleLoadTime.LazyLoadImages.AboveTheFold.4G", 0);

  image_resource.Complete(ReadTestImage());

  Compositor().BeginFrame();
  test::RunPendingTasks();

  histogram_tester.ExpectUniqueSample(
      "Blink.VisibleBeforeLoaded.LazyLoadImages.AboveTheFold",
      static_cast<int>(WebEffectiveConnectionType::kType4G), 1);
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleBeforeLoaded.LazyLoadImages.BelowTheFold", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleLoadTime.LazyLoadImages.AboveTheFold.4G", 1);
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleLoadTime.LazyLoadImages.BelowTheFold.4G", 0);
}

TEST_F(LazyLoadAutomaticImagesTest, BelowTheFoldImageLoadedBeforeVisible) {
  HistogramTester histogram_tester;

  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(String::Format(
      R"HTML(
        <body>
        <div style='height: %dpx;'></div>
        <img src='https://example.com/image.png' loading="lazy"/>
        </body>)HTML",
      kViewportHeight + kLoadingDistanceThreshold + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  SimSubresourceRequest image_resource("https://example.com/image.png",
                                       "image/png");

  // Scroll down such that the image is within kLoadingDistanceThreshold of the
  // viewport, but isn't visible yet.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 200), mojom::blink::ScrollType::kProgrammatic);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  image_resource.Complete(ReadTestImage());

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // VisibleLoadTime should not have been recorded yet, since the image is not
  // visible yet.
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleLoadTime.LazyLoadImages.BelowTheFold.4G", 0);

  // Scroll down such that the image is visible.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, kViewportHeight + kLoadingDistanceThreshold),
      mojom::blink::ScrollType::kProgrammatic);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  histogram_tester.ExpectTotalCount(
      "Blink.VisibleBeforeLoaded.LazyLoadImages.AboveTheFold", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleBeforeLoaded.LazyLoadImages.BelowTheFold", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleLoadTime.LazyLoadImages.AboveTheFold.4G", 0);
  histogram_tester.ExpectUniqueSample(
      "Blink.VisibleLoadTime.LazyLoadImages.BelowTheFold.4G", 0, 1);
}

TEST_F(LazyLoadAutomaticImagesTest, BelowTheFoldImageVisibleBeforeLoaded) {
  HistogramTester histogram_tester;

  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(String::Format(
      R"HTML(
        <body>
        <div style='height: %dpx;'></div>
        <img src='https://example.com/image.png' loading='lazy'/>
        </body>)HTML",
      kViewportHeight + kLoadingDistanceThreshold + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  SimSubresourceRequest image_resource("https://example.com/image.png",
                                       "image/png");

  // Scroll down such that the image is visible.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, kViewportHeight + kLoadingDistanceThreshold),
      mojom::blink::ScrollType::kProgrammatic);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // VisibleBeforeLoaded should have been recorded immediately when the image
  // became visible.
  histogram_tester.ExpectUniqueSample(
      "Blink.VisibleBeforeLoaded.LazyLoadImages.BelowTheFold",
      static_cast<int>(WebEffectiveConnectionType::kType4G), 1);

  // VisibleLoadTime should not have been recorded yet, since the image is not
  // finished loading yet.
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleLoadTime.LazyLoadImages.BelowTheFold.4G", 0);

  image_resource.Complete(ReadTestImage());

  Compositor().BeginFrame();
  test::RunPendingTasks();

  histogram_tester.ExpectTotalCount(
      "Blink.VisibleBeforeLoaded.LazyLoadImages.AboveTheFold", 0);
  histogram_tester.ExpectUniqueSample(
      "Blink.VisibleBeforeLoaded.LazyLoadImages.BelowTheFold",
      static_cast<int>(WebEffectiveConnectionType::kType4G), 1);
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleLoadTime.LazyLoadImages.AboveTheFold.4G", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleLoadTime.LazyLoadImages.BelowTheFold.4G", 1);
}

class DelayOutOfViewportLazyImagesTest : public SimTest {
 public:
  static constexpr int kViewportWidth = 800;
  static constexpr int kViewportHeight = 600;
  static constexpr int kDistanceThresholdPx = 1000;

  DelayOutOfViewportLazyImagesTest()
      : lazy_image_loading_for_test_(true),
        delay_out_of_viewport_for_test_(true) {}

  void SetUp() override {
    GetNetworkStateNotifier().SetNetworkConnectionInfoOverride(
        true /*on_line*/, kWebConnectionTypeWifi,
        WebEffectiveConnectionType::kType4G, 1000 /*http_rtt_msec*/,
        100 /*max_bandwidth_mbps*/);
    SimTest::SetUp();
    WebView().MainFrameViewWidget()->Resize(
        gfx::Size(kViewportWidth, kViewportHeight));

    Settings& settings = WebView().GetPage()->GetSettings();
    settings.SetLazyImageLoadingDistanceThresholdPx4G(kDistanceThresholdPx);
    settings.SetLazyLoadEnabled(true);
  }

 private:
  ScopedLazyImageLoadingForTest lazy_image_loading_for_test_;
  ScopedDelayOutOfViewportLazyImagesForTest delay_out_of_viewport_for_test_;
};

// Test that DelayOutOfViewportLazyImages causes lazy loading to use a viewport
// threshold of zero while the document loads, and that a non-zero threshold is
// used once the document finishes loading.
TEST_F(DelayOutOfViewportLazyImagesTest, DelayOutOfViewportLazyLoads) {
  SimRequest main_resource("https://a.com/", "text/html");
  SimSubresourceRequest in_viewport_resource("https://a.com/in_viewport.png",
                                             "image/png");
  SimSubresourceRequest near_viewport_resource(
      "https://a.com/near_viewport.png", "image/png");
  SimSubresourceRequest far_from_viewport_resource(
      "https://a.com/far_from_viewport.png", "image/png");

  LoadURL("https://a.com/");
  // Begin writing the document, but do not complete loading yet.
  main_resource.Write(R"HTML(
    <!doctype html>
    <html>
      <img src='https://a.com/in_viewport.png' loading='lazy'
        style='position:absolute; top:0; left:0; width:50px; height:50px;'
        id='in_viewport' />
      <img src='https://a.com/near_viewport.png' loading='lazy'
        style='position:absolute; top:101vh; left:0; width:50px; height:50px;'
        id='near_viewport' />
      <img src='https://a.com/far_from_viewport.png' loading='lazy'
        style='position:absolute; top:9999vh; left:0; width:50px; height:50px;'
        id='far_from_viewport' />
      )HTML");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  auto* in_viewport =
      To<HTMLImageElement>(GetDocument().getElementById("in_viewport"));
  auto* near_viewport =
      To<HTMLImageElement>(GetDocument().getElementById("near_viewport"));
  auto* far_from_viewport =
      To<HTMLImageElement>(GetDocument().getElementById("far_from_viewport"));

  // While loading (`main_resource` is not yet complete), only the in-viewport
  // image should be loading.
  EXPECT_TRUE(in_viewport->CachedImage()->IsLoading());
  EXPECT_FALSE(near_viewport->CachedImage()->IsLoading());
  EXPECT_FALSE(far_from_viewport->CachedImage()->IsLoading());

  // After the document completes loading, the lazy load threshold should
  // increase so the near-viewport image begins to load.
  main_resource.Complete("</html>");
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_TRUE(in_viewport->CachedImage()->IsLoading());
  EXPECT_TRUE(near_viewport->CachedImage()->IsLoading());
  EXPECT_FALSE(far_from_viewport->CachedImage()->IsLoading());
}

// Test that DelayOutOfViewportLazyImages has no effect on lazy loaded images
// inserted after the document has already loaded.
TEST_F(DelayOutOfViewportLazyImagesTest, DoNotDelayAfterDocumentLoads) {
  SimRequest main_resource("https://a.com/", "text/html");
  SimSubresourceRequest in_viewport_resource("https://a.com/in_viewport.png",
                                             "image/png");
  SimSubresourceRequest near_viewport_resource(
      "https://a.com/near_viewport.png", "image/png");
  SimSubresourceRequest far_from_viewport_resource(
      "https://a.com/far_from_viewport.png", "image/png");

  LoadURL("https://a.com/");
  main_resource.Complete("<!doctype html><html></html>");
  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(GetDocument().LoadEventFinished());

  // Insert three lazy loaded images and ensure they are loaded according to a
  // non-zero lazy loading viewport threshold.
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <img src='https://a.com/in_viewport.png' loading='lazy'
      style='position:absolute; top:0; left:0; width:50px; height:50px;'
      id='in_viewport' />
    <img src='https://a.com/near_viewport.png' loading='lazy'
      style='position:absolute; top:101vh; left:0; width:50px; height:50px;'
      id='near_viewport' />
    <img src='https://a.com/far_from_viewport.png' loading='lazy'
      style='position:absolute; top:9999vh; left:0; width:50px; height:50px;'
      id='far_from_viewport' />
    )HTML");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  auto* in_viewport =
      To<HTMLImageElement>(GetDocument().getElementById("in_viewport"));
  auto* near_viewport =
      To<HTMLImageElement>(GetDocument().getElementById("near_viewport"));
  auto* far_from_viewport =
      To<HTMLImageElement>(GetDocument().getElementById("far_from_viewport"));

  EXPECT_TRUE(in_viewport->CachedImage()->IsLoading());
  EXPECT_TRUE(near_viewport->CachedImage()->IsLoading());
  EXPECT_FALSE(far_from_viewport->CachedImage()->IsLoading());
}

}  // namespace

}  // namespace blink
