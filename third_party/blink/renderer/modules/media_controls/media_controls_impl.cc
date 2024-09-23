/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"

#include "base/auto_reset.h"
#include "media/base/media_switches.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/user_metrics_action.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mutation_observer_init.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_forbidden_scope.h"
#include "third_party/blink/renderer/core/dom/mutation_observer.h"
#include "third_party/blink/renderer/core/dom/mutation_record.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/events/gesture_event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/events/touch_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/media/autoplay_policy.h"
#include "third_party/blink/renderer/core/html/media/html_audio_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_element_controls_list.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/time_ranges.h"
#include "third_party/blink/renderer/core/html/track/text_track.h"
#include "third_party/blink/renderer/core/html/track/text_track_container.h"
#include "third_party/blink/renderer/core/html/track/text_track_list.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/page/spatial_navigation.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_entry.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_animated_arrow_container_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_button_panel_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_cast_button_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_consts.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_current_time_display_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_display_cutout_fullscreen_button_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_download_button_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_elements_helper.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_fullscreen_button_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_loading_panel_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_mute_button_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_overflow_menu_button_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_overflow_menu_list_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_overlay_enclosure_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_overlay_play_button_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_panel_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_panel_enclosure_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_picture_in_picture_button_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_play_button_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_playback_speed_button_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_playback_speed_list_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_remaining_time_display_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_scrubbing_message_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_text_track_list_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_timeline_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_toggle_closed_captions_button_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_volume_control_container_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_volume_slider_element.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_display_cutout_delegate.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_media_event_listener.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_orientation_lock_delegate.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_resource_loader.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_rotate_to_fullscreen_delegate.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_shared_helper.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_text_track_manager.h"
#include "third_party/blink/renderer/modules/remoteplayback/remote_playback.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

namespace {

// (2px left border + 6px left padding + 56px button + 6px right padding + 2px
// right border) = 72px.
constexpr int kMinWidthForOverlayPlayButton = 72;

constexpr int kMinScrubbingMessageWidth = 300;

const char* const kStateCSSClasses[8] = {
    "state-no-source",                 // kNoSource
    "state-no-metadata",               // kNotLoaded
    "state-loading-metadata-paused",   // kLoadingMetadataPaused
    "state-loading-metadata-playing",  // kLoadingMetadataPlaying
    "state-stopped",                   // kStopped
    "state-playing",                   // kPlaying
    "state-buffering",                 // kBuffering
    "state-scrubbing",                 // kScrubbing
};

// The padding in pixels inside the button panel.
constexpr int kAudioButtonPadding = 20;
constexpr int kVideoButtonPadding = 26;

const char kShowDefaultPosterCSSClass[] = "use-default-poster";
const char kActAsAudioControlsCSSClass[] = "audio-only";
const char kScrubbingMessageCSSClass[] = "scrubbing-message";
const char kTestModeCSSClass[] = "test-mode";

// The delay between two taps to be recognized as a double tap gesture.
constexpr base::TimeDelta kDoubleTapDelay = base::Milliseconds(300);

// The time user have to hover on mute button to show volume slider.
// If this value is changed, you need to change the corresponding value in
// media_controls_impl_test.cc
constexpr base::TimeDelta kTimeToShowVolumeSlider = base::Milliseconds(200);
constexpr base::TimeDelta kTimeToShowVolumeSliderTest = base::Milliseconds(0);

// The number of seconds to jump when double tapping.
constexpr int kNumberOfSecondsToJump = 10;

void MaybeParserAppendChild(Element* parent, Element* child) {
  DCHECK(parent);
  if (child)
    parent->ParserAppendChild(child);
}

bool ShouldShowPlaybackSpeedButton(HTMLMediaElement& media_element) {
  // The page disabled the button via the controlsList attribute.
  if (media_element.ControlsListInternal()->ShouldHidePlaybackRate() &&
      !media_element.UserWantsControlsVisible()) {
    UseCounter::Count(media_element.GetDocument(),
                      WebFeature::kHTMLMediaElementControlsListNoPlaybackRate);
    return false;
  }

  // A MediaStream is not seekable.
  if (media_element.GetLoadType() == WebMediaPlayer::kLoadTypeMediaStream)
    return false;

  // Don't allow for live infinite streams.
  if (media_element.duration() == std::numeric_limits<double>::infinity() &&
      media_element.getReadyState() > HTMLMediaElement::kHaveNothing) {
    return false;
  }

  return true;
}

bool ShouldShowPictureInPictureButton(HTMLMediaElement& media_element) {
  return media_element.SupportsPictureInPicture();
}

bool ShouldShowCastButton(HTMLMediaElement& media_element) {
  if (media_element.FastHasAttribute(html_names::kDisableremoteplaybackAttr))
    return false;

  // Explicitly do not show cast button when the mediaControlsEnabled setting is
  // false, to make sure the overlay does not appear.
  Document& document = media_element.GetDocument();
  if (document.GetSettings() &&
      (!document.GetSettings()->GetMediaControlsEnabled())) {
    return false;
  }

  // The page disabled the button via the attribute.
  if (media_element.ControlsListInternal()->ShouldHideRemotePlayback() &&
      !media_element.UserWantsControlsVisible()) {
    UseCounter::Count(
        media_element.GetDocument(),
        WebFeature::kHTMLMediaElementControlsListNoRemotePlayback);
    return false;
  }

  return RemotePlayback::From(media_element).RemotePlaybackAvailable();
}

bool ShouldShowCastOverlayButton(HTMLMediaElement& media_element) {
  return !media_element.ShouldShowControls() &&
         RuntimeEnabledFeatures::MediaCastOverlayButtonEnabled() &&
         ShouldShowCastButton(media_element);
}

bool PreferHiddenVolumeControls(const Document& document) {
  return !document.GetSettings() ||
         document.GetSettings()->GetPreferHiddenVolumeControls();
}

// If you change this value, then also update the corresponding value in
// web_tests/media/media-controls.js.
constexpr base::TimeDelta kTimeWithoutMouseMovementBeforeHidingMediaControls =
    base::Seconds(2.5);

base::TimeDelta GetTimeWithoutMouseMovementBeforeHidingMediaControls() {
  return kTimeWithoutMouseMovementBeforeHidingMediaControls;
}

}  // namespace

class MediaControlsImpl::BatchedControlUpdate {
  STACK_ALLOCATED();

 public:
  explicit BatchedControlUpdate(MediaControlsImpl* controls)
      : controls_(controls) {
    DCHECK(IsMainThread());
    DCHECK_GE(batch_depth_, 0);
    ++batch_depth_;
  }

  BatchedControlUpdate(const BatchedControlUpdate&) = delete;
  BatchedControlUpdate& operator=(const BatchedControlUpdate&) = delete;

  ~BatchedControlUpdate() {
    DCHECK(IsMainThread());
    DCHECK_GT(batch_depth_, 0);
    if (!(--batch_depth_))
      controls_->ComputeWhichControlsFit();
  }

 private:
  MediaControlsImpl* controls_;
  static int batch_depth_;
};

// Count of number open batches for controls visibility.
int MediaControlsImpl::BatchedControlUpdate::batch_depth_ = 0;

class MediaControlsImpl::MediaControlsResizeObserverDelegate final
    : public ResizeObserver::Delegate {
 public:
  explicit MediaControlsResizeObserverDelegate(MediaControlsImpl* controls)
      : controls_(controls) {
    DCHECK(controls);
  }
  ~MediaControlsResizeObserverDelegate() override = default;

  void OnResize(
      const HeapVector<Member<ResizeObserverEntry>>& entries) override {
    DCHECK_EQ(1u, entries.size());
    DCHECK_EQ(entries[0]->target(), controls_->MediaElement());
    controls_->NotifyElementSizeChanged(entries[0]->contentRect());
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(controls_);
    ResizeObserver::Delegate::Trace(visitor);
  }

 private:
  Member<MediaControlsImpl> controls_;
};

// Observes changes to the HTMLMediaElement attributes that affect controls.
class MediaControlsImpl::MediaElementMutationCallback
    : public MutationObserver::Delegate {
 public:
  explicit MediaElementMutationCallback(MediaControlsImpl* controls)
      : controls_(controls), observer_(MutationObserver::Create(this)) {
    MutationObserverInit* init = MutationObserverInit::Create();
    init->setAttributeOldValue(true);
    init->setAttributes(true);
    init->setAttributeFilter(
        {html_names::kDisableremoteplaybackAttr.ToString(),
         html_names::kDisablepictureinpictureAttr.ToString(),
         html_names::kPosterAttr.ToString()});
    observer_->observe(&controls_->MediaElement(), init, ASSERT_NO_EXCEPTION);
  }

  ExecutionContext* GetExecutionContext() const override {
    return controls_->GetDocument().GetExecutionContext();
  }

  void Deliver(const MutationRecordVector& records,
               MutationObserver&) override {
    for (const auto& record : records) {
      if (record->type() != "attributes")
        continue;

      const auto* element = To<Element>(record->target());
      if (record->oldValue() == element->getAttribute(record->attributeName()))
        continue;

      if (record->attributeName() ==
          html_names::kDisableremoteplaybackAttr.ToString()) {
        controls_->RefreshCastButtonVisibilityWithoutUpdate();
      }

      if (record->attributeName() ==
              html_names::kDisablepictureinpictureAttr.ToString() &&
          controls_->picture_in_picture_button_) {
        controls_->picture_in_picture_button_->SetIsWanted(
            ShouldShowPictureInPictureButton(controls_->MediaElement()));
      }

      if (record->attributeName() == html_names::kPosterAttr.ToString())
        controls_->UpdateCSSClassFromState();

      BatchedControlUpdate batch(controls_);
    }
  }

  void Disconnect() { observer_->disconnect(); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(controls_);
    visitor->Trace(observer_);
    MutationObserver::Delegate::Trace(visitor);
  }

 private:
  Member<MediaControlsImpl> controls_;
  Member<MutationObserver> observer_;
};

