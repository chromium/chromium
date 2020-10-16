// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/video_rvfc/video_frame_callback_requester_impl.h"

#include <memory>
#include <utility>

#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame_metadata.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/scripted_animation_controller.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/core/timing/time_clamper.h"
#include "third_party/blink/renderer/modules/video_rvfc/video_frame_request_callback_collection.h"
#include "third_party/blink/renderer/modules/xr/navigator_xr.h"
#include "third_party/blink/renderer/modules/xr/xr_frame_provider.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_system.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {
// Returns whether or not a video's frame rate is close to the browser's frame
// rate, as measured by their rendering intervals. For example, on a 60hz
// screen, this should return false for a 25fps video and true for a 60fps
// video. On a 144hz screen, both videos would return false.
static bool IsFrameRateRelativelyHigh(base::TimeDelta rendering_interval,
                                      base::TimeDelta average_frame_duration) {
  if (average_frame_duration.is_zero())
    return false;

  constexpr double kThreshold = 0.05;
  return kThreshold >
         std::abs(1.0 - (rendering_interval / average_frame_duration));
}

}  // namespace

VideoFrameCallbackRequesterImpl::VideoFrameCallbackRequesterImpl(
    HTMLVideoElement& element)
    : VideoFrameCallbackRequester(element),
      callback_collection_(
          MakeGarbageCollected<VideoFrameRequestCallbackCollection>(
              element.GetExecutionContext())) {}

VideoFrameCallbackRequesterImpl::~VideoFrameCallbackRequesterImpl() = default;

// static
VideoFrameCallbackRequesterImpl& VideoFrameCallbackRequesterImpl::From(
    HTMLVideoElement& element) {
  VideoFrameCallbackRequesterImpl* supplement =
      Supplement<HTMLVideoElement>::From<VideoFrameCallbackRequesterImpl>(
          element);
  if (!supplement) {
    supplement = MakeGarbageCollected<VideoFrameCallbackRequesterImpl>(element);
    Supplement<HTMLVideoElement>::ProvideTo(element, supplement);
  }

  return *supplement;
}

// static
int VideoFrameCallbackRequesterImpl::requestVideoFrameCallback(
    HTMLVideoElement& element,
    V8VideoFrameRequestCallback* callback) {
  return VideoFrameCallbackRequesterImpl::From(element)
      .requestVideoFrameCallback(callback);
}

// static
void VideoFrameCallbackRequesterImpl::cancelVideoFrameCallback(
    HTMLVideoElement& element,
    int callback_id) {
  VideoFrameCallbackRequesterImpl::From(element).cancelVideoFrameCallback(
      callback_id);
}

void VideoFrameCallbackRequesterImpl::OnWebMediaPlayerCreated() {
  DCHECK(RuntimeEnabledFeatures::RequestVideoFrameCallbackEnabled());
  if (!callback_collection_->IsEmpty())
    GetSupplementable()->GetWebMediaPlayer()->RequestVideoFrameCallback();
}

void VideoFrameCallbackRequesterImpl::ScheduleWindowRaf() {
  GetSupplementable()
      ->GetDocument()
      .GetScriptedAnimationController()
      .ScheduleVideoFrameCallbacksExecution(
          WTF::Bind(&VideoFrameCallbackRequesterImpl::OnExecution,
                    WrapWeakPersistent(this)));
}

void VideoFrameCallbackRequesterImpl::ScheduleExecution() {
  TRACE_EVENT1("blink", "VideoFrameCallbackRequesterImpl::ScheduleExecution",
               "did_schedule", !pending_execution_);

  if (pending_execution_)
    return;

  pending_execution_ = true;

  if (TryScheduleImmersiveXRSessionRaf())
    return;

  ScheduleWindowRaf();
}

void VideoFrameCallbackRequesterImpl::OnImmersiveSessionStart() {
  in_immersive_session_ = true;

  if (pending_execution_ && !callback_collection_->IsEmpty())
    TryScheduleImmersiveXRSessionRaf();
}

void VideoFrameCallbackRequesterImpl::OnImmersiveSessionEnd() {
  in_immersive_session_ = false;

  if (pending_execution_ && !callback_collection_->IsEmpty())
    ScheduleWindowRaf();
}

