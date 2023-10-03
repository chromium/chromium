// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/lazy_load_image_observer.h"

#include <tuple>

#include "base/test/metrics/histogram_tester.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
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
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

const Vector<char>& TestImage() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      const Vector<char>, test_image,
      (test::ReadFromFile(test::CoreTestDataPath("notifications/500x500.png"))
           ->CopyAs<Vector<char>>()));
  return test_image;
}

class LazyLoadImagesSimTest : public SimTest {
 protected:
  void LoadMainResource(const String& html_body) {
    SimRequest main_resource("https://example.com/", "text/html");
    LoadURL("https://example.com/");

    main_resource.Complete(html_body);
    GetDocument().UpdateStyleAndLayoutTree();
  }
};

TEST_F(LazyLoadImagesSimTest, ImgSrcset) {
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
  image_resource.Complete(TestImage());
  test::RunPendingTasks();
  EXPECT_TRUE(ConsoleMessages().Contains("deferred_image onload"));

  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadImageLoadingAttributeLazy));
}

TEST_F(LazyLoadImagesSimTest, LazyLoadedImageSizeHistograms) {
  base::HistogramTester histogram_tester;
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
  lazy_a_resource.Complete(TestImage());
  Compositor().BeginFrame();
  test::RunPendingTasks();

  // We should have one lazy load sample, and one before-load lazy load sample.
  histogram_tester.ExpectTotalCount("Blink.LazyLoadedImage.Size", 1);
  int size_kb = TestImage().size() / 1024;
  histogram_tester.ExpectUniqueSample("Blink.LazyLoadedImage.Size", size_kb, 1);
  ASSERT_FALSE(GetDocument().LoadEventFinished());
  histogram_tester.ExpectTotalCount(
      "Blink.LazyLoadedImageBeforeDocumentOnLoad.Size", 1);
  histogram_tester.ExpectUniqueSample(
      "Blink.LazyLoadedImageBeforeDocumentOnLoad.Size", size_kb, 1);

  // Load the eager image which completes the document load.
  eager_resource.Complete(TestImage());
  Compositor().BeginFrame();
  test::RunPendingTasks();

  // Load should finish, but because the eager image is not lazy, the lazy load
  // metrics should not change.
  ASSERT_TRUE(GetDocument().LoadEventFinished());
  histogram_tester.ExpectTotalCount("Blink.LazyLoadedImage.Size", 1);
  histogram_tester.ExpectTotalCount(
      "Blink.LazyLoadedImageBeforeDocumentOnLoad.Size", 1);

  // Load the second lazy loaded image.
  lazy_b_resource.Complete(TestImage());
  Compositor().BeginFrame();
  test::RunPendingTasks();

  // We should still only have one before-load sample, but we should have two
  // lazy load samples overall.
  histogram_tester.ExpectTotalCount("Blink.LazyLoadedImage.Size", 2);
  histogram_tester.ExpectUniqueSample("Blink.LazyLoadedImage.Size", size_kb, 2);
  histogram_tester.ExpectTotalCount(
      "Blink.LazyLoadedImageBeforeDocumentOnLoad.Size", 1);
}

enum class LazyLoadImagesParams {
  kDelayOutOfViewportDisabled,
  kDelayOutOfViewportEnabled
};

