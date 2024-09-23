// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/html/lazy_load_frame_observer.h"

#include <memory>
#include <optional>
#include <tuple>

#include "base/ranges/algorithm.h"
#include "base/test/scoped_feature_list.h"
#include "third_party/blink/public/platform/web_effective_connection_type.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

namespace {

// Convenience enums to make it easy to access the appropriate value of the
// tuple parameters in the parameterized tests below, e.g. so that
// std::get<LazyFrameLoadingFeatureStatus>(GetParam()) can be used instead of
// std::get<0>(GetParam()) if they were just booleans.
enum class LazyFrameVisibleLoadTimeFeatureStatus { kDisabled, kEnabled };

class LazyLoadFramesParamsTest
    : public SimTest,
      public ::testing::WithParamInterface<
          std::tuple<LazyFrameVisibleLoadTimeFeatureStatus,
                     WebEffectiveConnectionType>> {
 public:
  static constexpr int kViewportWidth = 800;
  static constexpr int kViewportHeight = 600;

  void SetUp() override {
    WebEffectiveConnectionType ect =
        std::get<WebEffectiveConnectionType>(GetParam());
    GetNetworkStateNotifier().SetNetworkConnectionInfoOverride(
        true /*on_line*/, kWebConnectionTypeWifi, ect, 1000 /*http_rtt_msec*/,
        100 /*max_bandwidth_mbps*/);

    SimTest::SetUp();
    WebView().MainFrameWidget()->Resize(
        gfx::Size(kViewportWidth, kViewportHeight));

    Settings& settings = WebView().GetPage()->GetSettings();

    // These should match the values that would be returned by
    // GetLoadingDistanceThreshold().
    settings.SetLazyLoadingFrameMarginPxUnknown(200);
    settings.SetLazyLoadingFrameMarginPxOffline(300);
    settings.SetLazyLoadingFrameMarginPxSlow2G(400);
    settings.SetLazyLoadingFrameMarginPx2G(500);
    settings.SetLazyLoadingFrameMarginPx3G(600);
    settings.SetLazyLoadingFrameMarginPx4G(700);
    settings.SetLazyLoadEnabled(true);
  }

  int GetLoadingDistanceThreshold() const {
    static constexpr int kDistanceThresholdByEffectiveConnectionType[] = {
        200, 300, 400, 500, 600, 700};
    return kDistanceThresholdByEffectiveConnectionType[static_cast<int>(
        std::get<WebEffectiveConnectionType>(GetParam()))];
  }

  // Convenience function to load a page with a cross origin frame far down the
  // page such that it's not near the viewport.
  std::unique_ptr<SimRequest> LoadPageWithCrossOriginFrameFarFromViewport() {
    SimRequest main_resource("https://example.com/", "text/html");
    std::unique_ptr<SimRequest> child_frame_resource;

    LoadURL("https://example.com/");

    main_resource.Complete(String::Format(
        R"HTML(
          <body onload='console.log("main body onload");'>
          <div style='height: %dpx;'></div>
          <iframe src='https://crossorigin.com/subframe.html'
               style='width: 400px; height: 400px;' loading='lazy'
               onload='console.log("child frame element onload");'></iframe>
          </body>)HTML",
        kViewportHeight + GetLoadingDistanceThreshold() + 100));

    Compositor().BeginFrame();
    test::RunPendingTasks();

    // If the child frame is being lazy loaded, then the body's load event
    // should have already fired.
    EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
    EXPECT_FALSE(ConsoleMessages().Contains("child frame element onload"));

    if (!child_frame_resource) {
      child_frame_resource = std::make_unique<SimRequest>(
          "https://crossorigin.com/subframe.html", "text/html");
    }

    return child_frame_resource;
  }
};

TEST_P(LazyLoadFramesParamsTest, SameOriginFrame) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest child_frame_resource("https://example.com/subframe.html",
                                  "text/html");

  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <iframe src='https://example.com/subframe.html'
             style='width: 200px; height: 200px;'
             onload='console.log("child frame element onload");'></iframe>
        </body>)HTML",
      kViewportHeight + GetLoadingDistanceThreshold() + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  child_frame_resource.Complete("");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("child frame element onload"));
}

