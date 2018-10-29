// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"

#include <limits>
#include <memory>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/modules/remoteplayback/web_remote_playback_availability.h"
#include "third_party/blink/public/platform/modules/remoteplayback/web_remote_playback_client.h"
#include "third_party/blink/public/platform/web_mouse_event.h"
#include "third_party/blink/public/platform/web_screen_info.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/document_style_environment_variables.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_current_time_display_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_download_button_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_overflow_menu_list_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_remaining_time_display_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_timeline_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_volume_slider_element.h"
#include "third_party/blink/renderer/modules/media_controls/media_download_in_product_help_manager.h"
#include "third_party/blink/renderer/modules/remoteplayback/html_media_element_remote_playback.h"
#include "third_party/blink/renderer/modules/remoteplayback/remote_playback.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/testing/empty_web_media_player.h"
#include "third_party/blink/renderer/platform/testing/histogram_tester.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

// The MediaTimelineWidths histogram suffix expected to be encountered in these
// tests. Depends on the OS, since Android sizes its timeline differently.
#if defined(OS_ANDROID)
#define TIMELINE_W "80_127"
#else
#define TIMELINE_W "128_255"
#endif

namespace blink {

namespace {

class MockChromeClientForImpl : public EmptyChromeClient {
 public:
  // EmptyChromeClient overrides:
  WebScreenInfo GetScreenInfo() const override {
    WebScreenInfo screen_info;
    screen_info.orientation_type = kWebScreenOrientationLandscapePrimary;
    return screen_info;
  }
};

class MockWebMediaPlayerForImpl : public EmptyWebMediaPlayer {
 public:
  // WebMediaPlayer overrides:
  WebTimeRanges Seekable() const override { return seekable_; }
  bool HasVideo() const override { return true; }
  SurfaceLayerMode GetVideoSurfaceLayerMode() const override {
    return SurfaceLayerMode::kAlways;
  }

  WebTimeRanges seekable_;
};

class MockLayoutObject : public LayoutObject {
 public:
  MockLayoutObject(Node* node) : LayoutObject(node) {}

  void SetVisible(bool visible) { visible_ = visible; }

  const char* GetName() const override { return "MockLayoutObject"; }
  void UpdateLayout() override {}
  FloatRect LocalBoundingBoxRectForAccessibility() const override {
    return FloatRect();
  }
  void AbsoluteQuads(Vector<FloatQuad>& quads,
                     MapCoordinatesFlags mode) const override {
    if (!visible_)
      return;
    quads.push_back(FloatQuad(FloatRect(0.f, 0.f, 10.f, 10.f)));
  }

 private:
  bool visible_ = false;
};

class StubLocalFrameClientForImpl : public EmptyLocalFrameClient {
 public:
  static StubLocalFrameClientForImpl* Create() {
    return new StubLocalFrameClientForImpl;
  }

  std::unique_ptr<WebMediaPlayer> CreateWebMediaPlayer(
      HTMLMediaElement&,
      const WebMediaPlayerSource&,
      WebMediaPlayerClient*,
      WebLayerTreeView*) override {
    return std::make_unique<MockWebMediaPlayerForImpl>();
  }

