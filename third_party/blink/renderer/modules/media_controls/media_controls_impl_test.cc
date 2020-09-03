// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"

#include <limits>
#include <memory>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/widget/screen_info.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/public/mojom/widget/screen_orientation.mojom-blink.h"
#include "third_party/blink/public/platform/modules/remoteplayback/web_remote_playback_client.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/document_style_environment_variables.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_parser.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_cast_button_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_current_time_display_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_download_button_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_mute_button_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_overflow_menu_button_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_overflow_menu_list_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_play_button_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_remaining_time_display_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_timeline_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_volume_slider_element.h"
#include "third_party/blink/renderer/modules/remoteplayback/remote_playback.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/testing/empty_web_media_player.h"
#include "third_party/blink/renderer/platform/testing/histogram_tester.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/web_test_support.h"

// The MediaTimelineWidths histogram suffix expected to be encountered in these
// tests.
#define TIMELINE_W "256_511"

namespace blink {

namespace {

class FakeChromeClient : public EmptyChromeClient {
 public:
  // ChromeClient overrides.
  ScreenInfo GetScreenInfo(LocalFrame&) const override {
    ScreenInfo screen_info;
    screen_info.orientation_type =
        mojom::blink::ScreenOrientation::kLandscapePrimary;
    return screen_info;
  }
};

class MockWebMediaPlayerForImpl : public EmptyWebMediaPlayer {
 public:
  // WebMediaPlayer overrides:
  WebTimeRanges Seekable() const override { return seekable_; }
  bool HasVideo() const override { return true; }
  bool HasAudio() const override { return has_audio_; }
  SurfaceLayerMode GetVideoSurfaceLayerMode() const override {
    return SurfaceLayerMode::kAlways;
  }

  bool has_audio_ = false;
  WebTimeRanges seekable_;
};

class MockLayoutObject : public LayoutObject {
 public:
  MockLayoutObject(Node* node) : LayoutObject(node) {}

  const char* GetName() const override { return "MockLayoutObject"; }
  void UpdateLayout() override {}
  FloatRect LocalBoundingBoxRectForAccessibility() const override {
    return FloatRect();
  }
};

class StubLocalFrameClientForImpl : public EmptyLocalFrameClient {
 public:
  std::unique_ptr<WebMediaPlayer> CreateWebMediaPlayer(
      HTMLMediaElement&,
      const WebMediaPlayerSource&,
      WebMediaPlayerClient*) override {
    return std::make_unique<MockWebMediaPlayerForImpl>();
  }

  WebRemotePlaybackClient* CreateWebRemotePlaybackClient(
      HTMLMediaElement& element) override {
    return &RemotePlayback::From(element);
  }
};

Element* GetElementByShadowPseudoId(Node& root_node,
                                    const char* shadow_pseudo_id) {
  for (Element& element : ElementTraversal::DescendantsOf(root_node)) {
    if (element.ShadowPseudoId() == shadow_pseudo_id)
      return &element;
  }
  return nullptr;
}

bool IsElementVisible(Element& element) {
  const CSSPropertyValueSet* inline_style = element.InlineStyle();

  if (!inline_style)
    return element.getAttribute("class") != "transparent";

  if (inline_style->GetPropertyValue(CSSPropertyID::kDisplay) == "none")
    return false;

  if (inline_style->HasProperty(CSSPropertyID::kOpacity) &&
      inline_style->GetPropertyValue(CSSPropertyID::kOpacity).ToDouble() ==
          0.0) {
    return false;
  }

  if (inline_style->GetPropertyValue(CSSPropertyID::kVisibility) == "hidden")
    return false;

  if (Element* parent = element.parentElement())
    return IsElementVisible(*parent);

  return true;
}

void SimulateTransitionEnd(Element& element) {
  element.DispatchEvent(*Event::Create(event_type_names::kTransitionend));
}

// This must match MediaControlDownloadButtonElement::DownloadActionMetrics.
enum DownloadActionMetrics {
  kShown = 0,
  kClicked,
  kCount  // Keep last.
};

}  // namespace

class MediaControlsImplTest : public PageTestBase,
                              private ScopedMediaCastOverlayButtonForTest {
 public:
  MediaControlsImplTest() : ScopedMediaCastOverlayButtonForTest(true) {}

 protected:
  void SetUp() override {
    InitializePage();
  }

  void InitializePage() {
    Page::PageClients clients;
    FillWithEmptyClients(clients);
    clients.chrome_client = MakeGarbageCollected<FakeChromeClient>();
    SetupPageWithClients(&clients,
                         MakeGarbageCollected<StubLocalFrameClientForImpl>());

    GetDocument().write("<video controls>");
    auto& video = To<HTMLVideoElement>(*GetDocument().QuerySelector("video"));
    media_controls_ = static_cast<MediaControlsImpl*>(video.GetMediaControls());

    // Scripts are disabled by default which forces controls to be on.
    GetFrame().GetSettings()->SetScriptEnabled(true);
  }

  void SimulateRouteAvailable() {
    RemotePlayback::From(media_controls_->MediaElement())
        .AvailabilityChangedForTesting(/* screen_is_available */ true);
  }

  void EnsureSizing() {
    // Fire the size-change callback to ensure that the controls have
    // been properly notified of the video size.
    media_controls_->NotifyElementSizeChanged(
        media_controls_->MediaElement().getBoundingClientRect());
  }

  void SimulateHideMediaControlsTimerFired() {
    media_controls_->HideMediaControlsTimerFired(nullptr);
  }

  void SimulateLoadedMetadata() { media_controls_->OnLoadedMetadata(); }

  void SimulateOnSeeking() { media_controls_->OnSeeking(); }
  void SimulateOnSeeked() { media_controls_->OnSeeked(); }

  MediaControlsImpl& MediaControls() { return *media_controls_; }
  MediaControlVolumeSliderElement* VolumeSliderElement() const {
    return media_controls_->volume_slider_;
  }
  MediaControlTimelineElement* TimelineElement() const {
    return media_controls_->timeline_;
  }
  Element* TimelineTrackElement() const {
    if (!TimelineElement())
      return nullptr;
    return &TimelineElement()->GetTrackElement();
  }
  MediaControlCurrentTimeDisplayElement* GetCurrentTimeDisplayElement() const {
    return media_controls_->current_time_display_;
  }
  MediaControlRemainingTimeDisplayElement* GetRemainingTimeDisplayElement()
      const {
    return media_controls_->duration_display_;
  }
  MediaControlMuteButtonElement* MuteButtonElement() const {
    return media_controls_->mute_button_;
  }
  MediaControlCastButtonElement* CastButtonElement() const {
    return media_controls_->cast_button_;
  }
  MediaControlDownloadButtonElement* DownloadButtonElement() const {
    return media_controls_->download_button_;
  }
  MediaControlPlayButtonElement* PlayButtonElement() const {
    return media_controls_->play_button_;
  }
  MediaControlOverflowMenuButtonElement* OverflowMenuButtonElement() const {
    return media_controls_->overflow_menu_;
  }
  MediaControlOverflowMenuListElement* OverflowMenuListElement() const {
    return media_controls_->overflow_list_;
  }

  MockWebMediaPlayerForImpl* WebMediaPlayer() {
    return static_cast<MockWebMediaPlayerForImpl*>(
        MediaControls().MediaElement().GetWebMediaPlayer());
  }

  HistogramTester& GetHistogramTester() { return histogram_tester_; }

  void LoadMediaWithDuration(double duration) {
    MediaControls().MediaElement().SetSrc("https://example.com/foo.mp4");
    test::RunPendingTasks();
    WebTimeRange time_range(0.0, duration);
    WebMediaPlayer()->seekable_.Assign(&time_range, 1);
    MediaControls().MediaElement().DurationChanged(duration,
                                                   false /* requestSeek */);
    SimulateLoadedMetadata();
  }

  void SetHasAudio(bool has_audio) { WebMediaPlayer()->has_audio_ = has_audio; }

  void ClickOverflowButton() {
    MediaControls()
        .download_button_->OverflowElementForTests()
        ->DispatchSimulatedClick(nullptr, kSendNoEvents,
                                 SimulatedClickCreationScope::kFromUserAgent);
  }

  void SetReady() {
    MediaControls().MediaElement().SetReadyState(
        HTMLMediaElement::kHaveEnoughData);
  }

  void MouseDownAt(gfx::PointF pos);
  void MouseMoveTo(gfx::PointF pos);
  void MouseUpAt(gfx::PointF pos);

  void GestureTapAt(gfx::PointF pos);
  void GestureDoubleTapAt(gfx::PointF pos);

  bool HasAvailabilityCallbacks(RemotePlayback& remote_playback) {
    return !remote_playback.availability_callbacks_.IsEmpty();
  }

  const String GetDisplayedTime(MediaControlTimeDisplayElement* display) {
    return To<Text>(display->firstChild())->data();
  }

  bool IsOverflowElementVisible(MediaControlInputElement& element) {
    MediaControlInputElement* overflow_element =
        element.OverflowElementForTests();
    if (!overflow_element)
      return false;

    Element* overflow_parent_label = overflow_element->parentElement();
    if (!overflow_parent_label)
      return false;

    const CSSPropertyValueSet* inline_style =
        overflow_parent_label->InlineStyle();
    if (inline_style->GetPropertyValue(CSSPropertyID::kDisplay) == "none")
      return false;

    return true;
  }

 private:
  Persistent<MediaControlsImpl> media_controls_;
  HistogramTester histogram_tester_;
};

void MediaControlsImplTest::MouseDownAt(gfx::PointF pos) {
  WebMouseEvent mouse_down_event(WebInputEvent::Type::kMouseDown,
                                 pos /* client pos */, pos /* screen pos */,
                                 WebPointerProperties::Button::kLeft, 1,
                                 WebInputEvent::Modifiers::kLeftButtonDown,
                                 WebInputEvent::GetStaticTimeStampForTests());
  mouse_down_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      mouse_down_event);
}