bool MediaControlsImpl::IsTouchEvent(Event* event) {
  auto* mouse_event = DynamicTo<MouseEvent>(event);
  return IsA<TouchEvent>(event) || IsA<GestureEvent>(event) ||
         (mouse_event && mouse_event->FromTouch());
}

MediaControlsImpl::MediaControlsImpl(HTMLMediaElement& media_element)
    : HTMLDivElement(media_element.GetDocument()),
      MediaControls(media_element),
      overlay_enclosure_(nullptr),
      overlay_play_button_(nullptr),
      overlay_cast_button_(nullptr),
      enclosure_(nullptr),
      panel_(nullptr),
      play_button_(nullptr),
      timeline_(nullptr),
      scrubbing_message_(nullptr),
      current_time_display_(nullptr),
      duration_display_(nullptr),
      mute_button_(nullptr),
      volume_slider_(nullptr),
      volume_control_container_(nullptr),
      toggle_closed_captions_button_(nullptr),
      text_track_list_(nullptr),
      playback_speed_button_(nullptr),
      playback_speed_list_(nullptr),
      overflow_list_(nullptr),
      media_button_panel_(nullptr),
      loading_panel_(nullptr),
      picture_in_picture_button_(nullptr),
      animated_arrow_container_element_(nullptr),
      cast_button_(nullptr),
      fullscreen_button_(nullptr),
      display_cutout_fullscreen_button_(nullptr),
      download_button_(nullptr),
      media_event_listener_(
          MakeGarbageCollected<MediaControlsMediaEventListener>(this)),
      orientation_lock_delegate_(nullptr),
      rotate_to_fullscreen_delegate_(nullptr),
      display_cutout_delegate_(nullptr),
      hide_media_controls_timer_(
          media_element.GetDocument().GetTaskRunner(TaskType::kInternalMedia),
          this,
          &MediaControlsImpl::HideMediaControlsTimerFired),
      hide_timer_behavior_flags_(kIgnoreNone),
      is_mouse_over_controls_(false),
      is_paused_for_scrubbing_(false),
      resize_observer_(ResizeObserver::Create(
          media_element.GetDocument().domWindow(),
          MakeGarbageCollected<MediaControlsResizeObserverDelegate>(this))),
      element_size_changed_timer_(
          media_element.GetDocument().GetTaskRunner(TaskType::kInternalMedia),
          this,
          &MediaControlsImpl::ElementSizeChangedTimerFired),
      keep_showing_until_timer_fires_(false),
      tap_timer_(
          media_element.GetDocument().GetTaskRunner(TaskType::kInternalMedia),
          this,
          &MediaControlsImpl::TapTimerFired),
      volume_slider_wanted_timer_(
          media_element.GetDocument().GetTaskRunner(TaskType::kInternalMedia),
          this,
          &MediaControlsImpl::VolumeSliderWantedTimerFired),
      text_track_manager_(
          MakeGarbageCollected<MediaControlsTextTrackManager>(media_element)) {
  // On touch devices, start with the assumption that the user will interact via
  // touch events.
  Settings* settings = media_element.GetDocument().GetSettings();
  is_touch_interaction_ = settings ? settings->GetMaxTouchPoints() > 0 : false;

  resize_observer_->observe(&media_element);
}

MediaControlsImpl* MediaControlsImpl::Create(HTMLMediaElement& media_element,
                                             ShadowRoot& shadow_root) {
  MediaControlsImpl* controls =
      MakeGarbageCollected<MediaControlsImpl>(media_element);
  controls->SetShadowPseudoId(AtomicString("-webkit-media-controls"));
  controls->InitializeControls();
  controls->Reset();

  if (RuntimeEnabledFeatures::VideoFullscreenOrientationLockEnabled() &&
      IsA<HTMLVideoElement>(media_element)) {
    // Initialize the orientation lock when going fullscreen feature.
    controls->orientation_lock_delegate_ =
        MakeGarbageCollected<MediaControlsOrientationLockDelegate>(
            To<HTMLVideoElement>(media_element));
  }

  if (MediaControlsDisplayCutoutDelegate::IsEnabled() &&
      IsA<HTMLVideoElement>(media_element)) {
    // Initialize the pinch gesture to expand into the display cutout feature.
    controls->display_cutout_delegate_ =
        MakeGarbageCollected<MediaControlsDisplayCutoutDelegate>(
            To<HTMLVideoElement>(media_element));
  }

  if (RuntimeEnabledFeatures::VideoRotateToFullscreenEnabled() &&
      IsA<HTMLVideoElement>(media_element)) {
    // Initialize the rotate-to-fullscreen feature.
    controls->rotate_to_fullscreen_delegate_ =
        MakeGarbageCollected<MediaControlsRotateToFullscreenDelegate>(
            To<HTMLVideoElement>(media_element));
  }

  MediaControlsResourceLoader::InjectMediaControlsUAStyleSheet();

  shadow_root.ParserAppendChild(controls);
  return controls;
}

// The media controls DOM structure looks like:
//
// MediaControlsImpl
//     (-webkit-media-controls)
// +-MediaControlLoadingPanelElement
// |    (-internal-media-controls-loading-panel)
// +-MediaControlOverlayEnclosureElement
// |    (-webkit-media-controls-overlay-enclosure)
// | \-MediaControlCastButtonElement
// |     (-internal-media-controls-overlay-cast-button)
// \-MediaControlPanelEnclosureElement
//   |    (-webkit-media-controls-enclosure)
//   \-MediaControlPanelElement
//     |    (-webkit-media-controls-panel)
//     +-MediaControlScrubbingMessageElement
//     |  (-internal-media-controls-scrubbing-message)
//     |  {if is video element}
//     +-MediaControlOverlayPlayButtonElement
//     |  (-webkit-media-controls-overlay-play-button)
//     |  {if mediaControlsOverlayPlayButtonEnabled}
//     +-MediaControlButtonPanelElement
//     |  |  (-internal-media-controls-button-panel)
//     |  |  <video> only, otherwise children are directly attached to parent
//     |  +-MediaControlPlayButtonElement
//     |  |    (-webkit-media-controls-play-button)
//     |  |    {if !mediaControlsOverlayPlayButtonEnabled}
//     |  +-MediaControlCurrentTimeDisplayElement
//     |  |    (-webkit-media-controls-current-time-display)
//     |  +-MediaControlRemainingTimeDisplayElement
//     |  |    (-webkit-media-controls-time-remaining-display)
//     |  |    {if !IsLivePlayback}
//     |  +-HTMLDivElement
//     |  |    (-internal-media-controls-button-spacer)
//     |  |    {if is video element}
//     |  +-MediaControlVolumeControlContainerElement
//     |  |  |  (-webkit-media-controls-volume-control-container)
//     |  |  +-HTMLDivElement
//     |  |  |    (-webkit-media-controls-volume-control-hover-background)
//     |  |  +-MediaControlMuteButtonElement
//     |  |  |    (-webkit-media-controls-mute-button)
//     |  |  +-MediaControlVolumeSliderElement
//     |  |       (-webkit-media-controls-volume-slider)
//     |  +-MediaControlPictureInPictureButtonElement
//     |  |    (-webkit-media-controls-picture-in-picture-button)
//     |  +-MediaControlFullscreenButtonElement
//     |  |    (-webkit-media-controls-fullscreen-button)
//     \-MediaControlTimelineElement
//          (-webkit-media-controls-timeline)
//          {if !IsLivePlayback}
// +-MediaControlTextTrackListElement
// |    (-internal-media-controls-text-track-list)
// | {for each renderable text track}
//  \-MediaControlTextTrackListItem
//  |   (-internal-media-controls-text-track-list-item)
//  +-MediaControlTextTrackListItemInput
//  |    (-internal-media-controls-text-track-list-item-input)
//  +-MediaControlTextTrackListItemCaptions
//  |    (-internal-media-controls-text-track-list-kind-captions)
//  +-MediaControlTextTrackListItemSubtitles
//       (-internal-media-controls-text-track-list-kind-subtitles)
// +-MediaControlDisplayCutoutFullscreenElement
//       (-internal-media-controls-display-cutout-fullscreen-button)
void MediaControlsImpl::InitializeControls() {
  if (ShouldShowVideoControls()) {
    loading_panel_ =
        MakeGarbageCollected<MediaControlLoadingPanelElement>(*this);
    ParserAppendChild(loading_panel_);
  }

  overlay_enclosure_ =
      MakeGarbageCollected<MediaControlOverlayEnclosureElement>(*this);

  if (RuntimeEnabledFeatures::MediaControlsOverlayPlayButtonEnabled()) {
    overlay_play_button_ =
        MakeGarbageCollected<MediaControlOverlayPlayButtonElement>(*this);
  }

  overlay_cast_button_ =
      MakeGarbageCollected<MediaControlCastButtonElement>(*this, true);
  overlay_enclosure_->ParserAppendChild(overlay_cast_button_);

  ParserAppendChild(overlay_enclosure_);

  // Create an enclosing element for the panel so we can visually offset the
  // controls correctly.
  enclosure_ = MakeGarbageCollected<MediaControlPanelEnclosureElement>(*this);

  panel_ = MakeGarbageCollected<MediaControlPanelElement>(*this);

  // On the video controls, the buttons belong to a separate button panel. This
  // is because they are displayed in two lines.
  if (ShouldShowVideoControls()) {
    media_button_panel_ =
        MakeGarbageCollected<MediaControlButtonPanelElement>(*this);
    scrubbing_message_ =
        MakeGarbageCollected<MediaControlScrubbingMessageElement>(*this);
  }

  play_button_ = MakeGarbageCollected<MediaControlPlayButtonElement>(*this);

  current_time_display_ =
      MakeGarbageCollected<MediaControlCurrentTimeDisplayElement>(*this);
  current_time_display_->SetIsWanted(true);

  duration_display_ =
      MakeGarbageCollected<MediaControlRemainingTimeDisplayElement>(*this);
  timeline_ = MakeGarbageCollected<MediaControlTimelineElement>(*this);
  mute_button_ = MakeGarbageCollected<MediaControlMuteButtonElement>(*this);

  volume_control_container_ =
      MakeGarbageCollected<MediaControlVolumeControlContainerElement>(*this);
  volume_slider_ = MakeGarbageCollected<MediaControlVolumeSliderElement>(
      *this, volume_control_container_.Get());
  if (PreferHiddenVolumeControls(GetDocument()))
    volume_slider_->SetIsWanted(false);

  if (GetDocument().GetSettings() &&
      GetDocument().GetSettings()->GetPictureInPictureEnabled() &&
      IsA<HTMLVideoElement>(MediaElement())) {
    picture_in_picture_button_ =
        MakeGarbageCollected<MediaControlPictureInPictureButtonElement>(*this);
    picture_in_picture_button_->SetIsWanted(
        ShouldShowPictureInPictureButton(MediaElement()));
  }

  if (RuntimeEnabledFeatures::DisplayCutoutAPIEnabled() &&
      IsA<HTMLVideoElement>(MediaElement())) {
    display_cutout_fullscreen_button_ =
        MakeGarbageCollected<MediaControlDisplayCutoutFullscreenButtonElement>(
            *this);
  }

  fullscreen_button_ =
      MakeGarbageCollected<MediaControlFullscreenButtonElement>(*this);
  download_button_ =
      MakeGarbageCollected<MediaControlDownloadButtonElement>(*this);
  cast_button_ =
      MakeGarbageCollected<MediaControlCastButtonElement>(*this, false);
  toggle_closed_captions_button_ =
      MakeGarbageCollected<MediaControlToggleClosedCaptionsButtonElement>(
          *this);
  playback_speed_button_ =
      MakeGarbageCollected<MediaControlPlaybackSpeedButtonElement>(*this);
  playback_speed_button_->SetIsWanted(
      ShouldShowPlaybackSpeedButton(MediaElement()));
  overflow_menu_ =
      MakeGarbageCollected<MediaControlOverflowMenuButtonElement>(*this);

  PopulatePanel();
  enclosure_->ParserAppendChild(panel_);

  ParserAppendChild(enclosure_);

  text_track_list_ =
      MakeGarbageCollected<MediaControlTextTrackListElement>(*this);
  ParserAppendChild(text_track_list_);

  playback_speed_list_ =
      MakeGarbageCollected<MediaControlPlaybackSpeedListElement>(*this);
  ParserAppendChild(playback_speed_list_);

  overflow_list_ =
      MakeGarbageCollected<MediaControlOverflowMenuListElement>(*this);
  ParserAppendChild(overflow_list_);

  // The order in which we append elements to the overflow list is significant
  // because it determines how the elements show up in the overflow menu
  // relative to each other.  The first item appended appears at the top of the
  // overflow menu.
  overflow_list_->ParserAppendChild(play_button_->CreateOverflowElement(
      MakeGarbageCollected<MediaControlPlayButtonElement>(*this)));
  overflow_list_->ParserAppendChild(fullscreen_button_->CreateOverflowElement(
      MakeGarbageCollected<MediaControlFullscreenButtonElement>(*this)));
  overflow_list_->ParserAppendChild(download_button_->CreateOverflowElement(
      MakeGarbageCollected<MediaControlDownloadButtonElement>(*this)));
  overflow_list_->ParserAppendChild(mute_button_->CreateOverflowElement(
      MakeGarbageCollected<MediaControlMuteButtonElement>(*this)));
  overflow_list_->ParserAppendChild(cast_button_->CreateOverflowElement(
      MakeGarbageCollected<MediaControlCastButtonElement>(*this, false)));
  overflow_list_->ParserAppendChild(
      toggle_closed_captions_button_->CreateOverflowElement(
          MakeGarbageCollected<MediaControlToggleClosedCaptionsButtonElement>(
              *this)));
  overflow_list_->ParserAppendChild(
      playback_speed_button_->CreateOverflowElement(
          MakeGarbageCollected<MediaControlPlaybackSpeedButtonElement>(*this)));
  if (picture_in_picture_button_) {
    overflow_list_->ParserAppendChild(
        picture_in_picture_button_->CreateOverflowElement(
            MakeGarbageCollected<MediaControlPictureInPictureButtonElement>(
                *this)));
  }

  // Set the default CSS classes.
  UpdateCSSClassFromState();
}

