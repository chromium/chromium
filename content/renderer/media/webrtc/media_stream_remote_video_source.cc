// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/media_stream_remote_video_source.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/trace_event/trace_event.h"
#include "content/renderer/media/webrtc/track_observer.h"
#include "content/renderer/media/webrtc/webrtc_video_frame_adapter.h"
#include "content/renderer/media/webrtc/webrtc_video_utils.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "third_party/webrtc/api/video/i420_buffer.h"
#include "third_party/webrtc/api/video/video_sink_interface.h"
#include "third_party/webrtc/rtc_base/timeutils.h"  // for TimeMicros

namespace content {

// Internal class used for receiving frames from the webrtc track on a
// libjingle thread and forward it to the IO-thread.
class MediaStreamRemoteVideoSource::RemoteVideoSourceDelegate
    : public base::RefCountedThreadSafe<RemoteVideoSourceDelegate>,
      public rtc::VideoSinkInterface<webrtc::VideoFrame> {
 public:
  RemoteVideoSourceDelegate(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      const VideoCaptureDeliverFrameCB& new_frame_callback);

 protected:
  friend class base::RefCountedThreadSafe<RemoteVideoSourceDelegate>;
  ~RemoteVideoSourceDelegate() override;

  // Implements rtc::VideoSinkInterface used for receiving video frames
  // from the PeerConnection video track. May be called on a libjingle internal
  // thread.
  void OnFrame(const webrtc::VideoFrame& frame) override;

  void DoRenderFrameOnIOThread(
      const scoped_refptr<media::VideoFrame>& video_frame);

 private:
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // |frame_callback_| is accessed on the IO thread.
  VideoCaptureDeliverFrameCB frame_callback_;

  // Timestamp of the first received frame.
  base::TimeDelta start_timestamp_;

  // WebRTC Chromium timestamp diff
  const base::TimeDelta time_diff_;
};

MediaStreamRemoteVideoSource::RemoteVideoSourceDelegate::
    RemoteVideoSourceDelegate(
        scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
        const VideoCaptureDeliverFrameCB& new_frame_callback)
    : io_task_runner_(io_task_runner),
      frame_callback_(new_frame_callback),
      start_timestamp_(media::kNoTimestamp),
      // TODO(qiangchen): There can be two differences between clocks: 1)
      // the offset, 2) the rate (i.e., one clock runs faster than the other).
      // See http://crbug/516700
      time_diff_(base::TimeTicks::Now() - base::TimeTicks() -
                 base::TimeDelta::FromMicroseconds(rtc::TimeMicros())) {}

MediaStreamRemoteVideoSource::
RemoteVideoSourceDelegate::~RemoteVideoSourceDelegate() {
}

namespace {
void DoNothing(const scoped_refptr<rtc::RefCountInterface>& ref) {}
}  // namespace

void MediaStreamRemoteVideoSource::RemoteVideoSourceDelegate::OnFrame(
    const webrtc::VideoFrame& incoming_frame) {
  const bool render_immediately = incoming_frame.timestamp_us() == 0;
  const base::TimeDelta incoming_timestamp =
      render_immediately
          ? base::TimeTicks::Now() - base::TimeTicks()
          : base::TimeDelta::FromMicroseconds(incoming_frame.timestamp_us());
  const base::TimeTicks render_time =
      render_immediately ? base::TimeTicks() + incoming_timestamp
                         : base::TimeTicks() + incoming_timestamp + time_diff_;
  if (start_timestamp_ == media::kNoTimestamp)
    start_timestamp_ = incoming_timestamp;
  const base::TimeDelta elapsed_timestamp =
      incoming_timestamp - start_timestamp_;
  TRACE_EVENT2("webrtc", "RemoteVideoSourceDelegate::RenderFrame",
               "Ideal Render Instant", render_time.ToInternalValue(),
               "Timestamp", elapsed_timestamp.InMicroseconds());

  scoped_refptr<media::VideoFrame> video_frame;
  scoped_refptr<webrtc::VideoFrameBuffer> buffer(
      incoming_frame.video_frame_buffer());
  const gfx::Size size(buffer->width(), buffer->height());

  switch (buffer->type()) {
    case webrtc::VideoFrameBuffer::Type::kNative: {
      video_frame = static_cast<WebRtcVideoFrameAdapter*>(buffer.get())
                        ->getMediaVideoFrame();
      video_frame->set_timestamp(elapsed_timestamp);
      break;
    }
    case webrtc::VideoFrameBuffer::Type::kI420A: {
      const webrtc::I420ABufferInterface* yuva_buffer = buffer->GetI420A();
      video_frame = media::VideoFrame::WrapExternalYuvaData(
          media::PIXEL_FORMAT_I420A, size, gfx::Rect(size), size,
          yuva_buffer->StrideY(), yuva_buffer->StrideU(),
          yuva_buffer->StrideV(), yuva_buffer->StrideA(),
          const_cast<uint8_t*>(yuva_buffer->DataY()),
          const_cast<uint8_t*>(yuva_buffer->DataU()),
          const_cast<uint8_t*>(yuva_buffer->DataV()),
          const_cast<uint8_t*>(yuva_buffer->DataA()), elapsed_timestamp);
      break;
    }
    case webrtc::VideoFrameBuffer::Type::kI420: {
      rtc::scoped_refptr<webrtc::I420BufferInterface> yuv_buffer =
          buffer->ToI420();
      video_frame = media::VideoFrame::WrapExternalYuvData(
          media::PIXEL_FORMAT_I420, size, gfx::Rect(size), size,
          yuv_buffer->StrideY(), yuv_buffer->StrideU(), yuv_buffer->StrideV(),
          const_cast<uint8_t*>(yuv_buffer->DataY()),
          const_cast<uint8_t*>(yuv_buffer->DataU()),
          const_cast<uint8_t*>(yuv_buffer->DataV()), elapsed_timestamp);
      break;
    }
    case webrtc::VideoFrameBuffer::Type::kI444: {
      webrtc::I444BufferInterface* yuv_buffer = buffer->GetI444();
      video_frame = media::VideoFrame::WrapExternalYuvData(
          media::PIXEL_FORMAT_I444, size, gfx::Rect(size), size,
          yuv_buffer->StrideY(), yuv_buffer->StrideU(), yuv_buffer->StrideV(),
          const_cast<uint8_t*>(yuv_buffer->DataY()),
          const_cast<uint8_t*>(yuv_buffer->DataU()),
          const_cast<uint8_t*>(yuv_buffer->DataV()), elapsed_timestamp);
      break;
    }
    case webrtc::VideoFrameBuffer::Type::kI010: {
      webrtc::I010BufferInterface* yuv_buffer = buffer->GetI010();
      // WebRTC defines I010 data as uint16 whereas Chromium uses uint8 for all
      // video formats, so conversion and cast is needed.
      video_frame = media::VideoFrame::WrapExternalYuvData(
          media::PIXEL_FORMAT_YUV420P10, size, gfx::Rect(size), size,
          yuv_buffer->StrideY() * 2, yuv_buffer->StrideU() * 2,
          yuv_buffer->StrideV() * 2,
          const_cast<uint8_t*>(
              reinterpret_cast<const uint8_t*>(yuv_buffer->DataY())),
          const_cast<uint8_t*>(
              reinterpret_cast<const uint8_t*>(yuv_buffer->DataU())),
          const_cast<uint8_t*>(
              reinterpret_cast<const uint8_t*>(yuv_buffer->DataV())),
          elapsed_timestamp);
      break;
    }
    default:
      NOTREACHED();
  }

  if (!video_frame)
    return;

  // The bind ensures that we keep a reference to the underlying buffer.
  if (buffer->type() != webrtc::VideoFrameBuffer::Type::kNative)
    video_frame->AddDestructionObserver(base::BindOnce(&DoNothing, buffer));

  // Rotation may be explicitly set sometimes.
  if (incoming_frame.rotation() != webrtc::kVideoRotation_0) {
    video_frame->metadata()->SetRotation(
        media::VideoFrameMetadata::ROTATION,
        WebRtcToMediaVideoRotation(incoming_frame.rotation()));
  }

  if (incoming_frame.color_space().has_value()) {
    video_frame->set_color_space(
        WebRtcToMediaVideoColorSpace(incoming_frame.color_space().value())
            .ToGfxColorSpace());
  }

  // Run render smoothness algorithm only when we don't have to render
  // immediately.
  if (!render_immediately) {
    video_frame->metadata()->SetTimeTicks(
        media::VideoFrameMetadata::REFERENCE_TIME, render_time);
  }

  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&RemoteVideoSourceDelegate::DoRenderFrameOnIOThread, this,
                     video_frame));
}