TEST_P(LazyLoadFramesParamsTest, AboveTheFoldFrame) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest child_frame_resource("https://crossorigin.com/subframe.html",
                                  "text/html");

  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <iframe src='https://crossorigin.com/subframe.html'
             style='width: 200px; height: 200px;' loading='lazy'
             onload='console.log("child frame element onload");'></iframe>
        </body>)HTML",
      kViewportHeight - 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  child_frame_resource.Complete("");
  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("child frame element onload"));
}

TEST_P(LazyLoadFramesParamsTest, BelowTheFoldButNearViewportFrame) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest child_frame_resource("https://crossorigin.com/subframe.html",
                                  "text/html");

  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <iframe src='https://crossorigin.com/subframe.html'
             style='width: 200px; height: 200px;' loading='lazy'
             onload='console.log("child frame element onload");'></iframe>
        </body>)HTML",
      kViewportHeight + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  child_frame_resource.Complete("");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("child frame element onload"));

  // Scroll down until the child frame is visible.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 150), mojom::blink::ScrollType::kProgrammatic);

  Compositor().BeginFrame();
  test::RunPendingTasks();
}

TEST_P(LazyLoadFramesParamsTest, LoadCrossOriginFrameFarFromViewport) {
  std::unique_ptr<SimRequest> child_frame_resource =
      LoadPageWithCrossOriginFrameFarFromViewport();

  // Scroll down near the child frame to cause the child frame to start loading.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 150), mojom::blink::ScrollType::kProgrammatic);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_FALSE(ConsoleMessages().Contains("child frame element onload"));

  child_frame_resource->Complete("");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("child frame element onload"));

  // Scroll down so that the child frame is visible.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, GetLoadingDistanceThreshold() + 150),
      mojom::blink::ScrollType::kProgrammatic);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("child frame element onload"));
}

TEST_P(LazyLoadFramesParamsTest,
       CrossOriginFrameFarFromViewportBecomesVisibleBeforeFinishedLoading) {
  std::unique_ptr<SimRequest> child_frame_resource =
      LoadPageWithCrossOriginFrameFarFromViewport();

  // Scroll down so that the child frame is visible.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, GetLoadingDistanceThreshold() + 150),
      mojom::blink::ScrollType::kProgrammatic);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_FALSE(ConsoleMessages().Contains("child frame element onload"));

  child_frame_resource->Complete("");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("child frame element onload"));
}

TEST_P(LazyLoadFramesParamsTest, NestedFrameInCrossOriginFrameFarFromViewport) {
  std::unique_ptr<SimRequest> child_frame_resource =
      LoadPageWithCrossOriginFrameFarFromViewport();

  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 150), mojom::blink::ScrollType::kProgrammatic);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // There's another nested cross origin iframe inside the first child frame,
  // even further down such that it's not near the viewport. It should start
  // loading immediately, even if LazyFrameLoading is enabled, since it's nested
  // inside a frame that was previously deferred.
  SimRequest nested_frame_resource("https://test.com/", "text/html");
  child_frame_resource->Complete(String::Format(
      "<div style='height: %dpx;'></div>"
      "<iframe src='https://test.com/' style='width: 200px; height: 200px;'>"
      "</iframe>",
      kViewportHeight + GetLoadingDistanceThreshold() + 100));

  nested_frame_resource.Complete("");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("child frame element onload"));
}

