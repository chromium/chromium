// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/autoplay_uma_helper.h"

#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/media/autoplay_policy.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"

namespace blink {

namespace {

constexpr base::TimeDelta kMaxOffscreenDurationUma =
    base::TimeDelta::FromHours(1);
constexpr int32_t kOffscreenDurationUmaBucketCount = 50;

// Returns a int64_t with the following structure:
// 0b0001 set if there is a user gesture on the stack.
// 0b0010 set if there was a user gesture on the page.
// 0b0100 set if there was a user gesture propagated after navigation.
int64_t GetUserGestureStatusForUkmMetric(LocalFrame* frame) {
  DCHECK(frame);

  int64_t result = 0;

  if (LocalFrame::HasTransientUserActivation(frame))
    result |= 0x01;
  if (frame->HasBeenActivated())
    result |= 0x02;
  if (frame->HasReceivedUserGestureBeforeNavigation())
    result |= 0x04;

  return result;
}

}  // namespace

AutoplayUmaHelper::AutoplayUmaHelper(HTMLMediaElement* element)
    : ContextLifecycleObserver(nullptr),
      element_(element),
      muted_video_play_method_intersection_observer_(nullptr),
      is_visible_(false),
      muted_video_offscreen_duration_intersection_observer_(nullptr) {
}

AutoplayUmaHelper::~AutoplayUmaHelper() = default;

void AutoplayUmaHelper::OnAutoplayInitiated(AutoplaySource source) {
  DEFINE_STATIC_LOCAL(EnumerationHistogram, video_histogram,
                      ("Media.Video.Autoplay",
                       static_cast<int>(AutoplaySource::kNumberOfUmaSources)));
  DEFINE_STATIC_LOCAL(EnumerationHistogram, muted_video_histogram,
                      ("Media.Video.Autoplay.Muted",
                       static_cast<int>(AutoplaySource::kNumberOfUmaSources)));
  DEFINE_STATIC_LOCAL(EnumerationHistogram, audio_histogram,
                      ("Media.Audio.Autoplay",
                       static_cast<int>(AutoplaySource::kNumberOfUmaSources)));
  DEFINE_STATIC_LOCAL(
      EnumerationHistogram, blocked_muted_video_histogram,
      ("Media.Video.Autoplay.Muted.Blocked", kAutoplayBlockedReasonMax));

  // Autoplay already initiated
  if (sources_.Contains(source))
    return;

  sources_.insert(source);

  // Record the source.
  if (element_->IsHTMLVideoElement()) {
    video_histogram.Count(static_cast<int>(source));
    if (element_->muted())
      muted_video_histogram.Count(static_cast<int>(source));
  } else {
    audio_histogram.Count(static_cast<int>(source));
  }

  // Record dual source.
  if (sources_.size() ==
      static_cast<size_t>(AutoplaySource::kNumberOfSources)) {
    if (element_->IsHTMLVideoElement()) {
      video_histogram.Count(static_cast<int>(AutoplaySource::kDualSource));
      if (element_->muted())
        muted_video_histogram.Count(
            static_cast<int>(AutoplaySource::kDualSource));
    } else {
      audio_histogram.Count(static_cast<int>(AutoplaySource::kDualSource));
    }
  }

  // Record if it will be blocked by the Autoplay setting.
  if (element_->IsHTMLVideoElement() && element_->muted() &&
      AutoplayPolicy::DocumentShouldAutoplayMutedVideos(
          element_->GetDocument()) &&
      !element_->GetAutoplayPolicy().IsAutoplayAllowedPerSettings()) {
    blocked_muted_video_histogram.Count(kAutoplayBlockedReasonSetting);
  }

  element_->addEventListener(event_type_names::kPlaying, this, false);

  // Record UKM autoplay event.
  if (!element_->GetDocument().IsActive())
    return;
  LocalFrame* frame = element_->GetDocument().GetFrame();
  DCHECK(frame);
  DCHECK(element_->GetDocument().GetPage());

  ukm::UkmRecorder* ukm_recorder = element_->GetDocument().UkmRecorder();
  DCHECK(ukm_recorder);
  ukm::builders::Media_Autoplay_Attempt(element_->GetDocument().UkmSourceID())
      .SetSource(source == AutoplaySource::kMethod)
      .SetAudioTrack(element_->HasAudio())
      .SetVideoTrack(element_->HasVideo())
      .SetUserGestureRequired(
          element_->GetAutoplayPolicy().IsGestureNeededForPlayback())
      .SetMuted(element_->muted())
      .SetHighMediaEngagement(AutoplayPolicy::DocumentHasHighMediaEngagement(
          element_->GetDocument()))
      .SetUserGestureStatus(GetUserGestureStatusForUkmMetric(frame))
      .Record(ukm_recorder);
}

void AutoplayUmaHelper::RecordAutoplayUnmuteStatus(
    AutoplayUnmuteActionStatus status) {
  DEFINE_STATIC_LOCAL(
      EnumerationHistogram, autoplay_unmute_histogram,
      ("Media.Video.Autoplay.Muted.UnmuteAction",
       static_cast<int>(AutoplayUnmuteActionStatus::kNumberOfStatus)));

  autoplay_unmute_histogram.Count(static_cast<int>(status));

  // Record UKM event for unmute muted autoplay.
  if (element_->GetDocument().IsInMainFrame()) {
    int source = static_cast<int>(AutoplaySource::kAttribute);
    if (sources_.size() ==
        static_cast<size_t>(AutoplaySource::kNumberOfSources)) {
      source = static_cast<int>(AutoplaySource::kDualSource);
    } else if (sources_.Contains(AutoplaySource::kMethod)) {
      source = static_cast<int>(AutoplaySource::kAttribute);
    }

    ukm::UkmRecorder* ukm_recorder = element_->GetDocument().UkmRecorder();
    DCHECK(ukm_recorder);
    ukm::builders::Media_Autoplay_Muted_UnmuteAction(
        element_->GetDocument().UkmSourceID())
        .SetSource(source)
        .SetResult(status == AutoplayUnmuteActionStatus::kSuccess)
        .Record(ukm_recorder);
  }
}

void AutoplayUmaHelper::VideoWillBeDrawnToCanvas() {
  if (HasSource() && !IsVisible()) {
    UseCounter::Count(element_->GetDocument(),
                      WebFeature::kHiddenAutoplayedVideoInCanvas);
  }
}

void AutoplayUmaHelper::DidMoveToNewDocument(Document& old_document) {
  if (!ShouldListenToContextDestroyed())
    return;

  SetContext(&element_->GetDocument());
}

void AutoplayUmaHelper::
    OnIntersectionChangedForMutedVideoPlayMethodBecomeVisible(
        const HeapVector<Member<IntersectionObserverEntry>>& entries) {
  bool is_visible = (entries.back()->intersectionRatio() > 0);
  if (!is_visible || !muted_video_play_method_intersection_observer_)
    return;

  MaybeStopRecordingMutedVideoPlayMethodBecomeVisible(true);
}

void AutoplayUmaHelper::OnIntersectionChangedForMutedVideoOffscreenDuration(
    const HeapVector<Member<IntersectionObserverEntry>>& entries) {
  bool is_visible = (entries.back()->intersectionRatio() > 0);
  if (is_visible == is_visible_)
    return;

  if (is_visible) {
    muted_video_autoplay_offscreen_duration_ +=
        base::TimeTicks::Now() - muted_video_autoplay_offscreen_start_time_;
  } else {
    muted_video_autoplay_offscreen_start_time_ = base::TimeTicks::Now();
  }

  is_visible_ = is_visible;
}

void AutoplayUmaHelper::Invoke(ExecutionContext* execution_context,
                               Event* event) {
  if (event->type() == event_type_names::kPlaying)
    HandlePlayingEvent();
  else if (event->type() == event_type_names::kPause)
    HandlePauseEvent();
  else
    NOTREACHED();
}

void AutoplayUmaHelper::HandlePlayingEvent() {
  MaybeStartRecordingMutedVideoPlayMethodBecomeVisible();
  MaybeStartRecordingMutedVideoOffscreenDuration();

  element_->removeEventListener(event_type_names::kPlaying, this, false);
}

void AutoplayUmaHelper::HandlePauseEvent() {
  MaybeStopRecordingMutedVideoOffscreenDuration();
}

void AutoplayUmaHelper::ContextDestroyed(ExecutionContext*) {
  HandleContextDestroyed();
}

void AutoplayUmaHelper::HandleContextDestroyed() {
  MaybeStopRecordingMutedVideoPlayMethodBecomeVisible(false);
  MaybeStopRecordingMutedVideoOffscreenDuration();
}

void AutoplayUmaHelper::MaybeStartRecordingMutedVideoPlayMethodBecomeVisible() {
  if (!sources_.Contains(AutoplaySource::kMethod) ||
      !element_->IsHTMLVideoElement() || !element_->muted())
    return;

  muted_video_play_method_intersection_observer_ = IntersectionObserver::Create(
      {}, {IntersectionObserver::kMinimumThreshold}, &element_->GetDocument(),
      WTF::BindRepeating(
          &AutoplayUmaHelper::
              OnIntersectionChangedForMutedVideoPlayMethodBecomeVisible,
          WrapWeakPersistent(this)));
  muted_video_play_method_intersection_observer_->observe(element_);
  SetContext(&element_->GetDocument());
}

void AutoplayUmaHelper::MaybeStopRecordingMutedVideoPlayMethodBecomeVisible(
    bool visible) {
  if (!muted_video_play_method_intersection_observer_)
    return;

  DEFINE_STATIC_LOCAL(BooleanHistogram, histogram,
                      ("Media.Video.Autoplay.Muted.PlayMethod.BecomesVisible"));

  histogram.Count(visible);
  muted_video_play_method_intersection_observer_->disconnect();
  muted_video_play_method_intersection_observer_ = nullptr;
  MaybeUnregisterContextDestroyedObserver();
}

void AutoplayUmaHelper::MaybeStartRecordingMutedVideoOffscreenDuration() {
  if (!element_->IsHTMLVideoElement() || !element_->muted() ||
      !sources_.Contains(AutoplaySource::kMethod))
    return;

  // Start recording muted video playing offscreen duration.
  muted_video_autoplay_offscreen_start_time_ = base::TimeTicks::Now();
  is_visible_ = false;
  muted_video_offscreen_duration_intersection_observer_ =
      IntersectionObserver::Create(
          {}, {IntersectionObserver::kMinimumThreshold},
          &element_->GetDocument(),
          WTF::BindRepeating(
              &AutoplayUmaHelper::
                  OnIntersectionChangedForMutedVideoOffscreenDuration,
              WrapWeakPersistent(this)));
  muted_video_offscreen_duration_intersection_observer_->observe(element_);
  element_->addEventListener(event_type_names::kPause, this, false);
  SetContext(&element_->GetDocument());
}

void AutoplayUmaHelper::MaybeStopRecordingMutedVideoOffscreenDuration() {
  if (!muted_video_offscreen_duration_intersection_observer_)
    return;

  if (!is_visible_) {
    muted_video_autoplay_offscreen_duration_ +=
        base::TimeTicks::Now() - muted_video_autoplay_offscreen_start_time_;
  }

  DCHECK(sources_.Contains(AutoplaySource::kMethod));

  UMA_HISTOGRAM_CUSTOM_TIMES(
      "Media.Video.Autoplay.Muted.PlayMethod.OffscreenDuration",
      muted_video_autoplay_offscreen_duration_,
      base::TimeDelta::FromMilliseconds(1), kMaxOffscreenDurationUma,
      kOffscreenDurationUmaBucketCount);

  muted_video_offscreen_duration_intersection_observer_->disconnect();
  muted_video_offscreen_duration_intersection_observer_ = nullptr;
  muted_video_autoplay_offscreen_duration_ = base::TimeDelta();
  MaybeUnregisterMediaElementPauseListener();
  MaybeUnregisterContextDestroyedObserver();
}

void AutoplayUmaHelper::MaybeUnregisterContextDestroyedObserver() {
  if (!ShouldListenToContextDestroyed()) {
    SetContext(nullptr);
  }
}

void AutoplayUmaHelper::MaybeUnregisterMediaElementPauseListener() {
  if (muted_video_offscreen_duration_intersection_observer_)
    return;
  element_->removeEventListener(event_type_names::kPause, this, false);
}

bool AutoplayUmaHelper::ShouldListenToContextDestroyed() const {
  return muted_video_play_method_intersection_observer_ ||
         muted_video_offscreen_duration_intersection_observer_;
}

void AutoplayUmaHelper::Trace(Visitor* visitor) {
  NativeEventListener::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
  visitor->Trace(element_);
  visitor->Trace(muted_video_play_method_intersection_observer_);
  visitor->Trace(muted_video_offscreen_duration_intersection_observer_);
}

}  // namespace blink