void MediaControlsImpl::PopulatePanel() {
  // Clear the panels.
  panel_->setInnerHTML("");
  if (media_button_panel_)
    media_button_panel_->setInnerHTML("");

  Element* button_panel = panel_;
  if (ShouldShowVideoControls()) {
    MaybeParserAppendChild(panel_, scrubbing_message_);
    if (display_cutout_fullscreen_button_)
      panel_->ParserAppendChild(display_cutout_fullscreen_button_);

    MaybeParserAppendChild(panel_, overlay_play_button_);
    panel_->ParserAppendChild(media_button_panel_);
    button_panel = media_button_panel_;
  }

  button_panel->ParserAppendChild(play_button_);
  button_panel->ParserAppendChild(current_time_display_);
  button_panel->ParserAppendChild(duration_display_);

  if (ShouldShowVideoControls()) {
    MediaControlElementsHelper::CreateDiv(
        AtomicString("-internal-media-controls-button-spacer"), button_panel);
  }

  panel_->ParserAppendChild(timeline_);

  MaybeParserAppendChild(volume_control_container_, volume_slider_);
  volume_control_container_->ParserAppendChild(mute_button_);
  button_panel->ParserAppendChild(volume_control_container_);

  button_panel->ParserAppendChild(fullscreen_button_);

  button_panel->ParserAppendChild(overflow_menu_);

  // Attach hover background divs.
  AttachHoverBackground(play_button_);
  AttachHoverBackground(fullscreen_button_);
  AttachHoverBackground(overflow_menu_);
}

void MediaControlsImpl::AttachHoverBackground(Element* element) {
  MediaControlElementsHelper::CreateDiv(
      AtomicString("-internal-media-controls-button-hover-background"),
      element->GetShadowRoot());
}

Node::InsertionNotificationRequest MediaControlsImpl::InsertedInto(
    ContainerNode& root) {
  if (!MediaElement().isConnected())
    return HTMLDivElement::InsertedInto(root);

  // TODO(mlamouri): we should show the controls instead of having
  // HTMLMediaElement do it.

  // m_windowEventListener doesn't need to be re-attached as it's only needed
  // when a menu is visible.
  media_event_listener_->Attach();
  if (orientation_lock_delegate_)
    orientation_lock_delegate_->Attach();
  if (rotate_to_fullscreen_delegate_)
    rotate_to_fullscreen_delegate_->Attach();
  if (display_cutout_delegate_)
    display_cutout_delegate_->Attach();

  if (!resize_observer_) {
    resize_observer_ = ResizeObserver::Create(
        MediaElement().GetDocument().domWindow(),
        MakeGarbageCollected<MediaControlsResizeObserverDelegate>(this));
    HTMLMediaElement& html_media_element = MediaElement();
    resize_observer_->observe(&html_media_element);
  }

  if (!element_mutation_callback_) {
    element_mutation_callback_ =
        MakeGarbageCollected<MediaElementMutationCallback>(this);
  }

  return HTMLDivElement::InsertedInto(root);
}

void MediaControlsImpl::UpdateCSSClassFromState() {
  // Skip CSS class updates when not needed in order to avoid triggering
  // unnecessary style calculation.
  if (!MediaElement().ShouldShowControls() && !is_hiding_controls_)
    return;

  const ControlsState state = State();

  Vector<String> toAdd;
  Vector<String> toRemove;

  if (state < kLoadingMetadataPaused)
    toAdd.push_back("phase-pre-ready");
  else
    toRemove.push_back("phase-pre-ready");

  if (state > kLoadingMetadataPlaying)
    toAdd.push_back("phase-ready");
  else
    toRemove.push_back("phase-ready");

  for (int i = 0; i < 8; i++) {
    if (i == state)
      toAdd.push_back(kStateCSSClasses[i]);
    else
      toRemove.push_back(kStateCSSClasses[i]);
  }

  if (MediaElement().ShouldShowControls() && ShouldShowVideoControls() &&
      !VideoElement().HasAvailableVideoFrame() &&
      VideoElement().PosterImageURL().IsEmpty() &&
      state <= ControlsState::kLoadingMetadataPlaying) {
    toAdd.push_back(kShowDefaultPosterCSSClass);
  } else {
    toRemove.push_back(kShowDefaultPosterCSSClass);
  }

  classList().add(toAdd, ASSERT_NO_EXCEPTION);
  classList().remove(toRemove, ASSERT_NO_EXCEPTION);

  if (loading_panel_)
    loading_panel_->UpdateDisplayState();

  // If we are in the "no-source" state we should show the overflow menu on a
  // video element.
  // TODO(https://crbug.org/930001): Reconsider skipping this block when not
  // connected.
  if (MediaElement().isConnected()) {
    bool updated = false;

    if (state == kNoSource) {
      // Check if the play button or overflow menu has the "disabled" attribute
      // set so we avoid unnecessarily resetting it.
      if (!play_button_->FastHasAttribute(html_names::kDisabledAttr)) {
        play_button_->setAttribute(html_names::kDisabledAttr, g_empty_atom);
        updated = true;
      }

      if (ShouldShowVideoControls() &&
          !overflow_menu_->FastHasAttribute(html_names::kDisabledAttr)) {
        overflow_menu_->setAttribute(html_names::kDisabledAttr, g_empty_atom);
        updated = true;
      }
    } else {
      if (play_button_->FastHasAttribute(html_names::kDisabledAttr)) {
        play_button_->removeAttribute(html_names::kDisabledAttr);
        updated = true;
      }

      if (overflow_menu_->FastHasAttribute(html_names::kDisabledAttr)) {
        overflow_menu_->removeAttribute(html_names::kDisabledAttr);
        updated = true;
      }
    }

    if (state == kNoSource || state == kNotLoaded) {
      if (!timeline_->FastHasAttribute(html_names::kDisabledAttr)) {
        timeline_->setAttribute(html_names::kDisabledAttr, g_empty_atom);
        updated = true;
      }
    } else {
      if (timeline_->FastHasAttribute(html_names::kDisabledAttr)) {
        timeline_->removeAttribute(html_names::kDisabledAttr);
        updated = true;
      }
    }

    if (updated)
      UpdateOverflowMenuWanted();
  }
}