TEST_P(LazyLoadFramesParamsTest, AboutBlankChildFrameNavigation) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest child_frame_resource("https://crossorigin.com/subframe.html",
                                  "text/html");

  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
        <body onload='BodyOnload()'>
        <script>
          function BodyOnload() {
            console.log('main body onload');
            document.getElementsByTagName('iframe')[0].src =
                'https://crossorigin.com/subframe.html';
          }
        </script>

        <div style='height: %dpx;'></div>
        <iframe
             style='width: 200px; height: 200px;' loading='lazy'
             onload='console.log("child frame element onload");'></iframe>
        </body>)HTML",
      kViewportHeight + GetLoadingDistanceThreshold() + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_EQ(1, static_cast<int>(base::ranges::count(
                   ConsoleMessages(), "child frame element onload")));

  // Scroll down near the child frame to cause the child frame to start loading.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 150), mojom::blink::ScrollType::kProgrammatic);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  child_frame_resource.Complete("");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_EQ(2, static_cast<int>(base::ranges::count(
                   ConsoleMessages(), "child frame element onload")));
}

TEST_P(LazyLoadFramesParamsTest, JavascriptStringFrameUrl) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <iframe src='javascript:"Hello World!";'
             style='width: 200px; height: 200px;'
             onload='console.log("child frame element onload");'></iframe>
        </body>)HTML",
      kViewportHeight + GetLoadingDistanceThreshold() + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("child frame element onload"));
}

TEST_P(LazyLoadFramesParamsTest,
       CrossOriginFrameFarFromViewportWithLoadingAttributeEager) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest child_frame_resource("https://crossorigin.com/subframe.html",
                                  "text/html");

  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <iframe src='https://crossorigin.com/subframe.html'
             style='width: 200px; height: 200px;' loading='eager'
             onload='console.log("child frame element onload");'></iframe>
        </body>)HTML",
      kViewportHeight + GetLoadingDistanceThreshold() + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  child_frame_resource.Complete("");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("child frame element onload"));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadFrameLoadingAttributeLazy));
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadFrameLoadingAttributeEager));
}

TEST_P(LazyLoadFramesParamsTest,
       LoadSameOriginFrameFarFromViewportWithLoadingAttributeLazy) {
  SimRequest main_resource("https://example.com/", "text/html");
  std::optional<SimRequest> child_frame_resource;

  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
          <body onload='console.log("main body onload");'>
          <div style='height: %dpx;'></div>
          <iframe src='https://example.com/subframe.html'
               style='width: 400px; height: 400px;' loading='lazy'
               onload='console.log("child frame element onload");'></iframe>
          </body>)HTML",
      kViewportHeight + GetLoadingDistanceThreshold() + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // If the child frame is being lazy loaded, then the body's load event
  // should have already fired.
  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("child frame element onload"));
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadFrameLoadingAttributeLazy));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadFrameLoadingAttributeEager));

  if (!child_frame_resource) {
    child_frame_resource.emplace("https://example.com/subframe.html",
                                 "text/html");
  }

  // Scroll down near the child frame to cause the child frame to start loading.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 150), mojom::blink::ScrollType::kProgrammatic);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_FALSE(ConsoleMessages().Contains("child frame element onload"));

  child_frame_resource->Complete("");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("child frame element onload"));

  // Scroll down so that the child frame is visible.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, GetLoadingDistanceThreshold() + 150),
      mojom::blink::ScrollType::kProgrammatic);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("child frame element onload"));
}

TEST_P(LazyLoadFramesParamsTest,
       LoadCrossOriginFrameFarFromViewportThenSetLoadingAttributeEager) {
  SimRequest main_resource("https://example.com/", "text/html");
  std::optional<SimRequest> child_frame_resource;

  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <iframe id='child_frame' src='https://crossorigin.com/subframe.html'
             style='width: 400px; height: 400px;' loading='lazy'
             onload='console.log("child frame element onload");'></iframe>
        </body>)HTML",
      kViewportHeight + GetLoadingDistanceThreshold() + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // If the child frame is being lazy loaded, then the body's load event
  // should have already fired.
  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("child frame element onload"));

  if (!child_frame_resource) {
    child_frame_resource.emplace("https://crossorigin.com/subframe.html",
                                 "text/html");
  }

  Element* child_frame_element =
      GetDocument().getElementById(AtomicString("child_frame"));
  ASSERT_TRUE(child_frame_element);
  child_frame_element->setAttribute(html_names::kLoadingAttr,
                                    AtomicString("eager"));

  child_frame_resource->Complete("");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("child frame element onload"));
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadFrameLoadingAttributeLazy));
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadFrameLoadingAttributeEager));
}

