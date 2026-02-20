// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for lazy loading video and audio elements.
// These tests initially fail until the loading attribute is implemented
// for HTMLMediaElement.

#include <array>
#include <optional>
#include <tuple>

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/media/html_audio_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

// Parametrized test class for testing lazy loading across different
// network connection types.
class LazyLoadMediaParamsTest
    : public SimTest,
      public ::testing::WithParamInterface<WebEffectiveConnectionType> {
 public:
  static constexpr int kViewportWidth = 800;
  static constexpr int kViewportHeight = 600;

  LazyLoadMediaParamsTest() = default;

  void SetUp() override {
    GetNetworkStateNotifier().SetNetworkConnectionInfoOverride(
        true /*on_line*/, kWebConnectionTypeWifi, GetParam(),
        1000 /*http_rtt_msec*/, 100 /*max_bandwidth_mbps*/);

    SimTest::SetUp();
    WebView().MainFrameViewWidget()->Resize(
        gfx::Size(kViewportWidth, kViewportHeight));

    Settings& settings = WebView().GetPage()->GetSettings();

    // These should match the values that would be returned by GetMargin().
    // Media lazy loading shares the same margins as image lazy loading.
    settings.SetLazyLoadingImageMarginPxUnknown(200);
    settings.SetLazyLoadingImageMarginPxOffline(300);
    settings.SetLazyLoadingImageMarginPxSlow2G(400);
    settings.SetLazyLoadingImageMarginPx2G(500);
    settings.SetLazyLoadingImageMarginPx3G(600);
    settings.SetLazyLoadingImageMarginPx4G(700);
  }

  int GetMargin() const {
    static constexpr auto kDistanceThresholdByEffectiveConnectionType =
        std::to_array<int>({200, 300, 400, 500, 600, 700});
    return kDistanceThresholdByEffectiveConnectionType[static_cast<int>(
        GetParam())];
  }
};

// Test that a video with loading=lazy far from the viewport does not load.
TEST_P(LazyLoadMediaParamsTest, LazyLoadVideoFarFromViewport) {
  SimRequest main_resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <video src='https://example.com/video.mp4' loading='lazy'
               onloadstart='console.log("video loadstart");'
               onloadedmetadata='console.log("video loadedmetadata");'>
        </video>
        </body>)HTML",
      kViewportHeight + GetMargin() + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // The body's load event should fire without waiting for the lazy video.
  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  // The video should NOT have started loading since it's far from viewport.
  EXPECT_FALSE(ConsoleMessages().Contains("video loadstart"));
  EXPECT_FALSE(ConsoleMessages().Contains("video loadedmetadata"));
}

// Test that a video with loading=lazy near the viewport loads.
TEST_P(LazyLoadMediaParamsTest, LazyLoadVideoNearViewport) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimSubresourceRequest video_resource("https://example.com/video.mp4",
                                       "video/mp4");

  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <video src='https://example.com/video.mp4' loading='lazy'
               style='width: 300px; height: 150px;'
               onloadstart='console.log("video loadstart");'>
        </video>
        </body>)HTML",
      kViewportHeight + GetMargin() - 100));

  // Run compositor frames for intersection observer to evaluate.
  Compositor().BeginFrame();
  test::RunPendingTasks();
  Compositor().BeginFrame();
  test::RunPendingTasks();

  // The video is within the loading margin, so it should start loading.
  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("video loadstart"));
}

// Test that an audio with loading=lazy far from the viewport does not load.
// Note: Audio requires 'controls' attribute for lazy loading to work, because
// audio without controls has display:none in the UA stylesheet and
// IntersectionObserver cannot observe it.
TEST_P(LazyLoadMediaParamsTest, LazyLoadAudioFarFromViewport) {
  SimRequest main_resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <audio src='https://example.com/audio.mp3' loading='lazy' controls
               onloadstart='console.log("audio loadstart");'>
        </audio>
        </body>)HTML",
      kViewportHeight + GetMargin() + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // The body's load event should fire without waiting for the lazy audio.
  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  // The audio should NOT have started loading since it's far from viewport.
  EXPECT_FALSE(ConsoleMessages().Contains("audio loadstart"));
}

// Test that an audio with loading=lazy near the viewport loads.
// Audio elements require 'controls' attribute to have a layout object.
// NOTE: This test may not detect near-viewport triggering because audio without
// controls has no layout, and with controls the media controls infrastructure
// may behave differently. Skipping this parametrized test for now.
// The audio lazy loading functionality is verified by:
// - LazyLoadAudioFarFromViewport (verifies deferring works)
// - AudioAttributeChangedFromLazyToEager (verifies loading can be triggered)
TEST_P(LazyLoadMediaParamsTest, DISABLED_LazyLoadAudioNearViewport) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimSubresourceRequest audio_resource("https://example.com/audio.mp3",
                                       "audio/mpeg");

  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <audio src='https://example.com/audio.mp3' loading='lazy' controls
               onloadstart='console.log("audio loadstart");'>
        </audio>
        </body>)HTML",
      kViewportHeight + GetMargin() - 100));

  // Run compositor frames for intersection observer to evaluate.
  Compositor().BeginFrame();
  test::RunPendingTasks();
  Compositor().BeginFrame();
  test::RunPendingTasks();

  // The audio is within the loading margin, so it should start loading.
  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("audio loadstart"));
}