void MediaControlsImpl::SetClass(const String& class_name,
                                 bool should_have_class) {
  AtomicString atomic_class = AtomicString(class_name);
  if (should_have_class && !classList().contains(atomic_class)) {
    classList().Add(atomic_class);
  } else if (!should_have_class && classList().contains(atomic_class)) {
    classList().Remove(atomic_class);
  }
}

MediaControlsImpl::ControlsState MediaControlsImpl::State() const {
  HTMLMediaElement::NetworkState network_state =
      MediaElement().getNetworkState();
  HTMLMediaElement::ReadyState ready_state = MediaElement().getReadyState();

  if (is_scrubbing_ && ready_state != HTMLMediaElement::kHaveNothing)
    return ControlsState::kScrubbing;

  switch (network_state) {
    case HTMLMediaElement::kNetworkEmpty:
    case HTMLMediaElement::kNetworkNoSource:
      return ControlsState::kNoSource;
    case HTMLMediaElement::kNetworkLoading:
      if (ready_state == HTMLMediaElement::kHaveNothing) {
        return MediaElement().paused() ? ControlsState::kLoadingMetadataPaused
                                       : ControlsState::kLoadingMetadataPlaying;
      }
      if (!MediaElement().paused() &&
          ready_state < HTMLMediaElement::kHaveFutureData) {
        return ControlsState::kBuffering;
      }
      break;
    case HTMLMediaElement::kNetworkIdle:
      if (ready_state == HTMLMediaElement::kHaveNothing)
        return ControlsState::kNotLoaded;
      break;
  }

  if (!MediaElement().paused())
    return ControlsState::kPlaying;
  return ControlsState::kStopped;
}

void MediaControlsImpl::RemovedFrom(ContainerNode& insertion_point) {
  DCHECK(!MediaElement().isConnected());

  HTMLDivElement::RemovedFrom(insertion_point);

  Hide();

  media_event_listener_->Detach();
  if (orientation_lock_delegate_)
    orientation_lock_delegate_->Detach();
  if (rotate_to_fullscreen_delegate_)
    rotate_to_fullscreen_delegate_->Detach();
  if (display_cutout_delegate_)
    display_cutout_delegate_->Detach();

  if (resize_observer_) {
    resize_observer_->disconnect();
    resize_observer_.Clear();
  }

  if (element_mutation_callback_) {
    element_mutation_callback_->Disconnect();
    element_mutation_callback_.Clear();
  }
}

void MediaControlsImpl::Reset() {
  EventDispatchForbiddenScope::AllowUserAgentEvents allow_events_in_shadow;
  BatchedControlUpdate batch(this);

  OnDurationChange();

  // Show everything that we might hide.
  current_time_display_->SetIsWanted(true);
  timeline_->SetIsWanted(true);

  // If the player has entered an error state, force it into the paused state.
  if (MediaElement().error())
    MediaElement().pause();

  UpdatePlayState();

  UpdateTimeIndicators();

  OnVolumeChange();
  OnTextTracksAddedOrRemoved();

  if (picture_in_picture_button_) {
    picture_in_picture_button_->SetIsWanted(
        ShouldShowPictureInPictureButton(MediaElement()));
  }

  UpdateCSSClassFromState();
  UpdateSizingCSSClass();
  OnControlsListUpdated();
}

void MediaControlsImpl::UpdateTimeIndicators(bool suppress_aria) {
  timeline_->SetPosition(MediaElement().currentTime(), suppress_aria);
  UpdateCurrentTimeDisplay();
}

void MediaControlsImpl::OnControlsListUpdated() {
  BatchedControlUpdate batch(this);

  if (ShouldShowVideoControls()) {
    fullscreen_button_->SetIsWanted(true);
    fullscreen_button_->setAttribute(
        html_names::kDisabledAttr,
        MediaControlsSharedHelpers::ShouldShowFullscreenButton(MediaElement())
            ? AtomicString()
            : AtomicString(""));
  } else {
    fullscreen_button_->SetIsWanted(
        MediaControlsSharedHelpers::ShouldShowFullscreenButton(MediaElement()));
    fullscreen_button_->removeAttribute(html_names::kDisabledAttr);
  }

  RefreshCastButtonVisibilityWithoutUpdate();

  download_button_->SetIsWanted(
      download_button_->ShouldDisplayDownloadButton());

  playback_speed_button_->SetIsWanted(
      ShouldShowPlaybackSpeedButton(MediaElement()));
}

LayoutObject* MediaControlsImpl::PanelLayoutObject() {
  return panel_->GetLayoutObject();
}

LayoutObject* MediaControlsImpl::TimelineLayoutObject() {
  return timeline_->GetLayoutObject();
}

LayoutObject* MediaControlsImpl::ButtonPanelLayoutObject() {
  return media_button_panel_->GetLayoutObject();
}

LayoutObject* MediaControlsImpl::ContainerLayoutObject() {
  return GetLayoutObject();
}

void MediaControlsImpl::SetTestMode(bool enable) {
  is_test_mode_ = enable;
  SetClass(kTestModeCSSClass, enable);
}

void MediaControlsImpl::MaybeShow() {
  panel_->SetIsWanted(true);
  panel_->SetIsDisplayed(true);

  UpdateCurrentTimeDisplay();

  if (overlay_play_button_ && !is_paused_for_scrubbing_)
    overlay_play_button_->UpdateDisplayType();
  // Only make the controls visible if they won't get hidden by OnTimeUpdate.
  if (MediaElement().paused() || !ShouldHideMediaControls())
    MakeOpaque();
  if (loading_panel_)
    loading_panel_->OnControlsShown();

  timeline_->OnControlsShown();
  volume_slider_->OnControlsShown();
  UpdateCSSClassFromState();
  UpdateActingAsAudioControls();
}

void MediaControlsImpl::Hide() {
  base::AutoReset<bool> auto_reset_hiding_controls(&is_hiding_controls_, true);

  panel_->SetIsWanted(false);
  panel_->SetIsDisplayed(false);

  // When we permanently hide the native media controls, we no longer want to
  // hide the cursor, since the video will be using custom controls.
  ShowCursor();

  if (overlay_play_button_)
    overlay_play_button_->SetIsWanted(false);
  if (loading_panel_)
    loading_panel_->OnControlsHidden();

  // Hide any popup menus.
  HidePopupMenu();

  // Cancel scrubbing if necessary.
  if (is_scrubbing_) {
    is_paused_for_scrubbing_ = false;
    EndScrubbing();
  }
  timeline_->OnControlsHidden();
  volume_slider_->OnControlsHidden();

  UpdateCSSClassFromState();

  // Hide is called when the HTMLMediaElement is removed from a document. If we
  // stop acting as audio controls during this removal, we end up inserting
  // nodes during the removal, firing a DCHECK. To avoid this, only update here
  // when the media element is connected.
  if (MediaElement().isConnected())
    UpdateActingAsAudioControls();
}

bool MediaControlsImpl::IsVisible() const {
  return panel_->IsOpaque();
}

void MediaControlsImpl::MaybeShowOverlayPlayButton() {
  if (overlay_play_button_)
    overlay_play_button_->SetIsDisplayed(true);
}

void MediaControlsImpl::MakeOpaque() {
  ShowCursor();
  panel_->MakeOpaque();
  MaybeShowOverlayPlayButton();
}

void MediaControlsImpl::MakeOpaqueFromPointerEvent() {
  // If we have quickly hidden the controls we should always show them when we
  // have a pointer event. If the controls are hidden the play button will
  // remain hidden.
  MaybeShowOverlayPlayButton();

  if (IsVisible())
    return;

  MakeOpaque();
}

void MediaControlsImpl::MakeTransparent() {
  // Only hide the cursor if the controls are enabled.
  if (MediaElement().ShouldShowControls())
    HideCursor();
  panel_->MakeTransparent();
}

bool MediaControlsImpl::ShouldHideMediaControls(unsigned behavior_flags) const {
  // Never hide for a media element without visual representation.
  auto* video_element = DynamicTo<HTMLVideoElement>(MediaElement());
  if (!video_element || !MediaElement().HasVideo() ||
      video_element->IsRemotingInterstitialVisible()) {
    return false;
  }

  if (RemotePlayback::From(MediaElement()).GetState() !=
      mojom::blink::PresentationConnectionState::CLOSED) {
    return false;
  }

  // Keep the controls visible as long as the timer is running.
  const bool ignore_wait_for_timer = behavior_flags & kIgnoreWaitForTimer;
  if (!ignore_wait_for_timer && keep_showing_until_timer_fires_)
    return false;

  // Don't hide if the mouse is over the controls.
  // Touch focus shouldn't affect controls visibility.
  const bool ignore_controls_hover = behavior_flags & kIgnoreControlsHover;
  if (!ignore_controls_hover && AreVideoControlsHovered() &&
      !is_touch_interaction_)
    return false;

  // Don't hide if the mouse is over the video area.
  const bool ignore_video_hover = behavior_flags & kIgnoreVideoHover;
  if (!ignore_video_hover && is_mouse_over_controls_)
    return false;

  // Don't hide if focus is on the HTMLMediaElement or within the
  // controls/shadow tree. (Perform the checks separately to avoid going
  // through all the potential ancestor hosts for the focused element.)
  const bool ignore_focus = behavior_flags & kIgnoreFocus;
  if (!ignore_focus && (MediaElement().IsFocused() ||
                        contains(GetDocument().FocusedElement()))) {
    return false;
  }

  // Don't hide the media controls when a panel is showing.
  if (text_track_list_->IsWanted() || playback_speed_list_->IsWanted() ||
      overflow_list_->IsWanted())
    return false;

  // Don't hide if we have accessiblity focus.
  if (panel_->KeepDisplayedForAccessibility())
    return false;

  if (MediaElement().seeking())
    return false;

  return true;
}

