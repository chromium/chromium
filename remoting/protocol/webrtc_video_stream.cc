// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_video_stream.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/frame_stats.h"
#include "remoting/protocol/host_video_stats_dispatcher.h"
#include "remoting/protocol/webrtc_frame_scheduler_constant_rate.h"
#include "remoting/protocol/webrtc_transport.h"
#include "remoting/protocol/webrtc_video_encoder_factory.h"
#include "remoting/protocol/webrtc_video_frame_adapter.h"
#include "remoting/protocol/webrtc_video_track_source.h"
#include "third_party/webrtc/api/media_stream_interface.h"
#include "third_party/webrtc/api/notifier.h"
#include "third_party/webrtc/api/peer_connection_interface.h"

namespace remoting {
namespace protocol {

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

WebrtcVideoStream::WebrtcVideoStream(const std::string& stream_name,
                                     const SessionOptions& session_options)
    : stream_name_(stream_name), session_options_(session_options) {}

WebrtcVideoStream::~WebrtcVideoStream() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (peer_connection_ && transceiver_) {
    // Ignore any error here, as this may return an error if the
    // peer-connection has been closed.
    peer_connection_->RemoveTrackOrError(transceiver_->sender());
  }
}

void WebrtcVideoStream::Start(
    std::unique_ptr<webrtc::DesktopCapturer> desktop_capturer,
    WebrtcTransport* webrtc_transport,
    WebrtcVideoEncoderFactory* video_encoder_factory) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(desktop_capturer);
  DCHECK(webrtc_transport);
  DCHECK(video_encoder_factory);

  scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory(
      webrtc_transport->peer_connection_factory());
  peer_connection_ = webrtc_transport->peer_connection();
  DCHECK(peer_connection_factory);
  DCHECK(peer_connection_);

  capturer_ = std::move(desktop_capturer);
  capturer_->Start(this);

  video_track_source_ = new rtc::RefCountedObject<WebrtcVideoTrackSource>(
      base::BindRepeating(&WebrtcVideoStream::OnSinkAddedOrUpdated,
                          weak_factory_.GetWeakPtr()));
  rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track =
      peer_connection_factory->CreateVideoTrack(stream_name_,
                                                video_track_source_);

  webrtc::RtpTransceiverInit init;
  init.stream_ids = {stream_name_};

  // value() DCHECKs if AddTransceiver() fails, which only happens if a track
  // was already added with the stream label.
  transceiver_ = peer_connection_->AddTransceiver(video_track, init).value();

  webrtc_transport->OnVideoTransceiverCreated(transceiver_);

  video_encoder_factory->SetVideoChannelStateObserver(
      weak_factory_.GetWeakPtr());
  scheduler_ = std::make_unique<WebrtcFrameSchedulerConstantRate>();
  scheduler_->Start(base::BindRepeating(&WebrtcVideoStream::CaptureNextFrame,
                                        base::Unretained(this)));
}

void WebrtcVideoStream::SelectSource(webrtc::ScreenId id) {
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
  NOTIMPLEMENTED();
}

void WebrtcVideoStream::SetLosslessColor(bool want_lossless) {
  NOTIMPLEMENTED() << "Changing lossless-color for VP9 requires SDP "
                      "offer/answer exchange.";
}

void WebrtcVideoStream::SetObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observer_ = observer;
}

void WebrtcVideoStream::OnKeyFrameRequested() {
  DCHECK(thread_checker_.CalledOnValidThread());
  scheduler_->OnKeyFrameRequested();
}

void WebrtcVideoStream::OnTargetBitrateChanged(int bitrate_kbps) {
  DCHECK(thread_checker_.CalledOnValidThread());
  scheduler_->OnTargetBitrateChanged(bitrate_kbps);
}

void WebrtcVideoStream::OnCaptureResult(
    webrtc::DesktopCapturer::Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  DCHECK(thread_checker_.CalledOnValidThread());

  current_frame_stats_->capture_ended_time = base::TimeTicks::Now();
  current_frame_stats_->capture_delay =
      base::Milliseconds(frame ? frame->capture_time_ms() : 0);

  if (!frame) {
    scheduler_->OnFrameCaptured(nullptr);
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

  scheduler_->OnFrameCaptured(frame.get());

  // Send the captured frame to the registered sink, if any. WebRTC will route
  // this to the appropriate encoder.
  video_track_source_->SendCapturedFrame(std::move(frame),
                                         std::move(current_frame_stats_));
}

void WebrtcVideoStream::CaptureNextFrame() {
  DCHECK(thread_checker_.CalledOnValidThread());

  current_frame_stats_ = std::make_unique<FrameStats>();
  current_frame_stats_->capture_started_time = base::TimeTicks::Now();
  current_frame_stats_->input_event_timestamps =
      event_timestamps_source_->TakeLastEventTimestamps();

  capturer_->CaptureFrame();
}

void WebrtcVideoStream::OnSinkAddedOrUpdated(const rtc::VideoSinkWants& wants) {
  DCHECK(thread_checker_.CalledOnValidThread());

  VLOG(0) << "WebRTC requested max framerate: " << wants.max_framerate_fps
          << " FPS";
  scheduler_->SetMaxFramerateFps(wants.max_framerate_fps);
}

void WebrtcVideoStream::OnFrameEncoded(
    WebrtcVideoEncoder::EncodeResult encode_result,
    const WebrtcVideoEncoder::EncodedFrame* frame) {
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
  if (video_stats_dispatcher_ && video_stats_dispatcher_->is_connected()) {
    // The down-cast is safe, because the |stats| object was originally created
    // by this class and attached to the frame.
    const auto* current_frame_stats =
        static_cast<const FrameStats*>(frame.stats.get());
    DCHECK(current_frame_stats);

    HostFrameStats stats;
    stats.bandwidth_estimate_kbps =
        current_frame_stats->bandwidth_estimate_kbps;
    stats.rtt_estimate = current_frame_stats->rtt_estimate;
    stats.send_pending_delay = current_frame_stats->send_pending_delay;

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

    video_stats_dispatcher_->OnVideoFrameStats(result.frame_id, stats);
  }
}

}  // namespace protocol
}  // namespace remoting
