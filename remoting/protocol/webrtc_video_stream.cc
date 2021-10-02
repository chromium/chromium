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
#include "remoting/protocol/webrtc_dummy_video_encoder.h"
#include "remoting/protocol/webrtc_frame_scheduler_simple.h"
#include "remoting/protocol/webrtc_transport.h"
#include "remoting/protocol/webrtc_video_frame_adapter.h"
#include "remoting/protocol/webrtc_video_track_source.h"
#include "third_party/webrtc/api/media_stream_interface.h"
#include "third_party/webrtc/api/notifier.h"
#include "third_party/webrtc/api/peer_connection_interface.h"
#include "third_party/webrtc/api/video_codecs/vp9_profile.h"

#if defined(USE_H264_ENCODER)
#include "remoting/codec/webrtc_video_encoder_gpu.h"
#endif

namespace remoting {
namespace protocol {

namespace {

const char kStreamLabel[] = "screen_stream";
const char kVideoLabel[] = "screen_video";

}  // namespace

struct WebrtcVideoStream::FrameStats : public WebrtcVideoEncoder::FrameStats {
  FrameStats() = default;
  FrameStats(const FrameStats&) = default;
  FrameStats& operator=(const FrameStats&) = default;
  ~FrameStats() override = default;

  // The input-event fields are non-null only for one frame after each
  // incoming input event.
  InputEventTimestamps input_event_timestamps;

  base::TimeDelta capture_delay;

  uint32_t capturer_id = 0;
};

WebrtcVideoStream::WebrtcVideoStream(const SessionOptions& session_options)
    : video_stats_dispatcher_(kStreamLabel), session_options_(session_options) {
  // WeakPtr can't be used here, as the Create...() methods return a value
  // which would be undefined if the pointer were invalidated. But
  // Unretained(this) is safe because |encoder_selector_| is owned by this
  // object.
  encoder_selector_.RegisterEncoder(
      base::BindRepeating(&WebrtcVideoEncoderVpx::IsSupportedByVP8),
      base::BindRepeating(&WebrtcVideoStream::CreateVP8Encoder,
                          base::Unretained(this)));
  encoder_selector_.RegisterEncoder(
      base::BindRepeating(&WebrtcVideoEncoderVpx::IsSupportedByVP9),
      base::BindRepeating(&WebrtcVideoStream::CreateVP9Encoder,
                          base::Unretained(this),
                          /*lossless_color=*/false));
  encoder_selector_.RegisterEncoder(
      base::BindRepeating(&WebrtcVideoEncoderVpx::IsSupportedByVP9),
      base::BindRepeating(&WebrtcVideoStream::CreateVP9Encoder,
                          base::Unretained(this),
                          /*lossless_color=*/true));
#if defined(USE_H264_ENCODER)
  encoder_selector_.RegisterEncoder(
      base::BindRepeating(&WebrtcVideoEncoderGpu::IsSupportedByH264),
      base::BindRepeating(&WebrtcVideoEncoderGpu::CreateForH264));
#endif
}

WebrtcVideoStream::~WebrtcVideoStream() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void WebrtcVideoStream::Start(
    std::unique_ptr<webrtc::DesktopCapturer> desktop_capturer,
    WebrtcTransport* webrtc_transport,
    WebrtcDummyVideoEncoderFactory* video_encoder_factory,
    scoped_refptr<base::SequencedTaskRunner> encode_task_runner) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(desktop_capturer);
  DCHECK(webrtc_transport);
  DCHECK(video_encoder_factory);
  DCHECK(encode_task_runner);

  scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory(
      webrtc_transport->peer_connection_factory());
  peer_connection_ = webrtc_transport->peer_connection();
  DCHECK(peer_connection_factory);
  DCHECK(peer_connection_);

  encode_task_runner_ = std::move(encode_task_runner);
  capturer_ = std::move(desktop_capturer);
  video_encoder_factory_ = video_encoder_factory;

  video_encoder_factory->RegisterEncoderSelectedCallback(base::BindRepeating(
      &WebrtcVideoStream::OnEncoderCreated, weak_factory_.GetWeakPtr()));

  capturer_->Start(this);

  video_track_source_ =
      new rtc::RefCountedObject<WebrtcVideoTrackSource>(base::BindRepeating(
          &WebrtcVideoStream::OnEncoderReady, weak_factory_.GetWeakPtr()));
  rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track =
      peer_connection_factory->CreateVideoTrack(kVideoLabel,
                                                video_track_source_);

