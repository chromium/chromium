// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"

#include <limits>
#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/public/platform/modules/remoteplayback/web_remote_playback_client.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_pointer_event_init.h"
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
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_fullscreen_button_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_mute_button_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_overflow_menu_button_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_overflow_menu_list_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_play_button_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_playback_speed_button_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_remaining_time_display_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_timeline_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_volume_slider_element.h"
#include "third_party/blink/renderer/modules/remoteplayback/remote_playback.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/empty_web_media_player.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "ui/display/mojom/screen_orientation.mojom-blink.h"
#include "ui/display/screen_info.h"

// The MediaTimelineWidths histogram suffix expected to be encountered in these
// tests.
#define TIMELINE_W "256_511"

namespace blink {

namespace {

class FakeChromeClient : public EmptyChromeClient {
 public:
  FakeChromeClient() {
    screen_info_.orientation_type =
        display::mojom::blink::ScreenOrientation::kLandscapePrimary;
  }

  // ChromeClient overrides.
  const display::ScreenInfo& GetScreenInfo(LocalFrame&) const override {
    return screen_info_;
  }

 private:
  display::ScreenInfo screen_info_;
};

class MockWebMediaPlayerForImpl : public EmptyWebMediaPlayer {
 public:
  // WebMediaPlayer overrides:
  WebTimeRanges Seekable() const override { return seekable_; }
  bool HasVideo() const override { return true; }
  bool HasAudio() const override { return has_audio_; }

  bool has_audio_ = false;
  WebTimeRanges seekable_;
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
    return element.getAttribute(html_names::kClassAttr) != "transparent";

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
  explicit MediaControlsImplTest(
      base::test::TaskEnvironment::TimeSource time_source)
      : PageTestBase(time_source), ScopedMediaCastOverlayButtonForTest(true) {}
  MediaControlsImplTest() : ScopedMediaCastOverlayButtonForTest(true) {}

 protected:
  void SetUp() override {
    InitializePage();
  }

  void InitializePage() {
    SetupPageWithClients(MakeGarbageCollected<FakeChromeClient>(),
                         MakeGarbageCollected<StubLocalFrameClientForImpl>());

    GetDocument().write("<video controls>");
    auto& video = To<HTMLVideoElement>(
        *GetDocument().QuerySelector(AtomicString("video")));
    media_controls_ = static_cast<MediaControlsImpl*>(video.GetMediaControls());

    // Scripts are disabled by default which forces controls to be on.
    GetFrame().GetSettings()->SetScriptEnabled(true);
  }

  void SetMediaControlsFromElement(HTMLMediaElement& elm) {
    media_controls_ = static_cast<MediaControlsImpl*>(elm.GetMediaControls());
  }

  void SimulateRemotePlaybackAvailable() {
    RemotePlayback::From(media_controls_->MediaElement())
        .AvailabilityChangedForTesting(/* screen_is_available */ true);
  }

  void EnsureSizing() {
    // Fire the size-change callback to ensure that the controls have
    // been properly notified of the video size.
    media_controls_->NotifyElementSizeChanged(
        media_controls_->MediaElement().GetBoundingClientRect());
  }

  void SimulateHideMediaControlsTimerFired() {
    media_controls_->HideMediaControlsTimerFired(nullptr);
  }

  void SimulateLoadedMetadata() { media_controls_->OnLoadedMetadata(); }

  void SimulateOnSeeking() { media_controls_->OnSeeking(); }
  void SimulateOnSeeked() { media_controls_->OnSeeked(); }
  void SimulateOnWaiting() { media_controls_->OnWaiting(); }
  void SimulateOnPlaying() { media_controls_->OnPlaying(); }

  void SimulateMediaControlPlaying() {
    MediaControls().MediaElement().SetReadyState(
        HTMLMediaElement::kHaveEnoughData);
    MediaControls().MediaElement().SetNetworkState(
        WebMediaPlayer::NetworkState::kNetworkStateLoading);
  }

  void SimulateMediaControlPlayingForFutureData() {
    MediaControls().MediaElement().SetReadyState(
        HTMLMediaElement::kHaveFutureData);
    MediaControls().MediaElement().SetNetworkState(
        WebMediaPlayer::NetworkState::kNetworkStateLoading);
  }

  void SimulateMediaControlBuffering() {
    MediaControls().MediaElement().SetReadyState(
        HTMLMediaElement::kHaveCurrentData);
    MediaControls().MediaElement().SetNetworkState(
        WebMediaPlayer::NetworkState::kNetworkStateLoading);
  }