void MediaControlsImplTest::MouseMoveTo(gfx::PointF pos) {
  WebMouseEvent mouse_move_event(WebInputEvent::Type::kMouseMove,
                                 pos /* client pos */, pos /* screen pos */,
                                 WebPointerProperties::Button::kLeft, 1,
                                 WebInputEvent::Modifiers::kLeftButtonDown,
                                 WebInputEvent::GetStaticTimeStampForTests());
  mouse_move_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_event, {}, {});
}

void MediaControlsImplTest::MouseUpAt(gfx::PointF pos) {
  WebMouseEvent mouse_up_event(
      WebMouseEvent::Type::kMouseUp, pos /* client pos */, pos /* screen pos */,
      WebPointerProperties::Button::kLeft, 1, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests());
  mouse_up_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMouseReleaseEvent(
      mouse_up_event);
}

void MediaControlsImplTest::GestureTapAt(gfx::PointF pos) {
  WebGestureEvent gesture_tap_event(
      WebInputEvent::Type::kGestureTap, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests());

  // Adjust |pos| by current frame scale.
  float frame_scale = GetDocument().GetFrame()->PageZoomFactor();
  gesture_tap_event.SetFrameScale(frame_scale);
  pos.Scale(frame_scale);
  gesture_tap_event.SetPositionInWidget(pos);

  // Fire the event.
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      gesture_tap_event);
}

void MediaControlsImplTest::GestureDoubleTapAt(gfx::PointF pos) {
  GestureTapAt(pos);
  GestureTapAt(pos);
}

TEST_F(MediaControlsImplTest, HideAndShow) {
  Element* panel = GetElementByShadowPseudoId(MediaControls(),
                                              "-webkit-media-controls-panel");
  ASSERT_NE(nullptr, panel);

  ASSERT_TRUE(IsElementVisible(*panel));
  MediaControls().Hide();
  ASSERT_FALSE(IsElementVisible(*panel));
  MediaControls().MaybeShow();
  ASSERT_TRUE(IsElementVisible(*panel));
}

TEST_F(MediaControlsImplTest, Reset) {
  Element* panel = GetElementByShadowPseudoId(MediaControls(),
                                              "-webkit-media-controls-panel");
  ASSERT_NE(nullptr, panel);

  ASSERT_TRUE(IsElementVisible(*panel));
  MediaControls().Reset();
  ASSERT_TRUE(IsElementVisible(*panel));
}

TEST_F(MediaControlsImplTest, HideAndReset) {
  Element* panel = GetElementByShadowPseudoId(MediaControls(),
                                              "-webkit-media-controls-panel");
  ASSERT_NE(nullptr, panel);

  ASSERT_TRUE(IsElementVisible(*panel));
  MediaControls().Hide();
  ASSERT_FALSE(IsElementVisible(*panel));
  MediaControls().Reset();
  ASSERT_FALSE(IsElementVisible(*panel));
}

TEST_F(MediaControlsImplTest, ResetDoesNotTriggerInitialLayout) {
  Document& document = this->GetDocument();
  int old_element_count = document.GetStyleEngine().StyleForElementCount();
  // Also assert that there are no layouts yet.
  ASSERT_EQ(0, old_element_count);
  MediaControls().Reset();
  int new_element_count = document.GetStyleEngine().StyleForElementCount();
  ASSERT_EQ(old_element_count, new_element_count);
}

TEST_F(MediaControlsImplTest, CastButtonRequiresRoute) {
  EnsureSizing();

  MediaControlCastButtonElement* cast_button = CastButtonElement();
  ASSERT_NE(nullptr, cast_button);

  ASSERT_FALSE(IsOverflowElementVisible(*cast_button));

  SimulateRouteAvailable();
  ASSERT_TRUE(IsOverflowElementVisible(*cast_button));
}

TEST_F(MediaControlsImplTest, CastButtonDisableRemotePlaybackAttr) {
  EnsureSizing();

  MediaControlCastButtonElement* cast_button = CastButtonElement();
  ASSERT_NE(nullptr, cast_button);

  ASSERT_FALSE(IsOverflowElementVisible(*cast_button));
  SimulateRouteAvailable();
  ASSERT_TRUE(IsOverflowElementVisible(*cast_button));

  MediaControls().MediaElement().SetBooleanAttribute(
      html_names::kDisableremoteplaybackAttr, true);
  test::RunPendingTasks();
  ASSERT_FALSE(IsOverflowElementVisible(*cast_button));

  MediaControls().MediaElement().SetBooleanAttribute(
      html_names::kDisableremoteplaybackAttr, false);
  test::RunPendingTasks();
  ASSERT_TRUE(IsOverflowElementVisible(*cast_button));
}

