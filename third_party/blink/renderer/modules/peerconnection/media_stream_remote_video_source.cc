// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/peerconnection/media_stream_remote_video_source.h"

#include <stdint.h>

#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/webrtc/convert_to_webrtc_video_frame_buffer.h"
#include "third_party/blink/renderer/platform/webrtc/track_observer.h"
#include "third_party/blink/renderer/platform/webrtc/webrtc_video_frame_adapter.h"
#include "third_party/blink/renderer/platform/webrtc/webrtc_video_utils.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/webrtc/api/video/i420_buffer.h"
#include "third_party/webrtc/api/video/recordable_encoded_frame.h"
#include "third_party/webrtc/rtc_base/time_utils.h"
#include "third_party/webrtc/system_wrappers/include/clock.h"

namespace blink {

namespace {

class WebRtcEncodedVideoFrame : public EncodedVideoFrame {
 public:
  explicit WebRtcEncodedVideoFrame(const webrtc::RecordableEncodedFrame& frame)
      : buffer_(frame.encoded_buffer()),
        codec_(WebRtcToMediaVideoCodec(frame.codec())),
        is_key_frame_(frame.is_key_frame()),
        resolution_(frame.resolution().width, frame.resolution().height) {
    if (frame.color_space()) {
      color_space_ = WebRtcToGfxColorSpace(*frame.color_space());
    }
  }

  base::span<const uint8_t> Data() const override {
    return base::make_span(buffer_->data(), buffer_->size());
  }

  media::VideoCodec Codec() const override { return codec_; }

  bool IsKeyFrame() const override { return is_key_frame_; }

  std::optional<gfx::ColorSpace> ColorSpace() const override {
    return color_space_;
  }

  gfx::Size Resolution() const override { return resolution_; }