  MediaControlsImpl& MediaControls() { return *media_controls_; }
  MediaControlVolumeSliderElement* VolumeSliderElement() const {
    return media_controls_->volume_slider_.Get();
  }
  MediaControlTimelineElement* TimelineElement() const {
    return media_controls_->timeline_.Get();
  }
  Element* TimelineTrackElement() const {
    if (!TimelineElement())
      return nullptr;
    return &TimelineElement()->GetTrackElement();
  }
  MediaControlCurrentTimeDisplayElement* GetCurrentTimeDisplayElement() const {
    return media_controls_->current_time_display_.Get();
  }
  MediaControlRemainingTimeDisplayElement* GetRemainingTimeDisplayElement()
      const {
    return media_controls_->duration_display_.Get();
  }
  MediaControlMuteButtonElement* MuteButtonElement() const {
    return media_controls_->mute_button_.Get();
  }
  MediaControlCastButtonElement* CastButtonElement() const {
    return media_controls_->cast_button_.Get();
  }
  MediaControlDownloadButtonElement* DownloadButtonElement() const {
    return media_controls_->download_button_.Get();
  }
  MediaControlFullscreenButtonElement* FullscreenButtonElement() const {
    return media_controls_->fullscreen_button_.Get();
  }
  MediaControlPlaybackSpeedButtonElement* PlaybackSpeedButtonElement() const {
    return media_controls_->playback_speed_button_.Get();
  }
  MediaControlPlayButtonElement* PlayButtonElement() const {
    return media_controls_->play_button_.Get();
  }
  MediaControlOverflowMenuButtonElement* OverflowMenuButtonElement() const {
    return media_controls_->overflow_menu_.Get();
  }
  MediaControlOverflowMenuListElement* OverflowMenuListElement() const {
    return media_controls_->overflow_list_.Get();
  }

  MockWebMediaPlayerForImpl* WebMediaPlayer() {
    return static_cast<MockWebMediaPlayerForImpl*>(
        MediaControls().MediaElement().GetWebMediaPlayer());
  }

  base::HistogramTester& GetHistogramTester() { return histogram_tester_; }

  void LoadMediaWithDuration(double duration) {
    MediaControls().MediaElement().SetSrc(
        AtomicString("https://example.com/foo.mp4"));
    test::RunPendingTasks();
    WebTimeRange time_range(0.0, duration);
    WebMediaPlayer()->seekable_.Assign(base::span_from_ref(time_range));
    MediaControls().MediaElement().DurationChanged(duration,
                                                   false /* requestSeek */);
    SimulateLoadedMetadata();
  }

  void SetHasAudio(bool has_audio) { WebMediaPlayer()->has_audio_ = has_audio; }