class LazyLoadImagesParamsTest
    : public SimTest,
      public ::testing::WithParamInterface<
          std::tuple<LazyLoadImagesParams, WebEffectiveConnectionType>> {
 public:
  static constexpr int kViewportWidth = 800;
  static constexpr int kViewportHeight = 600;

  LazyLoadImagesParamsTest()
      : delay_out_of_viewport_lazy_images_(
            std::get<LazyLoadImagesParams>(GetParam()) ==
            LazyLoadImagesParams::kDelayOutOfViewportEnabled) {}

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

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_FALSE(ConsoleMessages().Contains("main body onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("eager onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("lazy onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("auto onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("unset onload"));

  eager_resource.Complete(TestImage());

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_FALSE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("eager onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("lazy onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("auto onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("unset onload"));

  auto_resource->Complete(TestImage());
  unset_resource->Complete(TestImage());

  // Run pending tasks to process load events from `auto_resource` and
  // `unset_resource`.
  test::RunPendingTasks();

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // `loading=lazy` never blocks the window load event.
  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("eager onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("lazy onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("auto onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("unset onload"));

  lazy_resource->Complete(TestImage());

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

  eager_resource.Complete(TestImage());

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_FALSE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("eager onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("lazy onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("auto onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("unset onload"));

  auto_resource->Complete(TestImage());
  unset_resource->Complete(TestImage());

  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("eager onload"));

  // Scroll down so that the images are near the viewport.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 150), mojom::blink::ScrollType::kProgrammatic);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_FALSE(ConsoleMessages().Contains("lazy onload"));

  lazy_resource->Complete(TestImage());

  Compositor().BeginFrame();
  test::RunPendingTasks();

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
        ::testing::Values(LazyLoadImagesParams::kDelayOutOfViewportEnabled,
                          LazyLoadImagesParams::kDelayOutOfViewportDisabled),
        ::testing::Values(WebEffectiveConnectionType::kTypeUnknown,
                          WebEffectiveConnectionType::kTypeOffline,
                          WebEffectiveConnectionType::kTypeSlow2G,
                          WebEffectiveConnectionType::kType2G,
                          WebEffectiveConnectionType::kType3G,
                          WebEffectiveConnectionType::kType4G)));

class LazyLoadImagesTest : public SimTest {
 public:
  static constexpr int kViewportWidth = 800;
  static constexpr int kViewportHeight = 600;
  static constexpr int kLoadingDistanceThreshold = 300;

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
  }

  String MakeMainResourceString(const char* image_attributes) {
    return String::Format(
        R"HTML(
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <img src='https://example.com/image.png' %s
             onload='console.log("image onload");' />
        </body>)HTML",
        kViewportHeight + kLoadingDistanceThreshold + 100, image_attributes);
  }

  void LoadMainResourceWithImageFarFromViewport(
      const String& main_resource_string) {
    SimRequest main_resource("https://example.com/", "text/html");
    LoadURL("https://example.com/");

    main_resource.Complete(main_resource_string);

    Compositor().BeginFrame();
    test::RunPendingTasks();
  }

  void LoadMainResourceWithImageFarFromViewport(const char* image_attributes) {
    LoadMainResourceWithImageFarFromViewport(
        MakeMainResourceString(image_attributes));
  }

  void TestLoadImageExpectingLazyLoad(const char* image_attributes) {
    LoadMainResourceWithImageFarFromViewport(image_attributes);
    EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
    EXPECT_FALSE(ConsoleMessages().Contains("image onload"));
  }

  void TestLoadImageExpectingFullImageLoad(const char* image_attributes) {
    SimSubresourceRequest full_resource("https://example.com/image.png",
                                        "image/png");

    LoadMainResourceWithImageFarFromViewport(image_attributes);

    EXPECT_FALSE(ConsoleMessages().Contains("main body onload"));
    EXPECT_FALSE(ConsoleMessages().Contains("image onload"));

    full_resource.Complete(TestImage());

    Compositor().BeginFrame();
    test::RunPendingTasks();

    EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
    EXPECT_TRUE(ConsoleMessages().Contains("image onload"));
    EXPECT_FALSE(GetDocument().IsUseCounted(
        WebFeature::kLazyLoadImageLoadingAttributeLazy));
    EXPECT_FALSE(GetDocument().IsUseCounted(
        WebFeature::kLazyLoadImageLoadingAttributeEager));
  }
};

TEST_F(LazyLoadImagesTest, LoadAllImagesIfPrinting) {
  TestLoadImageExpectingLazyLoad("id='my_image' loading='lazy'");

  // The body's load event should have already fired.
  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("child frame element onload"));

  Element* img = GetDocument().getElementById(AtomicString("my_image"));
  ASSERT_TRUE(img);

  test::RunPendingTasks();

  EXPECT_FALSE(ConsoleMessages().Contains("image onload"));

  SimSubresourceRequest img_resource("https://example.com/image.png",
                                     "image/png");

  EXPECT_EQ(0, GetDocument().Fetcher()->BlockingRequestCount());

  EXPECT_TRUE(GetDocument().WillPrintSoon());

  // The loads in this case are blocking the load event.
  EXPECT_EQ(1, GetDocument().Fetcher()->BlockingRequestCount());

  img_resource.Complete(TestImage());

  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_TRUE(ConsoleMessages().Contains("image onload"));
}

TEST_F(LazyLoadImagesTest, LoadAllImagesIfPrintingIFrame) {
  SimRequest iframe_resource("https://example.com/iframe.html", "text/html");

  const String main_resource =
      String::Format(R"HTML(
    <body onload='console.log("main body onload");'>
    <div style='height: %dpx;'></div>
    <iframe id='iframe' src='iframe.html'></iframe>
    <img src='https://example.com/top-image.png' loading='lazy'
         onload='console.log("main body image onload");'>
    </body>)HTML",
                     kViewportHeight + kLoadingDistanceThreshold + 100);
  LoadMainResourceWithImageFarFromViewport(main_resource);

  iframe_resource.Complete(R"HTML(
    <!doctype html>
    <body onload='console.log("iframe body onload");'>
    <img src='https://example.com/image.png' id='my_image' loading='lazy'
         onload='console.log("iframe image onload");'>
  )HTML");

  // The body's load event should have already fired.
  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("iframe body onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("main body image onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("iframe image onload"));

  auto* iframe = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("iframe")));
  ASSERT_TRUE(iframe);
  ASSERT_TRUE(iframe->ContentFrame());

  test::RunPendingTasks();

  EXPECT_FALSE(ConsoleMessages().Contains("main body image onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("iframe image onload"));

  SimSubresourceRequest img_resource("https://example.com/image.png",
                                     "image/png");

  Document* iframe_doc = To<LocalFrame>(iframe->ContentFrame())->GetDocument();
  ASSERT_TRUE(iframe_doc);
  EXPECT_EQ(0, iframe_doc->Fetcher()->BlockingRequestCount());
  EXPECT_EQ(0, GetDocument().Fetcher()->BlockingRequestCount());

  EXPECT_TRUE(iframe_doc->WillPrintSoon());

  // The loads in this case are blocking the load event.
  ASSERT_EQ(1, iframe_doc->Fetcher()->BlockingRequestCount());
  ASSERT_EQ(0, GetDocument().Fetcher()->BlockingRequestCount());

  img_resource.Complete(TestImage());

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_FALSE(ConsoleMessages().Contains("main body image onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("iframe image onload"));
}

TEST_F(LazyLoadImagesTest, AttributeChangedFromLazyToEager) {
  TestLoadImageExpectingLazyLoad("id='my_image' loading='lazy'");

  SimSubresourceRequest full_resource("https://example.com/image.png",
                                      "image/png");
  GetDocument()
      .getElementById(AtomicString("my_image"))
      ->setAttribute(html_names::kLoadingAttr, AtomicString("eager"));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  full_resource.Complete(TestImage());

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("image onload"));
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadImageLoadingAttributeLazy));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadImageLoadingAttributeEager));
}