TEST_F(MediaControlsImplTest, CastOverlayDefault) {
  MediaControls().MediaElement().SetBooleanAttribute(html_names::kControlsAttr,
                                                     false);

  Element* cast_overlay_button = GetElementByShadowPseudoId(
      MediaControls(), "-internal-media-controls-overlay-cast-button");
  ASSERT_NE(nullptr, cast_overlay_button);

  SimulateRouteAvailable();
  ASSERT_TRUE(IsElementVisible(*cast_overlay_button));
}

TEST_F(MediaControlsImplTest, CastOverlayDisabled) {
  MediaControls().MediaElement().SetBooleanAttribute(html_names::kControlsAttr,
                                                     false);

  ScopedMediaCastOverlayButtonForTest media_cast_overlay_button(false);

  Element* cast_overlay_button = GetElementByShadowPseudoId(
      MediaControls(), "-internal-media-controls-overlay-cast-button");
  ASSERT_NE(nullptr, cast_overlay_button);

  SimulateRouteAvailable();
  ASSERT_FALSE(IsElementVisible(*cast_overlay_button));
}

TEST_F(MediaControlsImplTest, CastOverlayDisableRemotePlaybackAttr) {
  MediaControls().MediaElement().SetBooleanAttribute(html_names::kControlsAttr,
                                                     false);

  Element* cast_overlay_button = GetElementByShadowPseudoId(
      MediaControls(), "-internal-media-controls-overlay-cast-button");
  ASSERT_NE(nullptr, cast_overlay_button);

  ASSERT_FALSE(IsElementVisible(*cast_overlay_button));
  SimulateRouteAvailable();
  ASSERT_TRUE(IsElementVisible(*cast_overlay_button));

  MediaControls().MediaElement().SetBooleanAttribute(
      html_names::kDisableremoteplaybackAttr, true);
  test::RunPendingTasks();
  ASSERT_FALSE(IsElementVisible(*cast_overlay_button));

  MediaControls().MediaElement().SetBooleanAttribute(
      html_names::kDisableremoteplaybackAttr, false);
  test::RunPendingTasks();
  ASSERT_TRUE(IsElementVisible(*cast_overlay_button));
}

TEST_F(MediaControlsImplTest, CastOverlayMediaControlsDisabled) {
  MediaControls().MediaElement().SetBooleanAttribute(html_names::kControlsAttr,
                                                     false);

  Element* cast_overlay_button = GetElementByShadowPseudoId(
      MediaControls(), "-internal-media-controls-overlay-cast-button");
  ASSERT_NE(nullptr, cast_overlay_button);

  EXPECT_FALSE(IsElementVisible(*cast_overlay_button));
  SimulateRouteAvailable();
  EXPECT_TRUE(IsElementVisible(*cast_overlay_button));

  GetDocument().GetSettings()->SetMediaControlsEnabled(false);
  EXPECT_FALSE(IsElementVisible(*cast_overlay_button));

  GetDocument().GetSettings()->SetMediaControlsEnabled(true);
  EXPECT_TRUE(IsElementVisible(*cast_overlay_button));
}

TEST_F(MediaControlsImplTest, CastOverlayDisabledMediaControlsDisabled) {
  MediaControls().MediaElement().SetBooleanAttribute(html_names::kControlsAttr,
                                                     false);

  ScopedMediaCastOverlayButtonForTest media_cast_overlay_button(false);

  Element* cast_overlay_button = GetElementByShadowPseudoId(
      MediaControls(), "-internal-media-controls-overlay-cast-button");
  ASSERT_NE(nullptr, cast_overlay_button);

  EXPECT_FALSE(IsElementVisible(*cast_overlay_button));
  SimulateRouteAvailable();
  EXPECT_FALSE(IsElementVisible(*cast_overlay_button));

  GetDocument().GetSettings()->SetMediaControlsEnabled(false);
  EXPECT_FALSE(IsElementVisible(*cast_overlay_button));

  GetDocument().GetSettings()->SetMediaControlsEnabled(true);
  EXPECT_FALSE(IsElementVisible(*cast_overlay_button));
}

TEST_F(MediaControlsImplTest, KeepControlsVisibleIfOverflowListVisible) {
  Element* overflow_list = GetElementByShadowPseudoId(
      MediaControls(), "-internal-media-controls-overflow-menu-list");
  ASSERT_NE(nullptr, overflow_list);

  Element* panel = GetElementByShadowPseudoId(MediaControls(),
                                              "-webkit-media-controls-panel");
  ASSERT_NE(nullptr, panel);

  MediaControls().MediaElement().SetSrc("http://example.com");
  MediaControls().MediaElement().Play();
  test::RunPendingTasks();

  MediaControls().MaybeShow();
  MediaControls().ToggleOverflowMenu();
  EXPECT_TRUE(IsElementVisible(*overflow_list));

  SimulateHideMediaControlsTimerFired();
  EXPECT_TRUE(IsElementVisible(*overflow_list));
  EXPECT_TRUE(IsElementVisible(*panel));
}

TEST_F(MediaControlsImplTest, DownloadButtonDisplayed) {
  EnsureSizing();

  MediaControlDownloadButtonElement* download_button = DownloadButtonElement();
  ASSERT_NE(nullptr, download_button);

  MediaControls().MediaElement().SetSrc("https://example.com/foo.mp4");
  test::RunPendingTasks();
  SimulateLoadedMetadata();

  // Download button should normally be displayed.
  EXPECT_TRUE(IsOverflowElementVisible(*download_button));
}

TEST_F(MediaControlsImplTest, DownloadButtonNotDisplayedEmptyUrl) {
  EnsureSizing();

  MediaControlDownloadButtonElement* download_button = DownloadButtonElement();
  ASSERT_NE(nullptr, download_button);

  // Download button should not be displayed when URL is empty.
  MediaControls().MediaElement().SetSrc("");
  test::RunPendingTasks();
  SimulateLoadedMetadata();
  EXPECT_FALSE(IsOverflowElementVisible(*download_button));
}

TEST_F(MediaControlsImplTest, DownloadButtonNotDisplayedInfiniteDuration) {
  EnsureSizing();

  MediaControlDownloadButtonElement* download_button = DownloadButtonElement();
  ASSERT_NE(nullptr, download_button);

  MediaControls().MediaElement().SetSrc("https://example.com/foo.mp4");
  test::RunPendingTasks();

  // Download button should not be displayed when duration is infinite.
  MediaControls().MediaElement().DurationChanged(
      std::numeric_limits<double>::infinity(), false /* requestSeek */);
  SimulateLoadedMetadata();
  EXPECT_FALSE(IsOverflowElementVisible(*download_button));

  // Download button should be shown if the duration changes back to finite.
  MediaControls().MediaElement().DurationChanged(20.0f,
                                                 false /* requestSeek */);
  SimulateLoadedMetadata();
  EXPECT_TRUE(IsOverflowElementVisible(*download_button));
}