 private:
  rtc::scoped_refptr<const webrtc::EncodedImageBufferInterface> buffer_;
  media::VideoCodec codec_;
  bool is_key_frame_;
  std::optional<gfx::ColorSpace> color_space_;
  gfx::Size resolution_;
};

}  // namespace

// Internal class used for receiving frames from the webrtc track on a
// libjingle thread and forward it to the IO-thread.
class MediaStreamRemoteVideoSource::RemoteVideoSourceDelegate
    : public WTF::ThreadSafeRefCounted<RemoteVideoSourceDelegate>,
      public rtc::VideoSinkInterface<webrtc::VideoFrame>,
      public rtc::VideoSinkInterface<webrtc::RecordableEncodedFrame> {
 public:
  RemoteVideoSourceDelegate(
      scoped_refptr<base::SequencedTaskRunner> video_task_runner,
      VideoCaptureDeliverFrameCB new_frame_callback,
      EncodedVideoFrameCB encoded_frame_callback,
      VideoCaptureSubCaptureTargetVersionCB
          sub_capture_target_version_callback);

 protected:
  friend class WTF::ThreadSafeRefCounted<RemoteVideoSourceDelegate>;
  ~RemoteVideoSourceDelegate() override;

  // Implements rtc::VideoSinkInterface used for receiving video frames
  // from the PeerConnection video track. May be called on a libjingle internal
  // thread.
  void OnFrame(const webrtc::VideoFrame& frame) override;

  // VideoSinkInterface<webrtc::RecordableEncodedFrame>
  void OnFrame(const webrtc::RecordableEncodedFrame& frame) override;

  void DoRenderFrameOnIOThread(scoped_refptr<media::VideoFrame> video_frame,
                               base::TimeTicks estimated_capture_time);

 private:
  void OnEncodedVideoFrameOnIO(scoped_refptr<EncodedVideoFrame> frame,
                               base::TimeTicks estimated_capture_time);

  scoped_refptr<base::SequencedTaskRunner> video_task_runner_;

  // |frame_callback_| is accessed on the IO thread.
  VideoCaptureDeliverFrameCB frame_callback_;

  // |encoded_frame_callback_| is accessed on the IO thread.
  EncodedVideoFrameCB encoded_frame_callback_;

  // |sub_capture_target_version_callback| is accessed on the IO thread.
  VideoCaptureSubCaptureTargetVersionCB sub_capture_target_version_callback_;

  // Timestamp of the first received frame.
  std::optional<base::TimeTicks> start_timestamp_;

  // WebRTC real time clock, needed to determine NTP offset.
  raw_ptr<webrtc::Clock> clock_;

  // Offset between NTP clock and WebRTC clock.
  const int64_t ntp_offset_;

  // Determined from a feature flag; if set WebRTC won't forward an unspecified
  // color space.
  const bool ignore_unspecified_color_space_;
};

MediaStreamRemoteVideoSource::RemoteVideoSourceDelegate::
    RemoteVideoSourceDelegate(
        scoped_refptr<base::SequencedTaskRunner> video_task_runner,
        VideoCaptureDeliverFrameCB new_frame_callback,
        EncodedVideoFrameCB encoded_frame_callback,
        VideoCaptureSubCaptureTargetVersionCB
            sub_capture_target_version_callback)
    : video_task_runner_(video_task_runner),
      frame_callback_(std::move(new_frame_callback)),
      encoded_frame_callback_(std::move(encoded_frame_callback)),
      sub_capture_target_version_callback_(
          std::move(sub_capture_target_version_callback)),
      clock_(webrtc::Clock::GetRealTimeClock()),
      ntp_offset_(clock_->TimeInMilliseconds() -
                  clock_->CurrentNtpInMilliseconds()),
      ignore_unspecified_color_space_(base::FeatureList::IsEnabled(
          features::kWebRtcIgnoreUnspecifiedColorSpace)) {}

MediaStreamRemoteVideoSource::RemoteVideoSourceDelegate::
    ~RemoteVideoSourceDelegate() = default;

void MediaStreamRemoteVideoSource::RemoteVideoSourceDelegate::OnFrame(
    const webrtc::VideoFrame& incoming_frame) {
  const webrtc::VideoFrame::RenderParameters render_parameters =
      incoming_frame.render_parameters();
  const bool render_immediately = render_parameters.use_low_latency_rendering ||
                                  incoming_frame.timestamp_us() == 0;

  const base::TimeTicks current_time = base::TimeTicks::Now();
  const base::TimeTicks render_time =
      render_immediately
          ? current_time
          : base::TimeTicks() +
                base::Microseconds(incoming_frame.timestamp_us());
  if (!start_timestamp_)
    start_timestamp_ = render_time;
  const base::TimeDelta elapsed_timestamp = render_time - *start_timestamp_;
  TRACE_EVENT2("webrtc", "RemoteVideoSourceDelegate::RenderFrame",
               "Ideal Render Instant", render_time.ToInternalValue(),
               "Timestamp", elapsed_timestamp.InMicroseconds());

  rtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer =
      incoming_frame.video_frame_buffer();
  scoped_refptr<media::VideoFrame> video_frame;
  if (buffer->type() == webrtc::VideoFrameBuffer::Type::kNative) {
    video_frame = static_cast<WebRtcVideoFrameAdapter*>(buffer.get())
                      ->getMediaVideoFrame();
    video_frame->set_timestamp(elapsed_timestamp);
  } else {
    video_frame =
        ConvertFromMappedWebRtcVideoFrameBuffer(buffer, elapsed_timestamp);
  }
  if (!video_frame)
    return;

  // Rotation may be explicitly set sometimes.
  if (incoming_frame.rotation() != webrtc::kVideoRotation_0) {
    video_frame->metadata().transformation =
        WebRtcToMediaVideoRotation(incoming_frame.rotation());
  }

  // The second clause of the condition is controlled by the feature flag
  // WebRtcIgnoreUnspecifiedColorSpace. If the feature is enabled we won't try
  // to guess a color space if the webrtc::ColorSpace is unspecified. If the
  // feature is disabled (default), an unspecified color space will get
  // converted into a gfx::ColorSpace set to BT709.
  if (incoming_frame.color_space() &&
      !(ignore_unspecified_color_space_ &&
        incoming_frame.color_space()->primaries() ==
            webrtc::ColorSpace::PrimaryID::kUnspecified &&
        incoming_frame.color_space()->transfer() ==
            webrtc::ColorSpace::TransferID::kUnspecified &&
        incoming_frame.color_space()->matrix() ==
            webrtc::ColorSpace::MatrixID::kUnspecified)) {
    video_frame->set_color_space(
        WebRtcToGfxColorSpace(*incoming_frame.color_space()));
  }

  // Run render smoothness algorithm only when we don't have to render
  // immediately.
  if (!render_immediately)
    video_frame->metadata().reference_time = render_time;

  if (render_parameters.max_composition_delay_in_frames) {
    video_frame->metadata().maximum_composition_delay_in_frames =
        render_parameters.max_composition_delay_in_frames;
  }

  video_frame->metadata().decode_end_time = current_time;

  // RTP_TIMESTAMP, PROCESSING_TIME, and CAPTURE_BEGIN_TIME are all exposed
  // through the JavaScript callback mechanism
  // video.requestVideoFrameCallback().
  video_frame->metadata().rtp_timestamp =
      static_cast<double>(incoming_frame.rtp_timestamp());

  if (incoming_frame.processing_time()) {
    video_frame->metadata().processing_time =
        base::Microseconds(incoming_frame.processing_time()->Elapsed().us());
  }

  // Set capture time to the NTP time, which is the estimated capture time
  // converted to the local clock.
  if (incoming_frame.ntp_time_ms() > 0) {
    video_frame->metadata().capture_begin_time =
        base::TimeTicks() +
        base::Milliseconds(incoming_frame.ntp_time_ms() + ntp_offset_);
  }

  // Set receive time to arrival of last packet.
  if (!incoming_frame.packet_infos().empty()) {
    webrtc::Timestamp last_packet_arrival =
        std::max_element(
            incoming_frame.packet_infos().cbegin(),
            incoming_frame.packet_infos().cend(),
            [](const webrtc::RtpPacketInfo& a, const webrtc::RtpPacketInfo& b) {
              return a.receive_time() < b.receive_time();
            })
            ->receive_time();
    video_frame->metadata().receive_time =
        base::TimeTicks() + base::Microseconds(last_packet_arrival.us());
    base::UmaHistogramTimes(
        "WebRTC.Video.TotalReceiveDelay",
        current_time - *video_frame->metadata().receive_time);
  }

  // Use our computed render time as estimated capture time. If timestamp_us()
  // (which is actually the suggested render time) is set by WebRTC, it's based
  // on the RTP timestamps in the frame's packets, so congruent with the
  // received frame capture timestamps. If set by us, it's as congruent as we
  // can get with the timestamp sequence of frames we received.
  PostCrossThreadTask(
      *video_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&RemoteVideoSourceDelegate::DoRenderFrameOnIOThread,
                          WrapRefCounted(this), video_frame, render_time));
}