TEST_F(LazyLoadImagesTest, AttributeChangedFromAutoToEager) {
  TestLoadImageExpectingFullImageLoad("id='my_image' loading='auto'");

  SimSubresourceRequest full_resource("https://example.com/image.png",
                                      "image/png");
  GetDocument()
      .getElementById(AtomicString("my_image"))
      ->setAttribute(html_names::kLoadingAttr, AtomicString("eager"));

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("image onload"));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadImageLoadingAttributeLazy));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadImageLoadingAttributeEager));
}

TEST_F(LazyLoadImagesTest, AttributeChangedFromUnsetToEager) {
  TestLoadImageExpectingFullImageLoad("id='my_image'");

  SimSubresourceRequest full_resource("https://example.com/image.png",
                                      "image/png");
  GetDocument()
      .getElementById(AtomicString("my_image"))
      ->setAttribute(html_names::kLoadingAttr, AtomicString("eager"));

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("image onload"));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadImageLoadingAttributeLazy));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadImageLoadingAttributeEager));
}

TEST_F(LazyLoadImagesTest, ImageInsideLazyLoadedFrame) {
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

  eager_resource.Complete(TestImage());
  auto_resource.Complete(TestImage());
  unset_resource.Complete(TestImage());

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

  lazy_resource.Complete(TestImage());

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

TEST_F(LazyLoadImagesTest, AboveTheFoldImageLoadedBeforeVisible) {
  base::HistogramTester histogram_tester;

  SimRequest main_resource("https://example.com/", "text/html");
  SimSubresourceRequest image_resource("https://example.com/image.png",
                                       "image/png");

  LoadURL("https://example.com/");
  // In-viewport lazy loaded images will be visible before being loaded because
  // the intersection observer only starts loading them after they are visible.
  // To work around this, we use a non-lazy image that synchronously loads which
  // causes the in-viewport lazy image to also be loaded before it is visible.
  main_resource.Complete(R"HTML(
        <!doctype html>
        <img src='https://example.com/image.png'/>
        <img src='https://example.com/image.png' loading="lazy"/>
      )HTML");
  image_resource.Complete(TestImage());

  // VisibleLoadTime should not have been recorded yet, since the image is not
  // visible yet.
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleLoadTime.LazyLoadImages.AboveTheFold3.4G", 0);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  histogram_tester.ExpectTotalCount(
      "Blink.VisibleBeforeLoaded.LazyLoadImages.AboveTheFold3", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleBeforeLoaded.LazyLoadImages.BelowTheFold3", 0);
  histogram_tester.ExpectUniqueSample(
      "Blink.VisibleLoadTime.LazyLoadImages.AboveTheFold3.4G", 0, 1);
  histogram_tester.ExpectUniqueSample("Blink.VisibleLoadTime.LazyLoadImages", 0,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "Blink.VisibleLoadTime.LazyLoadImages.AboveTheFold3", 0, 1);
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleLoadTime.LazyLoadImages.BelowTheFold3.4G", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleLoadTime.LazyLoadImages.BelowTheFold3", 0);
}