TEST_F(MediaControlsImplTest, DownloadButtonNotDisplayedHLS) {
  EnsureSizing();

  MediaControlDownloadButtonElement* download_button = DownloadButtonElement();
  ASSERT_NE(nullptr, download_button);

  // Download button should not be displayed for HLS streams.
  MediaControls().MediaElement().SetSrc("https://example.com/foo.m3u8");
  test::RunPendingTasks();
  SimulateLoadedMetadata();
  EXPECT_FALSE(IsOverflowElementVisible(*download_button));
}

TEST_F(MediaControlsImplTest, TimelineSeekToRoundedEnd) {
  EnsureSizing();

  // Tests the case where the real length of the video, |exact_duration|, gets
  // rounded up slightly to |rounded_up_duration| when setting the timeline's
  // |max| attribute (crbug.com/695065).
  double exact_duration = 596.586667;
  double rounded_up_duration = 596.586667;
  LoadMediaWithDuration(exact_duration);

  // Simulate a click slightly past the end of the track of the timeline's
  // underlying <input type="range">. This would set the |value| to the |max|
  // attribute, which can be slightly rounded relative to the duration.
  MediaControlTimelineElement* timeline = TimelineElement();
  timeline->setValueAsNumber(rounded_up_duration, ASSERT_NO_EXCEPTION);
  ASSERT_EQ(rounded_up_duration, timeline->valueAsNumber());
  EXPECT_EQ(0.0, MediaControls().MediaElement().currentTime());
  timeline->DispatchInputEvent();
  EXPECT_EQ(exact_duration, MediaControls().MediaElement().currentTime());
}

TEST_F(MediaControlsImplTest, TimelineImmediatelyUpdatesCurrentTime) {
  EnsureSizing();

  MediaControlCurrentTimeDisplayElement* current_time_display =
      GetCurrentTimeDisplayElement();
  double duration = 600;
  LoadMediaWithDuration(duration);

  // Simulate seeking the underlying range to 50%. Current time display should
  // update synchronously (rather than waiting for media to finish seeking).
  TimelineElement()->setValueAsNumber(duration / 2, ASSERT_NO_EXCEPTION);
  TimelineElement()->DispatchInputEvent();
  EXPECT_EQ(duration / 2, current_time_display->CurrentValue());
}

TEST_F(MediaControlsImplTest, TimeIndicatorsUpdatedOnSeeking) {
  EnsureSizing();

  MediaControlCurrentTimeDisplayElement* current_time_display =
      GetCurrentTimeDisplayElement();
  MediaControlTimelineElement* timeline = TimelineElement();
  double duration = 1000;
  LoadMediaWithDuration(duration);

  EXPECT_EQ(0, current_time_display->CurrentValue());
  EXPECT_EQ(0, timeline->valueAsNumber());

  MediaControls().MediaElement().setCurrentTime(duration / 4);

  // Time indicators are not yet updated.
  EXPECT_EQ(0, current_time_display->CurrentValue());
  EXPECT_EQ(0, timeline->valueAsNumber());

  SimulateOnSeeking();

  // The time indicators should be updated immediately when the 'seeking' event
  // is fired.
  EXPECT_EQ(duration / 4, current_time_display->CurrentValue());
  EXPECT_EQ(duration / 4, timeline->valueAsNumber());
}

TEST_F(MediaControlsImplTest, TimelineMetricsClick) {
  double duration = 540;  // 9 minutes
  LoadMediaWithDuration(duration);
  EnsureSizing();
  test::RunPendingTasks();

  ASSERT_TRUE(IsElementVisible(*TimelineElement()));
  DOMRect* timelineRect = TimelineElement()->getBoundingClientRect();
  ASSERT_LT(0, timelineRect->width());

  EXPECT_EQ(0, MediaControls().MediaElement().currentTime());

  gfx::PointF trackCenter(timelineRect->left() + timelineRect->width() / 2,
                          timelineRect->top() + timelineRect->height() / 2);
  MouseDownAt(trackCenter);
  MouseUpAt(trackCenter);
  test::RunPendingTasks();

  EXPECT_LE(0.49 * duration, MediaControls().MediaElement().currentTime());
  EXPECT_GE(0.51 * duration, MediaControls().MediaElement().currentTime());

  GetHistogramTester().ExpectUniqueSample("Media.Timeline.SeekType." TIMELINE_W,
                                          0 /* SeekType::kClick */, 1);
  GetHistogramTester().ExpectTotalCount(
      "Media.Timeline.DragGestureDuration." TIMELINE_W, 0);
  GetHistogramTester().ExpectTotalCount(
      "Media.Timeline.DragPercent." TIMELINE_W, 0);
  GetHistogramTester().ExpectTotalCount(
      "Media.Timeline.DragSumAbsTimeDelta." TIMELINE_W, 0);
  GetHistogramTester().ExpectTotalCount(
      "Media.Timeline.DragTimeDelta." TIMELINE_W, 0);
}

TEST_F(MediaControlsImplTest, TimelineMetricsDragFromCurrentPosition) {
  double duration = 540;  // 9 minutes
  LoadMediaWithDuration(duration);
  EnsureSizing();
  test::RunPendingTasks();

  ASSERT_TRUE(IsElementVisible(*TimelineElement()));
  DOMRect* timeline_rect = TimelineTrackElement()->getBoundingClientRect();
  ASSERT_LT(0, timeline_rect->width());

  EXPECT_EQ(0, MediaControls().MediaElement().currentTime());

  DOMRect* thumb_rect =
      TimelineElement()
          ->UserAgentShadowRoot()
          ->getElementById(shadow_element_names::kIdSliderThumb)
          ->getBoundingClientRect();
  gfx::PointF thumb(thumb_rect->x() + (thumb_rect->width() / 2),
                    thumb_rect->y() + 1);

  float y = timeline_rect->top() + timeline_rect->height() / 2;
  gfx::PointF track_two_thirds(
      timeline_rect->left() + timeline_rect->width() * 2 / 3, y);
  MouseDownAt(thumb);
  MouseMoveTo(track_two_thirds);
  MouseUpAt(track_two_thirds);

  EXPECT_LE(0.66 * duration, MediaControls().MediaElement().currentTime());
  EXPECT_GE(0.68 * duration, MediaControls().MediaElement().currentTime());

  GetHistogramTester().ExpectUniqueSample(
      "Media.Timeline.SeekType." TIMELINE_W,
      1 /* SeekType::kDragFromCurrentPosition */, 1);
  GetHistogramTester().ExpectTotalCount(
      "Media.Timeline.DragGestureDuration." TIMELINE_W, 1);
  GetHistogramTester().ExpectUniqueSample(
      "Media.Timeline.DragPercent." TIMELINE_W, 47 /* [60.0%, 70.0%) */, 1);
  GetHistogramTester().ExpectUniqueSample(
      "Media.Timeline.DragSumAbsTimeDelta." TIMELINE_W, 16 /* [4m, 8m) */, 1);
  GetHistogramTester().ExpectUniqueSample(
      "Media.Timeline.DragTimeDelta." TIMELINE_W, 40 /* [4m, 8m) */, 1);
}