void MediaStreamRemoteVideoSource::RemoteVideoSourceDelegate::
    DoRenderFrameOnIOThread(scoped_refptr<media::VideoFrame> video_frame,
                            base::TimeTicks estimated_capture_time) {
  DCHECK(video_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("webrtc", "RemoteVideoSourceDelegate::DoRenderFrameOnIOThread");
  frame_callback_.Run(std::move(video_frame), estimated_capture_time);
}

void MediaStreamRemoteVideoSource::RemoteVideoSourceDelegate::OnFrame(
    const webrtc::RecordableEncodedFrame& frame) {
  const bool render_immediately = frame.render_time().us() == 0;
  const base::TimeTicks current_time = base::TimeTicks::Now();
  const base::TimeTicks render_time =
      render_immediately
          ? current_time
          : base::TimeTicks() + base::Microseconds(frame.render_time().us());

  // Use our computed render time as estimated capture time. If render_time()
  // is set by WebRTC, it's based on the RTP timestamps in the frame's packets,
  // so congruent with the received frame capture timestamps. If set by us, it's
  // as congruent as we can get with the timestamp sequence of frames we
  // received.
  PostCrossThreadTask(
      *video_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&RemoteVideoSourceDelegate::OnEncodedVideoFrameOnIO,
                          WrapRefCounted(this),
                          base::MakeRefCounted<WebRtcEncodedVideoFrame>(frame),
                          render_time));
}