// A fully above-the-fold cached image should not report below-the-fold metrics.
TEST_F(LazyLoadImagesTest, AboveTheFoldCachedImageMetrics) {
  base::HistogramTester histogram_tester;
  SimRequest main_resource("https://example.com/", "text/html");
  SimSubresourceRequest image_resource("https://example.com/image.png",
                                       "image/png");
  LoadURL("https://example.com/");
  // Load a page with a non-lazy image that loads immediately, inserting the
  // image into the cache.
  main_resource.Complete(R"HTML(
        <!doctype html>
        <img id='image' src='https://example.com/image.png'/>
        <div id='container'></div>
      )HTML");
  image_resource.Complete(TestImage());
  Compositor().BeginFrame();
  test::RunPendingTasks();

  auto* image =
      To<HTMLImageElement>(GetDocument().getElementById(AtomicString("image")));
  EXPECT_TRUE(image->CachedImage()->IsLoaded());

  // Insert a lazy loaded image with a src that is already cached.
  auto* container = GetDocument().getElementById(AtomicString("container"));
  container->setInnerHTML(R"HTML(
    <img src='https://example.com/image.png' loading='lazy' id='lazy'/>
  )HTML");

  // The lazy image should have completed loading.
  auto* lazy_image =
      To<HTMLImageElement>(GetDocument().getElementById(AtomicString("lazy")));
  EXPECT_TRUE(lazy_image->CachedImage()->IsLoaded());

  // We should have a load time, but not yet `is_initially_intersecting`.
  auto& visible_load_time = lazy_image->EnsureVisibleLoadTimeMetrics();
  EXPECT_FALSE(visible_load_time.time_when_first_load_finished.is_null());
  EXPECT_FALSE(visible_load_time.has_initial_intersection_been_set);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // After a frame, the visibility metrics observer should fire and correctly
  // set `is_initially_intersecting`.
  EXPECT_TRUE(visible_load_time.has_initial_intersection_been_set);
  EXPECT_TRUE(visible_load_time.is_initially_intersecting);

  // Nothing was below the fold so no BelowTheFold metrics should be reported.
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleBeforeLoaded.LazyLoadImages.BelowTheFold3", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleLoadTime.LazyLoadImages.BelowTheFold3.4G", 0);
}

