// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/autoplay_uma_helper.h"

#include "base/metrics/histogram_functions.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/media/autoplay_policy.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"

namespace blink {

namespace {

constexpr base::TimeDelta kMaxOffscreenDurationUma = base::Hours(1);
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
  if (frame->HasStickyUserActivation())
    result |= 0x02;
  if (frame->HadStickyUserActivationBeforeNavigation())
    result |= 0x04;

  return result;
}

}  // namespace

AutoplayUmaHelper::AutoplayUmaHelper(HTMLMediaElement* element)
    : ExecutionContextLifecycleObserver(
          static_cast<ExecutionContext*>(nullptr)),
      element_(element),
      muted_video_play_method_intersection_observer_(nullptr),
      is_visible_(false),
      muted_video_offscreen_duration_intersection_observer_(nullptr) {}

AutoplayUmaHelper::~AutoplayUmaHelper() = default;

static void RecordAutoplaySourceMetrics(HTMLMediaElement* element,
                                        AutoplaySource source) {
  if (IsA<HTMLVideoElement>(element)) {
    base::UmaHistogramEnumeration("Media.Video.Autoplay", source);
    return;
  }
  base::UmaHistogramEnumeration("Media.Audio.Autoplay", source);
}

void AutoplayUmaHelper::OnAutoplayInitiated(AutoplaySource source) {

  // Autoplay already initiated
  if (sources_.Contains(source))
    return;

  sources_.insert(source);

  // Record the source.
  RecordAutoplaySourceMetrics(element_.Get(), source);

  // Record dual source.
  if (sources_.size() == kDualSourceSize)
    RecordAutoplaySourceMetrics(element_.Get(), AutoplaySource::kDualSource);

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
  base::UmaHistogramEnumeration("Media.Video.Autoplay.Muted.UnmuteAction",
                                status);

  // Record UKM event for unmute muted autoplay.
  if (element_->GetDocument().IsInOutermostMainFrame()) {
    int source = static_cast<int>(AutoplaySource::kAttribute);
    if (sources_.size() == kDualSourceSize) {
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

  SetExecutionContext(element_->GetExecutionContext());
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
    NOTREACHED_IN_MIGRATION();
}

void AutoplayUmaHelper::HandlePlayingEvent() {
  MaybeStartRecordingMutedVideoPlayMethodBecomeVisible();
  MaybeStartRecordingMutedVideoOffscreenDuration();

  element_->removeEventListener(event_type_names::kPlaying, this, false);
}

void AutoplayUmaHelper::HandlePauseEvent() {
  MaybeStopRecordingMutedVideoOffscreenDuration();
}

void AutoplayUmaHelper::ContextDestroyed() {
  HandleContextDestroyed();
}

void AutoplayUmaHelper::HandleContextDestroyed() {
  MaybeStopRecordingMutedVideoPlayMethodBecomeVisible(false);
  MaybeStopRecordingMutedVideoOffscreenDuration();
}

void AutoplayUmaHelper::MaybeStartRecordingMutedVideoPlayMethodBecomeVisible() {
  if (!sources_.Contains(AutoplaySource::kMethod) ||
      !IsA<HTMLVideoElement>(element_.Get()) || !element_->muted())
    return;

  muted_video_play_method_intersection_observer_ = IntersectionObserver::Create(
      element_->GetDocument(),
      WTF::BindRepeating(
          &AutoplayUmaHelper::
              OnIntersectionChangedForMutedVideoPlayMethodBecomeVisible,
          WrapWeakPersistent(this)),
      LocalFrameUkmAggregator::kMediaIntersectionObserver,
      IntersectionObserver::Params{
          .thresholds = {IntersectionObserver::kMinimumThreshold}});
  muted_video_play_method_intersection_observer_->observe(element_);
  SetExecutionContext(element_->GetExecutionContext());
}

void AutoplayUmaHelper::MaybeStopRecordingMutedVideoPlayMethodBecomeVisible(
    bool visible) {
  if (!muted_video_play_method_intersection_observer_)
    return;

  base::UmaHistogramBoolean(
      "Media.Video.Autoplay.Muted.PlayMethod.BecomesVisible", visible);

  muted_video_play_method_intersection_observer_->disconnect();
  muted_video_play_method_intersection_observer_ = nullptr;
  MaybeUnregisterContextDestroyedObserver();
}

void AutoplayUmaHelper::MaybeStartRecordingMutedVideoOffscreenDuration() {
  if (!IsA<HTMLVideoElement>(element_.Get()) || !element_->muted() ||
      !sources_.Contains(AutoplaySource::kMethod))
    return;

  // Start recording muted video playing offscreen duration.
  muted_video_autoplay_offscreen_start_time_ = base::TimeTicks::Now();
  is_visible_ = false;
  muted_video_offscreen_duration_intersection_observer_ =
      IntersectionObserver::Create(
          element_->GetDocument(),
          WTF::BindRepeating(
              &AutoplayUmaHelper::
                  OnIntersectionChangedForMutedVideoOffscreenDuration,
              WrapWeakPersistent(this)),
          LocalFrameUkmAggregator::kMediaIntersectionObserver,
          IntersectionObserver::Params{
              .thresholds = {IntersectionObserver::kMinimumThreshold}});
  muted_video_offscreen_duration_intersection_observer_->observe(element_);
  element_->addEventListener(event_type_names::kPause, this, false);
  SetExecutionContext(element_->GetExecutionContext());
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
      muted_video_autoplay_offscreen_duration_, base::Milliseconds(1),
      kMaxOffscreenDurationUma, kOffscreenDurationUmaBucketCount);

  muted_video_offscreen_duration_intersection_observer_->disconnect();
  muted_video_offscreen_duration_intersection_observer_ = nullptr;
  muted_video_autoplay_offscreen_duration_ = base::TimeDelta();
  MaybeUnregisterMediaElementPauseListener();
  MaybeUnregisterContextDestroyedObserver();
}

void AutoplayUmaHelper::MaybeUnregisterContextDestroyedObserver() {
  // TODO(keishi): Remove IsIteratingOverObservers() check when
  // HeapObserverList() supports removal while iterating.
  if (!ShouldListenToContextDestroyed() && !GetExecutionContext()
                                                ->ContextLifecycleObserverSet()
                                                .IsIteratingOverObservers()) {
    SetExecutionContext(nullptr);
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

void AutoplayUmaHelper::Trace(Visitor* visitor) const {
  NativeEventListener::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  visitor->Trace(element_);
  visitor->Trace(muted_video_play_method_intersection_observer_);
  visitor->Trace(muted_video_offscreen_duration_intersection_observer_);
}

}  // namespace blink
