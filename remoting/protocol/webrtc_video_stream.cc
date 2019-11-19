// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_video_stream.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "remoting/base/constants.h"
#include "remoting/codec/webrtc_video_encoder_proxy.h"
#include "remoting/codec/webrtc_video_encoder_vpx.h"
#include "remoting/protocol/frame_stats.h"
#include "remoting/protocol/host_video_stats_dispatcher.h"
#include "remoting/protocol/webrtc_frame_scheduler_simple.h"
#include "remoting/protocol/webrtc_transport.h"
#include "third_party/webrtc/api/media_stream_interface.h"
#include "third_party/webrtc/api/notifier.h"
#include "third_party/webrtc/api/peer_connection_interface.h"

#if defined(USE_H264_ENCODER)
#include "remoting/codec/webrtc_video_encoder_gpu.h"
#endif

namespace remoting {
namespace protocol {

namespace {

const char kStreamLabel[] = "screen_stream";
const char kVideoLabel[] = "screen_video";

std::string EncodeResultToString(WebrtcVideoEncoder::EncodeResult result) {
  using EncodeResult = WebrtcVideoEncoder::EncodeResult;

  switch (result) {
    case EncodeResult::SUCCEEDED:
      return "Succeeded";
    case EncodeResult::FRAME_SIZE_EXCEEDS_CAPABILITY:
      return "Frame size exceeds capability";
    case EncodeResult::UNKNOWN_ERROR:
      return "Unknown error";
  }
  NOTREACHED();
  return "";
}

class DummyVideoTrackSource
    : public webrtc::Notifier<webrtc::VideoTrackSourceInterface> {
 public:
  SourceState state() const override { return kLive; }
  bool remote() const override { return false; }
  bool is_screencast() const override { return true; }
  absl::optional<bool> needs_denoising() const override {
    return absl::nullopt;
  }
  bool GetStats(Stats* stats) override { return false; }
  void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink,
                       const rtc::VideoSinkWants& wants) override {}
  void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) override {}
};

}  // namespace

struct WebrtcVideoStream::FrameStats {
  // The following fields are non-null only for one frame after each incoming
  // input event.
  InputEventTimestamps input_event_timestamps;

  base::TimeTicks capture_started_time;
  base::TimeTicks capture_ended_time;
  base::TimeDelta capture_delay;
  base::TimeTicks encode_started_time;
  base::TimeTicks encode_ended_time;

  uint32_t capturer_id = 0;
  int frame_quality = -1;
};

WebrtcVideoStream::WebrtcVideoStream(const SessionOptions& session_options)
    : video_stats_dispatcher_(kStreamLabel), session_options_(session_options) {
  // WeakPtr can't be used here, as the Create...() methods return a value
  // which would be undefined if the pointer were invalidated. But
  // Unretained(this) is safe because |encoder_selector_| is owned by this
  // object.
  encoder_selector_.RegisterEncoder(
      base::Bind(&WebrtcVideoEncoderVpx::IsSupportedByVP8),
      base::Bind(&WebrtcVideoStream::CreateVP8Encoder, base::Unretained(this)));
  encoder_selector_.RegisterEncoder(
      base::Bind(&WebrtcVideoEncoderVpx::IsSupportedByVP9),
      base::Bind(&WebrtcVideoStream::CreateVP9Encoder, base::Unretained(this)));
#if defined(USE_H264_ENCODER)
  encoder_selector_.RegisterEncoder(
      base::Bind(&WebrtcVideoEncoderGpu::IsSupportedByH264),
      base::Bind(&WebrtcVideoEncoderGpu::CreateForH264));
#endif
}

WebrtcVideoStream::~WebrtcVideoStream() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void WebrtcVideoStream::Start(
    std::unique_ptr<webrtc::DesktopCapturer> desktop_capturer,
    WebrtcTransport* webrtc_transport,
    scoped_refptr<base::SequencedTaskRunner> encode_task_runner) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(webrtc_transport);
  DCHECK(desktop_capturer);
  DCHECK(encode_task_runner);

  scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory(
      webrtc_transport->peer_connection_factory());
  peer_connection_ = webrtc_transport->peer_connection();
  DCHECK(peer_connection_factory);
  DCHECK(peer_connection_);

  encode_task_runner_ = std::move(encode_task_runner);
  capturer_ = std::move(desktop_capturer);
  webrtc_transport_ = webrtc_transport;

  webrtc_transport_->video_encoder_factory()->RegisterEncoderSelectedCallback(
      base::Bind(&WebrtcVideoStream::OnEncoderCreated,
                 weak_factory_.GetWeakPtr()));

  capturer_->Start(this);

  rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> src =
      new rtc::RefCountedObject<DummyVideoTrackSource>();
  rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track =
      peer_connection_factory->CreateVideoTrack(kVideoLabel, src);

  webrtc::RtpTransceiverInit init;
  init.stream_ids = {kStreamLabel};

  // value() DCHECKs if AddTransceiver() fails, which only happens if a track
  // was already added with the stream label.
  auto transceiver =
      peer_connection_->AddTransceiver(video_track, init).value();

  webrtc_transport_->OnVideoTransceiverCreated(transceiver);

  scheduler_.reset(new WebrtcFrameSchedulerSimple(session_options_));
  scheduler_->Start(
      webrtc_transport_->video_encoder_factory(),
      base::Bind(&WebrtcVideoStream::CaptureNextFrame, base::Unretained(this)));

  video_stats_dispatcher_.Init(webrtc_transport_->CreateOutgoingChannel(
                                   video_stats_dispatcher_.channel_name()),
                               this);
}

