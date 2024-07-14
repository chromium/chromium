// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/video_wake_lock.h"

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/mojom/wake_lock/wake_lock.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/picture_in_picture_controller.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_read_only.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/media/remote_playback_controller.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/core/page/page.h"

namespace blink {

namespace {

// Require most of the video to be onscreen. For simplicity this is the same
// threshold we use for rotate-for-fullscreen.
constexpr float kStrictVisibilityThreshold = 0.75f;

// A YouTube embed works out to ~24% of the root window, so round down to 20% to
// ensure we aren't taking the wake lock for videos that are too small.
constexpr float kSizeThreshold = 0.2f;

Page* GetContainingPage(HTMLVideoElement& video) {
  return video.GetDocument().GetPage();
}

}  // namespace

VideoWakeLock::VideoWakeLock(HTMLVideoElement& video)
    : PageVisibilityObserver(GetContainingPage(video)),
      ExecutionContextLifecycleStateObserver(video.GetExecutionContext()),
      video_element_(video),
      wake_lock_service_(video.GetExecutionContext()),
      visibility_threshold_(kStrictVisibilityThreshold) {
  VideoElement().addEventListener(event_type_names::kPlaying, this, true);
  VideoElement().addEventListener(event_type_names::kPause, this, true);
  VideoElement().addEventListener(event_type_names::kEmptied, this, true);
  VideoElement().addEventListener(event_type_names::kEnterpictureinpicture,
                                  this, true);
  VideoElement().addEventListener(event_type_names::kLeavepictureinpicture,
                                  this, true);
  VideoElement().addEventListener(event_type_names::kVolumechange, this, true);
  StartIntersectionObserver();

  RemotePlaybackController* remote_playback_controller =
      RemotePlaybackController::From(VideoElement());
  if (remote_playback_controller)
    remote_playback_controller->AddObserver(this);

  UpdateStateIfNeeded();
}

void VideoWakeLock::ElementDidMoveToNewDocument() {
  SetExecutionContext(VideoElement().GetExecutionContext());
  SetPage(GetContainingPage(VideoElement()));
  visibility_observer_->disconnect();
  size_observer_->disconnect();
  StartIntersectionObserver();
}

void VideoWakeLock::PageVisibilityChanged() {
  Update();
}

void VideoWakeLock::OnVisibilityChanged(
    const HeapVector<Member<IntersectionObserverEntry>>& entries) {
  is_visible_ = entries.back()->intersectionRatio() > visibility_threshold_;
  Update();
}

void VideoWakeLock::OnSizeChanged(
    const HeapVector<Member<IntersectionObserverEntry>>& entries) {
  is_big_enough_ = entries.back()->intersectionRatio() > kSizeThreshold;
  Update();
}

void VideoWakeLock::Trace(Visitor* visitor) const {
  NativeEventListener::Trace(visitor);
  PageVisibilityObserver::Trace(visitor);
  ExecutionContextLifecycleStateObserver::Trace(visitor);
  visitor->Trace(video_element_);
  visitor->Trace(visibility_observer_);
  visitor->Trace(size_observer_);
  visitor->Trace(wake_lock_service_);
}

void VideoWakeLock::Invoke(ExecutionContext*, Event* event) {
  if (event->type() == event_type_names::kPlaying) {
    playing_ = true;
  } else if (event->type() == event_type_names::kPause ||
             event->type() == event_type_names::kEmptied) {
    // In 4.8.12.5 steps 6.6.1, the media element is paused when a new load
    // happens without actually firing a pause event. Because of this, we need
    // to listen to the emptied event.
    playing_ = false;
  } else {
    DCHECK(event->type() == event_type_names::kEnterpictureinpicture ||
           event->type() == event_type_names::kLeavepictureinpicture ||
           event->type() == event_type_names::kVolumechange);
  }

  Update();
}

void VideoWakeLock::OnRemotePlaybackStateChanged(
    mojom::blink::PresentationConnectionState state) {
  remote_playback_state_ = state;
  Update();
}

void VideoWakeLock::ContextLifecycleStateChanged(mojom::FrameLifecycleState) {
  Update();
}

void VideoWakeLock::ContextDestroyed() {
  Update();
}

float VideoWakeLock::GetSizeThresholdForTests() const {
  return kSizeThreshold;
}

void VideoWakeLock::Update() {
  bool should_be_active = ShouldBeActive();
  if (should_be_active == active_)
    return;

  active_ = should_be_active;
  UpdateWakeLockService();
}

bool VideoWakeLock::ShouldBeActive() const {
  bool page_visible = GetPage() && GetPage()->IsPageVisible();
  bool in_picture_in_picture =
      PictureInPictureController::IsElementInPictureInPicture(&VideoElement());
  bool context_is_running =
      VideoElement().GetExecutionContext() &&
      !VideoElement().GetExecutionContext()->IsContextPaused();

  bool has_volume = VideoElement().EffectiveMediaVolume() > 0;
  bool has_audio = VideoElement().HasAudio() && has_volume;

  // Self-view MediaStreams may often be very small.
  bool is_size_exempt =
      VideoElement().GetLoadType() == WebMediaPlayer::kLoadTypeMediaStream;

  bool is_big_enough = is_big_enough_ || is_size_exempt;

  // The visibility requirements are met if one of the following is true:
  //  - it's in Picture-in-Picture;
  //  - it's audibly playing on a visible page;
  //  - it's visible to the user and big enough (>=`kSizeThreshold` of view)
  bool visibility_requirements_met =
      VideoElement().HasVideo() &&
      (in_picture_in_picture ||
       (page_visible && ((is_visible_ && is_big_enough) || has_audio)));

  // The video wake lock should be active iff:
  //  - it's playing;
  //  - it has video frames;
  //  - the visibility requirements are met (see above);
  //  - it's *not* playing in Remote Playback;
  //  - the document is not paused nor destroyed.
  return playing_ && visibility_requirements_met &&
         remote_playback_state_ !=
             mojom::blink::PresentationConnectionState::CONNECTED &&
         context_is_running;
}

void VideoWakeLock::EnsureWakeLockService() {
  if (wake_lock_service_)
    return;

  LocalFrame* frame = VideoElement().GetDocument().GetFrame();
  if (!frame)
    return;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      frame->GetTaskRunner(TaskType::kMediaElementEvent);

  mojo::Remote<blink::mojom::blink::WakeLockService> service;
  frame->GetBrowserInterfaceBroker().GetInterface(
      service.BindNewPipeAndPassReceiver(task_runner));
  service->GetWakeLock(
      device::mojom::WakeLockType::kPreventDisplaySleep,
      device::mojom::blink::WakeLockReason::kVideoPlayback, "Video Wake Lock",
      wake_lock_service_.BindNewPipeAndPassReceiver(task_runner));
  wake_lock_service_.set_disconnect_handler(WTF::BindOnce(
      &VideoWakeLock::OnConnectionError, WrapWeakPersistent(this)));
}

void VideoWakeLock::OnConnectionError() {
  wake_lock_service_.reset();
}

void VideoWakeLock::UpdateWakeLockService() {
  EnsureWakeLockService();

  if (!wake_lock_service_)
    return;

  if (active_) {
    wake_lock_service_->RequestWakeLock();
  } else {
    wake_lock_service_->CancelWakeLock();
  }
}

void VideoWakeLock::StartIntersectionObserver() {
  // Most screen timeouts are at least 5s, so we don't need high frequency
  // intersection updates. Choose a value such that we're never more than 5s
  // apart w/ a 100ms of delivery leeway.
  //
  // TODO(crbug.com/1376286): Delay values appear to be broken. If a change
  // occurs during the delay window, the update is dropped entirely...
  constexpr base::TimeDelta kDelay;

  visibility_observer_ = IntersectionObserver::Create(
      VideoElement().GetDocument(),
      WTF::BindRepeating(&VideoWakeLock::OnVisibilityChanged,
                         WrapWeakPersistent(this)),
      LocalFrameUkmAggregator::kMediaIntersectionObserver,
      IntersectionObserver::Params{
          .thresholds = {visibility_threshold_},
          .delay = kDelay,
      });
  visibility_observer_->observe(&VideoElement());

  // Creating an IntersectionObserver with a null root provides us with the
  // total fraction of the viewport a video consumes.
  //
  // TODO(crbug.com/1416396): This doesn't work properly with cross origin
  // iframes. The observer doesn't know the outermost viewport size when
  // running from within an iframe.
  size_observer_ = IntersectionObserver::Create(
      VideoElement().GetDocument().TopDocument(),
      WTF::BindRepeating(&VideoWakeLock::OnSizeChanged,
                         WrapWeakPersistent(this)),
      LocalFrameUkmAggregator::kMediaIntersectionObserver,
      IntersectionObserver::Params{
          .thresholds = {kSizeThreshold},
          .semantics = IntersectionObserver::kFractionOfRoot,
          .delay = kDelay,
      });
  size_observer_->observe(&VideoElement());
}

}  // namespace blink