bool MediaControlsImpl::AreVideoControlsHovered() const {
  DCHECK(IsA<HTMLVideoElement>(MediaElement()));

  return media_button_panel_->IsHovered() || timeline_->IsHovered();
}

void MediaControlsImpl::UpdatePlayState() {
  if (is_paused_for_scrubbing_)
    return;

  if (overlay_play_button_)
    overlay_play_button_->UpdateDisplayType();
  play_button_->UpdateDisplayType();
}

HTMLDivElement* MediaControlsImpl::PanelElement() {
  return panel_;
}

HTMLDivElement* MediaControlsImpl::ButtonPanelElement() {
  return media_button_panel_;
}

void MediaControlsImpl::BeginScrubbing(bool is_touch_event) {
  if (!MediaElement().paused()) {
    is_paused_for_scrubbing_ = true;
    MediaElement().pause();
  }

  if (scrubbing_message_ && is_touch_event) {
    scrubbing_message_->SetIsWanted(true);
    if (scrubbing_message_->DoesFit()) {
      panel_->setAttribute(html_names::kClassAttr,
                           AtomicString(kScrubbingMessageCSSClass));
    }
  }

  is_scrubbing_ = true;
  UpdateCSSClassFromState();
}

void MediaControlsImpl::EndScrubbing() {
  if (is_paused_for_scrubbing_) {
    is_paused_for_scrubbing_ = false;
    if (MediaElement().paused())
      MediaElement().Play();
  }

  if (scrubbing_message_) {
    scrubbing_message_->SetIsWanted(false);
    panel_->removeAttribute(html_names::kClassAttr);
  }

  is_scrubbing_ = false;
  UpdateCSSClassFromState();
}

void MediaControlsImpl::UpdateCurrentTimeDisplay() {
  timeline_->SetIsWanted(!IsLivePlayback());
  if (panel_->IsWanted()) {
    current_time_display_->SetCurrentValue(MediaElement().currentTime());
  }
}

void MediaControlsImpl::ToggleTextTrackList() {
  if (!MediaElement().HasClosedCaptions()) {
    text_track_list_->SetIsWanted(false);
    return;
  }

  text_track_list_->SetIsWanted(!text_track_list_->IsWanted());
}

bool MediaControlsImpl::TextTrackListIsWanted() {
  return text_track_list_->IsWanted();
}

void MediaControlsImpl::TogglePlaybackSpeedList() {
  playback_speed_list_->SetIsWanted(!playback_speed_list_->IsWanted());
}

bool MediaControlsImpl::PlaybackSpeedListIsWanted() {
  return playback_speed_list_->IsWanted();
}

MediaControlsTextTrackManager& MediaControlsImpl::GetTextTrackManager() {
  return *text_track_manager_;
}

void MediaControlsImpl::RefreshCastButtonVisibility() {
  RefreshCastButtonVisibilityWithoutUpdate();
  BatchedControlUpdate batch(this);
}

void MediaControlsImpl::RefreshCastButtonVisibilityWithoutUpdate() {
  if (!ShouldShowCastButton(MediaElement())) {
    cast_button_->SetIsWanted(false);
    overlay_cast_button_->SetIsWanted(false);
    return;
  }

  cast_button_->SetIsWanted(MediaElement().ShouldShowControls());

  // On sites with muted autoplaying videos as background, it's unlikely that
  // users want to cast such content and showing a Cast overlay button is
  // distracting.  If a user does want to cast a muted autoplay video then they
  // can still do so by touching or clicking on the video, which will cause the
  // cast button to appear.
  if (!MediaElement().GetAutoplayPolicy().IsOrWillBeAutoplayingMuted() &&
      ShouldShowCastOverlayButton(MediaElement())) {
    // Note that this is a case where we add the overlay cast button
    // without wanting the panel cast button.  We depend on the fact
    // that computeWhichControlsFit() won't change overlay cast button
    // visibility in the case where the cast button isn't wanted.
    // We don't call compute...() here, but it will be called as
    // non-cast changes (e.g., resize) occur.  If the panel button
    // is shown, however, compute...() will take control of the
    // overlay cast button if it needs to hide it from the panel.
      overlay_cast_button_->TryShowOverlay();
  } else {
    overlay_cast_button_->SetIsWanted(false);
  }
}

void MediaControlsImpl::ShowOverlayCastButtonIfNeeded() {
  if (!ShouldShowCastOverlayButton(MediaElement())) {
    overlay_cast_button_->SetIsWanted(false);
    return;
  }

  overlay_cast_button_->TryShowOverlay();
  ResetHideMediaControlsTimer();
}

void MediaControlsImpl::EnterFullscreen() {
  Fullscreen::RequestFullscreen(MediaElement());
}

void MediaControlsImpl::ExitFullscreen() {
  Fullscreen::ExitFullscreen(GetDocument());
}

bool MediaControlsImpl::IsFullscreenEnabled() const {
  return fullscreen_button_->IsWanted() &&
         !fullscreen_button_->FastHasAttribute(html_names::kDisabledAttr);
}

void MediaControlsImpl::RemotePlaybackStateChanged() {
  cast_button_->UpdateDisplayType();
  overlay_cast_button_->UpdateDisplayType();
}

void MediaControlsImpl::UpdateOverflowMenuWanted() const {
  // If the bool is true then the element is "sticky" this means that we will
  // always try and show it unless there is not room for it.
  std::pair<MediaControlElementBase*, bool> row_elements[] = {
      std::make_pair(play_button_.Get(), true),
      std::make_pair(mute_button_.Get(), true),
      std::make_pair(fullscreen_button_.Get(), true),
      std::make_pair(current_time_display_.Get(), true),
      std::make_pair(duration_display_.Get(), true),
      std::make_pair(picture_in_picture_button_.Get(), false),
      std::make_pair(cast_button_.Get(), false),
      std::make_pair(download_button_.Get(), false),
      std::make_pair(toggle_closed_captions_button_.Get(), false),
      std::make_pair(playback_speed_button_.Get(), false),
  };

  // These are the elements in order of priority that take up vertical room.
  MediaControlElementBase* column_elements[] = {
      media_button_panel_.Get(), timeline_.Get(),
  };

  // Current size of the media controls.
  gfx::Size controls_size = size_;

  // The video controls are more than one row so we need to allocate vertical
  // room and hide the overlay play button if there is not enough room.
  if (ShouldShowVideoControls()) {
    // Allocate vertical room for overlay play button if necessary.
    if (overlay_play_button_) {
      gfx::Size overlay_play_button_size =
          overlay_play_button_->GetSizeOrDefault();
      if (controls_size.height() >= overlay_play_button_size.height() &&
          controls_size.width() >= kMinWidthForOverlayPlayButton) {
        overlay_play_button_->SetDoesFit(true);
        controls_size.Enlarge(0, -overlay_play_button_size.height());
      } else {
        overlay_play_button_->SetDoesFit(false);
      }
    }

    controls_size.Enlarge(-kVideoButtonPadding, 0);

    // Allocate vertical room for the column elements.
    for (MediaControlElementBase* element : column_elements) {
      gfx::Size element_size = element->GetSizeOrDefault();
      if (controls_size.height() - element_size.height() >= 0) {
        element->SetDoesFit(true);
        controls_size.Enlarge(0, -element_size.height());
      } else {
        element->SetDoesFit(false);
      }
    }

    // If we cannot show the overlay play button, show the normal one.
    play_button_->SetIsWanted(!overlay_play_button_ ||
                              !overlay_play_button_->DoesFit());
  } else {
    controls_size.Enlarge(-kAudioButtonPadding, 0);

    // Undo any IsWanted/DoesFit changes made in the above block if we're
    // switching to act as audio controls.
    if (is_acting_as_audio_controls_) {
      play_button_->SetIsWanted(true);

      for (MediaControlElementBase* element : column_elements)
        element->SetDoesFit(true);
    }
  }

  // Go through the elements and if they are sticky allocate them to the panel
  // if we have enough room. If not (or they are not sticky) then add them to
  // the overflow menu. Once we have run out of room add_elements will be
  // made false and no more elements will be added.
  MediaControlElementBase* last_element = nullptr;
  bool add_elements = true;
  bool overflow_wanted = false;
  for (std::pair<MediaControlElementBase*, bool> pair : row_elements) {
    MediaControlElementBase* element = pair.first;
    if (!element)
      continue;

    // If the element is wanted then it should take up space, otherwise skip it.
    element->SetOverflowElementIsWanted(false);
    if (!element->IsWanted())
      continue;

    // Get the size of the element and see if we should allocate space to it.
    gfx::Size element_size = element->GetSizeOrDefault();
    bool does_fit = add_elements && pair.second &&
                    ((controls_size.width() - element_size.width()) >= 0);
    element->SetDoesFit(does_fit);

    if (element == mute_button_.Get())
      volume_control_container_->SetIsWanted(does_fit);

    // The element does fit and is sticky so we should allocate space for it. If
    // we cannot fit this element we should stop allocating space for other
    // elements.
    if (does_fit) {
      controls_size.Enlarge(-element_size.width(), 0);
      last_element = element;
    } else {
      add_elements = false;
      if (element->HasOverflowButton() && !element->IsDisabled()) {
        overflow_wanted = true;
        element->SetOverflowElementIsWanted(true);
      }
    }
  }

  // The overflow menu is always wanted if it has the "disabled" attr set.
  overflow_wanted = overflow_wanted ||
                    overflow_menu_->FastHasAttribute(html_names::kDisabledAttr);
  overflow_menu_->SetDoesFit(overflow_wanted);
  overflow_menu_->SetIsWanted(overflow_wanted);

  // If we want to show the overflow button and we do not have any space to show
  // it then we should hide the last shown element.
  int overflow_icon_width = overflow_menu_->GetSizeOrDefault().width();
  if (overflow_wanted && last_element &&
      controls_size.width() < overflow_icon_width) {
    last_element->SetDoesFit(false);
    last_element->SetOverflowElementIsWanted(true);

    if (last_element == mute_button_.Get())
      volume_control_container_->SetIsWanted(false);
  }

  MaybeRecordElementsDisplayed();

  UpdateOverflowMenuItemCSSClass();
}