  webrtc::RtpTransceiverInit init;
  init.stream_ids = {kStreamLabel};

  // value() DCHECKs if AddTransceiver() fails, which only happens if a track
  // was already added with the stream label.
  auto transceiver =
      peer_connection_->AddTransceiver(video_track, init).value();

  webrtc_transport->OnVideoTransceiverCreated(transceiver);

  video_encoder_factory->SetVideoChannelStateObserver(
      weak_factory_.GetWeakPtr());
  scheduler_ = std::make_unique<WebrtcFrameSchedulerSimple>(session_options_);
  scheduler_->Start(base::BindRepeating(&WebrtcVideoStream::CaptureNextFrame,
                                        base::Unretained(this)));

  video_stats_dispatcher_.Init(webrtc_transport->CreateOutgoingChannel(
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
  NOTIMPLEMENTED() << "Changing lossless-color for VP9 requires SDP "
                      "offer/answer exchange.";
}

void WebrtcVideoStream::SetObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observer_ = observer;
}

void WebrtcVideoStream::OnEncoderReady() {
  DCHECK(thread_checker_.CalledOnValidThread());
  scheduler_->OnEncoderReady();
}

void WebrtcVideoStream::OnKeyFrameRequested() {
  DCHECK(thread_checker_.CalledOnValidThread());
  scheduler_->OnKeyFrameRequested();
}

void WebrtcVideoStream::OnTargetBitrateChanged(int bitrate_kbps) {
  DCHECK(thread_checker_.CalledOnValidThread());
  scheduler_->OnTargetBitrateChanged(bitrate_kbps);
}

void WebrtcVideoStream::OnRttUpdate(base::TimeDelta rtt) {
  DCHECK(thread_checker_.CalledOnValidThread());
  scheduler_->OnRttUpdate(rtt);
}

void WebrtcVideoStream::OnTopOffActive(bool active) {
  DCHECK(thread_checker_.CalledOnValidThread());
  scheduler_->OnTopOffActive(active);
}

void WebrtcVideoStream::OnCaptureResult(
    webrtc::DesktopCapturer::Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  DCHECK(thread_checker_.CalledOnValidThread());

  current_frame_stats_->capture_ended_time = base::TimeTicks::Now();
  current_frame_stats_->capture_delay =
      base::Milliseconds(frame ? frame->capture_time_ms() : 0);

  if (!frame) {
    scheduler_->OnFrameCaptured(nullptr, nullptr);
    return;
  }

  // TODO(sergeyu): Handle ERROR_PERMANENT result here.
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

  WebrtcVideoEncoder::FrameParams frame_params;
  if (!scheduler_->OnFrameCaptured(frame.get(), &frame_params)) {
    return;
  }

  // Send the captured frame to the registered sink, if any. WebRTC will route
  // this to the appropriate encoder.
  video_track_source_->SendCapturedFrame(std::move(frame),
                                         std::move(current_frame_stats_));
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

  current_frame_stats_ = std::make_unique<FrameStats>();
  current_frame_stats_->capture_started_time = base::TimeTicks::Now();
  current_frame_stats_->input_event_timestamps =
      event_timestamps_source_->TakeLastEventTimestamps();

  capturer_->CaptureFrame();
}

void WebrtcVideoStream::OnFrameEncoded(
    WebrtcVideoEncoder::EncodeResult encode_result,
    WebrtcVideoEncoder::EncodedFrame* frame) {
  DCHECK(thread_checker_.CalledOnValidThread());

  scheduler_->OnFrameEncoded(encode_result, frame);
}

void WebrtcVideoStream::OnEncodedFrameSent(
    webrtc::EncodedImageCallback::Result result,
    const WebrtcVideoEncoder::EncodedFrame& frame) {
  if (result.error != webrtc::EncodedImageCallback::Result::OK) {
    // TODO(sergeyu): Stop the stream.
    LOG(ERROR) << "Failed to send video frame.";
    return;
  }

  // Send FrameStats message.
  if (video_stats_dispatcher_.is_connected()) {
    // The down-cast is safe, because the |stats| object was originally created
    // by this class and attached to the frame.
    const auto* current_frame_stats =
        static_cast<const FrameStats*>(frame.stats.get());
    DCHECK(current_frame_stats);

    HostFrameStats stats;

    // Get bandwidth, RTT and send_pending_delay into |stats|.
    scheduler_->GetSchedulerStats(stats);

    stats.frame_size = frame.data.size();

    if (!current_frame_stats->input_event_timestamps.is_null()) {
      stats.capture_pending_delay =
          current_frame_stats->capture_started_time -
          current_frame_stats->input_event_timestamps.host_timestamp;
      stats.latest_event_timestamp =
          current_frame_stats->input_event_timestamps.client_timestamp;
    }

    stats.capture_delay = current_frame_stats->capture_delay;

    // Total overhead time for IPC and threading when capturing frames.
    stats.capture_overhead_delay = (current_frame_stats->capture_ended_time -
                                    current_frame_stats->capture_started_time) -
                                   stats.capture_delay;

    stats.encode_pending_delay = current_frame_stats->encode_started_time -
                                 current_frame_stats->capture_ended_time;

    stats.encode_delay = current_frame_stats->encode_ended_time -
                         current_frame_stats->encode_started_time;

    stats.capturer_id = current_frame_stats->capturer_id;

    // Convert the frame quantizer to a measure of frame quality between 0 and
    // 100, for a simple visualization of quality over time. The quantizer from
    // VP8/VP9 encoder lies within 0-63, with 0 representing a lossless
    // frame.
    // TODO(crbug.com/891571): Remove |quantizer| from the WebrtcVideoEncoder
    // interface, and move this logic to the encoders.
    stats.frame_quality = (63 - frame.quantizer) * 100 / 63;

    video_stats_dispatcher_.OnVideoFrameStats(result.frame_id, stats);
  }
}

void WebrtcVideoStream::EncodeCallback(
    WebrtcVideoEncoder::EncodeResult encode_result,
    std::unique_ptr<WebrtcVideoEncoder::EncodedFrame> frame) {
  OnFrameEncoded(encode_result, frame.get());
}

void WebrtcVideoStream::OnEncoderCreated(
    webrtc::VideoCodecType codec_type,
    const webrtc::SdpVideoFormat::Parameters& parameters) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // TODO(crbug.com/1192865): Remove this method (and the callback
  // machinery that calls it), as this class no longer creates encoders.
  // The encoder-selector, and the encoder-creation methods can also be
  // removed.