// An image that loads immediately due to being cached should not report
// Blink.VisibleBeforeLoaded.LazyLoadImages metrics.
TEST_F(LazyLoadImagesTest, CachedImageVisibleBeforeLoadedMetrics) {
  base::HistogramTester histogram_tester;
  SimRequest main_resource("https://a.com/", "text/html");
  SimSubresourceRequest image_resource("https://a.com/image.png", "image/png");
  LoadURL("https://a.com/");

  // Load a page with a non-lazy image that loads immediately, inserting the
  // image into the cache.
  main_resource.Complete(
      String::Format(R"HTML(
        <!doctype html>
        <div id='spacer' style='height: %dpx;'></div>
        <div id='container'></div>
        <!-- This non-lazy image will load immediately. -->
        <img src='https://a.com/image.png' />
      )HTML",
                     kViewportHeight + kLoadingDistanceThreshold - 50));
  image_resource.Complete(TestImage());
  Compositor().BeginFrame();
  test::RunPendingTasks();

  // Insert a lazy loaded image with a src that is already cached.
  auto* container = GetDocument().getElementById(AtomicString("container"));
  container->setInnerHTML("<img src='https://a.com/image.png' loading='lazy'>");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // VisibleBeforeLoaded should not be recorded since the image is not visible.
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleBeforeLoaded.LazyLoadImages.AboveTheFold3", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleBeforeLoaded.LazyLoadImages.BelowTheFold3", 0);

  // Scroll down so that the image is in the viewport.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, kViewportHeight + kLoadingDistanceThreshold + 50),
      mojom::blink::ScrollType::kProgrammatic);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // The image is now visible but loaded before being visible, so no
  // VisibleBeforeLoaded metrics should have been recorded.
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleBeforeLoaded.LazyLoadImages.AboveTheFold3", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleBeforeLoaded.LazyLoadImages.BelowTheFold3", 0);
}

TEST_F(LazyLoadImagesTest, AboveTheFoldImageVisibleBeforeLoaded) {
  base::HistogramTester histogram_tester;

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
      "Blink.VisibleBeforeLoaded.LazyLoadImages.AboveTheFold3",
      static_cast<int>(WebEffectiveConnectionType::kType4G), 1);

  // VisibleLoadTime should not have been recorded yet, since the image is not
  // finished loading yet.
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleLoadTime.LazyLoadImages.AboveTheFold3.4G", 0);

  image_resource.Complete(TestImage());

  Compositor().BeginFrame();
  test::RunPendingTasks();

  histogram_tester.ExpectUniqueSample(
      "Blink.VisibleBeforeLoaded.LazyLoadImages.AboveTheFold3",
      static_cast<int>(WebEffectiveConnectionType::kType4G), 1);
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleBeforeLoaded.LazyLoadImages.BelowTheFold3", 0);
  histogram_tester.ExpectTotalCount("Blink.VisibleLoadTime.LazyLoadImages", 1);
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleLoadTime.LazyLoadImages.AboveTheFold3", 1);
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleLoadTime.LazyLoadImages.BelowTheFold3", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleLoadTime.LazyLoadImages.AboveTheFold3.4G", 1);
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleLoadTime.LazyLoadImages.BelowTheFold3.4G", 0);
}