TEST_P(LazyLoadFramesParamsTest,
       NestedFrameWithLazyLoadAttributeOnInFrameWithNoLoadingAttribute) {
  std::unique_ptr<SimRequest> child_frame_resource =
      LoadPageWithCrossOriginFrameFarFromViewport();

  // Scroll down near the child frame to cause the child frame to start loading.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 150), mojom::blink::ScrollType::kProgrammatic);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  child_frame_resource->Complete(
      String::Format("<div style='height: %dpx;'></div>"
                     "<iframe src='https://test.com/' loading='lazy'"
                     "     style='width: 200px; height: 200px;'>"
                     "</iframe>",
                     kViewportHeight + GetLoadingDistanceThreshold() + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("child frame element onload"));
}

TEST_P(LazyLoadFramesParamsTest,
       NestedFrameWithLazyLoadAttributeOnInFrameWithLoadingAttributeEager) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest child_frame_resource("https://crossorigin.com/subframe.html",
                                  "text/html");

  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <iframe src='https://crossorigin.com/subframe.html'
             style='width: 200px; height: 200px;' loading='eager'
             onload='console.log("child frame element onload");'></iframe>
        </body>)HTML",
      kViewportHeight + GetLoadingDistanceThreshold() + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadFrameLoadingAttributeLazy));
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadFrameLoadingAttributeEager));

  child_frame_resource.Complete(
      String::Format("<div style='height: %dpx;'></div>"
                     "<iframe src='https://test.com/' loading='lazy'"
                     "     style='width: 200px; height: 200px;'>"
                     "</iframe>",
                     kViewportHeight + GetLoadingDistanceThreshold() + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("child frame element onload"));
}

TEST_P(LazyLoadFramesParamsTest,
       NestedFrameWithLazyLoadAttributeOffInFrameWithLoadingAttributeEager) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest child_frame_resource("https://crossorigin.com/subframe.html",
                                  "text/html");

  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <iframe src='https://crossorigin.com/subframe.html'
             style='width: 200px; height: 200px;' loading='eager'
             onload='console.log("child frame element onload");'></iframe>
        </body>)HTML",
      kViewportHeight + GetLoadingDistanceThreshold() + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // There's another nested cross origin iframe inside the first child frame,
  // even further down such that it's not near the viewport. Since it has the
  // attribute loading=eager, it shouldn't be deferred. Note that this also
  // matches the default behavior that would happen if the load attribute was
  // omitted on the nested iframe entirely.
  SimRequest nested_frame_resource("https://test.com/", "text/html");

  child_frame_resource.Complete(
      String::Format("<div style='height: %dpx;'></div>"
                     "<iframe src='https://test.com/' loading='eager'"
                     "     style='width: 200px; height: 200px;'>"
                     "</iframe>",
                     kViewportHeight + GetLoadingDistanceThreshold() + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  nested_frame_resource.Complete("");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("child frame element onload"));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadFrameLoadingAttributeLazy));
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadFrameLoadingAttributeEager));
}

INSTANTIATE_TEST_SUITE_P(
    LazyFrameLoading,
    LazyLoadFramesParamsTest,
    ::testing::Combine(
        ::testing::Values(LazyFrameVisibleLoadTimeFeatureStatus::kDisabled,
                          LazyFrameVisibleLoadTimeFeatureStatus::kEnabled),
        ::testing::Values(WebEffectiveConnectionType::kTypeUnknown,
                          WebEffectiveConnectionType::kTypeOffline,
                          WebEffectiveConnectionType::kTypeSlow2G,
                          WebEffectiveConnectionType::kType2G,
                          WebEffectiveConnectionType::kType3G,
                          WebEffectiveConnectionType::kType4G)));