// This method is responsible for adding css class to overflow menu list
// items to achieve the animation that items appears one after another when
// open the overflow menu.
void MediaControlsImpl::UpdateOverflowMenuItemCSSClass() const {
  unsigned int id = 0;
  for (Element* item = ElementTraversal::LastChild(*overflow_list_); item;
       item = ElementTraversal::PreviousSibling(*item)) {
    const CSSPropertyValueSet* inline_style = item->InlineStyle();
    DOMTokenList& class_list = item->classList();

    // We don't care if the hidden element still have animated-* CSS class
    if (inline_style &&
        inline_style->GetPropertyValue(CSSPropertyID::kDisplay) == "none")
      continue;

    AtomicString css_class =
        AtomicString("animated-") + AtomicString::Number(id++);
    if (!class_list.contains(css_class))
      class_list.setValue(css_class);
  }
}

void MediaControlsImpl::UpdateScrubbingMessageFits() const {
  if (scrubbing_message_)
    scrubbing_message_->SetDoesFit(size_.width() >= kMinScrubbingMessageWidth);
}

void MediaControlsImpl::UpdateSizingCSSClass() {
  MediaControlsSizingClass sizing_class =
      MediaControls::GetSizingClass(size_.width());

  SetClass(kMediaControlsSizingSmallCSSClass,
           ShouldShowVideoControls() &&
               (sizing_class == MediaControlsSizingClass::kSmall ||
                sizing_class == MediaControlsSizingClass::kMedium));
  SetClass(kMediaControlsSizingLargeCSSClass,
           ShouldShowVideoControls() &&
               sizing_class == MediaControlsSizingClass::kLarge);
}

void MediaControlsImpl::MaybeToggleControlsFromTap() {
  if (MediaElement().paused())
    return;

  // If the controls are visible then hide them. If the controls are not visible
  // then show them and start the timer to automatically hide them.
  if (IsVisible()) {
    MakeTransparent();
  } else {
    MakeOpaque();
    // Touch focus shouldn't affect controls visibility.
    if (ShouldHideMediaControls(kIgnoreWaitForTimer | kIgnoreFocus)) {
      keep_showing_until_timer_fires_ = true;
      StartHideMediaControlsTimer();
    }
  }
}

void MediaControlsImpl::OnAccessibleFocus() {
  if (panel_->KeepDisplayedForAccessibility())
    return;

  panel_->SetKeepDisplayedForAccessibility(true);

  if (!MediaElement().ShouldShowControls())
    return;

  OpenVolumeSliderIfNecessary();

  keep_showing_until_timer_fires_ = true;
  StartHideMediaControlsTimer();
  MaybeShow();
}

void MediaControlsImpl::OnAccessibleBlur() {
  panel_->SetKeepDisplayedForAccessibility(false);

  if (MediaElement().ShouldShowControls())
    return;

  CloseVolumeSliderIfNecessary();

  keep_showing_until_timer_fires_ = false;
  ResetHideMediaControlsTimer();
}

void MediaControlsImpl::DefaultEventHandler(Event& event) {
  HTMLDivElement::DefaultEventHandler(event);

  // Do not handle events to not interfere with the rest of the page if no
  // controls should be visible.
  if (!MediaElement().ShouldShowControls())
    return;

  // Add IgnoreControlsHover to m_hideTimerBehaviorFlags when we see a touch
  // event, to allow the hide-timer to do the right thing when it fires.
  // FIXME: Preferably we would only do this when we're actually handling the
  // event here ourselves.
  bool is_touch_event = IsTouchEvent(&event);
  hide_timer_behavior_flags_ |=
      is_touch_event ? kIgnoreControlsHover : kIgnoreNone;

  // Touch events are treated differently to avoid fake mouse events to trigger
  // random behavior. The expect behaviour for touch is that a tap will show the
  // controls and they will hide when the timer to hide fires.
  if (is_touch_event)
    HandleTouchEvent(&event);

  if (event.type() == event_type_names::kMouseover && !is_touch_event)
    is_touch_interaction_ = false;

  if ((event.type() == event_type_names::kPointerover ||
       event.type() == event_type_names::kPointermove ||
       event.type() == event_type_names::kPointerout) &&
      !is_touch_interaction_) {
    HandlePointerEvent(&event);
  }

  if (event.type() == event_type_names::kClick && !is_touch_interaction_)
    HandleClickEvent(&event);

  // If the user is interacting with the controls via the keyboard, don't hide
  // the controls. This will fire when the user tabs between controls (focusin)
  // or when they seek either the timeline or volume sliders (input).
  if (event.type() == event_type_names::kFocusin ||
      event.type() == event_type_names::kInput) {
    ResetHideMediaControlsTimer();
  }

  auto* keyboard_event = DynamicTo<KeyboardEvent>(event);
  if (keyboard_event && !event.defaultPrevented() &&
      !IsSpatialNavigationEnabled(GetDocument().GetFrame())) {
    const AtomicString key(keyboard_event->key());
    if (key == keywords::kCapitalEnter || keyboard_event->keyCode() == ' ') {
      if (overlay_play_button_) {
        overlay_play_button_->OnMediaKeyboardEvent(&event);
      } else {
        play_button_->OnMediaKeyboardEvent(&event);
      }
      return;
    }
    if (key == keywords::kArrowLeft || key == keywords::kArrowRight ||
        key == keywords::kHome || key == keywords::kEnd) {
      timeline_->OnMediaKeyboardEvent(&event);
      return;
    }
    if (volume_slider_ &&
        (key == keywords::kArrowDown || key == keywords::kArrowUp)) {
      for (int i = 0; i < 5; i++)
        volume_slider_->OnMediaKeyboardEvent(&event);
      return;
    }
  }
}

void MediaControlsImpl::HandlePointerEvent(Event* event) {
  if (event->type() == event_type_names::kPointerover) {
    if (!ContainsRelatedTarget(event)) {
      is_mouse_over_controls_ = true;
      if (!MediaElement().paused()) {
        MakeOpaqueFromPointerEvent();
        StartHideMediaControlsIfNecessary();
      }
    }
  } else if (event->type() == event_type_names::kPointerout) {
    if (!ContainsRelatedTarget(event)) {
      is_mouse_over_controls_ = false;
      StopHideMediaControlsTimer();

      // When we get a mouse out, if video is playing and control should
      // hide regardless of focus, hide the control.
      // This will fix the issue that when mouse out event happen while video is
      // focused, control never hides.
      if (!MediaElement().paused() && ShouldHideMediaControls(kIgnoreFocus))
        MakeTransparent();
    }
  } else if (event->type() == event_type_names::kPointermove) {
    // When we get a mouse move, show the media controls, and start a timer
    // that will hide the media controls after a 3 seconds without a mouse move.
    is_mouse_over_controls_ = true;
    MakeOpaqueFromPointerEvent();

    // Start the timer regardless of focus state
    if (ShouldHideMediaControls(kIgnoreVideoHover | kIgnoreFocus))
      StartHideMediaControlsTimer();
  }
}

void MediaControlsImpl::HandleClickEvent(Event* event) {
  if (ContainsRelatedTarget(event) || !IsFullscreenEnabled())
    return;

  if (tap_timer_.IsActive()) {
    tap_timer_.Stop();

    // Toggle fullscreen.
    if (MediaElement().IsFullscreen())
      ExitFullscreen();
    else
      EnterFullscreen();

    // If we paused for the first click of this double-click, then we need to
    // resume playback, since the user was just toggling fullscreen.
    if (is_paused_for_double_tap_) {
      MediaElement().Play();
      is_paused_for_double_tap_ = false;
    }
  } else {
    // If the video is not paused, assume the user is clicking to pause the
    // video. If the user clicks again for a fullscreen-toggling double-tap, we
    // will resume playback.
    if (!MediaElement().paused()) {
      MediaElement().pause();
      is_paused_for_double_tap_ = true;
    }
    tap_timer_.StartOneShot(kDoubleTapDelay, FROM_HERE);
  }
}

void MediaControlsImpl::HandleTouchEvent(Event* event) {
  is_mouse_over_controls_ = false;
  is_touch_interaction_ = true;

  if (event->type() == event_type_names::kGesturetap &&
      !ContainsRelatedTarget(event)) {
    event->SetDefaultHandled();

    if (tap_timer_.IsActive()) {
      // Cancel the visibility toggle event.
      tap_timer_.Stop();

      if (IsOnLeftSide(event)) {
        MaybeJump(kNumberOfSecondsToJump * -1);
      } else {
        MaybeJump(kNumberOfSecondsToJump);
      }
    } else {
      tap_timer_.StartOneShot(kDoubleTapDelay, FROM_HERE);
    }
  }
}

void MediaControlsImpl::EnsureAnimatedArrowContainer() {
  if (!animated_arrow_container_element_) {
    animated_arrow_container_element_ =
        MakeGarbageCollected<MediaControlAnimatedArrowContainerElement>(*this);
    ParserAppendChild(animated_arrow_container_element_);
  }
}