TEST_F(LazyLoadImagesTest, BelowTheFoldImageLoadedBeforeVisible) {
  base::HistogramTester histogram_tester;

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

  image_resource.Complete(TestImage());

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // VisibleLoadTime should not have been recorded yet, since the image is not
  // visible yet.
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleLoadTime.LazyLoadImages.BelowTheFold3.4G", 0);

  // Scroll down such that the image is visible.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, kViewportHeight + kLoadingDistanceThreshold),
      mojom::blink::ScrollType::kProgrammatic);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  histogram_tester.ExpectTotalCount(
      "Blink.VisibleBeforeLoaded.LazyLoadImages.AboveTheFold3", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleBeforeLoaded.LazyLoadImages.BelowTheFold3", 0);
  histogram_tester.ExpectUniqueSample("Blink.VisibleLoadTime.LazyLoadImages", 0,
                                      1);
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleLoadTime.LazyLoadImages.AboveTheFold3", 0);
  histogram_tester.ExpectUniqueSample(
      "Blink.VisibleLoadTime.LazyLoadImages.BelowTheFold3", 0, 1);
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleLoadTime.LazyLoadImages.AboveTheFold3.4G", 0);
  histogram_tester.ExpectUniqueSample(
      "Blink.VisibleLoadTime.LazyLoadImages.BelowTheFold3.4G", 0, 1);
}

TEST_F(LazyLoadImagesTest, BelowTheFoldImageVisibleBeforeLoaded) {
  base::HistogramTester histogram_tester;

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
      "Blink.VisibleBeforeLoaded.LazyLoadImages.BelowTheFold3",
      static_cast<int>(WebEffectiveConnectionType::kType4G), 1);

  // VisibleLoadTime should not have been recorded yet, since the image is not
  // finished loading yet.
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleLoadTime.LazyLoadImages.BelowTheFold3.4G", 0);

  image_resource.Complete(TestImage());

  Compositor().BeginFrame();
  test::RunPendingTasks();

  histogram_tester.ExpectTotalCount(
      "Blink.VisibleBeforeLoaded.LazyLoadImages.AboveTheFold3", 0);
  histogram_tester.ExpectUniqueSample(
      "Blink.VisibleBeforeLoaded.LazyLoadImages.BelowTheFold3",
      static_cast<int>(WebEffectiveConnectionType::kType4G), 1);
  histogram_tester.ExpectTotalCount("Blink.VisibleLoadTime.LazyLoadImages", 1);
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleLoadTime.LazyLoadImages.AboveTheFold3", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleLoadTime.LazyLoadImages.BelowTheFold3", 1);
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleLoadTime.LazyLoadImages.AboveTheFold3.4G", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleLoadTime.LazyLoadImages.BelowTheFold3.4G", 1);
}

// LazyLoadImages metrics should not be recorded for non-lazy image loads.
TEST_F(LazyLoadImagesTest, NonLazyIgnoredForLazyLoadImagesMetrics) {
  base::HistogramTester histogram_tester;

  SimRequest main_resource("https://aa.com/", "text/html");
  SimSubresourceRequest above_resource("https://aa.com/above.png", "image/png");
  SimSubresourceRequest below_resource("https://aa.com/below.png", "image/png");
  LoadURL("https://aa.com/");
  main_resource.Complete(
      String::Format(R"HTML(
        <!doctype html>
        <img src='https://aa.com/above.png'/>
        <div style='height: %dpx;'></div>
        <img src='https://aa.com/below.png'/>)HTML",
                     kViewportHeight + kLoadingDistanceThreshold + 100));
  above_resource.Complete(TestImage());
  below_resource.Complete(TestImage());

  Compositor().BeginFrame();
  test::RunPendingTasks();
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleBeforeLoaded.LazyLoadImages.AboveTheFold3", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleBeforeLoaded.LazyLoadImages.BelowTheFold3", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleLoadTime.LazyLoadImages.AboveTheFold3.4G", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.VisibleLoadTime.LazyLoadImages.BelowTheFold3.4G", 0);
}