  // This method is called when SDP has been negotiated and WebRTC has
  // created a preferred encoder via the WebrtcDummyVideoEncoderFactory
  // implementation.
  // Trigger recreating the encoder in case a previous one was being used and
  // SDP renegotiation selected a different one. The proper encoder will be
  // created after the next frame is captured.
  // Note that this flag controls the encoder created by this class, and not
  // the encoder that was just "created" by WebRTC.
  // An optimization would be to do this only if the new encoder is different
  // from the current one. However, SDP renegotiation is expected to occur
  // infrequently (only when the user changes a setting), and should typically
  // not cause the same codec to be repeatedly selected.
  recreate_encoder_ = true;

  // The preferred codec id depends on the order of
  // |encoder_selector_|.RegisterEncoder().
  if (codec_type == webrtc::kVideoCodecVP8) {
    VLOG(0) << "VP8 video codec is preferred.";
    encoder_selector_.SetPreferredCodec(0);
  } else if (codec_type == webrtc::kVideoCodecVP9) {
    encoder_selector_.SetPreferredCodec(1);
    const auto iter = parameters.find(webrtc::kVP9FmtpProfileId);
    bool losslessColor = iter != parameters.end() && iter->second == "1";
    VLOG(0) << "VP9 video codec is preferred, losslessColor="
            << (losslessColor ? "true" : "false");
    encoder_selector_.SetPreferredCodec(losslessColor ? 2 : 1);
  } else if (codec_type == webrtc::kVideoCodecH264) {
#if defined(USE_H264_ENCODER)
    VLOG(0) << "H264 video codec is preferred.";
    encoder_selector_.SetPreferredCodec(3);
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

std::unique_ptr<WebrtcVideoEncoder> WebrtcVideoStream::CreateVP9Encoder(
    bool lossless_color) {
  auto vp9encoder = WebrtcVideoEncoderVpx::CreateForVP9();
  vp9encoder->SetLosslessColor(lossless_color);
  return std::make_unique<WebrtcVideoEncoderProxy>(std::move(vp9encoder),
                                                   encode_task_runner_);
}

}  // namespace protocol
}  // namespace remoting