class LazyLoadFramesTest : public SimTest {
 public:
  static constexpr int kViewportWidth = 800;
  static constexpr int kViewportHeight = 600;
  static constexpr int kLoadingDistanceThresholdPx = 1000;

  void SetUp() override {
    GetNetworkStateNotifier().SetSaveDataEnabledOverride(false);
    GetNetworkStateNotifier().SetNetworkConnectionInfoOverride(
        true /*on_line*/, kWebConnectionTypeWifi,
        WebEffectiveConnectionType::kType4G, 1000 /*http_rtt_msec*/,
        100 /*max_bandwidth_mbps*/);

    SimTest::SetUp();
    WebView().MainFrameWidget()->Resize(
        gfx::Size(kViewportWidth, kViewportHeight));

    Settings& settings = WebView().GetPage()->GetSettings();
    settings.SetLazyLoadingFrameMarginPx4G(kLoadingDistanceThresholdPx);
    settings.SetLazyLoadEnabled(true);
  }

  void TearDown() override {
    GetNetworkStateNotifier().SetSaveDataEnabledOverride(false);
    SimTest::TearDown();
  }

  void TestCrossOriginFrameIsImmediatelyLoaded(const char* iframe_attributes) {
    SimRequest main_resource("https://example.com/", "text/html");
    SimRequest child_frame_resource("https://crossorigin.com/subframe.html",
                                    "text/html");
    LoadURL("https://example.com/");

    main_resource.Complete(String::Format(
        R"HTML(
          <body onload='console.log("main body onload");'>
          <div style='height: %dpx;'></div>
          <iframe src='https://crossorigin.com/subframe.html'
               style='width: 200px; height: 200px;' %s
               onload='console.log("child frame element onload");'></iframe>
          </body>)HTML",
        kViewportHeight + kLoadingDistanceThresholdPx + 100,
        iframe_attributes));

    Compositor().BeginFrame();
    test::RunPendingTasks();

    child_frame_resource.Complete("");
    Compositor().BeginFrame();
    test::RunPendingTasks();

    EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
    EXPECT_TRUE(ConsoleMessages().Contains("child frame element onload"));
  }

  void TestCrossOriginFrameIsLazilyLoaded(const char* iframe_attributes) {
    SimRequest main_resource("https://example.com/", "text/html");
    LoadURL("https://example.com/");

    main_resource.Complete(String::Format(
        R"HTML(
          <body onload='console.log("main body onload");'>
          <div style='height: %dpx;'></div>
          <iframe src='https://crossorigin.com/subframe.html'
               style='width: 200px; height: 200px;' %s
               onload='console.log("child frame element onload");'></iframe>
          </body>)HTML",
        kViewportHeight + kLoadingDistanceThresholdPx + 100,
        iframe_attributes));

    Compositor().BeginFrame();
    test::RunPendingTasks();

    // The body's load event should have already fired.
    EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
    EXPECT_FALSE(ConsoleMessages().Contains("child frame element onload"));

    SimRequest child_frame_resource("https://crossorigin.com/subframe.html",
                                    "text/html");

    // Scroll down near the child frame to cause the child frame to start
    // loading.
    GetDocument().View()->LayoutViewport()->SetScrollOffset(
        ScrollOffset(0, 150), mojom::blink::ScrollType::kProgrammatic);

    Compositor().BeginFrame();
    test::RunPendingTasks();

    EXPECT_FALSE(ConsoleMessages().Contains("child frame element onload"));

    child_frame_resource.Complete("");

    Compositor().BeginFrame();
    test::RunPendingTasks();

    EXPECT_TRUE(ConsoleMessages().Contains("child frame element onload"));
  }

  void TestLazyLoadUsedInPageReload(const char* iframe_attributes,
                                    bool is_deferral_expected_on_reload) {
    ConsoleMessages().clear();

    SimRequest main_resource("https://example.com/", "text/html");
    MainFrame().StartReload(WebFrameLoadType::kReload);

    main_resource.Complete(String::Format(
        R"HTML(
            <body onload='console.log("main body onload");'>
            <div style='height: %dpx;'></div>
            <iframe src='https://crossorigin.com/subframe.html' %s
                 style='width: 400px; height: 400px;'
                 onload='console.log("child frame element onload");'></iframe>
            </body>)HTML",
        LazyLoadFramesTest::kViewportHeight +
            LazyLoadFramesTest::kLoadingDistanceThresholdPx + 100,
        iframe_attributes));

    if (is_deferral_expected_on_reload) {
      // The body's load event should have already fired.
      EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
      EXPECT_FALSE(ConsoleMessages().Contains("child frame element onload"));

      GetDocument().View()->LayoutViewport()->SetScrollOffset(
          ScrollOffset(0, 150), mojom::blink::ScrollType::kProgrammatic);

      SimRequest child_frame_resource("https://crossorigin.com/subframe.html",
                                      "text/html");
      Compositor().BeginFrame();
      test::RunPendingTasks();
      child_frame_resource.Complete("");
      test::RunPendingTasks();

      // Scroll down near the child frame to cause the child frame to start
      // loading.
      GetDocument().View()->LayoutViewport()->SetScrollOffset(
          ScrollOffset(0, LazyLoadFramesTest::kViewportHeight +
                              LazyLoadFramesTest::kLoadingDistanceThresholdPx),
          mojom::blink::ScrollType::kProgrammatic);

      Compositor().BeginFrame();
      test::RunPendingTasks();
    } else {
      SimRequest child_frame_resource("https://crossorigin.com/subframe.html",
                                      "text/html");
      Compositor().BeginFrame();
      test::RunPendingTasks();
      child_frame_resource.Complete("");
    }
    test::RunPendingTasks();
    EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
    EXPECT_TRUE(ConsoleMessages().Contains("child frame element onload"));
  }
};

TEST_F(LazyLoadFramesTest, LazyLoadFrameUnsetLoadingAttributeWithoutAutomatic) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <iframe id='child_frame' src='https://crossorigin.com/subframe.html'
             loading='lazy' style='width: 200px; height: 200px;'
             onload='console.log("child frame element onload");'></iframe>
        </body>)HTML",
      kViewportHeight + kLoadingDistanceThresholdPx + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // The body's load event should have already fired.
  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("child frame element onload"));

  SimRequest child_frame_resource("https://crossorigin.com/subframe.html",
                                  "text/html");

  Element* child_frame_element =
      GetDocument().getElementById(AtomicString("child_frame"));
  ASSERT_TRUE(child_frame_element);
  child_frame_element->removeAttribute(html_names::kLoadingAttr);

  test::RunPendingTasks();

  EXPECT_FALSE(ConsoleMessages().Contains("child frame element onload"));

  child_frame_resource.Complete("");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("child frame element onload"));
}

TEST_F(LazyLoadFramesTest, LazyLoadWhenAttrLazy) {
  TestCrossOriginFrameIsLazilyLoaded("loading='lazy'");
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadFrameLoadingAttributeLazy));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadFrameLoadingAttributeEager));
}

TEST_F(LazyLoadFramesTest, LazyLoadWhenAttrEager) {
  TestCrossOriginFrameIsImmediatelyLoaded("loading='eager'");
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadFrameLoadingAttributeLazy));
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kLazyLoadFrameLoadingAttributeEager));
}

TEST_F(LazyLoadFramesTest, LazyLoadWhenAutomaticDisabled) {
  TestCrossOriginFrameIsImmediatelyLoaded("");
}

// Frames with loading=lazy should be deferred.
TEST_F(LazyLoadFramesTest, DeferredForAttributeLazy) {
  TestCrossOriginFrameIsLazilyLoaded("loading='lazy'");
  TestLazyLoadUsedInPageReload("loading='lazy'", true);
}

}  // namespace

}  // namespace blink