void MediaStreamRemoteVideoSource::
RemoteVideoSourceDelegate::DoRenderFrameOnIOThread(
    const scoped_refptr<media::VideoFrame>& video_frame) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("webrtc", "RemoteVideoSourceDelegate::DoRenderFrameOnIOThread");
  // TODO(hclam): Give the estimated capture time.
  frame_callback_.Run(video_frame, base::TimeTicks());
}

MediaStreamRemoteVideoSource::MediaStreamRemoteVideoSource(
    std::unique_ptr<TrackObserver> observer)
    : observer_(std::move(observer)) {
  // The callback will be automatically cleared when 'observer_' goes out of
  // scope and no further callbacks will occur.
  observer_->SetCallback(base::Bind(&MediaStreamRemoteVideoSource::OnChanged,
      base::Unretained(this)));
}

MediaStreamRemoteVideoSource::~MediaStreamRemoteVideoSource() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!observer_);
}

void MediaStreamRemoteVideoSource::OnSourceTerminated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StopSourceImpl();
}

void MediaStreamRemoteVideoSource::StartSourceImpl(
    const VideoCaptureDeliverFrameCB& frame_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!delegate_.get());
  delegate_ = new RemoteVideoSourceDelegate(io_task_runner(), frame_callback);
  scoped_refptr<webrtc::VideoTrackInterface> video_track(
      static_cast<webrtc::VideoTrackInterface*>(observer_->track().get()));
  video_track->AddOrUpdateSink(delegate_.get(), rtc::VideoSinkWants());
  OnStartDone(MEDIA_DEVICE_OK);
}