TEST_F(MediaControlsImplTest, TimelineMetricsDragFromElsewhere) {
  double duration = 540;  // 9 minutes
  LoadMediaWithDuration(duration);
  EnsureSizing();
  test::RunPendingTasks();

  ASSERT_TRUE(IsElementVisible(*TimelineElement()));
  DOMRect* timelineRect = TimelineTrackElement()->getBoundingClientRect();
  ASSERT_LT(0, timelineRect->width());

  EXPECT_EQ(0, MediaControls().MediaElement().currentTime());

  float y = timelineRect->top() + timelineRect->height() / 2;
  gfx::PointF trackOneThird(
      timelineRect->left() + timelineRect->width() * 1 / 3, y);
  gfx::PointF trackTwoThirds(
      timelineRect->left() + timelineRect->width() * 2 / 3, y);
  MouseDownAt(trackOneThird);
  MouseMoveTo(trackTwoThirds);
  MouseUpAt(trackTwoThirds);

  EXPECT_LE(0.66 * duration, MediaControls().MediaElement().currentTime());
  EXPECT_GE(0.68 * duration, MediaControls().MediaElement().currentTime());

  GetHistogramTester().ExpectUniqueSample("Media.Timeline.SeekType." TIMELINE_W,
                                          2 /* SeekType::kDragFromElsewhere */,
                                          1);
  GetHistogramTester().ExpectTotalCount(
      "Media.Timeline.DragGestureDuration." TIMELINE_W, 1);
  GetHistogramTester().ExpectUniqueSample(
      "Media.Timeline.DragPercent." TIMELINE_W, 42 /* [30.0%, 35.0%) */, 1);
  GetHistogramTester().ExpectUniqueSample(
      "Media.Timeline.DragSumAbsTimeDelta." TIMELINE_W, 15 /* [2m, 4m) */, 1);
  GetHistogramTester().ExpectUniqueSample(
      "Media.Timeline.DragTimeDelta." TIMELINE_W, 39 /* [2m, 4m) */, 1);
}

TEST_F(MediaControlsImplTest, TimelineMetricsDragBackAndForth) {
  double duration = 540;  // 9 minutes
  LoadMediaWithDuration(duration);
  EnsureSizing();
  test::RunPendingTasks();

  ASSERT_TRUE(IsElementVisible(*TimelineElement()));
  DOMRect* timelineRect = TimelineTrackElement()->getBoundingClientRect();
  ASSERT_LT(0, timelineRect->width());

  EXPECT_EQ(0, MediaControls().MediaElement().currentTime());

  float y = timelineRect->top() + timelineRect->height() / 2;
  gfx::PointF trackTwoThirds(
      timelineRect->left() + timelineRect->width() * 2 / 3, y);
  gfx::PointF trackEnd(timelineRect->left() + timelineRect->width(), y);
  gfx::PointF trackOneThird(
      timelineRect->left() + timelineRect->width() * 1 / 3, y);
  MouseDownAt(trackTwoThirds);
  MouseMoveTo(trackEnd);
  MouseMoveTo(trackOneThird);
  MouseUpAt(trackOneThird);

  EXPECT_LE(0.32 * duration, MediaControls().MediaElement().currentTime());
  EXPECT_GE(0.34 * duration, MediaControls().MediaElement().currentTime());

  GetHistogramTester().ExpectUniqueSample("Media.Timeline.SeekType." TIMELINE_W,
                                          2 /* SeekType::kDragFromElsewhere */,
                                          1);
  GetHistogramTester().ExpectTotalCount(
      "Media.Timeline.DragGestureDuration." TIMELINE_W, 1);
  GetHistogramTester().ExpectUniqueSample(
      "Media.Timeline.DragPercent." TIMELINE_W, 8 /* (-35.0%, -30.0%] */, 1);
  GetHistogramTester().ExpectUniqueSample(
      "Media.Timeline.DragSumAbsTimeDelta." TIMELINE_W, 17 /* [8m, 15m) */, 1);
  GetHistogramTester().ExpectUniqueSample(
      "Media.Timeline.DragTimeDelta." TIMELINE_W, 9 /* (-4m, -2m] */, 1);
}

TEST_F(MediaControlsImplTest, TimeIsCorrectlyFormatted) {
  struct {
    double time;
    String expected_result;
  } tests[] = {
      {-3661, "-1:01:01"},   {-1, "-0:01"},     {0, "0:00"},
      {1, "0:01"},           {15, "0:15"},      {125, "2:05"},
      {615, "10:15"},        {3666, "1:01:06"}, {75123, "20:52:03"},
      {360600, "100:10:00"},
  };

  double duration = 360600;  // Long enough to check each of the tests.
  LoadMediaWithDuration(duration);
  EnsureSizing();
  test::RunPendingTasks();

  MediaControlCurrentTimeDisplayElement* current_display =
      GetCurrentTimeDisplayElement();
  MediaControlRemainingTimeDisplayElement* duration_display =
      GetRemainingTimeDisplayElement();

  // The value and format of the duration display should be correct.
  EXPECT_EQ(360600, duration_display->CurrentValue());
  EXPECT_EQ("/ 100:10:00", GetDisplayedTime(duration_display));

  for (const auto& testcase : tests) {
    current_display->SetCurrentValue(testcase.time);

    // Current value should be updated.
    EXPECT_EQ(testcase.time, current_display->CurrentValue());

    // Display text should be updated and correctly formatted.
    EXPECT_EQ(testcase.expected_result, GetDisplayedTime(current_display));
  }
}

namespace {

class MediaControlsImplTestWithMockScheduler : public MediaControlsImplTest {
 public:
  MediaControlsImplTestWithMockScheduler() { EnablePlatform(); }

 protected:
  void SetUp() override {
    // DocumentParserTiming has DCHECKS to make sure time > 0.0.
    platform()->AdvanceClockSeconds(1);
    platform()->SetAutoAdvanceNowToPendingTasks(false);

    MediaControlsImplTest::SetUp();
  }

  void TearDown() override {
    platform()->SetAutoAdvanceNowToPendingTasks(true);
  }

  void ToggleOverflowMenu() {
    MediaControls().ToggleOverflowMenu();
    platform()->RunUntilIdle();
  }

  bool IsCursorHidden() {
    const CSSPropertyValueSet* style = MediaControls().InlineStyle();
    if (!style)
      return false;
    return style->GetPropertyValue(CSSPropertyID::kCursor) == "none";
  }
};

}  // namespace

TEST_F(MediaControlsImplTestWithMockScheduler, SeekingShowsControls) {
  Element* panel = GetElementByShadowPseudoId(MediaControls(),
                                              "-webkit-media-controls-panel");
  ASSERT_NE(nullptr, panel);

  MediaControls().MediaElement().SetSrc("http://example.com");
  MediaControls().MediaElement().Play();

  // Hide the controls to start.
  MediaControls().Hide();
  EXPECT_FALSE(IsElementVisible(*panel));

  // Seeking should cause the controls to become visible.
  SimulateOnSeeking();
  EXPECT_TRUE(IsElementVisible(*panel));
}

