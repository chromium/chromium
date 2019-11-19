// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/content/android/thread_safe_capture_oracle.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bits.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_metadata.h"
#include "media/base/video_util.h"
#include "media/capture/video_capture_types.h"
#include "ui/gfx/geometry/rect.h"

namespace media {

namespace {

// The target maximum amount of the buffer pool to utilize.  Actual buffer pool
// utilization is attenuated by this amount before being reported to the
// VideoCaptureOracle.  This value takes into account the maximum number of
// buffer pool buffers and a desired safety margin.
const int kTargetMaxPoolUtilizationPercent = 60;

}  // namespace

struct ThreadSafeCaptureOracle::InFlightFrameCapture {
  int frame_number;
  VideoCaptureDevice::Client::Buffer buffer;
  std::unique_ptr<VideoCaptureBufferHandle> buffer_access;
  base::TimeTicks begin_time;
  base::TimeDelta frame_duration;
};

ThreadSafeCaptureOracle::ThreadSafeCaptureOracle(
    std::unique_ptr<VideoCaptureDevice::Client> client,
    const VideoCaptureParams& params)
    : client_(std::move(client)), oracle_(false), params_(params) {
  DCHECK_GE(params.requested_format.frame_rate, 1e-6f);
  oracle_.SetMinCapturePeriod(base::TimeDelta::FromMicroseconds(
      static_cast<int64_t>(1000000.0 / params.requested_format.frame_rate +
                           0.5 /* to round to nearest int */)));
  const auto constraints = params.SuggestConstraints();
  oracle_.SetCaptureSizeConstraints(constraints.min_frame_size,
                                    constraints.max_frame_size,
                                    constraints.fixed_aspect_ratio);
}

ThreadSafeCaptureOracle::~ThreadSafeCaptureOracle() = default;

bool ThreadSafeCaptureOracle::ObserveEventAndDecideCapture(
    VideoCaptureOracle::Event event,
    const gfx::Rect& damage_rect,
    base::TimeTicks event_time,
    scoped_refptr<VideoFrame>* storage,
    CaptureFrameCallback* callback) {
  // Grab the current time before waiting to acquire the |lock_|.
  const base::TimeTicks capture_begin_time = base::TimeTicks::Now();

  gfx::Size visible_size;
  gfx::Size coded_size;
  media::VideoCaptureDevice::Client::Buffer output_buffer;
  double attenuated_utilization;
  int frame_number;
  base::TimeDelta estimated_frame_duration;
  {
    base::AutoLock guard(lock_);

    if (!client_)
      return false;  // Capture is stopped.

    if (!oracle_.ObserveEventAndDecideCapture(event, damage_rect, event_time)) {
      // This is a normal and acceptable way to drop a frame. We've hit our
      // capture rate limit: for example, the content is animating at 60fps but
      // we're capturing at 30fps.
      TRACE_EVENT_INSTANT1("gpu.capture", "FpsRateLimited",
                           TRACE_EVENT_SCOPE_THREAD, "trigger",
                           VideoCaptureOracle::EventAsString(event));
      return false;
    }

    frame_number = oracle_.next_frame_number();
    visible_size = oracle_.capture_size();
    // TODO(miu): Clients should request exact padding, instead of this
    // memory-wasting hack to make frames that are compatible with all HW
    // encoders.  http://crbug.com/555911
    coded_size.SetSize(base::bits::Align(visible_size.width(), 16),
                       base::bits::Align(visible_size.height(), 16));

    const auto result_code = client_->ReserveOutputBuffer(
        coded_size, params_.requested_format.pixel_format, frame_number,
        &output_buffer);

    // Get the current buffer pool utilization and attenuate it: The utilization
    // reported to the oracle is in terms of a maximum sustainable amount (not
    // the absolute maximum).
    attenuated_utilization = client_->GetBufferPoolUtilization() *
                             (100.0 / kTargetMaxPoolUtilizationPercent);

    if (result_code != VideoCaptureDevice::Client::ReserveResult::kSucceeded) {
      TRACE_EVENT_INSTANT2(
          "gpu.capture", "PipelineLimited", TRACE_EVENT_SCOPE_THREAD, "trigger",
          VideoCaptureOracle::EventAsString(event), "atten_util_percent",
          base::saturated_cast<int>(attenuated_utilization * 100.0 + 0.5));
      oracle_.RecordWillNotCapture(attenuated_utilization);
      return false;
    }

    oracle_.RecordCapture(attenuated_utilization);
    estimated_frame_duration = oracle_.estimated_frame_duration();
  }  // End of critical section.

  if (attenuated_utilization >= 1.0) {
    TRACE_EVENT_INSTANT2(
        "gpu.capture", "NearlyPipelineLimited", TRACE_EVENT_SCOPE_THREAD,
        "trigger", VideoCaptureOracle::EventAsString(event),
        "atten_util_percent",
        base::saturated_cast<int>(attenuated_utilization * 100.0 + 0.5));
  }

  TRACE_EVENT_ASYNC_BEGIN2("gpu.capture", "Capture", output_buffer.id,
                           "frame_number", frame_number, "trigger",
                           VideoCaptureOracle::EventAsString(event));

  std::unique_ptr<VideoCaptureBufferHandle> output_buffer_access =
      output_buffer.handle_provider->GetHandleForInProcessAccess();
  *storage = VideoFrame::WrapExternalData(
      params_.requested_format.pixel_format, coded_size,
      gfx::Rect(visible_size), visible_size, output_buffer_access->data(),
      output_buffer_access->mapped_size(), base::TimeDelta());

  // Note: Passing the |output_buffer_access| in the callback is a bit of a
  // hack. Really, the access should be owned by the VideoFrame so that access
  // is released (unpinning the shared memory) when the VideoFrame goes
  // out-of-scope. However, there is an an issue where, at browser shutdown, the
  // callback below may never be run, and instead it self-deletes: If this
  // happens, the VideoFrame will release access *after* the Buffer goes
  // out-of-scope, which is an invalid sequence of steps. This could be fixed in
  // upstream implementation, but it's not worth spending time tracking it down
  // because all of this code (and upstream code) is about to be replaced.
  // http://crbug.com/754872
  //
  // To be clear, this solution allows |output_buffer_access| to be deleted
  // before |output_buffer| if the callback self-deletes rather than ever being
  // run. The InFlightFrameCapture destructor ensures this.
  std::unique_ptr<InFlightFrameCapture> capture(new InFlightFrameCapture{
      frame_number, std::move(output_buffer), std::move(output_buffer_access),
      capture_begin_time, estimated_frame_duration});

  // If creating the VideoFrame wrapper failed, call DidCaptureFrame() with
  // !success to execute the required post-capture steps (tracing, notification
  // of failure to VideoCaptureOracle, etc.).
  if (!(*storage)) {
    DidCaptureFrame(std::move(capture), *storage, event_time, false);
    return false;
  }

  *callback = base::BindOnce(&ThreadSafeCaptureOracle::DidCaptureFrame, this,
                             base::Passed(&capture));

  return true;
}

gfx::Size ThreadSafeCaptureOracle::GetCaptureSize() const {
  base::AutoLock guard(lock_);
  return oracle_.capture_size();
}

void ThreadSafeCaptureOracle::UpdateCaptureSize(const gfx::Size& source_size) {
  base::AutoLock guard(lock_);
  VLOG(1) << "Source size changed to " << source_size.ToString();
  oracle_.SetSourceSize(source_size);
}

void ThreadSafeCaptureOracle::Stop() {
  base::AutoLock guard(lock_);
  client_.reset();
}

void ThreadSafeCaptureOracle::ReportError(media::VideoCaptureError error,
                                          const base::Location& from_here,
                                          const std::string& reason) {
  base::AutoLock guard(lock_);
  if (client_)
    client_->OnError(error, from_here, reason);
}

void ThreadSafeCaptureOracle::ReportStarted() {
  base::AutoLock guard(lock_);
  if (client_)
    client_->OnStarted();
}

void ThreadSafeCaptureOracle::DidCaptureFrame(
    std::unique_ptr<InFlightFrameCapture> capture,
    scoped_refptr<VideoFrame> frame,
    base::TimeTicks reference_time,
    bool success) {
  // Release |buffer_access| now that nothing is accessing the memory via the
  // VideoFrame data pointers anymore.
  capture->buffer_access.reset();

  base::AutoLock guard(lock_);

  const bool should_deliver_frame =
      oracle_.CompleteCapture(capture->frame_number, success, &reference_time);

  TRACE_EVENT_ASYNC_END2("gpu.capture", "Capture", capture->buffer.id,
                         "success", should_deliver_frame, "timestamp",
                         (reference_time - base::TimeTicks()).InMicroseconds());

  if (!should_deliver_frame || !client_)
    return;

  frame->metadata()->SetDouble(VideoFrameMetadata::FRAME_RATE,
                               params_.requested_format.frame_rate);
  frame->metadata()->SetTimeTicks(VideoFrameMetadata::CAPTURE_BEGIN_TIME,
                                  capture->begin_time);
  frame->metadata()->SetTimeTicks(VideoFrameMetadata::CAPTURE_END_TIME,
                                  base::TimeTicks::Now());
  frame->metadata()->SetTimeDelta(VideoFrameMetadata::FRAME_DURATION,
                                  capture->frame_duration);
  frame->metadata()->SetTimeTicks(VideoFrameMetadata::REFERENCE_TIME,
                                  reference_time);

  media::VideoCaptureFormat format(frame->coded_size(),
                                   params_.requested_format.frame_rate,
                                   frame->format());
  client_->OnIncomingCapturedBufferExt(
      std::move(capture->buffer), format, frame->ColorSpace(), reference_time,
      frame->timestamp(), frame->visible_rect(), *frame->metadata());
}

void ThreadSafeCaptureOracle::OnConsumerReportingUtilization(
    int frame_number,
    double utilization) {
  base::AutoLock guard(lock_);
  oracle_.RecordConsumerFeedback(frame_number, utilization);
}

}  // namespace media