  void ClickOverflowButton() {
    MediaControls()
        .download_button_->OverflowElementForTests()
        ->DispatchSimulatedClick(nullptr);
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
    return !remote_playback.availability_callbacks_.empty();
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

  PointerEvent* CreatePointerEvent(const AtomicString& name) {
    PointerEventInit* init = PointerEventInit::Create();
    return PointerEvent::Create(name, init);
  }

 private:
  Persistent<MediaControlsImpl> media_controls_;
  base::HistogramTester histogram_tester_;
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
  float frame_scale = GetDocument().GetFrame()->LayoutZoomFactor();
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
  Document& document = GetDocument();
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

  SimulateRemotePlaybackAvailable();
  ASSERT_TRUE(IsOverflowElementVisible(*cast_button));
}

TEST_F(MediaControlsImplTest, CastButtonDisableRemotePlaybackAttr) {
  EnsureSizing();

  MediaControlCastButtonElement* cast_button = CastButtonElement();
  ASSERT_NE(nullptr, cast_button);

  ASSERT_FALSE(IsOverflowElementVisible(*cast_button));
  SimulateRemotePlaybackAvailable();
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

  SimulateRemotePlaybackAvailable();
  ASSERT_TRUE(IsElementVisible(*cast_overlay_button));
}

TEST_F(MediaControlsImplTest, CastOverlayDisabled) {
  MediaControls().MediaElement().SetBooleanAttribute(html_names::kControlsAttr,
                                                     false);

  ScopedMediaCastOverlayButtonForTest media_cast_overlay_button(false);

  Element* cast_overlay_button = GetElementByShadowPseudoId(
      MediaControls(), "-internal-media-controls-overlay-cast-button");
  ASSERT_NE(nullptr, cast_overlay_button);

  SimulateRemotePlaybackAvailable();
  ASSERT_FALSE(IsElementVisible(*cast_overlay_button));
}

TEST_F(MediaControlsImplTest, CastOverlayDisableRemotePlaybackAttr) {
  MediaControls().MediaElement().SetBooleanAttribute(html_names::kControlsAttr,
                                                     false);

  Element* cast_overlay_button = GetElementByShadowPseudoId(
      MediaControls(), "-internal-media-controls-overlay-cast-button");
  ASSERT_NE(nullptr, cast_overlay_button);

  ASSERT_FALSE(IsElementVisible(*cast_overlay_button));
  SimulateRemotePlaybackAvailable();
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
  SimulateRemotePlaybackAvailable();
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
  SimulateRemotePlaybackAvailable();
  EXPECT_FALSE(IsElementVisible(*cast_overlay_button));

  GetDocument().GetSettings()->SetMediaControlsEnabled(false);
  EXPECT_FALSE(IsElementVisible(*cast_overlay_button));

  GetDocument().GetSettings()->SetMediaControlsEnabled(true);
  EXPECT_FALSE(IsElementVisible(*cast_overlay_button));
}

TEST_F(MediaControlsImplTest, CastOverlayDisabledAutoplayMuted) {
  MediaControls().MediaElement().SetBooleanAttribute(html_names::kControlsAttr,
                                                     false);

  // Set the video to autoplay muted.
  ScopedMediaEngagementBypassAutoplayPoliciesForTest scoped_feature(true);
  MediaControls().MediaElement().GetDocument().GetSettings()->SetAutoplayPolicy(
      AutoplayPolicy::Type::kDocumentUserActivationRequired);
  MediaControls().MediaElement().setMuted(true);

  Element* cast_overlay_button = GetElementByShadowPseudoId(
      MediaControls(), "-internal-media-controls-overlay-cast-button");
  ASSERT_NE(nullptr, cast_overlay_button);

  SimulateRemotePlaybackAvailable();
  EXPECT_FALSE(IsElementVisible(*cast_overlay_button));
}

TEST_F(MediaControlsImplTest, CastButtonVisibilityDependsOnControlslistAttr) {
  EnsureSizing();

  MediaControlCastButtonElement* cast_button = CastButtonElement();
  ASSERT_NE(nullptr, cast_button);

  SimulateRemotePlaybackAvailable();
  ASSERT_TRUE(IsOverflowElementVisible(*cast_button));

  MediaControls().MediaElement().setAttribute(
      blink::html_names::kControlslistAttr, AtomicString("noremoteplayback"));
  test::RunPendingTasks();

  // Cast button should not be displayed because of
  // controlslist="noremoteplayback".
  ASSERT_FALSE(IsOverflowElementVisible(*cast_button));

  // If the user explicitly shows all controls, that should override the
  // controlsList attribute and cast button should be displayed.
  MediaControls().MediaElement().SetUserWantsControlsVisible(true);
  ASSERT_TRUE(IsOverflowElementVisible(*cast_button));
}

TEST_F(MediaControlsImplTest, KeepControlsVisibleIfOverflowListVisible) {
  Element* overflow_list = GetElementByShadowPseudoId(
      MediaControls(), "-internal-media-controls-overflow-menu-list");
  ASSERT_NE(nullptr, overflow_list);

  Element* panel = GetElementByShadowPseudoId(MediaControls(),
                                              "-webkit-media-controls-panel");
  ASSERT_NE(nullptr, panel);

  MediaControls().MediaElement().SetSrc(AtomicString("http://example.com"));
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

  MediaControls().MediaElement().SetSrc(
      AtomicString("https://example.com/foo.mp4"));
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
  MediaControls().MediaElement().SetSrc(g_empty_atom);
  test::RunPendingTasks();
  SimulateLoadedMetadata();
  EXPECT_FALSE(IsOverflowElementVisible(*download_button));
}

TEST_F(MediaControlsImplTest, DownloadButtonNotDisplayedInfiniteDuration) {
  EnsureSizing();

  MediaControlDownloadButtonElement* download_button = DownloadButtonElement();
  ASSERT_NE(nullptr, download_button);

  MediaControls().MediaElement().SetSrc(
      AtomicString("https://example.com/foo.mp4"));
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
  MediaControls().MediaElement().SetSrc(
      AtomicString("https://example.com/foo.m3u8"));
  test::RunPendingTasks();
  SimulateLoadedMetadata();
  EXPECT_FALSE(IsOverflowElementVisible(*download_button));

  MediaControls().MediaElement().SetSrc(
      AtomicString("https://example.com/foo.m3u8?title=foo"));
  test::RunPendingTasks();
  SimulateLoadedMetadata();
  EXPECT_FALSE(IsOverflowElementVisible(*download_button));

  // However, it *should* be displayed for otherwise valid sources containing
  // the text 'm3u8'.
  MediaControls().MediaElement().SetSrc(
      AtomicString("https://example.com/foo.m3u8.mp4"));
  test::RunPendingTasks();
  SimulateLoadedMetadata();
  EXPECT_TRUE(IsOverflowElementVisible(*download_button));
}

TEST_F(MediaControlsImplTest,
       DownloadButtonVisibilityDependsOnControlslistAttr) {
  EnsureSizing();

  MediaControlDownloadButtonElement* download_button = DownloadButtonElement();
  ASSERT_NE(nullptr, download_button);

  MediaControls().MediaElement().SetSrc(
      AtomicString("https://example.com/foo.mp4"));
  MediaControls().MediaElement().setAttribute(
      blink::html_names::kControlslistAttr, AtomicString("nodownload"));
  test::RunPendingTasks();
  SimulateLoadedMetadata();

  // Download button should not be displayed because of
  // controlslist="nodownload".
  EXPECT_FALSE(IsOverflowElementVisible(*download_button));

  // If the user explicitly shows all controls, that should override the
  // controlsList attribute and download button should be displayed.
  MediaControls().MediaElement().SetUserWantsControlsVisible(true);
  EXPECT_TRUE(IsOverflowElementVisible(*download_button));
}

TEST_F(MediaControlsImplTest,
       FullscreenButtonDisabledDependsOnControlslistAttr) {
  EnsureSizing();

  MediaControlFullscreenButtonElement* fullscreen_button =
      FullscreenButtonElement();
  ASSERT_NE(nullptr, fullscreen_button);

  MediaControls().MediaElement().SetSrc(
      AtomicString("https://example.com/foo.mp4"));
  MediaControls().MediaElement().setAttribute(
      blink::html_names::kControlslistAttr, AtomicString("nofullscreen"));
  test::RunPendingTasks();
  SimulateLoadedMetadata();

  // Fullscreen button should be disabled because of
  // controlslist="nofullscreen".
  EXPECT_TRUE(fullscreen_button->IsDisabled());

  // If the user explicitly shows all controls, that should override the
  // controlsList attribute and fullscreen button should be enabled.
  MediaControls().MediaElement().SetUserWantsControlsVisible(true);
  EXPECT_FALSE(fullscreen_button->IsDisabled());
}

TEST_F(MediaControlsImplTest,
       PlaybackSpeedButtonVisibilityDependsOnControlslistAttr) {
  EnsureSizing();

  MediaControlPlaybackSpeedButtonElement* playback_speed_button =
      PlaybackSpeedButtonElement();
  ASSERT_NE(nullptr, playback_speed_button);

  MediaControls().MediaElement().SetSrc(
      AtomicString("https://example.com/foo.mp4"));
  MediaControls().MediaElement().setAttribute(
      blink::html_names::kControlslistAttr, AtomicString("noplaybackrate"));
  test::RunPendingTasks();
  SimulateLoadedMetadata();

  // Fullscreen button should not be displayed because of
  // controlslist="noplaybackrate".
  EXPECT_FALSE(IsOverflowElementVisible(*playback_speed_button));

  // If the user explicitly shows all controls, that should override the
  // controlsList attribute and playback speed button should be displayed.
  MediaControls().MediaElement().SetUserWantsControlsVisible(true);
  EXPECT_TRUE(IsOverflowElementVisible(*playback_speed_button));
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
  MediaControlsImplTestWithMockScheduler()
      : MediaControlsImplTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    EnablePlatform();
  }