// Allow lazy loading of file:/// urls.
TEST_F(LazyLoadImagesTest, LazyLoadFileUrls) {
  SimRequest main_resource("file:///test.html", "text/html");
  SimSubresourceRequest image_resource("file:///image.png", "image/png");

  LoadURL("file:///test.html");
  main_resource.Complete(String::Format(
      R"HTML(
        <div style='height: %dpx;'></div>
        <img id='lazy' src='file:///image.png' loading='lazy'/>
      )HTML",
      kViewportHeight + kLoadingDistanceThreshold + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  auto* lazy =
      To<HTMLImageElement>(GetDocument().getElementById(AtomicString("lazy")));
  EXPECT_FALSE(lazy->CachedImage()->IsLoading());

  // Scroll down such that the image is visible.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, kViewportHeight + kLoadingDistanceThreshold),
      mojom::blink::ScrollType::kProgrammatic);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(lazy->CachedImage()->IsLoading());
}

// This is a regression test added for https://crbug.com/1213045, which was
// filed for a memory leak whereby lazy loaded images currently being deferred
// but that were removed from the DOM were never actually garbage collected.
TEST_F(LazyLoadImagesTest, GarbageCollectDeferredLazyLoadImages) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(String::Format(
      R"HTML(
        <body>
        <div style='height: %dpx;'></div>
        <img src='https://example.com/image.png' loading='lazy'>
        </body>)HTML",
      kViewportHeight + kLoadingDistanceThreshold + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  WeakPersistent<HTMLImageElement> image =
      To<HTMLImageElement>(GetDocument().QuerySelector(AtomicString("img")));
  EXPECT_FALSE(image->complete());
  image->remove();
  EXPECT_FALSE(image->isConnected());
  EXPECT_FALSE(image->complete());
  EXPECT_NE(image, nullptr);

  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  test::RunPendingTasks();
  ThreadState::Current()->CollectAllGarbageForTesting();

  EXPECT_EQ(nullptr, image);
}

class DelayOutOfViewportLazyImagesTest : public SimTest {
 public:
  static constexpr int kViewportWidth = 800;
  static constexpr int kViewportHeight = 600;
  static constexpr int kDistanceThresholdPx = 1000;

  DelayOutOfViewportLazyImagesTest() : delay_out_of_viewport_for_test_(true) {}

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
  }

 private:
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

  auto* in_viewport = To<HTMLImageElement>(
      GetDocument().getElementById(AtomicString("in_viewport")));
  auto* near_viewport = To<HTMLImageElement>(
      GetDocument().getElementById(AtomicString("near_viewport")));
  auto* far_from_viewport = To<HTMLImageElement>(
      GetDocument().getElementById(AtomicString("far_from_viewport")));

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

  auto* in_viewport = To<HTMLImageElement>(
      GetDocument().getElementById(AtomicString("in_viewport")));
  auto* near_viewport = To<HTMLImageElement>(
      GetDocument().getElementById(AtomicString("near_viewport")));
  auto* far_from_viewport = To<HTMLImageElement>(
      GetDocument().getElementById(AtomicString("far_from_viewport")));

  EXPECT_TRUE(in_viewport->CachedImage()->IsLoading());
  EXPECT_TRUE(near_viewport->CachedImage()->IsLoading());
  EXPECT_FALSE(far_from_viewport->CachedImage()->IsLoading());
}

}  // namespace

}  // namespace blink