void MediaStreamRemoteVideoSource::RemoteVideoSourceDelegate::
    OnEncodedVideoFrameOnIO(scoped_refptr<EncodedVideoFrame> frame,
                            base::TimeTicks estimated_capture_time) {
  DCHECK(video_task_runner_->RunsTasksInCurrentSequence());
  encoded_frame_callback_.Run(std::move(frame), estimated_capture_time);
}

MediaStreamRemoteVideoSource::MediaStreamRemoteVideoSource(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    std::unique_ptr<TrackObserver> observer)
    : MediaStreamVideoSource(std::move(task_runner)),
      observer_(std::move(observer)) {
  // The callback will be automatically cleared when 'observer_' goes out of
  // scope and no further callbacks will occur.
  observer_->SetCallback(WTF::BindRepeating(
      &MediaStreamRemoteVideoSource::OnChanged, WTF::Unretained(this)));
}

MediaStreamRemoteVideoSource::~MediaStreamRemoteVideoSource() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!observer_);
}

void MediaStreamRemoteVideoSource::OnSourceTerminated() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  StopSourceImpl();
}

void MediaStreamRemoteVideoSource::StartSourceImpl(
    VideoCaptureDeliverFrameCB frame_callback,
    EncodedVideoFrameCB encoded_frame_callback,
    VideoCaptureSubCaptureTargetVersionCB sub_capture_target_version_callback,
    // The remote track does not not report frame drops.
    VideoCaptureNotifyFrameDroppedCB) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!delegate_.get());
  delegate_ = base::MakeRefCounted<RemoteVideoSourceDelegate>(
      video_task_runner(), std::move(frame_callback),
      std::move(encoded_frame_callback),
      std::move(sub_capture_target_version_callback));
  scoped_refptr<webrtc::VideoTrackInterface> video_track(
      static_cast<webrtc::VideoTrackInterface*>(observer_->track().get()));
  video_track->AddOrUpdateSink(delegate_.get(), rtc::VideoSinkWants());
  OnStartDone(mojom::MediaStreamRequestResult::OK);
}

void MediaStreamRemoteVideoSource::StopSourceImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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

rtc::VideoSinkInterface<webrtc::RecordableEncodedFrame>*
MediaStreamRemoteVideoSource::EncodedSinkInterfaceForTesting() {
  return delegate_.get();
}

void MediaStreamRemoteVideoSource::OnChanged(
    webrtc::MediaStreamTrackInterface::TrackState state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  switch (state) {
    case webrtc::MediaStreamTrackInterface::kLive:
      SetReadyState(WebMediaStreamSource::kReadyStateLive);
      break;
    case webrtc::MediaStreamTrackInterface::kEnded:
      SetReadyState(WebMediaStreamSource::kReadyStateEnded);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

bool MediaStreamRemoteVideoSource::SupportsEncodedOutput() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!observer_ || !observer_->track()) {
    return false;
  }
  scoped_refptr<webrtc::VideoTrackInterface> video_track(
      static_cast<webrtc::VideoTrackInterface*>(observer_->track().get()));
  return video_track->GetSource()->SupportsEncodedOutput();
}

void MediaStreamRemoteVideoSource::RequestKeyFrame() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!observer_ || !observer_->track()) {
    return;
  }
  scoped_refptr<webrtc::VideoTrackInterface> video_track(
      static_cast<webrtc::VideoTrackInterface*>(observer_->track().get()));
  if (video_track->GetSource()) {
    video_track->GetSource()->GenerateKeyFrame();
  }
}

base::WeakPtr<MediaStreamVideoSource>
MediaStreamRemoteVideoSource::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void MediaStreamRemoteVideoSource::OnEncodedSinkEnabled() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!observer_ || !observer_->track()) {
    return;
  }
  scoped_refptr<webrtc::VideoTrackInterface> video_track(
      static_cast<webrtc::VideoTrackInterface*>(observer_->track().get()));
  video_track->GetSource()->AddEncodedSink(delegate_.get());
}

void MediaStreamRemoteVideoSource::OnEncodedSinkDisabled() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!observer_ || !observer_->track()) {
    return;
  }
  scoped_refptr<webrtc::VideoTrackInterface> video_track(
      static_cast<webrtc::VideoTrackInterface*>(observer_->track().get()));
  video_track->GetSource()->RemoveEncodedSink(delegate_.get());
}

}  // namespace blink