 protected:
  void SetUp() override {
    // DocumentParserTiming has DCHECKS to make sure time > 0.0.
    AdvanceClock(base::Seconds(1));

    MediaControlsImplTest::SetUp();
  }

  void TearDown() override { PageTestBase::TearDown(); }

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

  MediaControls().MediaElement().SetSrc(AtomicString("http://example.com"));
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

  MediaControls().MediaElement().SetSrc(AtomicString("http://example.com"));
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

  MediaControls().MediaElement().SetSrc(AtomicString("http://example.com"));
  MediaControls().MediaElement().Play();

  // Controls start out visible.
  EXPECT_TRUE(IsElementVisible(*panel));

  // Tabbing between controls prevents controls from hiding.
  FastForwardBy(base::Seconds(2));
  MuteButtonElement()->DispatchEvent(
      *Event::CreateBubble(event_type_names::kFocusin));
  FastForwardBy(base::Seconds(2));
  EXPECT_TRUE(IsElementVisible(*panel));

  // Seeking on the timeline or volume bar prevents controls from hiding.
  TimelineElement()->DispatchEvent(
      *Event::CreateBubble(event_type_names::kInput));
  FastForwardBy(base::Seconds(2));
  EXPECT_TRUE(IsElementVisible(*panel));

  // Pressing a key prevents controls from hiding.
  MuteButtonElement()->DispatchEvent(
      *Event::CreateBubble(event_type_names::kKeypress));
  FastForwardBy(base::Seconds(2));
  EXPECT_TRUE(IsElementVisible(*panel));