void WebrtcVideoStream::SelectSource(int id) {
  capturer_->SelectSource(id);
}

void WebrtcVideoStream::SetEventTimestampsSource(
    scoped_refptr<InputEventTimestampsSource> event_timestamps_source) {
  event_timestamps_source_ = event_timestamps_source;
}

void WebrtcVideoStream::Pause(bool pause) {
  DCHECK(thread_checker_.CalledOnValidThread());
  scheduler_->Pause(pause);
}

void WebrtcVideoStream::SetLosslessEncode(bool want_lossless) {
  lossless_encode_ = want_lossless;
  if (encoder_) {
    encoder_->SetLosslessEncode(want_lossless);
  }
}

void WebrtcVideoStream::SetLosslessColor(bool want_lossless) {
  lossless_color_ = want_lossless;
  if (encoder_) {
    encoder_->SetLosslessColor(want_lossless);
  }
}

void WebrtcVideoStream::SetObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observer_ = observer;
}

void WebrtcVideoStream::OnCaptureResult(
    webrtc::DesktopCapturer::Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  DCHECK(thread_checker_.CalledOnValidThread());

  current_frame_stats_->capture_ended_time = base::TimeTicks::Now();
  current_frame_stats_->capture_delay =
      base::TimeDelta::FromMilliseconds(frame ? frame->capture_time_ms() : 0);

  WebrtcVideoEncoder::FrameParams frame_params;
  if (!scheduler_->OnFrameCaptured(frame.get(), &frame_params)) {
    return;
  }

  // TODO(sergeyu): Handle ERROR_PERMANENT result here.
  if (frame) {
    webrtc::DesktopVector dpi =
        frame->dpi().is_zero() ? webrtc::DesktopVector(kDefaultDpi, kDefaultDpi)
                               : frame->dpi();

    if (!frame_size_.equals(frame->size()) || !frame_dpi_.equals(dpi)) {
      frame_size_ = frame->size();
      frame_dpi_ = dpi;
      if (observer_)
        observer_->OnVideoSizeChanged(this, frame_size_, frame_dpi_);
    }

    current_frame_stats_->capturer_id = frame->capturer_id();

    if (!encoder_) {
      encoder_selector_.SetDesktopFrame(*frame);
      encoder_ = encoder_selector_.CreateEncoder();
      encoder_->SetLosslessEncode(lossless_encode_);
      encoder_->SetLosslessColor(lossless_color_);

      // TODO(zijiehe): Permanently stop the video stream if we cannot create an
      // encoder for the |frame|.
    }
  }

  if (encoder_) {
    current_frame_stats_->encode_started_time = base::TimeTicks::Now();
    encoder_->Encode(
        std::move(frame), frame_params,
        base::Bind(&WebrtcVideoStream::OnFrameEncoded, base::Unretained(this)));
  }
}

void WebrtcVideoStream::OnChannelInitialized(
    ChannelDispatcherBase* channel_dispatcher) {
  DCHECK(&video_stats_dispatcher_ == channel_dispatcher);
}
void WebrtcVideoStream::OnChannelClosed(
    ChannelDispatcherBase* channel_dispatcher) {
  DCHECK(&video_stats_dispatcher_ == channel_dispatcher);
  LOG(WARNING) << "video_stats channel was closed.";
}

void WebrtcVideoStream::CaptureNextFrame() {
  DCHECK(thread_checker_.CalledOnValidThread());

  current_frame_stats_.reset(new FrameStats());
  current_frame_stats_->capture_started_time = base::TimeTicks::Now();
  current_frame_stats_->input_event_timestamps =
      event_timestamps_source_->TakeLastEventTimestamps();

  capturer_->CaptureFrame();
}