  WebRemotePlaybackClient* CreateWebRemotePlaybackClient(
      HTMLMediaElement& element) override {
    return HTMLMediaElementRemotePlayback::remote(element);
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

MediaControlDownloadButtonElement& GetDownloadButton(
    MediaControlsImpl& controls) {
  Element* element = GetElementByShadowPseudoId(
      controls, "-internal-media-controls-download-button");
  return static_cast<MediaControlDownloadButtonElement&>(*element);
}

bool IsElementVisible(Element& element) {
  const CSSPropertyValueSet* inline_style = element.InlineStyle();

  if (!inline_style)
    return element.getAttribute("class") != "transparent";

  if (inline_style->GetPropertyValue(CSSPropertyDisplay) == "none")
    return false;

  if (inline_style->HasProperty(CSSPropertyOpacity) &&
      inline_style->GetPropertyValue(CSSPropertyOpacity).ToDouble() == 0.0) {
    return false;
  }

  if (inline_style->GetPropertyValue(CSSPropertyVisibility) == "hidden")
    return false;

  if (Element* parent = element.parentElement())
    return IsElementVisible(*parent);

  return true;
}

void SimulateTransitionEnd(Element& element) {
  element.DispatchEvent(*Event::Create(EventTypeNames::transitionend));
}

// This must match MediaControlDownloadButtonElement::DownloadActionMetrics.
enum DownloadActionMetrics {
  kShown = 0,
  kClicked,
  kCount  // Keep last.
};

}  // namespace

static const char* kTimeToActionHistogramName =
    "Media.Controls.Overflow.TimeToAction";

static const char* kTimeToDismissHistogramName =
    "Media.Controls.Overflow.TimeToDismiss";

static double g_current_time = 1000.0;

static void AdvanceClock(double seconds) {
  g_current_time += seconds;
}

class MediaControlsImplTest : public PageTestBase,
                              private ScopedMediaCastOverlayButtonForTest {
 public:
  MediaControlsImplTest() : ScopedMediaCastOverlayButtonForTest(true) {}

 protected:
  void SetUp() override {
    original_time_function_ =
        SetTimeFunctionsForTesting([] { return g_current_time; });

    InitializePage();
  }

  void TearDown() override {
    SetTimeFunctionsForTesting(original_time_function_);
  }

  void InitializePage() {
    Page::PageClients clients;
    FillWithEmptyClients(clients);
    clients.chrome_client = new MockChromeClientForImpl();
    SetupPageWithClients(&clients, StubLocalFrameClientForImpl::Create());
    GetDocument().GetSettings()->SetMediaDownloadInProductHelpEnabled(
        EnableDownloadInProductHelp());

    GetDocument().write("<video controls>");
    HTMLVideoElement& video =
        ToHTMLVideoElement(*GetDocument().QuerySelector("video"));
    media_controls_ = static_cast<MediaControlsImpl*>(video.GetMediaControls());

    // Scripts are disabled by default which forces controls to be on.
    GetFrame().GetSettings()->SetScriptEnabled(true);
  }

  void SimulateRouteAvailable() {
    media_controls_->MediaElement().RemoteRouteAvailabilityChanged(
        WebRemotePlaybackAvailability::kDeviceAvailable);
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

  MediaControlsImpl& MediaControls() { return *media_controls_; }
  MediaControlVolumeSliderElement* VolumeSliderElement() const {
    return media_controls_->volume_slider_;
  }
  MediaControlTimelineElement* TimelineElement() const {
    return media_controls_->timeline_;
  }
  MediaControlCurrentTimeDisplayElement* GetCurrentTimeDisplayElement() const {
    return media_controls_->current_time_display_;
  }
  MediaControlRemainingTimeDisplayElement* GetRemainingTimeDisplayElement()
      const {
    return media_controls_->duration_display_;
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

  void MouseDownAt(WebFloatPoint pos);
  void MouseMoveTo(WebFloatPoint pos);
  void MouseUpAt(WebFloatPoint pos);

  bool HasAvailabilityCallbacks(RemotePlayback* remote_playback) {
    return !remote_playback->availability_callbacks_.IsEmpty();
  }

  virtual bool EnableDownloadInProductHelp() { return false; }

  const String& GetDisplayedTime(MediaControlTimeDisplayElement* display) {
    return ToText(display->firstChild())->data();
  }

  void ToggleOverflowMenu() {
    MediaControls().ToggleOverflowMenu();
    test::RunPendingTasks();
  }

 private:
  Persistent<MediaControlsImpl> media_controls_;
  HistogramTester histogram_tester_;
  TimeFunction original_time_function_;
};

void MediaControlsImplTest::MouseDownAt(WebFloatPoint pos) {
  WebMouseEvent mouse_down_event(WebInputEvent::kMouseDown,
                                 pos /* client pos */, pos /* screen pos */,
                                 WebPointerProperties::Button::kLeft, 1,
                                 WebInputEvent::Modifiers::kLeftButtonDown,
                                 WebInputEvent::GetStaticTimeStampForTests());
  mouse_down_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      mouse_down_event);
}

void MediaControlsImplTest::MouseMoveTo(WebFloatPoint pos) {
  WebMouseEvent mouse_move_event(WebInputEvent::kMouseMove,
                                 pos /* client pos */, pos /* screen pos */,
                                 WebPointerProperties::Button::kLeft, 1,
                                 WebInputEvent::Modifiers::kLeftButtonDown,
                                 WebInputEvent::GetStaticTimeStampForTests());
  mouse_move_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_event, {}, {});
}

void MediaControlsImplTest::MouseUpAt(WebFloatPoint pos) {
  WebMouseEvent mouse_up_event(
      WebMouseEvent::kMouseUp, pos /* client pos */, pos /* screen pos */,
      WebPointerProperties::Button::kLeft, 1, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests());
  mouse_up_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMouseReleaseEvent(
      mouse_up_event);
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

  Element* cast_button = GetElementByShadowPseudoId(
      MediaControls(), "-internal-media-controls-cast-button");
  ASSERT_NE(nullptr, cast_button);

  ASSERT_FALSE(IsElementVisible(*cast_button));

  SimulateRouteAvailable();
  ASSERT_TRUE(IsElementVisible(*cast_button));
}

TEST_F(MediaControlsImplTest, CastButtonDisableRemotePlaybackAttr) {
  EnsureSizing();

  Element* cast_button = GetElementByShadowPseudoId(
      MediaControls(), "-internal-media-controls-cast-button");
  ASSERT_NE(nullptr, cast_button);

  ASSERT_FALSE(IsElementVisible(*cast_button));
  SimulateRouteAvailable();
  ASSERT_TRUE(IsElementVisible(*cast_button));

  MediaControls().MediaElement().SetBooleanAttribute(
      HTMLNames::disableremoteplaybackAttr, true);
  test::RunPendingTasks();
  ASSERT_FALSE(IsElementVisible(*cast_button));

  MediaControls().MediaElement().SetBooleanAttribute(
      HTMLNames::disableremoteplaybackAttr, false);
  test::RunPendingTasks();
  ASSERT_TRUE(IsElementVisible(*cast_button));
}

TEST_F(MediaControlsImplTest, CastOverlayDefault) {
  MediaControls().MediaElement().SetBooleanAttribute(HTMLNames::controlsAttr,
                                                     false);

  Element* cast_overlay_button = GetElementByShadowPseudoId(
      MediaControls(), "-internal-media-controls-overlay-cast-button");
  ASSERT_NE(nullptr, cast_overlay_button);

  SimulateRouteAvailable();
  ASSERT_TRUE(IsElementVisible(*cast_overlay_button));
}

TEST_F(MediaControlsImplTest, CastOverlayDisabled) {
  MediaControls().MediaElement().SetBooleanAttribute(HTMLNames::controlsAttr,
                                                     false);

  ScopedMediaCastOverlayButtonForTest media_cast_overlay_button(false);

  Element* cast_overlay_button = GetElementByShadowPseudoId(
      MediaControls(), "-internal-media-controls-overlay-cast-button");
  ASSERT_NE(nullptr, cast_overlay_button);

  SimulateRouteAvailable();
  ASSERT_FALSE(IsElementVisible(*cast_overlay_button));
}

TEST_F(MediaControlsImplTest, CastOverlayDisableRemotePlaybackAttr) {
  MediaControls().MediaElement().SetBooleanAttribute(HTMLNames::controlsAttr,
                                                     false);

  Element* cast_overlay_button = GetElementByShadowPseudoId(
      MediaControls(), "-internal-media-controls-overlay-cast-button");
  ASSERT_NE(nullptr, cast_overlay_button);

  ASSERT_FALSE(IsElementVisible(*cast_overlay_button));
  SimulateRouteAvailable();
  ASSERT_TRUE(IsElementVisible(*cast_overlay_button));

  MediaControls().MediaElement().SetBooleanAttribute(
      HTMLNames::disableremoteplaybackAttr, true);
  test::RunPendingTasks();
  ASSERT_FALSE(IsElementVisible(*cast_overlay_button));

  MediaControls().MediaElement().SetBooleanAttribute(
      HTMLNames::disableremoteplaybackAttr, false);
  test::RunPendingTasks();
  ASSERT_TRUE(IsElementVisible(*cast_overlay_button));
}

TEST_F(MediaControlsImplTest, CastOverlayMediaControlsDisabled) {
  MediaControls().MediaElement().SetBooleanAttribute(HTMLNames::controlsAttr,
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
  MediaControls().MediaElement().SetBooleanAttribute(HTMLNames::controlsAttr,
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

  Element* download_button = GetElementByShadowPseudoId(
      MediaControls(), "-internal-media-controls-download-button");
  ASSERT_NE(nullptr, download_button);

  MediaControls().MediaElement().SetSrc("https://example.com/foo.mp4");
  test::RunPendingTasks();
  SimulateLoadedMetadata();

  // Download button should normally be displayed.
  EXPECT_TRUE(IsElementVisible(*download_button));
}

TEST_F(MediaControlsImplTest, DownloadButtonNotDisplayedEmptyUrl) {
  EnsureSizing();

  Element* download_button = GetElementByShadowPseudoId(
      MediaControls(), "-internal-media-controls-download-button");
  ASSERT_NE(nullptr, download_button);

  // Download button should not be displayed when URL is empty.
  MediaControls().MediaElement().SetSrc("");
  test::RunPendingTasks();
  SimulateLoadedMetadata();
  EXPECT_FALSE(IsElementVisible(*download_button));
}

TEST_F(MediaControlsImplTest, DownloadButtonNotDisplayedInfiniteDuration) {
  EnsureSizing();

  Element* download_button = GetElementByShadowPseudoId(
      MediaControls(), "-internal-media-controls-download-button");
  ASSERT_NE(nullptr, download_button);

  MediaControls().MediaElement().SetSrc("https://example.com/foo.mp4");
  test::RunPendingTasks();

  // Download button should not be displayed when duration is infinite.
  MediaControls().MediaElement().DurationChanged(
      std::numeric_limits<double>::infinity(), false /* requestSeek */);
  SimulateLoadedMetadata();
  EXPECT_FALSE(IsElementVisible(*download_button));

  // Download button should be shown if the duration changes back to finite.
  MediaControls().MediaElement().DurationChanged(20.0f,
                                                 false /* requestSeek */);
  SimulateLoadedMetadata();
  EXPECT_TRUE(IsElementVisible(*download_button));
}

TEST_F(MediaControlsImplTest, DownloadButtonNotDisplayedHLS) {
  EnsureSizing();

  Element* download_button = GetElementByShadowPseudoId(
      MediaControls(), "-internal-media-controls-download-button");
  ASSERT_NE(nullptr, download_button);

  // Download button should not be displayed for HLS streams.
  MediaControls().MediaElement().SetSrc("https://example.com/foo.m3u8");
  test::RunPendingTasks();
  SimulateLoadedMetadata();
  EXPECT_FALSE(IsElementVisible(*download_button));
}

TEST_F(MediaControlsImplTest, DownloadButtonInProductHelpDisabled) {
  EXPECT_FALSE(MediaControls().DownloadInProductHelp());
}

class MediaControlsImplPictureInPictureTest : public MediaControlsImplTest {
 public:
  void SetUp() override {
    RuntimeEnabledFeatures::SetPictureInPictureEnabled(true);
    MediaControlsImplTest::SetUp();
  }
};

TEST_F(MediaControlsImplPictureInPictureTest, PictureInPictureButtonVisible) {
  EnsureSizing();

  Element* picture_in_picture_button = GetElementByShadowPseudoId(
      MediaControls(), "-internal-media-controls-picture-in-picture-button");
  ASSERT_NE(nullptr, picture_in_picture_button);
  ASSERT_FALSE(IsElementVisible(*picture_in_picture_button));

  MediaControls().MediaElement().SetSrc("https://example.com/foo.mp4");
  test::RunPendingTasks();
  SetReady();
  test::RunPendingTasks();
  SimulateLoadedMetadata();
  ASSERT_TRUE(IsElementVisible(*picture_in_picture_button));

  MediaControls().MediaElement().SetSrc("");
  test::RunPendingTasks();
  SimulateLoadedMetadata();
  ASSERT_FALSE(IsElementVisible(*picture_in_picture_button));

  MediaControls().MediaElement().SetBooleanAttribute(
      HTMLNames::disablepictureinpictureAttr, true);
  MediaControls().MediaElement().SetSrc("https://example.com/foo.mp4");
  test::RunPendingTasks();
  SetReady();
  test::RunPendingTasks();
  SimulateLoadedMetadata();
  ASSERT_FALSE(IsElementVisible(*picture_in_picture_button));

  MediaControls().MediaElement().SetBooleanAttribute(
      HTMLNames::disablepictureinpictureAttr, false);
  test::RunPendingTasks();
  ASSERT_TRUE(IsElementVisible(*picture_in_picture_button));
}

class MediaControlsImplInProductHelpTest : public MediaControlsImplTest {
 public:
  void SetUp() override {
    MediaControlsImplTest::SetUp();
    ASSERT_TRUE(MediaControls().DownloadInProductHelp());
  }

  MediaDownloadInProductHelpManager& Manager() {
    return *MediaControls().DownloadInProductHelp();
  }

  void Play() { MediaControls().OnPlaying(); }
  void OnTimeUpdate() { MediaControls().OnTimeUpdate(); }

  bool EnableDownloadInProductHelp() override { return true; }
};

TEST_F(MediaControlsImplInProductHelpTest, DownloadButtonInProductHelp_Button) {
  EnsureSizing();

  // Inject the LayoutObject for the button to override the rect returned in
  // visual viewport.
  MediaControlDownloadButtonElement& button =
      GetDownloadButton(MediaControls());
  MockLayoutObject layout_object(&button);
  layout_object.SetVisible(true);
  button.SetLayoutObject(&layout_object);

  MediaControls().MediaElement().SetSrc("https://example.com/foo.mp4");
  test::RunPendingTasks();
  SimulateLoadedMetadata();
  Play();

  // Load above should have made the button wanted, which should trigger showing
  // in-product help.
  EXPECT_TRUE(Manager().IsShowingInProductHelp());

  // Disable the download button, which dismisses the in-product-help.
  button.SetIsWanted(false);
  EXPECT_FALSE(Manager().IsShowingInProductHelp());

  // Toggle again. In-product help is shown only once.
  button.SetIsWanted(true);
  EXPECT_FALSE(Manager().IsShowingInProductHelp());

  button.SetLayoutObject(nullptr);
}

TEST_F(MediaControlsImplInProductHelpTest,
       DownloadButtonInProductHelp_ControlsVisibility) {
  EnsureSizing();

  // Inject the LayoutObject for the button to override the rect returned in
  // visual viewport.
  MediaControlDownloadButtonElement& button =
      GetDownloadButton(MediaControls());
  MockLayoutObject layout_object(&button);
  layout_object.SetVisible(true);
  button.SetLayoutObject(&layout_object);

  // The in-product-help should not be shown while the controls are hidden.
  MediaControls().Hide();
  MediaControls().MediaElement().SetSrc("https://example.com/foo.mp4");
  test::RunPendingTasks();
  SimulateLoadedMetadata();
  Play();

  ASSERT_TRUE(button.IsWanted());
  EXPECT_FALSE(Manager().IsShowingInProductHelp());

  // Showing the controls initiates showing in-product-help.
  MediaControls().MaybeShow();
  EXPECT_TRUE(Manager().IsShowingInProductHelp());

  OnTimeUpdate();
  EXPECT_TRUE(Manager().IsShowingInProductHelp());

  // Hiding the controls dismissed in-product-help.
  MediaControls().Hide();
  EXPECT_FALSE(Manager().IsShowingInProductHelp());

  button.SetLayoutObject(nullptr);
}

TEST_F(MediaControlsImplInProductHelpTest,
       DownloadButtonInProductHelp_ButtonVisibility) {
  EnsureSizing();

  // Inject the LayoutObject for the button to override the rect returned in
  // visual viewport.
  MediaControlDownloadButtonElement& button =
      GetDownloadButton(MediaControls());
  MockLayoutObject layout_object(&button);
  button.SetLayoutObject(&layout_object);

  // The in-product-help should not be shown while the button is hidden.
  layout_object.SetVisible(false);
  MediaControls().MediaElement().SetSrc("https://example.com/foo.mp4");
  test::RunPendingTasks();
  SimulateLoadedMetadata();
  Play();

  ASSERT_TRUE(button.IsWanted());
  EXPECT_FALSE(Manager().IsShowingInProductHelp());

  // Make the button visible to show in-product-help.
  layout_object.SetVisible(true);
  button.SetIsWanted(false);
  button.SetIsWanted(true);
  EXPECT_TRUE(Manager().IsShowingInProductHelp());

  button.SetLayoutObject(nullptr);
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

TEST_F(MediaControlsImplTest, TimelineMetricsWidth) {
  MediaControls().MediaElement().SetSrc("https://example.com/foo.mp4");
  test::RunPendingTasks();
  SetReady();
  EnsureSizing();
  test::RunPendingTasks();

  MediaControlTimelineElement* timeline = TimelineElement();
  ASSERT_TRUE(IsElementVisible(*timeline));
  ASSERT_LT(0, timeline->getBoundingClientRect()->width());

  MediaControls().MediaElement().Play();
  test::RunPendingTasks();

  GetHistogramTester().ExpectUniqueSample(
      "Media.Timeline.Width.InlineLandscape",
      timeline->getBoundingClientRect()->width(), 1);
  GetHistogramTester().ExpectTotalCount("Media.Timeline.Width.InlinePortrait",
                                        0);
  GetHistogramTester().ExpectTotalCount(
      "Media.Timeline.Width.FullscreenLandscape", 0);
  GetHistogramTester().ExpectTotalCount(
      "Media.Timeline.Width.FullscreenPortrait", 0);
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

  WebFloatPoint trackCenter(timelineRect->left() + timelineRect->width() / 2,
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
  DOMRect* timeline_rect = TimelineElement()->getBoundingClientRect();
  ASSERT_LT(0, timeline_rect->width());

  EXPECT_EQ(0, MediaControls().MediaElement().currentTime());

  float y = timeline_rect->top() + timeline_rect->height() / 2;
  WebFloatPoint thumb(timeline_rect->left(), y);
  WebFloatPoint track_two_thirds(
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
  DOMRect* timelineRect = TimelineElement()->getBoundingClientRect();
  ASSERT_LT(0, timelineRect->width());

  EXPECT_EQ(0, MediaControls().MediaElement().currentTime());

  float y = timelineRect->top() + timelineRect->height() / 2;
  WebFloatPoint trackOneThird(
      timelineRect->left() + timelineRect->width() * 1 / 3, y);
  WebFloatPoint trackTwoThirds(
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
  DOMRect* timelineRect = TimelineElement()->getBoundingClientRect();
  ASSERT_LT(0, timelineRect->width());

  EXPECT_EQ(0, MediaControls().MediaElement().currentTime());

  float y = timelineRect->top() + timelineRect->height() / 2;
  WebFloatPoint trackTwoThirds(
      timelineRect->left() + timelineRect->width() * 2 / 3, y);
  WebFloatPoint trackEnd(timelineRect->left() + timelineRect->width(), y);
  WebFloatPoint trackOneThird(
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

    MediaControlsImplTest::SetUp();
  }

  bool IsCursorHidden() {
    const CSSPropertyValueSet* style = MediaControls().InlineStyle();
    if (!style)
      return false;
    return style->GetPropertyValue(CSSPropertyCursor) == "none";
  }
};

}  // namespace

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
  MediaControls().DispatchEvent(*Event::Create("focusin"));
  platform()->RunForPeriodSeconds(2);
  EXPECT_TRUE(IsElementVisible(*panel));

  // Seeking on the timeline or volume bar prevents controls from hiding.
  MediaControls().DispatchEvent(*Event::Create("input"));
  platform()->RunForPeriodSeconds(2);
  EXPECT_TRUE(IsElementVisible(*panel));

  // Pressing a key prevents controls from hiding.
  MediaControls().PanelElement()->DispatchEvent(*Event::Create("keypress"));
  platform()->RunForPeriodSeconds(2);
  EXPECT_TRUE(IsElementVisible(*panel));

  // Once user interaction stops, controls can hide.
  platform()->RunForPeriodSeconds(2);
  SimulateTransitionEnd(*panel);
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
  auto page_holder = DummyPageHolder::Create();

  HTMLMediaElement* element =
      HTMLVideoElement::Create(page_holder->GetDocument());
  page_holder->GetDocument().body()->AppendChild(element);

  RemotePlayback* remote_playback =
      HTMLMediaElementRemotePlayback::remote(*element);

  EXPECT_TRUE(remote_playback->HasEventListeners());
  EXPECT_TRUE(HasAvailabilityCallbacks(remote_playback));

  WeakPersistent<HTMLMediaElement> weak_persistent_video = element;
  {
    Persistent<HTMLMediaElement> persistent_video = element;
    page_holder->GetDocument().body()->SetInnerHTMLFromString("");

    // When removed from the document, the event listeners should have been
    // dropped.
    EXPECT_FALSE(remote_playback->HasEventListeners());
    EXPECT_FALSE(HasAvailabilityCallbacks(remote_playback));
  }

  test::RunPendingTasks();

  ThreadState::Current()->CollectAllGarbage();

  // It has been GC'd.
  EXPECT_EQ(nullptr, weak_persistent_video);
}

TEST_F(MediaControlsImplTest,
       ReInsertingInDocumentRestoresListenersAndCallbacks) {
  auto page_holder = DummyPageHolder::Create();

  HTMLMediaElement* element =
      HTMLVideoElement::Create(page_holder->GetDocument());
  page_holder->GetDocument().body()->AppendChild(element);

  RemotePlayback* remote_playback =
      HTMLMediaElementRemotePlayback::remote(*element);

  // This should be a no-op. We keep a reference on the media element to avoid
  // an unexpected GC.
  {
    Persistent<HTMLMediaElement> video_holder = element;
    page_holder->GetDocument().body()->RemoveChild(element);
    page_holder->GetDocument().body()->AppendChild(video_holder.Get());
    EXPECT_TRUE(remote_playback->HasEventListeners());
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

TEST_F(MediaControlsImplTest, OverflowMenuMetricsTimeToAction) {
  GetHistogramTester().ExpectTotalCount(kTimeToActionHistogramName, 0);
  GetHistogramTester().ExpectTotalCount(kTimeToDismissHistogramName, 0);

  // Test with the menu open for 42 seconds.
  ToggleOverflowMenu();
  AdvanceClock(42);
  ClickOverflowButton();
  GetHistogramTester().ExpectBucketCount(kTimeToActionHistogramName, 42, 1);
  GetHistogramTester().ExpectTotalCount(kTimeToActionHistogramName, 1);

  // Test with the menu open for 90 seconds.
  ToggleOverflowMenu();
  AdvanceClock(90);
  ClickOverflowButton();

  GetHistogramTester().ExpectBucketCount(kTimeToActionHistogramName, 90, 1);
  GetHistogramTester().ExpectTotalCount(kTimeToActionHistogramName, 2);

  // Test with the menu open for 42 seconds.
  ToggleOverflowMenu();
  AdvanceClock(42);
  ClickOverflowButton();
  GetHistogramTester().ExpectBucketCount(kTimeToActionHistogramName, 42, 2);
  GetHistogramTester().ExpectTotalCount(kTimeToActionHistogramName, 3);

  // Test with the menu open for 1000 seconds.
  ToggleOverflowMenu();
  AdvanceClock(1000);
  ClickOverflowButton();
  GetHistogramTester().ExpectBucketCount(kTimeToActionHistogramName, 100, 1);
  GetHistogramTester().ExpectTotalCount(kTimeToActionHistogramName, 4);
  GetHistogramTester().ExpectTotalCount(kTimeToDismissHistogramName, 0);
}

TEST_F(MediaControlsImplTest, OverflowMenuMetricsTimeToDismiss) {
  GetHistogramTester().ExpectTotalCount(kTimeToDismissHistogramName, 0);
  GetHistogramTester().ExpectTotalCount(kTimeToActionHistogramName, 0);

  // Test with the menu open for 42 seconds.
  ToggleOverflowMenu();
  AdvanceClock(42);
  ToggleOverflowMenu();
  GetHistogramTester().ExpectBucketCount(kTimeToDismissHistogramName, 42, 1);
  GetHistogramTester().ExpectTotalCount(kTimeToDismissHistogramName, 1);

  // Test with the menu open for 90 seconds.
  ToggleOverflowMenu();
  AdvanceClock(90);
  ToggleOverflowMenu();
  GetHistogramTester().ExpectBucketCount(kTimeToDismissHistogramName, 90, 1);
  GetHistogramTester().ExpectTotalCount(kTimeToDismissHistogramName, 2);

  // Test with the menu open for 42 seconds.
  ToggleOverflowMenu();
  AdvanceClock(42);
  ToggleOverflowMenu();
  GetHistogramTester().ExpectBucketCount(kTimeToDismissHistogramName, 42, 2);
  GetHistogramTester().ExpectTotalCount(kTimeToDismissHistogramName, 3);

  // Test with the menu open for 1000 seconds.
  ToggleOverflowMenu();
  AdvanceClock(1000);
  ToggleOverflowMenu();
  GetHistogramTester().ExpectBucketCount(kTimeToDismissHistogramName, 100, 1);
  GetHistogramTester().ExpectTotalCount(kTimeToDismissHistogramName, 4);
  GetHistogramTester().ExpectTotalCount(kTimeToActionHistogramName, 0);
}

TEST_F(MediaControlsImplTest, CastOverlayDefaultHidesOnTimer) {
  MediaControls().MediaElement().SetBooleanAttribute(HTMLNames::controlsAttr,
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
  MediaControls().MediaElement().SetBooleanAttribute(HTMLNames::controlsAttr,
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

class ModernMediaControlsImplTest : public MediaControlsImplTest {
 public:
  void SetUp() override {
    RuntimeEnabledFeatures::SetModernMediaControlsEnabled(true);
    MediaControlsImplTest::SetUp();
  }
};

TEST_F(ModernMediaControlsImplTest, ControlsShouldUseSafeAreaInsets) {
  GetDocument().View()->UpdateAllLifecyclePhases();
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
  GetDocument().View()->UpdateAllLifecyclePhases();

  {
    const ComputedStyle* style = MediaControls().GetComputedStyle();
    EXPECT_EQ(1.0, style->MarginTop().Pixels());
    EXPECT_EQ(2.0, style->MarginLeft().Pixels());
    EXPECT_EQ(3.0, style->MarginBottom().Pixels());
    EXPECT_EQ(4.0, style->MarginRight().Pixels());
  }
}

}  // namespace blink