void MediaStreamRemoteVideoSource::StopSourceImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // StopSourceImpl is called either when MediaStreamTrack.stop is called from
  // JS or blink gc the MediaStreamSource object or when OnSourceTerminated()
  // is called. Garbage collection will happen after the PeerConnection no
  // longer receives the video track.
  if (!observer_)
    return;
  DCHECK(state() != MediaStreamVideoSource::ENDED);
  scoped_refptr<webrtc::VideoTrackInterface> video_track(
      static_cast<webrtc::VideoTrackInterface*>(observer_->track().get()));
  video_track->RemoveSink(delegate_.get());
  // This removes the references to the webrtc video track.
  observer_.reset();
}

rtc::VideoSinkInterface<webrtc::VideoFrame>*
MediaStreamRemoteVideoSource::SinkInterfaceForTesting() {
  return delegate_.get();
}

void MediaStreamRemoteVideoSource::OnChanged(
    webrtc::MediaStreamTrackInterface::TrackState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (state) {
    case webrtc::MediaStreamTrackInterface::kLive:
      SetReadyState(blink::WebMediaStreamSource::kReadyStateLive);
      break;
    case webrtc::MediaStreamTrackInterface::kEnded:
      SetReadyState(blink::WebMediaStreamSource::kReadyStateEnded);
      break;
    default:
      NOTREACHED();
      break;
  }
}

}  // namespace content