void WebrtcVideoStream::OnFrameEncoded(
    WebrtcVideoEncoder::EncodeResult encode_result,
    std::unique_ptr<WebrtcVideoEncoder::EncodedFrame> frame) {
  DCHECK(thread_checker_.CalledOnValidThread());

  current_frame_stats_->encode_ended_time = base::TimeTicks::Now();

  // Convert the frame quantizer to a measure of frame quality between 0 and
  // 100, for a simple visualization of quality over time. The quantizer from
  // VP8/VP9 encoder lies within 0-63, with 0 representing a lossless
  // frame.
  // TODO(crbug.com/891571): Remove |quantizer| from the WebrtcVideoEncoder
  // interface, and move this logic to the encoders.
  current_frame_stats_->frame_quality = (63 - frame->quantizer) * 100 / 63;

  HostFrameStats stats;
  scheduler_->OnFrameEncoded(frame.get(), &stats);

  if (encode_result != WebrtcVideoEncoder::EncodeResult::SUCCEEDED) {
    LOG(ERROR) << "Video encoder returns error "
               << EncodeResultToString(encode_result);
    // TODO(zijiehe): Restart the video stream.
    encoder_.reset();
    return;
  }

  if (!frame) {
    return;
  }

  webrtc::EncodedImageCallback::Result result =
      webrtc_transport_->video_encoder_factory()->SendEncodedFrame(
          *frame, current_frame_stats_->capture_started_time,
          current_frame_stats_->encode_started_time,
          current_frame_stats_->encode_ended_time);
  if (result.error != webrtc::EncodedImageCallback::Result::OK) {
    // TODO(sergeyu): Stop the stream.
    LOG(ERROR) << "Failed to send video frame.";
    return;
  }

  // Send FrameStats message.
  if (video_stats_dispatcher_.is_connected()) {
    stats.frame_size = frame ? frame->data.size() : 0;

    if (!current_frame_stats_->input_event_timestamps.is_null()) {
      stats.capture_pending_delay =
          current_frame_stats_->capture_started_time -
          current_frame_stats_->input_event_timestamps.host_timestamp;
      stats.latest_event_timestamp =
          current_frame_stats_->input_event_timestamps.client_timestamp;
    }

    stats.capture_delay = current_frame_stats_->capture_delay;

    // Total overhead time for IPC and threading when capturing frames.
    stats.capture_overhead_delay =
        (current_frame_stats_->capture_ended_time -
         current_frame_stats_->capture_started_time) -
        stats.capture_delay;

    stats.encode_pending_delay = current_frame_stats_->encode_started_time -
                                 current_frame_stats_->capture_ended_time;

    stats.encode_delay = current_frame_stats_->encode_ended_time -
                         current_frame_stats_->encode_started_time;

    stats.capturer_id = current_frame_stats_->capturer_id;

    stats.frame_quality = current_frame_stats_->frame_quality;

    video_stats_dispatcher_.OnVideoFrameStats(result.frame_id, stats);
  }
}

void WebrtcVideoStream::OnEncoderCreated(webrtc::VideoCodecType codec_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // The preferred codec id depends on the order of
  // |encoder_selector_|.RegisterEncoder().
  if (codec_type == webrtc::kVideoCodecVP8) {
    LOG(WARNING) << "VP8 video codec is preferred.";
    encoder_selector_.SetPreferredCodec(0);
  } else if (codec_type == webrtc::kVideoCodecVP9) {
    LOG(WARNING) << "VP9 video codec is preferred.";
    encoder_selector_.SetPreferredCodec(1);
  } else if (codec_type == webrtc::kVideoCodecH264) {
#if defined(USE_H264_ENCODER)
    LOG(WARNING) << "H264 video codec is preferred.";
    encoder_selector_.SetPreferredCodec(2);
#else
    NOTIMPLEMENTED();
#endif
  } else {
    LOG(FATAL) << "Unknown codec type: " << codec_type;
  }
}

std::unique_ptr<WebrtcVideoEncoder> WebrtcVideoStream::CreateVP8Encoder() {
  return std::make_unique<WebrtcVideoEncoderProxy>(
      WebrtcVideoEncoderVpx::CreateForVP8(), encode_task_runner_);
}

std::unique_ptr<WebrtcVideoEncoder> WebrtcVideoStream::CreateVP9Encoder() {
  return std::make_unique<WebrtcVideoEncoderProxy>(
      WebrtcVideoEncoderVpx::CreateForVP9(), encode_task_runner_);
}

}  // namespace protocol
}  // namespace remoting