TEST_F(MediaControlsImplTestWithMockScheduler,
       SeekingDoesNotShowControlsWhenNoControlsAttr) {
  Element* panel = GetElementByShadowPseudoId(MediaControls(),
                                              "-webkit-media-controls-panel");
  ASSERT_NE(nullptr, panel);

  MediaControls().MediaElement().SetBooleanAttribute(html_names::kControlsAttr,
                                                     false);

  MediaControls().MediaElement().SetSrc("http://example.com");
  MediaControls().MediaElement().Play();

  // Hide the controls to start.
  MediaControls().Hide();
  EXPECT_FALSE(IsElementVisible(*panel));

  // Seeking should not cause the controls to become visible because the
  // controls attribute is not set.
  SimulateOnSeeking();
  EXPECT_FALSE(IsElementVisible(*panel));
}

TEST_F(MediaControlsImplTestWithMockScheduler,
       ControlsRemainVisibleDuringKeyboardInteraction) {
  EnsureSizing();

  Element* panel = MediaControls().PanelElement();

  MediaControls().MediaElement().SetSrc("http://example.com");
  MediaControls().MediaElement().Play();

  // Controls start out visible.
  EXPECT_TRUE(IsElementVisible(*panel));

  // Tabbing between controls prevents controls from hiding.
  platform()->RunForPeriodSeconds(2);
  MuteButtonElement()->DispatchEvent(*Event::CreateBubble("focusin"));
  platform()->RunForPeriodSeconds(2);
  EXPECT_TRUE(IsElementVisible(*panel));

  // Seeking on the timeline or volume bar prevents controls from hiding.
  TimelineElement()->DispatchEvent(*Event::CreateBubble("input"));
  platform()->RunForPeriodSeconds(2);
  EXPECT_TRUE(IsElementVisible(*panel));

  // Pressing a key prevents controls from hiding.
  MuteButtonElement()->DispatchEvent(*Event::CreateBubble("keypress"));
  platform()->RunForPeriodSeconds(2);
  EXPECT_TRUE(IsElementVisible(*panel));

  // Once user interaction stops, controls can hide.
  platform()->RunForPeriodSeconds(2);
  SimulateTransitionEnd(*panel);
  EXPECT_FALSE(IsElementVisible(*panel));
}

TEST_F(MediaControlsImplTestWithMockScheduler,
       ControlsHideAfterFocusedAndMouseMovement) {
  EnsureSizing();

  Element* panel = MediaControls().PanelElement();
  MediaControls().MediaElement().SetSrc("http://example.com");
  MediaControls().MediaElement().Play();

  // Controls start out visible
  EXPECT_TRUE(IsElementVisible(*panel));
  platform()->RunForPeriodSeconds(1);

  // Mouse move while focused
  MediaControls().DispatchEvent(*Event::Create("focusin"));
  MediaControls().MediaElement().SetFocused(true,
                                            mojom::blink::FocusType::kNone);
  MediaControls().DispatchEvent(*Event::Create("pointermove"));

  // Controls should remain visible
  platform()->RunForPeriodSeconds(2);
  EXPECT_TRUE(IsElementVisible(*panel));

  // Controls should hide after being inactive for 4 seconds.
  platform()->RunForPeriodSeconds(2);
  EXPECT_FALSE(IsElementVisible(*panel));
}

TEST_F(MediaControlsImplTestWithMockScheduler,
       ControlsHideAfterFocusedAndMouseMoveout) {
  EnsureSizing();

  Element* panel = MediaControls().PanelElement();
  MediaControls().MediaElement().SetSrc("http://example.com");
  MediaControls().MediaElement().Play();

  // Controls start out visible
  EXPECT_TRUE(IsElementVisible(*panel));
  platform()->RunForPeriodSeconds(1);

  // Mouse move out while focused, controls should hide
  MediaControls().DispatchEvent(*Event::Create("focusin"));
  MediaControls().MediaElement().SetFocused(true,
                                            mojom::blink::FocusType::kNone);
  MediaControls().DispatchEvent(*Event::Create("pointerout"));
  EXPECT_FALSE(IsElementVisible(*panel));
}

TEST_F(MediaControlsImplTestWithMockScheduler, CursorHidesWhenControlsHide) {
  EnsureSizing();

  MediaControls().MediaElement().SetSrc("http://example.com");

  // Cursor is not initially hidden.
  EXPECT_FALSE(IsCursorHidden());

  MediaControls().MediaElement().Play();

  // Tabbing into the controls shows the controls and therefore the cursor.
  MediaControls().DispatchEvent(*Event::Create("focusin"));
  EXPECT_FALSE(IsCursorHidden());

  // Once the controls hide, the cursor is hidden.
  platform()->RunForPeriodSeconds(4);
  EXPECT_TRUE(IsCursorHidden());

  // If the mouse moves, the controls are shown and the cursor is no longer
  // hidden.
  MediaControls().DispatchEvent(*Event::Create("pointermove"));
  EXPECT_FALSE(IsCursorHidden());

  // Once the controls hide again, the cursor is hidden again.
  platform()->RunForPeriodSeconds(4);
  EXPECT_TRUE(IsCursorHidden());
}

TEST_F(MediaControlsImplTestWithMockScheduler, AccessibleFocusShowsControls) {
  EnsureSizing();

  Element* panel = MediaControls().PanelElement();

  MediaControls().MediaElement().SetSrc("http://example.com");
  MediaControls().MediaElement().Play();

  platform()->RunForPeriodSeconds(2);
  EXPECT_TRUE(IsElementVisible(*panel));

  MediaControls().OnAccessibleFocus();
  platform()->RunForPeriodSeconds(2);
  EXPECT_TRUE(IsElementVisible(*panel));

  platform()->RunForPeriodSeconds(2);
  SimulateHideMediaControlsTimerFired();
  EXPECT_TRUE(IsElementVisible(*panel));

  MediaControls().OnAccessibleBlur();
  platform()->RunForPeriodSeconds(4);
  SimulateHideMediaControlsTimerFired();
  EXPECT_FALSE(IsElementVisible(*panel));
}

TEST_F(MediaControlsImplTest,
       RemovingFromDocumentRemovesListenersAndCallbacks) {
  auto page_holder = std::make_unique<DummyPageHolder>();

  auto* element =
      MakeGarbageCollected<HTMLVideoElement>(page_holder->GetDocument());
  page_holder->GetDocument().body()->AppendChild(element);

  RemotePlayback& remote_playback = RemotePlayback::From(*element);

  EXPECT_TRUE(remote_playback.HasEventListeners());
  EXPECT_TRUE(HasAvailabilityCallbacks(remote_playback));

  WeakPersistent<HTMLMediaElement> weak_persistent_video = element;
  {
    Persistent<HTMLMediaElement> persistent_video = element;
    page_holder->GetDocument().body()->setInnerHTML("");

    // When removed from the document, the event listeners should have been
    // dropped.
    EXPECT_FALSE(remote_playback.HasEventListeners());
    EXPECT_FALSE(HasAvailabilityCallbacks(remote_playback));
  }

  test::RunPendingTasks();

  ThreadState::Current()->CollectAllGarbageForTesting();

  // It has been GC'd.
  EXPECT_EQ(nullptr, weak_persistent_video);
}

