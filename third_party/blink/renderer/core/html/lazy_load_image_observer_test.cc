// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/html/lazy_load_image_observer.h"

#include <optional>
#include <tuple>

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
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

const Vector<char>& TestImage() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(const Vector<char>, test_image,
                                  (*test::ReadFromFile(test::CoreTestDataPath(
                                      "notifications/500x500.png"))));
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

class LazyLoadImagesParamsTest
    : public SimTest,
      public ::testing::WithParamInterface<WebEffectiveConnectionType> {
 public:
  static constexpr int kViewportWidth = 800;
  static constexpr int kViewportHeight = 600;

  LazyLoadImagesParamsTest() = default;

  void SetUp() override {
    GetNetworkStateNotifier().SetNetworkConnectionInfoOverride(
        true /*on_line*/, kWebConnectionTypeWifi, GetParam(),
        1000 /*http_rtt_msec*/, 100 /*max_bandwidth_mbps*/);

    SimTest::SetUp();
    WebView().MainFrameViewWidget()->Resize(
        gfx::Size(kViewportWidth, kViewportHeight));

    Settings& settings = WebView().GetPage()->GetSettings();

    // These should match the values that would be returned by
    // GetMargin().
    settings.SetLazyLoadingImageMarginPxUnknown(200);
    settings.SetLazyLoadingImageMarginPxOffline(300);
    settings.SetLazyLoadingImageMarginPxSlow2G(400);
    settings.SetLazyLoadingImageMarginPx2G(500);
    settings.SetLazyLoadingImageMarginPx3G(600);
    settings.SetLazyLoadingImageMarginPx4G(700);
  }

  int GetMargin() const {
    static constexpr int kDistanceThresholdByEffectiveConnectionType[] = {
        200, 300, 400, 500, 600, 700};
    return kDistanceThresholdByEffectiveConnectionType[static_cast<int>(
        GetParam())];
  }
};

TEST_P(LazyLoadImagesParamsTest, NearViewport) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimSubresourceRequest css_resource("https://example.com/style.css",
                                     "text/css");

  SimSubresourceRequest eager_resource("https://example.com/eager.png",
                                       "image/png");
  std::optional<SimSubresourceRequest> lazy_resource, auto_resource,
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
      kViewportHeight + GetMargin() - 100));

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
  std::optional<SimSubresourceRequest> lazy_resource, auto_resource,
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
      kViewportHeight + GetMargin() + 100));

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
    ::testing::Values(WebEffectiveConnectionType::kTypeUnknown,
                      WebEffectiveConnectionType::kTypeOffline,
                      WebEffectiveConnectionType::kTypeSlow2G,
                      WebEffectiveConnectionType::kType2G,
                      WebEffectiveConnectionType::kType3G,
                      WebEffectiveConnectionType::kType4G));

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
    settings.SetLazyLoadingImageMarginPx4G(kLoadingDistanceThreshold);
    settings.SetLazyLoadingFrameMarginPx4G(kLoadingDistanceThreshold);
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

// This is a regression test added for https://crbug.com/40071424, which was
// filed as a result of outstanding decode promises *not* keeping an underlying
// lazyload-deferred image alive, even after removal from the DOM. Images of
// this sort must kept alive for the underlying decode request promise's sake.
TEST_F(LazyLoadImagesTest, DeferredLazyLoadImagesKeptAliveForDecodeRequest) {
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

  ScriptState* script_state =
      ToScriptStateForMainWorld(GetDocument().GetFrame());
  v8::HandleScope handle_scope(script_state->GetIsolate());
  // This creates an outstanding decode request for the underlying image, which
  // keeps it alive solely for the sake of the promise's existence.
  image->decode(script_state, ASSERT_NO_EXCEPTION);

  EXPECT_FALSE(image->complete());
  image->remove();
  EXPECT_FALSE(image->isConnected());
  EXPECT_FALSE(image->complete());
  EXPECT_NE(image, nullptr);

  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  test::RunPendingTasks();
  ThreadState::Current()->CollectAllGarbageForTesting();

  // After GC, the image is still non-null, since it is kept alive due to the
  // outstanding decode request.
  EXPECT_NE(image, nullptr);
}

}  // namespace

}  // namespace blink