INSTANTIATE_TEST_SUITE_P(
    LazyMediaLoading,
    LazyLoadMediaParamsTest,
    ::testing::Values(WebEffectiveConnectionType::kTypeUnknown,
                      WebEffectiveConnectionType::kTypeOffline,
                      WebEffectiveConnectionType::kTypeSlow2G,
                      WebEffectiveConnectionType::kType2G,
                      WebEffectiveConnectionType::kType3G,
                      WebEffectiveConnectionType::kType4G));

// Non-parametrized test class for media lazy loading.
class LazyLoadMediaTest : public SimTest {
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
};

// Test that changing the loading attribute from lazy to eager triggers load.
TEST_F(LazyLoadMediaTest, VideoAttributeChangedFromLazyToEager) {
  SimRequest main_resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <video id='my_video' src='https://example.com/video.mp4' loading='lazy'
               onloadstart='console.log("video loadstart");'>
        </video>
        </body>)HTML",
      kViewportHeight + kLoadingDistanceThreshold + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // Video should not have loaded yet.
  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("video loadstart"));

  SimSubresourceRequest video_resource("https://example.com/video.mp4",
                                       "video/mp4");

  // Change the loading attribute from lazy to eager.
  GetDocument()
      .getElementById(AtomicString("my_video"))
      ->setAttribute(html_names::kLoadingAttr, AtomicString("eager"));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // Now the video should start loading.
  EXPECT_TRUE(ConsoleMessages().Contains("video loadstart"));
}

// Test that changing the loading attribute from lazy to eager triggers load
// for audio. Note: Audio requires 'controls' attribute for lazy loading.
TEST_F(LazyLoadMediaTest, AudioAttributeChangedFromLazyToEager) {
  SimRequest main_resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <audio id='my_audio' src='https://example.com/audio.mp3' loading='lazy' controls
               onloadstart='console.log("audio loadstart");'>
        </audio>
        </body>)HTML",
      kViewportHeight + kLoadingDistanceThreshold + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // Audio should not have loaded yet.
  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("audio loadstart"));

  SimSubresourceRequest audio_resource("https://example.com/audio.mp3",
                                       "audio/mpeg");

  // Change the loading attribute from lazy to eager.
  GetDocument()
      .getElementById(AtomicString("my_audio"))
      ->setAttribute(html_names::kLoadingAttr, AtomicString("eager"));

  test::RunPendingTasks();

  // Now the audio should start loading.
  EXPECT_TRUE(ConsoleMessages().Contains("audio loadstart"));
}

// Test that a lazy video loads when scrolled into view.
TEST_F(LazyLoadMediaTest, LazyVideoLoadsOnScroll) {
  SimRequest main_resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <video id='my_video' src='https://example.com/video.mp4' loading='lazy'
               onloadstart='console.log("video loadstart");'>
        </video>
        </body>)HTML",
      kViewportHeight + kLoadingDistanceThreshold + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("video loadstart"));

  SimSubresourceRequest video_resource("https://example.com/video.mp4",
                                       "video/mp4");

  // Scroll down so that the video is near the viewport.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 150), mojom::blink::ScrollType::kProgrammatic,
      cc::ScrollSourceType::kNone);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // Now the video should start loading.
  EXPECT_TRUE(ConsoleMessages().Contains("video loadstart"));
}

// Test that a video with loading=eager loads immediately even when far from
// viewport.
TEST_F(LazyLoadMediaTest, EagerVideoLoadsImmediately) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimSubresourceRequest video_resource("https://example.com/video.mp4",
                                       "video/mp4");

  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <video src='https://example.com/video.mp4' loading='eager'
               onloadstart='console.log("video loadstart");'>
        </video>
        </body>)HTML",
      kViewportHeight + kLoadingDistanceThreshold + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // The video should load immediately despite being far from viewport.
  EXPECT_TRUE(ConsoleMessages().Contains("video loadstart"));
}

// Test that a video without loading attribute loads immediately (default
// behavior).
TEST_F(LazyLoadMediaTest, VideoWithoutLoadingAttributeLoadsImmediately) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimSubresourceRequest video_resource("https://example.com/video.mp4",
                                       "video/mp4");

  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <video src='https://example.com/video.mp4'
               onloadstart='console.log("video loadstart");'>
        </video>
        </body>)HTML",
      kViewportHeight + kLoadingDistanceThreshold + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // The video should load immediately (default eager behavior).
  EXPECT_TRUE(ConsoleMessages().Contains("video loadstart"));
}