  // Once user interaction stops, controls can hide.
  FastForwardBy(base::Seconds(2));
  SimulateTransitionEnd(*panel);
  EXPECT_FALSE(IsElementVisible(*panel));
}

TEST_F(MediaControlsImplTestWithMockScheduler,
       ControlsHideAfterFocusedAndMouseMovement) {
  EnsureSizing();

  Element* panel = MediaControls().PanelElement();
  MediaControls().MediaElement().SetSrc(AtomicString("http://example.com"));
  MediaControls().MediaElement().Play();

  // Controls start out visible
  EXPECT_TRUE(IsElementVisible(*panel));
  FastForwardBy(base::Seconds(1));

  // Mouse move while focused
  MediaControls().DispatchEvent(*Event::Create(event_type_names::kFocusin));
  MediaControls().MediaElement().SetFocused(true,
                                            mojom::blink::FocusType::kNone);
  MediaControls().DispatchEvent(
      *CreatePointerEvent(event_type_names::kPointermove));

  // Controls should remain visible
  FastForwardBy(base::Seconds(2));
  EXPECT_TRUE(IsElementVisible(*panel));

  // Controls should hide after being inactive for 4 seconds.
  FastForwardBy(base::Seconds(2));
  EXPECT_FALSE(IsElementVisible(*panel));
}

TEST_F(MediaControlsImplTestWithMockScheduler,
       ControlsHideAfterFocusedAndMouseMoveout) {
  EnsureSizing();

  Element* panel = MediaControls().PanelElement();
  MediaControls().MediaElement().SetSrc(AtomicString("http://example.com"));
  MediaControls().MediaElement().Play();

  // Controls start out visible
  EXPECT_TRUE(IsElementVisible(*panel));
  FastForwardBy(base::Seconds(1));

  // Mouse move out while focused, controls should hide
  MediaControls().DispatchEvent(*Event::Create(event_type_names::kFocusin));
  MediaControls().MediaElement().SetFocused(true,
                                            mojom::blink::FocusType::kNone);
  MediaControls().DispatchEvent(*Event::Create(event_type_names::kPointerout));
  EXPECT_FALSE(IsElementVisible(*panel));
}

TEST_F(MediaControlsImplTestWithMockScheduler, CursorHidesWhenControlsHide) {
  EnsureSizing();

  MediaControls().MediaElement().SetSrc(AtomicString("http://example.com"));

  // Cursor is not initially hidden.
  EXPECT_FALSE(IsCursorHidden());

  MediaControls().MediaElement().Play();

  // Tabbing into the controls shows the controls and therefore the cursor.
  MediaControls().DispatchEvent(*Event::Create(event_type_names::kFocusin));
  EXPECT_FALSE(IsCursorHidden());

  // Once the controls hide, the cursor is hidden.
  FastForwardBy(base::Seconds(4));
  EXPECT_TRUE(IsCursorHidden());

  // If the mouse moves, the controls are shown and the cursor is no longer
  // hidden.
  MediaControls().DispatchEvent(
      *CreatePointerEvent(event_type_names::kPointermove));
  EXPECT_FALSE(IsCursorHidden());

  // Once the controls hide again, the cursor is hidden again.
  FastForwardBy(base::Seconds(4));
  EXPECT_TRUE(IsCursorHidden());
}

TEST_F(MediaControlsImplTestWithMockScheduler, AccessibleFocusShowsControls) {
  EnsureSizing();

  Element* panel = MediaControls().PanelElement();

  MediaControls().MediaElement().SetSrc(AtomicString("http://example.com"));
  MediaControls().MediaElement().Play();

  FastForwardBy(base::Seconds(4));
  EXPECT_TRUE(IsElementVisible(*panel));

  MediaControls().OnAccessibleFocus();
  FastForwardBy(base::Seconds(4));
  EXPECT_TRUE(IsElementVisible(*panel));

  FastForwardBy(base::Seconds(4));
  SimulateHideMediaControlsTimerFired();
  EXPECT_TRUE(IsElementVisible(*panel));

  MediaControls().OnAccessibleBlur();
  FastForwardBy(base::Seconds(4));
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

  page_holder->GetDocument().View()->UpdateAllLifecyclePhasesForTest();
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

  auto& video = To<HTMLVideoElement>(
      *page_holder->GetDocument().QuerySelector(AtomicString("video")));
  WeakPersistent<HTMLMediaElement> weak_persistent_video = &video;

  video.remove();
  page_holder->GetDocument().View()->UpdateAllLifecyclePhasesForTest();
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
  MediaControls().MediaElement().SetSrc(
      AtomicString("https://example.com/foo.mp4"));
  FastForwardBy(base::Seconds(1));
  SetHasAudio(true);
  SimulateLoadedMetadata();

  ScopedWebTestMode web_test_mode(false);

  Element* volume_slider = VolumeSliderElement();
  Element* mute_btn = MuteButtonElement();

  ASSERT_NE(nullptr, volume_slider);
  ASSERT_NE(nullptr, mute_btn);

  EXPECT_TRUE(IsElementVisible(*mute_btn));
  EXPECT_TRUE(volume_slider->classList().contains(AtomicString("closed")));

  DOMRect* mute_btn_rect = mute_btn->GetBoundingClientRect();
  gfx::PointF mute_btn_center(
      mute_btn_rect->left() + mute_btn_rect->width() / 2,
      mute_btn_rect->top() + mute_btn_rect->height() / 2);
  gfx::PointF edge(0, 0);

  // Hover on mute button and stay
  MouseMoveTo(mute_btn_center);
  FastForwardBy(base::Seconds(kTimeToShowVolumeSlider - 0.001));
  EXPECT_TRUE(volume_slider->classList().contains(AtomicString("closed")));

  FastForwardBy(base::Seconds(0.002));
  EXPECT_FALSE(volume_slider->classList().contains(AtomicString("closed")));

  MouseMoveTo(edge);
  EXPECT_TRUE(volume_slider->classList().contains(AtomicString("closed")));

  // Hover on mute button and move away before timer fired
  MouseMoveTo(mute_btn_center);
  FastForwardBy(base::Seconds(kTimeToShowVolumeSlider - 0.001));
  EXPECT_TRUE(volume_slider->classList().contains(AtomicString("closed")));

  MouseMoveTo(edge);
  EXPECT_TRUE(volume_slider->classList().contains(AtomicString("closed")));
}

TEST_F(MediaControlsImplTestWithMockScheduler,
       VolumeSliderBehaviorWhenFocused) {
  MediaControls().MediaElement().SetSrc(
      AtomicString("https://example.com/foo.mp4"));
  FastForwardBy(base::Seconds(1));

  SetHasAudio(true);

  ScopedWebTestMode web_test_mode(false);

  Element* volume_slider = VolumeSliderElement();

  ASSERT_NE(nullptr, volume_slider);

  // Volume slider starts out hidden
  EXPECT_TRUE(volume_slider->classList().contains(AtomicString("closed")));

  // Tab focus should open volume slider immediately.
  volume_slider->SetFocused(true, mojom::blink::FocusType::kNone);
  volume_slider->DispatchEvent(*Event::Create(event_type_names::kFocus));
  EXPECT_FALSE(volume_slider->classList().contains(AtomicString("closed")));

  // Unhover slider while focused should not close slider.
  volume_slider->DispatchEvent(*Event::Create(event_type_names::kMouseout));
  EXPECT_FALSE(volume_slider->classList().contains(AtomicString("closed")));
}

TEST_F(MediaControlsImplTestWithMockScheduler,
       VolumeSliderDoesNotOpenWithoutAudio) {
  MediaControls().MediaElement().SetSrc(
      AtomicString("https://example.com/foo.mp4"));
  FastForwardBy(base::Seconds(1));
  SetHasAudio(false);

  ScopedWebTestMode web_test_mode(false);

  Element* volume_slider = VolumeSliderElement();
  Element* mute_button = MuteButtonElement();

  ASSERT_NE(nullptr, volume_slider);

  // Volume slider starts out hidden.
  EXPECT_TRUE(volume_slider->classList().contains(AtomicString("closed")));

  // Tab focus on the mute button should not open the volume slider since there
  // is no audio to control.
  mute_button->SetFocused(true, mojom::blink::FocusType::kNone);
  mute_button->DispatchEvent(*Event::Create(event_type_names::kFocus));
  EXPECT_TRUE(volume_slider->classList().contains(AtomicString("closed")));
}

TEST_F(MediaControlsImplTest, CastOverlayDefaultHidesOnTimer) {
  MediaControls().MediaElement().SetBooleanAttribute(html_names::kControlsAttr,
                                                     false);

  Element* cast_overlay_button = GetElementByShadowPseudoId(
      MediaControls(), "-internal-media-controls-overlay-cast-button");
  ASSERT_NE(nullptr, cast_overlay_button);

  SimulateRemotePlaybackAvailable();
  EXPECT_TRUE(IsElementVisible(*cast_overlay_button));

  // Starts playback because overlay never hides if paused.
  MediaControls().MediaElement().SetSrc(AtomicString("http://example.com"));
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

  SimulateRemotePlaybackAvailable();
  EXPECT_TRUE(IsElementVisible(*cast_overlay_button));

  // Starts playback because overlay never hides if paused.
  MediaControls().MediaElement().SetSrc(AtomicString("http://example.com"));
  MediaControls().MediaElement().Play();
  test::RunPendingTasks();

  SimulateRemotePlaybackAvailable();
  SimulateHideMediaControlsTimerFired();
  EXPECT_FALSE(IsElementVisible(*cast_overlay_button));

  // The overlay button appears on tap and click.
  for (const AtomicString& event_name :
       {event_type_names::kGesturetap, event_type_names::kClick}) {
    overlay_enclosure->DispatchEvent(event_name == "gesturetap"
                                         ? *Event::Create(event_name)
                                         : *CreatePointerEvent(event_name));
    EXPECT_TRUE(IsElementVisible(*cast_overlay_button));

    SimulateHideMediaControlsTimerFired();
    EXPECT_FALSE(IsElementVisible(*cast_overlay_button));
  }

  // The overlay button does not appear on pointer move.
  overlay_enclosure->DispatchEvent(
      *CreatePointerEvent(event_type_names::kPointerover));
  EXPECT_FALSE(IsElementVisible(*cast_overlay_button));

  // The overlay button does not appear on click if the overlay button shouldn't
  // be shown.
  MediaControls().MediaElement().SetBooleanAttribute(html_names::kControlsAttr,
                                                     true);
  overlay_enclosure->DispatchEvent(
      *CreatePointerEvent(event_type_names::kClick));
  EXPECT_FALSE(IsElementVisible(*cast_overlay_button));
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
      UADefinedVariable::kSafeAreaInsetTop, "1px");
  GetStyleEngine().EnsureEnvironmentVariables().SetVariable(
      UADefinedVariable::kSafeAreaInsetLeft, "2px");
  GetStyleEngine().EnsureEnvironmentVariables().SetVariable(
      UADefinedVariable::kSafeAreaInsetBottom, "3px");
  GetStyleEngine().EnsureEnvironmentVariables().SetVariable(
      UADefinedVariable::kSafeAreaInsetRight, "4px");

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