void VideoFrameCallbackRequesterImpl::OnImmersiveFrame() {
  if (callback_collection_->IsEmpty())
    return;

  if (auto* player = GetSupplementable()->GetWebMediaPlayer())
    player->UpdateFrameIfStale();
}

XRFrameProvider* VideoFrameCallbackRequesterImpl::GetXRFrameProvider() {
  auto& document = GetSupplementable()->GetDocument();

  // Do not force the lazy creation of the NavigatorXR by accessing it through
  // NavigatorXR::From(). If it doesn't exist already exist, the webpage isn't
  // using XR.
  if (!NavigatorXR::AlreadyExists(document))
    return nullptr;

  auto* system = NavigatorXR::From(document)->xr();

  if (!system)
    return nullptr;

  return system->frameProvider();
}

bool VideoFrameCallbackRequesterImpl::TryScheduleImmersiveXRSessionRaf() {
  // Nothing to do here, we will be notified via OnImmersiveSessionStart() when
  // a new immersive session starts.
  if (observing_immersive_session_ && !in_immersive_session_)
    return false;

  auto* frame_provider = GetXRFrameProvider();

  if (!frame_provider)
    return false;

  if (!observing_immersive_session_) {
    frame_provider->AddImmersiveSessionObserver(this);
    observing_immersive_session_ = true;
  }

  XRSession* session = frame_provider->immersive_session();

  in_immersive_session_ = session && !session->ended();

  if (!in_immersive_session_)
    return false;

  session->ScheduleVideoFrameCallbacksExecution(WTF::Bind(
      &VideoFrameCallbackRequesterImpl::OnExecution, WrapWeakPersistent(this)));

  return true;
}

void VideoFrameCallbackRequesterImpl::OnRequestVideoFrameCallback() {
  DCHECK(RuntimeEnabledFeatures::RequestVideoFrameCallbackEnabled());
  TRACE_EVENT1("blink",
               "VideoFrameCallbackRequesterImpl::OnRequestVideoFrameCallback",
               "has_callbacks", !callback_collection_->IsEmpty());

  // Skip this work if there are no registered callbacks.
  if (callback_collection_->IsEmpty())
    return;

  ScheduleExecution();
}

void VideoFrameCallbackRequesterImpl::ExecuteVideoFrameCallbacks(
    double high_res_now_ms,
    std::unique_ptr<WebMediaPlayer::VideoFramePresentationMetadata>
        frame_metadata) {
  TRACE_EVENT0("blink",
               "VideoFrameCallbackRequesterImpl::ExecuteVideoFrameCallbacks");

  last_presented_frames_ = frame_metadata->presented_frames;

  auto* metadata = VideoFrameMetadata::Create();
  auto& time_converter =
      GetSupplementable()->GetDocument().Loader()->GetTiming();

  metadata->setPresentationTime(GetClampedTimeInMillis(
      time_converter.MonotonicTimeToZeroBasedDocumentTime(
          frame_metadata->presentation_time)));

  metadata->setExpectedDisplayTime(GetClampedTimeInMillis(
      time_converter.MonotonicTimeToZeroBasedDocumentTime(
          frame_metadata->expected_display_time)));

  metadata->setPresentedFrames(frame_metadata->presented_frames);

  metadata->setWidth(frame_metadata->width);
  metadata->setHeight(frame_metadata->height);

  metadata->setMediaTime(frame_metadata->media_time.InSecondsF());

  if (frame_metadata->metadata.processing_time) {
    metadata->setProcessingDuration(GetCoarseClampedTimeInSeconds(
        *frame_metadata->metadata.processing_time));
  }

  if (frame_metadata->metadata.capture_begin_time) {
    metadata->setCaptureTime(GetClampedTimeInMillis(
        time_converter.MonotonicTimeToZeroBasedDocumentTime(
            *frame_metadata->metadata.capture_begin_time)));
  }

  if (frame_metadata->metadata.receive_time) {
    metadata->setReceiveTime(GetClampedTimeInMillis(
        time_converter.MonotonicTimeToZeroBasedDocumentTime(
            *frame_metadata->metadata.receive_time)));
  }

  if (frame_metadata->metadata.rtp_timestamp) {
    double rtp_timestamp = *frame_metadata->metadata.rtp_timestamp;
    base::CheckedNumeric<uint32_t> uint_rtp_timestamp = rtp_timestamp;
    if (uint_rtp_timestamp.IsValid())
      metadata->setRtpTimestamp(rtp_timestamp);
  }

  callback_collection_->ExecuteFrameCallbacks(high_res_now_ms, metadata);
}