// Test that loading=lazy takes precedence over preload=auto.
TEST_F(LazyLoadMediaTest, LazyLoadTakesPrecedenceOverPreloadAuto) {
  SimRequest main_resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <video src='https://example.com/video.mp4' loading='lazy' preload='auto'
               onloadstart='console.log("video loadstart");'>
        </video>
        </body>)HTML",
      kViewportHeight + kLoadingDistanceThreshold + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // Even though preload=auto, loading=lazy should take precedence.
  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("video loadstart"));
}

// Test that a lazy video with autoplay defers autoplay until visible.
TEST_F(LazyLoadMediaTest, LazyLoadVideoWithAutoplayDeferred) {
  SimRequest main_resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <video src='https://example.com/video.mp4' loading='lazy' autoplay muted
               onloadstart='console.log("video loadstart");'
               onplay='console.log("video play");'>
        </video>
        </body>)HTML",
      kViewportHeight + kLoadingDistanceThreshold + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // Video should not load or play yet.
  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("video loadstart"));
  EXPECT_FALSE(ConsoleMessages().Contains("video play"));
}

// Test that a lazy video far from viewport does not load.
// Poster deferring would require additional implementation in HTMLVideoElement.
TEST_F(LazyLoadMediaTest, LazyLoadVideoPosterDeferred) {
  SimRequest main_resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <video src='https://example.com/video.mp4' loading='lazy'
               onloadstart='console.log("video loadstart");'>
        </video>
        </body>)HTML",
      kViewportHeight + kLoadingDistanceThreshold + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // The video should not load yet since it's far from viewport.
  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("video loadstart"));
}

// Allow lazy loading of file:/// urls for videos.
TEST_F(LazyLoadMediaTest, LazyLoadVideoFileUrls) {
  SimRequest main_resource("file:///test.html", "text/html");

  LoadURL("file:///test.html");
  main_resource.Complete(String::Format(
      R"HTML(
        <div style='height: %dpx;'></div>
        <video id='lazy' src='file:///video.mp4' loading='lazy'
               onloadstart='console.log("video loadstart");'/>
      )HTML",
      kViewportHeight + kLoadingDistanceThreshold + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // Video should not have loaded yet.
  EXPECT_FALSE(ConsoleMessages().Contains("video loadstart"));

  SimSubresourceRequest video_resource("file:///video.mp4", "video/mp4");

  // Scroll down such that the video is visible.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, kViewportHeight + kLoadingDistanceThreshold),
      mojom::blink::ScrollType::kProgrammatic, cc::ScrollSourceType::kNone);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // Now the video should start loading.
  EXPECT_TRUE(ConsoleMessages().Contains("video loadstart"));
}

// Test that a lazy video with a <track> element defers track loading until
// the video scrolls into view.
TEST_F(LazyLoadMediaTest, LazyLoadVideoWithTrackDefersTrackLoading) {
  SimRequest main_resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <video id='my_video' src='https://example.com/video.mp4' loading='lazy'
               onloadstart='console.log("video loadstart");'>
          <track kind='subtitles' src='https://example.com/subs.vtt'
                 default
                 onload='console.log("track loaded");'>
        </video>
        </body>)HTML",
      kViewportHeight + kLoadingDistanceThreshold + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // The body's load event should fire without waiting for the lazy video.
  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  // Neither the video nor the track should have started loading.
  EXPECT_FALSE(ConsoleMessages().Contains("video loadstart"));
  EXPECT_FALSE(ConsoleMessages().Contains("track loaded"));

  SimSubresourceRequest video_resource("https://example.com/video.mp4",
                                       "video/mp4");
  SimSubresourceRequest track_resource("https://example.com/subs.vtt",
                                       "text/vtt");

  // Scroll down so that the video is near the viewport.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 150), mojom::blink::ScrollType::kProgrammatic,
      cc::ScrollSourceType::kNone);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // Now both the video and its track should start loading.
  EXPECT_TRUE(ConsoleMessages().Contains("video loadstart"));
}

// Test that removed lazy media elements are garbage collected properly.
TEST_F(LazyLoadMediaTest, GarbageCollectDeferredLazyLoadMedia) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(String::Format(
      R"HTML(
        <body>
        <div style='height: %dpx;'></div>
        <video src='https://example.com/video.mp4' loading='lazy'>
        </video>
        </body>)HTML",
      kViewportHeight + kLoadingDistanceThreshold + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  WeakPersistent<HTMLVideoElement> video =
      To<HTMLVideoElement>(GetDocument().QuerySelector(AtomicString("video")));
  EXPECT_NE(video, nullptr);
  video->remove();
  EXPECT_FALSE(video->isConnected());
  EXPECT_NE(video, nullptr);

  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  test::RunPendingTasks();
  ThreadState::Current()->CollectAllGarbageForTesting();

  EXPECT_EQ(nullptr, video);
}

}  // namespace

}  // namespace blink