  MediaControls().MediaElement().setAttribute(html_names::kPreloadAttr,
                                              AtomicString("none"));
  MediaControls().MediaElement().SetSrc(
      AtomicString("https://example.com/foo.mp4"));
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

  DOMRect* videoRect = MediaControls().MediaElement().GetBoundingClientRect();
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

  DOMRect* videoRect = MediaControls().MediaElement().GetBoundingClientRect();
  ASSERT_LT(0, videoRect->width());
  gfx::PointF leftOfCenter(videoRect->left() + (videoRect->width() / 2) - 5,
                           videoRect->top() + 10);
  gfx::PointF rightOfCenter(videoRect->left() + (videoRect->width() / 2) + 5,
                            videoRect->top() + 10);

  // Add a zoom factor and ensure that it's properly handled.
  MediaControls().GetDocument().GetFrame()->SetLayoutZoomFactor(2);

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

TEST_F(MediaControlsImplTest, HideControlsDefersStyleCalculationOnPlaying) {
  MediaControls().MediaElement().SetBooleanAttribute(html_names::kControlsAttr,
                                                     false);
  MediaControls().MediaElement().SetSrc(
      AtomicString("https://example.com/foo.mp4"));
  MediaControls().MediaElement().Play();
  test::RunPendingTasks();

  Element* panel = GetElementByShadowPseudoId(MediaControls(),
                                              "-webkit-media-controls-panel");
  ASSERT_NE(nullptr, panel);
  EXPECT_FALSE(IsElementVisible(*panel));
  UpdateAllLifecyclePhasesForTest();
  Document& document = this->GetDocument();
  EXPECT_FALSE(document.NeedsLayoutTreeUpdate());

  int old_element_count = document.GetStyleEngine().StyleForElementCount();

  SimulateMediaControlPlaying();
  SimulateOnPlaying();
  EXPECT_EQ(MediaControls().State(),
            MediaControlsImpl::ControlsState::kPlaying);

  // With the controls hidden, playback state change should not trigger style
  // calculation.
  EXPECT_FALSE(document.NeedsLayoutTreeUpdate());
  UpdateAllLifecyclePhasesForTest();
  int new_element_count = document.GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(old_element_count, new_element_count);

  MediaControls().MediaElement().SetBooleanAttribute(html_names::kControlsAttr,
                                                     true);
  EXPECT_TRUE(IsElementVisible(*panel));

  // Showing the controls should trigger the deferred style calculation.
  EXPECT_TRUE(document.NeedsLayoutTreeUpdate());
  UpdateAllLifecyclePhasesForTest();
  new_element_count = document.GetStyleEngine().StyleForElementCount();
  EXPECT_LT(old_element_count, new_element_count);
}

TEST_F(MediaControlsImplTest, HideControlsDefersStyleCalculationOnWaiting) {
  MediaControls().MediaElement().SetBooleanAttribute(html_names::kControlsAttr,
                                                     false);
  MediaControls().MediaElement().SetSrc(
      AtomicString("https://example.com/foo.mp4"));
  MediaControls().MediaElement().Play();
  test::RunPendingTasks();

  Element* panel = GetElementByShadowPseudoId(MediaControls(),
                                              "-webkit-media-controls-panel");
  ASSERT_NE(nullptr, panel);
  EXPECT_FALSE(IsElementVisible(*panel));
  UpdateAllLifecyclePhasesForTest();
  Document& document = this->GetDocument();
  EXPECT_FALSE(document.NeedsLayoutTreeUpdate());

  int old_element_count = document.GetStyleEngine().StyleForElementCount();

  SimulateMediaControlBuffering();
  SimulateOnWaiting();
  EXPECT_EQ(MediaControls().State(),
            MediaControlsImpl::ControlsState::kBuffering);

  // With the controls hidden, playback state change should not trigger style
  // calculation.
  EXPECT_FALSE(document.NeedsLayoutTreeUpdate());
  UpdateAllLifecyclePhasesForTest();
  int new_element_count = document.GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(old_element_count, new_element_count);

  MediaControls().MediaElement().SetBooleanAttribute(html_names::kControlsAttr,
                                                     true);
  EXPECT_TRUE(IsElementVisible(*panel));

  // Showing the controls should trigger the deferred style calculation.
  EXPECT_TRUE(document.NeedsLayoutTreeUpdate());
  UpdateAllLifecyclePhasesForTest();
  new_element_count = document.GetStyleEngine().StyleForElementCount();
  EXPECT_LT(old_element_count, new_element_count);
}

TEST_F(MediaControlsImplTest, CheckStateOnPlayingForFutureData) {
  MediaControls().MediaElement().SetSrc(
      AtomicString("https://example.com/foo.mp4"));
  MediaControls().MediaElement().Play();
  test::RunPendingTasks();
  UpdateAllLifecyclePhasesForTest();

  SimulateMediaControlPlayingForFutureData();
  EXPECT_EQ(MediaControls().State(),
            MediaControlsImpl::ControlsState::kPlaying);
}

TEST_F(MediaControlsImplTest, OverflowMenuInPaintContainment) {
  // crbug.com/1244130
  auto page_holder = std::make_unique<DummyPageHolder>();
  page_holder->GetDocument().write("<audio controls style='contain:paint'>");
  page_holder->GetDocument().Parser()->Finish();
  test::RunPendingTasks();
  UpdateAllLifecyclePhasesForTest();
  SetMediaControlsFromElement(To<HTMLMediaElement>(
      *page_holder->GetDocument().QuerySelector(AtomicString("audio"))));

  MediaControls().ToggleOverflowMenu();
  UpdateAllLifecyclePhasesForTest();
  Element* overflow_list = GetElementByShadowPseudoId(
      MediaControls(), "-internal-media-controls-overflow-menu-list");
  ASSERT_TRUE(overflow_list);
  EXPECT_TRUE(overflow_list->IsInTopLayer());

  MediaControls().ToggleOverflowMenu();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(overflow_list->IsInTopLayer());
}

}  // namespace blink