void VideoFrameCallbackRequesterImpl::OnExecution(double high_res_now_ms) {
  TRACE_EVENT1("blink", "VideoFrameCallbackRequesterImpl::OnRenderingSteps",
               "has_callbacks", !callback_collection_->IsEmpty());
  pending_execution_ = false;

  // Callbacks could have been canceled from the time we scheduled their
  // execution.
  // We could also be executing a leftover callback scheduled through the
  // ScriptedAnimationController, right after exiting an immersive XR session.
  if (callback_collection_->IsEmpty())
    return;

  auto* player = GetSupplementable()->GetWebMediaPlayer();
  if (!player)
    return;

  auto metadata = player->GetVideoFramePresentationMetadata();

  const bool is_hfr = IsFrameRateRelativelyHigh(
      metadata->rendering_interval, metadata->average_frame_duration);

  // Check if we have a new frame or not.
  if (last_presented_frames_ == metadata->presented_frames) {
    ++consecutive_stale_frames_;
  } else {
    consecutive_stale_frames_ = 0;
    ExecuteVideoFrameCallbacks(high_res_now_ms, std::move(metadata));
  }

  // If the video's frame rate is relatively close to the screen's refresh rate
  // (or brower's current frame rate), schedule ourselves immediately.
  // Otherwise, jittering and thread hopping means that the call to
  // OnRequestVideoFrameCallback() would barely miss the rendering steps, and we
  // would miss a frame.
  // Also check |consecutive_stale_frames_| to make sure we don't schedule
  // executions when paused, or in other scenarios where potentially scheduling
  // extra rendering steps would be wasteful.
  if (is_hfr && !callback_collection_->IsEmpty() &&
      consecutive_stale_frames_ < 2) {
    ScheduleExecution();
  }
}

// static
double VideoFrameCallbackRequesterImpl::GetClampedTimeInMillis(
    base::TimeDelta time) {
  return Performance::ClampTimeResolution(time.InSecondsF()) *
         base::Time::kMillisecondsPerSecond;
}

// static
double VideoFrameCallbackRequesterImpl::GetCoarseClampedTimeInSeconds(
    base::TimeDelta time) {
  constexpr auto kCoarseResolution = base::TimeDelta::FromMicroseconds(100);
  // Add this assert, in case TimeClamper's resolution were to change to be
  // stricter.
  static_assert(kCoarseResolution >= base::TimeDelta::FromSecondsD(
                                         TimeClamper::kResolutionSeconds),
                "kCoarseResolution should be at least as coarse as other clock "
                "resolutions");

  return time.FloorToMultiple(kCoarseResolution).InSecondsF();
}

int VideoFrameCallbackRequesterImpl::requestVideoFrameCallback(
    V8VideoFrameRequestCallback* callback) {
  TRACE_EVENT0("blink",
               "VideoFrameCallbackRequesterImpl::requestVideoFrameCallback");

  if (auto* player = GetSupplementable()->GetWebMediaPlayer())
    player->RequestVideoFrameCallback();

  auto* frame_callback = MakeGarbageCollected<
      VideoFrameRequestCallbackCollection::V8VideoFrameCallback>(callback);

  return callback_collection_->RegisterFrameCallback(frame_callback);
}

void VideoFrameCallbackRequesterImpl::RegisterCallbackForTest(
    VideoFrameRequestCallbackCollection::VideoFrameCallback* callback) {
  pending_execution_ = true;

  callback_collection_->RegisterFrameCallback(callback);
}

void VideoFrameCallbackRequesterImpl::cancelVideoFrameCallback(int id) {
  callback_collection_->CancelFrameCallback(id);
}

void VideoFrameCallbackRequesterImpl::Trace(Visitor* visitor) const {
  visitor->Trace(callback_collection_);
  Supplement<HTMLVideoElement>::Trace(visitor);
}

}  // namespace blink