void MediaControlsImpl::MaybeJump(int seconds) {
  // Update the current time.
  double new_time = std::max(0.0, MediaElement().currentTime() + seconds);
  new_time = std::min(new_time, MediaElement().duration());
  MediaElement().setCurrentTime(new_time);

  // Show the arrow animation.
  EnsureAnimatedArrowContainer();
  MediaControlAnimatedArrowContainerElement::ArrowDirection direction =
      (seconds > 0)
          ? MediaControlAnimatedArrowContainerElement::ArrowDirection::kRight
          : MediaControlAnimatedArrowContainerElement::ArrowDirection::kLeft;
  animated_arrow_container_element_->ShowArrowAnimation(direction);
}

bool MediaControlsImpl::IsOnLeftSide(Event* event) {
  auto* gesture_event = DynamicTo<GestureEvent>(event);
  if (!gesture_event)
    return false;

  float tap_x = gesture_event->NativeEvent().PositionInWidget().x();

  DOMRect* rect = GetBoundingClientRect();
  double middle = rect->x() + (rect->width() / 2);
  if (GetDocument().GetFrame())
    middle *= GetDocument().GetFrame()->LayoutZoomFactor();

  return tap_x < middle;
}

void MediaControlsImpl::TapTimerFired(TimerBase*) {
  if (is_touch_interaction_) {
    MaybeToggleControlsFromTap();
  } else if (MediaElement().paused()) {
    // If this is not a touch interaction and the video is paused, then either
    // the user has just paused via click (in which case we've already paused
    // and there's nothing to do), or the user is playing by click (in which
    // case we need to start playing).
    if (is_paused_for_double_tap_) {
      Platform::Current()->RecordAction(
          UserMetricsAction("Media.Controls.ClickAnywhereToPause"));
      // TODO(https://crbug.com/896252): Show overlay pause animation.
      is_paused_for_double_tap_ = false;
    } else {
      Platform::Current()->RecordAction(
          UserMetricsAction("Media.Controls.ClickAnywhereToPlay"));
      // TODO(https://crbug.com/896252): Show overlay play animation.
      MediaElement().Play();
    }
  }
}

void MediaControlsImpl::HideMediaControlsTimerFired(TimerBase*) {
  unsigned behavior_flags =
      hide_timer_behavior_flags_ | kIgnoreFocus | kIgnoreVideoHover;
  hide_timer_behavior_flags_ = kIgnoreNone;
  keep_showing_until_timer_fires_ = false;

  if (MediaElement().paused())
    return;

  if (!ShouldHideMediaControls(behavior_flags))
    return;

  MakeTransparent();
  overlay_cast_button_->SetIsWanted(false);
}

void MediaControlsImpl::StartHideMediaControlsIfNecessary() {
  if (ShouldHideMediaControls())
    StartHideMediaControlsTimer();
}

void MediaControlsImpl::StartHideMediaControlsTimer() {
  hide_media_controls_timer_.StartOneShot(
      GetTimeWithoutMouseMovementBeforeHidingMediaControls(), FROM_HERE);
}

void MediaControlsImpl::StopHideMediaControlsTimer() {
  keep_showing_until_timer_fires_ = false;
  hide_media_controls_timer_.Stop();
}

void MediaControlsImpl::ResetHideMediaControlsTimer() {
  StopHideMediaControlsTimer();
  if (!MediaElement().paused())
    StartHideMediaControlsTimer();
}

void MediaControlsImpl::HideCursor() {
  SetInlineStyleProperty(CSSPropertyID::kCursor, "none", false);
}

void MediaControlsImpl::ShowCursor() {
  RemoveInlineStyleProperty(CSSPropertyID::kCursor);
}

bool MediaControlsImpl::ContainsRelatedTarget(Event* event) {
  auto* pointer_event = DynamicTo<PointerEvent>(event);
  if (!pointer_event)
    return false;
  EventTarget* related_target = pointer_event->relatedTarget();
  if (!related_target)
    return false;
  return contains(related_target->ToNode());
}

void MediaControlsImpl::OnVolumeChange() {
  mute_button_->UpdateDisplayType();

  // Update visibility of volume controls.
  // TODO(mlamouri): it should not be part of the volumechange handling because
  // it is using audio availability as input.
  if (volume_slider_) {
    volume_slider_->SetVolume(MediaElement().muted() ? 0
                                                     : MediaElement().volume());
    volume_slider_->SetIsWanted(MediaElement().HasAudio() &&
                                !PreferHiddenVolumeControls(GetDocument()));
  }

  mute_button_->SetIsWanted(true);
  mute_button_->setAttribute(
      html_names::kDisabledAttr,
      MediaElement().HasAudio() ? AtomicString() : AtomicString(""));

  // If the volume slider is being used we don't want to update controls
  // visibility, since this can shift the position of the volume slider and make
  // it unusable.
  if (!volume_slider_ || !volume_slider_->IsHovered())
    BatchedControlUpdate batch(this);
}

void MediaControlsImpl::OnFocusIn() {
  // If the tap timer is active, then we will toggle the controls when the timer
  // completes, so we don't want to start showing here.
  if (!MediaElement().ShouldShowControls() || tap_timer_.IsActive())
    return;

  ResetHideMediaControlsTimer();
  MaybeShow();
}

void MediaControlsImpl::OnTimeUpdate() {
  UpdateTimeIndicators(true /* suppress_aria */);

  // 'timeupdate' might be called in a paused state. The controls should not
  // become transparent in that case.
  if (MediaElement().paused()) {
    MakeOpaque();
    return;
  }

  if (IsVisible() && ShouldHideMediaControls())
    MakeTransparent();
}

void MediaControlsImpl::OnDurationChange() {
  BatchedControlUpdate batch(this);

  const double duration = MediaElement().duration();
  bool was_finite_duration = std::isfinite(duration_display_->CurrentValue());

  // Update the displayed current time/duration.
  duration_display_->SetCurrentValue(duration);

  // Show the duration display if we have a duration or if we are showing the
  // audio controls without a source.
  duration_display_->SetIsWanted(
      std::isfinite(duration) ||
      (ShouldShowAudioControls() && State() == kNoSource));

  // TODO(crbug.com/756698): Determine if this is still needed since the format
  // of the current time no longer depends on the duration.
  UpdateCurrentTimeDisplay();

  // Update the timeline (the UI with the seek marker).
  timeline_->SetDuration(duration);
  if (!was_finite_duration && std::isfinite(duration)) {
    download_button_->SetIsWanted(
        download_button_->ShouldDisplayDownloadButton());
  }
}

void MediaControlsImpl::OnPlay() {
  UpdatePlayState();
  UpdateTimeIndicators();
  UpdateCSSClassFromState();
}

void MediaControlsImpl::OnPlaying() {
  StartHideMediaControlsTimer();
  UpdateCSSClassFromState();
  timeline_->OnMediaPlaying();
}

void MediaControlsImpl::OnPause() {
  UpdatePlayState();
  UpdateTimeIndicators();
  timeline_->OnMediaStoppedPlaying();
  MakeOpaque();

  StopHideMediaControlsTimer();

  UpdateCSSClassFromState();
}

void MediaControlsImpl::OnSeeking() {
  UpdateTimeIndicators();
  if (!is_scrubbing_) {
    is_scrubbing_ = true;
    UpdateCSSClassFromState();
  }

  // Don't try to show the controls if the seek was caused by the video being
  // looped.
  if (MediaElement().Loop() && MediaElement().currentTime() == 0)
    return;

  if (!MediaElement().ShouldShowControls())
    return;

  MaybeShow();
  StopHideMediaControlsTimer();
}

void MediaControlsImpl::OnSeeked() {
  StartHideMediaControlsIfNecessary();

  is_scrubbing_ = false;
  UpdateCSSClassFromState();
}

void MediaControlsImpl::OnTextTracksAddedOrRemoved() {
  toggle_closed_captions_button_->UpdateDisplayType();
  toggle_closed_captions_button_->SetIsWanted(
      MediaElement().HasClosedCaptions());
  BatchedControlUpdate batch(this);
}

void MediaControlsImpl::OnTextTracksChanged() {
  toggle_closed_captions_button_->UpdateDisplayType();
}

void MediaControlsImpl::OnError() {
  // TODO(mlamouri): we should only change the aspects of the control that need
  // to be changed.
  Reset();
  UpdateCSSClassFromState();
}

void MediaControlsImpl::OnLoadedMetadata() {
  // TODO(mlamouri): we should only change the aspects of the control that need
  // to be changed.
  Reset();
  UpdateCSSClassFromState();
  UpdateActingAsAudioControls();
}

void MediaControlsImpl::OnEnteredFullscreen() {
  fullscreen_button_->SetIsFullscreen(true);
  if (display_cutout_fullscreen_button_)
    display_cutout_fullscreen_button_->SetIsWanted(true);

  StopHideMediaControlsTimer();
  StartHideMediaControlsTimer();
}

void MediaControlsImpl::OnExitedFullscreen() {
  fullscreen_button_->SetIsFullscreen(false);
  if (display_cutout_fullscreen_button_)
    display_cutout_fullscreen_button_->SetIsWanted(false);

  HidePopupMenu();
  StopHideMediaControlsTimer();
  StartHideMediaControlsTimer();
}

void MediaControlsImpl::OnPictureInPictureChanged() {
  // This will only be called if the media controls are listening to the
  // Picture-in-Picture events which only happen when they provide a
  // Picture-in-Picture button.
  DCHECK(picture_in_picture_button_);
  picture_in_picture_button_->UpdateDisplayType();
}

void MediaControlsImpl::OnPanelKeypress() {
  // If the user is interacting with the controls via the keyboard, don't hide
  // the controls. This is called when the user mutes/unmutes, turns CC on/off,
  // etc.
  ResetHideMediaControlsTimer();
}