TEST_F(MediaControlsImplTest,
       RemovingFromDocumentWhenResettingSrcAllowsReclamation) {
  // Regression test: https://crbug.com/918064
  //
  // Test ensures that unified heap garbage collections are able to collect
  // detached HTMLVideoElements. The tricky part is that ResizeObserver's are
  // treated as roots as long as they have observations which prevent the video
  // element from being collected.

  auto page_holder = std::make_unique<DummyPageHolder>();
  page_holder->GetDocument().write("<video controls>");
  page_holder->GetDocument().Parser()->Finish();

  auto& video =
      To<HTMLVideoElement>(*page_holder->GetDocument().QuerySelector("video"));
  WeakPersistent<HTMLMediaElement> weak_persistent_video = &video;
  video.remove();

  test::RunPendingTasks();

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(nullptr, weak_persistent_video);
}

TEST_F(MediaControlsImplTest,
       ReInsertingInDocumentRestoresListenersAndCallbacks) {
  auto page_holder = std::make_unique<DummyPageHolder>();

  auto* element =
      MakeGarbageCollected<HTMLVideoElement>(page_holder->GetDocument());
  page_holder->GetDocument().body()->AppendChild(element);

  RemotePlayback& remote_playback = RemotePlayback::From(*element);

  // This should be a no-op. We keep a reference on the media element to avoid
  // an unexpected GC.
  {
    Persistent<HTMLMediaElement> video_holder = element;
    page_holder->GetDocument().body()->RemoveChild(element);
    page_holder->GetDocument().body()->AppendChild(video_holder.Get());
    EXPECT_TRUE(remote_playback.HasEventListeners());
    EXPECT_TRUE(HasAvailabilityCallbacks(remote_playback));
  }
}

TEST_F(MediaControlsImplTest, InitialInfinityDurationHidesDurationField) {
  EnsureSizing();

  LoadMediaWithDuration(std::numeric_limits<double>::infinity());

  MediaControlRemainingTimeDisplayElement* duration_display =
      GetRemainingTimeDisplayElement();

  EXPECT_FALSE(duration_display->IsWanted());
  EXPECT_EQ(std::numeric_limits<double>::infinity(),
            duration_display->CurrentValue());
}

TEST_F(MediaControlsImplTest, InfinityDurationChangeHidesDurationField) {
  EnsureSizing();

  LoadMediaWithDuration(42);

  MediaControlRemainingTimeDisplayElement* duration_display =
      GetRemainingTimeDisplayElement();

  EXPECT_TRUE(duration_display->IsWanted());
  EXPECT_EQ(42, duration_display->CurrentValue());

  MediaControls().MediaElement().DurationChanged(
      std::numeric_limits<double>::infinity(), false /* request_seek */);
  test::RunPendingTasks();

  EXPECT_FALSE(duration_display->IsWanted());
  EXPECT_EQ(std::numeric_limits<double>::infinity(),
            duration_display->CurrentValue());
}

TEST_F(MediaControlsImplTestWithMockScheduler,
       ShowVolumeSliderAfterHoverTimerFired) {
  const double kTimeToShowVolumeSlider = 0.2;

  EnsureSizing();
  MediaControls().MediaElement().SetSrc("https://example.com/foo.mp4");
  platform()->RunForPeriodSeconds(1);
  SetHasAudio(true);
  SimulateLoadedMetadata();

  WebTestSupport::SetIsRunningWebTest(false);

  Element* volume_slider = VolumeSliderElement();
  Element* mute_btn = MuteButtonElement();

  ASSERT_NE(nullptr, volume_slider);
  ASSERT_NE(nullptr, mute_btn);

  EXPECT_TRUE(IsElementVisible(*mute_btn));
  EXPECT_TRUE(volume_slider->classList().contains("closed"));

  DOMRect* mute_btn_rect = mute_btn->getBoundingClientRect();
  gfx::PointF mute_btn_center(
      mute_btn_rect->left() + mute_btn_rect->width() / 2,
      mute_btn_rect->top() + mute_btn_rect->height() / 2);
  gfx::PointF edge(0, 0);

  // Hover on mute button and stay
  MouseMoveTo(mute_btn_center);
  platform()->RunForPeriodSeconds(kTimeToShowVolumeSlider - 0.001);
  EXPECT_TRUE(volume_slider->classList().contains("closed"));

  platform()->RunForPeriodSeconds(0.002);
  EXPECT_FALSE(volume_slider->classList().contains("closed"));

  MouseMoveTo(edge);
  EXPECT_TRUE(volume_slider->classList().contains("closed"));

  // Hover on mute button and move away before timer fired
  MouseMoveTo(mute_btn_center);
  platform()->RunForPeriodSeconds(kTimeToShowVolumeSlider - 0.001);
  EXPECT_TRUE(volume_slider->classList().contains("closed"));

  MouseMoveTo(edge);
  EXPECT_TRUE(volume_slider->classList().contains("closed"));
}

TEST_F(MediaControlsImplTestWithMockScheduler,
       VolumeSliderBehaviorWhenFocused) {
  MediaControls().MediaElement().SetSrc("https://example.com/foo.mp4");
  platform()->RunForPeriodSeconds(1);
  SetHasAudio(true);

  WebTestSupport::SetIsRunningWebTest(false);

  Element* volume_slider = VolumeSliderElement();

  ASSERT_NE(nullptr, volume_slider);

  // Volume slider starts out hidden
  EXPECT_TRUE(volume_slider->classList().contains("closed"));

  // Tab focus should open volume slider immediately.
  volume_slider->SetFocused(true, mojom::blink::FocusType::kNone);
  volume_slider->DispatchEvent(*Event::Create("focus"));
  EXPECT_FALSE(volume_slider->classList().contains("closed"));

  // Unhover slider while focused should not close slider.
  volume_slider->DispatchEvent(*Event::Create("mouseout"));
  EXPECT_FALSE(volume_slider->classList().contains("closed"));
}

TEST_F(MediaControlsImplTest, CastOverlayDefaultHidesOnTimer) {
  MediaControls().MediaElement().SetBooleanAttribute(html_names::kControlsAttr,
                                                     false);

  Element* cast_overlay_button = GetElementByShadowPseudoId(
      MediaControls(), "-internal-media-controls-overlay-cast-button");
  ASSERT_NE(nullptr, cast_overlay_button);

  SimulateRouteAvailable();
  EXPECT_TRUE(IsElementVisible(*cast_overlay_button));

  // Starts playback because overlay never hides if paused.
  MediaControls().MediaElement().SetSrc("http://example.com");
  MediaControls().MediaElement().Play();
  test::RunPendingTasks();

  SimulateHideMediaControlsTimerFired();
  EXPECT_FALSE(IsElementVisible(*cast_overlay_button));
}