void MediaControlsImpl::NotifyElementSizeChanged(DOMRectReadOnly* new_size) {
  // Note that this code permits a bad frame on resize, since it is
  // run after the relayout / paint happens.  It would be great to improve
  // this, but it would be even greater to move this code entirely to
  // JS and fix it there.

  gfx::Size old_size = size_;
  size_.set_width(new_size->width());
  size_.set_height(new_size->height());

  // Don't bother to do any work if this matches the most recent size.
  if (old_size != size_) {
    // Update the sizing CSS class before computing which controls fit so that
    // the element sizes can update from the CSS class change before we start
    // calculating.
    UpdateSizingCSSClass();
    element_size_changed_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);
  }
}

void MediaControlsImpl::ElementSizeChangedTimerFired(TimerBase*) {
  if (!MediaElement().isConnected())
    return;

  ComputeWhichControlsFit();

  // Rerender timeline bar segments when size changed.
  timeline_->RenderBarSegments();
}

void MediaControlsImpl::OnLoadingProgress() {
  timeline_->OnProgress();
}

void MediaControlsImpl::ComputeWhichControlsFit() {
  // Hide all controls that don't fit, and show the ones that do.
  // This might be better suited for a layout, but since JS media controls
  // won't benefit from that anwyay, we just do it here like JS will.
  UpdateOverflowMenuWanted();
  UpdateScrubbingMessageFits();
}

void MediaControlsImpl::MaybeRecordElementsDisplayed() const {
  // Record the display state when needed. It is only recorded when the media
  // element is in a state that allows it in order to reduce noise in the
  // metrics.
  if (!MediaControlInputElement::ShouldRecordDisplayStates(MediaElement()))
    return;

  MediaControlElementBase* elements[] = {
      play_button_.Get(),
      fullscreen_button_.Get(),
      download_button_.Get(),
      timeline_.Get(),
      mute_button_.Get(),
      volume_slider_.Get(),
      toggle_closed_captions_button_.Get(),
      playback_speed_button_.Get(),
      picture_in_picture_button_.Get(),
      cast_button_.Get(),
      current_time_display_.Get(),
      duration_display_.Get(),
      overlay_play_button_.Get(),
      overlay_cast_button_.Get(),
  };

  // Record which controls are used.
  for (auto* const element : elements) {
    if (element)
      element->MaybeRecordDisplayed();
  }
  overflow_menu_->MaybeRecordDisplayed();
}

const MediaControlCurrentTimeDisplayElement&
MediaControlsImpl::CurrentTimeDisplay() const {
  return *current_time_display_;
}

const MediaControlRemainingTimeDisplayElement&
MediaControlsImpl::RemainingTimeDisplay() const {
  return *duration_display_;
}

MediaControlToggleClosedCaptionsButtonElement&
MediaControlsImpl::ToggleClosedCaptions() {
  return *toggle_closed_captions_button_;
}

bool MediaControlsImpl::ShouldActAsAudioControls() const {
  // A video element should act like an audio element when it has an audio track
  // but no video track.
  return MediaElement().ShouldShowControls() &&
         IsA<HTMLVideoElement>(MediaElement()) && MediaElement().HasAudio() &&
         !MediaElement().HasVideo();
}

void MediaControlsImpl::StartActingAsAudioControls() {
  DCHECK(ShouldActAsAudioControls());
  DCHECK(!is_acting_as_audio_controls_);

  is_acting_as_audio_controls_ = true;
  SetClass(kActAsAudioControlsCSSClass, true);
  PopulatePanel();
  Reset();
}

void MediaControlsImpl::StopActingAsAudioControls() {
  DCHECK(!ShouldActAsAudioControls());
  DCHECK(is_acting_as_audio_controls_);

  is_acting_as_audio_controls_ = false;
  SetClass(kActAsAudioControlsCSSClass, false);
  PopulatePanel();
  Reset();
}

void MediaControlsImpl::UpdateActingAsAudioControls() {
  if (ShouldActAsAudioControls() != is_acting_as_audio_controls_) {
    if (is_acting_as_audio_controls_)
      StopActingAsAudioControls();
    else
      StartActingAsAudioControls();
  }
}

bool MediaControlsImpl::ShouldShowAudioControls() const {
  return IsA<HTMLAudioElement>(MediaElement()) || is_acting_as_audio_controls_;
}

bool MediaControlsImpl::ShouldShowVideoControls() const {
  return IsA<HTMLVideoElement>(MediaElement()) && !ShouldShowAudioControls();
}

bool MediaControlsImpl::IsLivePlayback() const {
  // It can't be determined whether a player with no source element is a live
  // playback or not, similarly with an unloaded player.
  return MediaElement().seekable()->length() == 0 && (State() >= kStopped);
}

void MediaControlsImpl::NetworkStateChanged() {
  // Update the display state of the download button in case we now have a
  // source or no longer have a source.
  download_button_->SetIsWanted(
      download_button_->ShouldDisplayDownloadButton());

  UpdateCSSClassFromState();
}

void MediaControlsImpl::OpenOverflowMenu() {
  overflow_list_->OpenOverflowMenu();
}

void MediaControlsImpl::CloseOverflowMenu() {
  overflow_list_->CloseOverflowMenu();
}

bool MediaControlsImpl::OverflowMenuIsWanted() {
  return overflow_list_->IsWanted();
}

bool MediaControlsImpl::OverflowMenuVisible() {
  return overflow_list_ ? overflow_list_->IsWanted() : false;
}

void MediaControlsImpl::ToggleOverflowMenu() {
  DCHECK(overflow_list_);

  overflow_list_->SetIsWanted(!overflow_list_->IsWanted());
}

void MediaControlsImpl::HidePopupMenu() {
  if (OverflowMenuVisible())
    ToggleOverflowMenu();

  if (TextTrackListIsWanted())
    ToggleTextTrackList();

  if (PlaybackSpeedListIsWanted())
    TogglePlaybackSpeedList();
}

void MediaControlsImpl::VolumeSliderWantedTimerFired(TimerBase*) {
  volume_slider_->OpenSlider();
  volume_control_container_->OpenContainer();
}

void MediaControlsImpl::OpenVolumeSliderIfNecessary() {
  if (ShouldOpenVolumeSlider()) {
    if (volume_slider_->IsFocused() || mute_button_->IsFocused()) {
      // When we're focusing with the keyboard, we don't need the delay.
      volume_slider_->OpenSlider();
      volume_control_container_->OpenContainer();
    } else {
      volume_slider_wanted_timer_.StartOneShot(
          WebTestSupport::IsRunningWebTest() ? kTimeToShowVolumeSliderTest
                                             : kTimeToShowVolumeSlider,
          FROM_HERE);
    }
  }
}

void MediaControlsImpl::CloseVolumeSliderIfNecessary() {
  if (ShouldCloseVolumeSlider()) {
    volume_slider_->CloseSlider();
    volume_control_container_->CloseContainer();

    if (volume_slider_wanted_timer_.IsActive())
      volume_slider_wanted_timer_.Stop();
  }
}

bool MediaControlsImpl::ShouldOpenVolumeSlider() const {
  if (!volume_slider_) {
    return false;
  }

  if (!MediaElement().HasAudio()) {
    return false;
  }

  return !PreferHiddenVolumeControls(GetDocument());
}

bool MediaControlsImpl::ShouldCloseVolumeSlider() const {
  if (!volume_slider_)
    return false;

  return !(volume_control_container_->IsHovered() ||
           volume_slider_->IsFocused() || mute_button_->IsFocused());
}

const MediaControlOverflowMenuButtonElement& MediaControlsImpl::OverflowButton()
    const {
  return *overflow_menu_;
}

MediaControlOverflowMenuButtonElement& MediaControlsImpl::OverflowButton() {
  return *overflow_menu_;
}

void MediaControlsImpl::OnWaiting() {
  timeline_->OnMediaStoppedPlaying();
  UpdateCSSClassFromState();
}

void MediaControlsImpl::OnLoadedData() {
  UpdateCSSClassFromState();
}

HTMLVideoElement& MediaControlsImpl::VideoElement() {
  return *To<HTMLVideoElement>(&MediaElement());
}

void MediaControlsImpl::Trace(Visitor* visitor) const {
  visitor->Trace(element_mutation_callback_);
  visitor->Trace(element_size_changed_timer_);
  visitor->Trace(tap_timer_);
  visitor->Trace(volume_slider_wanted_timer_);
  visitor->Trace(resize_observer_);
  visitor->Trace(panel_);
  visitor->Trace(overlay_play_button_);
  visitor->Trace(overlay_enclosure_);
  visitor->Trace(play_button_);
  visitor->Trace(current_time_display_);
  visitor->Trace(timeline_);
  visitor->Trace(scrubbing_message_);
  visitor->Trace(mute_button_);
  visitor->Trace(volume_slider_);
  visitor->Trace(picture_in_picture_button_);
  visitor->Trace(animated_arrow_container_element_);
  visitor->Trace(toggle_closed_captions_button_);
  visitor->Trace(playback_speed_button_);
  visitor->Trace(fullscreen_button_);
  visitor->Trace(download_button_);
  visitor->Trace(duration_display_);
  visitor->Trace(enclosure_);
  visitor->Trace(text_track_list_);
  visitor->Trace(playback_speed_list_);
  visitor->Trace(overflow_menu_);
  visitor->Trace(overflow_list_);
  visitor->Trace(cast_button_);
  visitor->Trace(overlay_cast_button_);
  visitor->Trace(media_event_listener_);
  visitor->Trace(orientation_lock_delegate_);
  visitor->Trace(rotate_to_fullscreen_delegate_);
  visitor->Trace(display_cutout_delegate_);
  visitor->Trace(hide_media_controls_timer_);
  visitor->Trace(media_button_panel_);
  visitor->Trace(loading_panel_);
  visitor->Trace(display_cutout_fullscreen_button_);
  visitor->Trace(volume_control_container_);
  visitor->Trace(text_track_manager_);
  MediaControls::Trace(visitor);
  HTMLDivElement::Trace(visitor);
}

}  // namespace blink