TEST_F(MediaControlsImplTest, CastOverlayShowsOnSomeEvents) {
  MediaControls().MediaElement().SetBooleanAttribute(html_names::kControlsAttr,
                                                     false);

  Element* cast_overlay_button = GetElementByShadowPseudoId(
      MediaControls(), "-internal-media-controls-overlay-cast-button");
  ASSERT_NE(nullptr, cast_overlay_button);

  Element* overlay_enclosure = GetElementByShadowPseudoId(
      MediaControls(), "-webkit-media-controls-overlay-enclosure");
  ASSERT_NE(nullptr, overlay_enclosure);

  SimulateRouteAvailable();
  EXPECT_TRUE(IsElementVisible(*cast_overlay_button));

  // Starts playback because overlay never hides if paused.
  MediaControls().MediaElement().SetSrc("http://example.com");
  MediaControls().MediaElement().Play();
  test::RunPendingTasks();

  SimulateRouteAvailable();
  SimulateHideMediaControlsTimerFired();
  EXPECT_FALSE(IsElementVisible(*cast_overlay_button));

  for (auto* const event_name :
       {"gesturetap", "click", "pointerover", "pointermove"}) {
    overlay_enclosure->DispatchEvent(*Event::Create(event_name));
    EXPECT_TRUE(IsElementVisible(*cast_overlay_button));

    SimulateHideMediaControlsTimerFired();
    EXPECT_FALSE(IsElementVisible(*cast_overlay_button));
  }
}

TEST_F(MediaControlsImplTest, isConnected) {
  EXPECT_TRUE(MediaControls().isConnected());
  MediaControls().MediaElement().remove();
  EXPECT_FALSE(MediaControls().isConnected());
}

TEST_F(MediaControlsImplTest, ControlsShouldUseSafeAreaInsets) {
  UpdateAllLifecyclePhasesForTest();
  {
    const ComputedStyle* style = MediaControls().GetComputedStyle();
    EXPECT_EQ(0.0, style->MarginTop().Pixels());
    EXPECT_EQ(0.0, style->MarginLeft().Pixels());
    EXPECT_EQ(0.0, style->MarginBottom().Pixels());
    EXPECT_EQ(0.0, style->MarginRight().Pixels());
  }

  GetStyleEngine().EnsureEnvironmentVariables().SetVariable(
      "safe-area-inset-top", "1px");
  GetStyleEngine().EnsureEnvironmentVariables().SetVariable(
      "safe-area-inset-left", "2px");
  GetStyleEngine().EnsureEnvironmentVariables().SetVariable(
      "safe-area-inset-bottom", "3px");
  GetStyleEngine().EnsureEnvironmentVariables().SetVariable(
      "safe-area-inset-right", "4px");

  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  UpdateAllLifecyclePhasesForTest();

  {
    const ComputedStyle* style = MediaControls().GetComputedStyle();
    EXPECT_EQ(1.0, style->MarginTop().Pixels());
    EXPECT_EQ(2.0, style->MarginLeft().Pixels());
    EXPECT_EQ(3.0, style->MarginBottom().Pixels());
    EXPECT_EQ(4.0, style->MarginRight().Pixels());
  }
}

TEST_F(MediaControlsImplTest, MediaControlsDisabledWithNoSource) {
  EXPECT_EQ(MediaControls().State(), MediaControlsImpl::kNoSource);

  EXPECT_TRUE(PlayButtonElement()->FastHasAttribute(html_names::kDisabledAttr));
  EXPECT_TRUE(
      OverflowMenuButtonElement()->FastHasAttribute(html_names::kDisabledAttr));
  EXPECT_TRUE(TimelineElement()->FastHasAttribute(html_names::kDisabledAttr));

  MediaControls().MediaElement().setAttribute(html_names::kPreloadAttr, "none");
  MediaControls().MediaElement().SetSrc("https://example.com/foo.mp4");
  test::RunPendingTasks();
  SimulateLoadedMetadata();

  EXPECT_EQ(MediaControls().State(), MediaControlsImpl::kNotLoaded);

  EXPECT_FALSE(
      PlayButtonElement()->FastHasAttribute(html_names::kDisabledAttr));
  EXPECT_FALSE(
      OverflowMenuButtonElement()->FastHasAttribute(html_names::kDisabledAttr));
  EXPECT_TRUE(TimelineElement()->FastHasAttribute(html_names::kDisabledAttr));

  MediaControls().MediaElement().removeAttribute(html_names::kPreloadAttr);
  SimulateLoadedMetadata();

  EXPECT_EQ(MediaControls().State(), MediaControlsImpl::kLoadingMetadataPaused);

  EXPECT_FALSE(
      PlayButtonElement()->FastHasAttribute(html_names::kDisabledAttr));
  EXPECT_FALSE(
      OverflowMenuButtonElement()->FastHasAttribute(html_names::kDisabledAttr));
  EXPECT_FALSE(TimelineElement()->FastHasAttribute(html_names::kDisabledAttr));
}

TEST_F(MediaControlsImplTest, DoubleTouchChangesTime) {
  double duration = 60;  // 1 minute.
  LoadMediaWithDuration(duration);
  EnsureSizing();
  MediaControls().MediaElement().setCurrentTime(30);
  test::RunPendingTasks();

  // We've set the video to the halfway mark.
  EXPECT_EQ(30, MediaControls().MediaElement().currentTime());

  DOMRect* videoRect = MediaControls().MediaElement().getBoundingClientRect();
  ASSERT_LT(0, videoRect->width());
  gfx::PointF leftOfCenter(videoRect->left() + (videoRect->width() / 2) - 5,
                           videoRect->top() + 5);
  gfx::PointF rightOfCenter(videoRect->left() + (videoRect->width() / 2) + 5,
                            videoRect->top() + 5);

  // Double-tapping left of center should shift the time backwards by 10
  // seconds.
  GestureDoubleTapAt(leftOfCenter);
  test::RunPendingTasks();
  EXPECT_EQ(20, MediaControls().MediaElement().currentTime());

  // Double-tapping right of center should shift the time forwards by 10
  // seconds.
  GestureDoubleTapAt(rightOfCenter);
  test::RunPendingTasks();
  EXPECT_EQ(30, MediaControls().MediaElement().currentTime());
}

TEST_F(MediaControlsImplTest, DoubleTouchChangesTimeWhenZoomed) {
  double duration = 60;  // 1 minute.
  LoadMediaWithDuration(duration);
  EnsureSizing();
  MediaControls().MediaElement().setCurrentTime(30);
  test::RunPendingTasks();

  // We've set the video to the halfway mark.
  EXPECT_EQ(30, MediaControls().MediaElement().currentTime());

  DOMRect* videoRect = MediaControls().MediaElement().getBoundingClientRect();
  ASSERT_LT(0, videoRect->width());
  gfx::PointF leftOfCenter(videoRect->left() + (videoRect->width() / 2) - 5,
                           videoRect->top() + 10);
  gfx::PointF rightOfCenter(videoRect->left() + (videoRect->width() / 2) + 5,
                            videoRect->top() + 10);

  // Add a zoom factor and ensure that it's properly handled.
  MediaControls().GetDocument().GetFrame()->SetPageZoomFactor(2);

  // Double-tapping left of center should shift the time backwards by 10
  // seconds.
  GestureDoubleTapAt(leftOfCenter);
  test::RunPendingTasks();
  EXPECT_EQ(20, MediaControls().MediaElement().currentTime());

  // Double-tapping right of center should shift the time forwards by 10
  // seconds.
  GestureDoubleTapAt(rightOfCenter);
  test::RunPendingTasks();
  EXPECT_EQ(30, MediaControls().MediaElement().currentTime());
}

}  // namespace blink
