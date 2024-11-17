// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_video_stream.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/desktop_capturer.h"
#include "remoting/protocol/frame_stats.h"
#include "remoting/protocol/host_video_stats_dispatcher.h"
#include "remoting/protocol/no_op_webrtc_frame_scheduler.h"
#include "remoting/protocol/webrtc_frame_scheduler_constant_rate.h"
#include "remoting/protocol/webrtc_transport.h"
#include "remoting/protocol/webrtc_video_encoder_factory.h"
#include "remoting/protocol/webrtc_video_frame_adapter.h"
#include "remoting/protocol/webrtc_video_track_source.h"
#include "third_party/webrtc/api/media_stream_interface.h"
#include "third_party/webrtc/api/notifier.h"
#include "third_party/webrtc/api/peer_connection_interface.h"

namespace remoting::protocol {

class ScopedAllowSyncPrimitivesForWebRtcVideoStream
    : public base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope {};

FrameStatsMessage::VideoCodec VideoCodecToProtoEnum(
    webrtc::VideoCodecType codec) {
  switch (codec) {
    case webrtc::VideoCodecType::kVideoCodecVP8:
      return FrameStatsMessage::VP8;
    case webrtc::VideoCodecType::kVideoCodecVP9:
      return FrameStatsMessage::VP9;
    case webrtc::VideoCodecType::kVideoCodecAV1:
      return FrameStatsMessage::AV1;
    case webrtc::VideoCodecType::kVideoCodecH264:
      return FrameStatsMessage::H264;
    default:
      return FrameStatsMessage::UNKNOWN;
  }
}

struct WebrtcVideoStream::FrameStats : public WebrtcVideoEncoder::FrameStats {
  FrameStats() = default;
  FrameStats(const FrameStats&) = default;
  FrameStats& operator=(const FrameStats&) = default;
  ~FrameStats() override = default;

  // The input-event fields are only valid for the frame after an input event.
  InputEventTimestamps input_event_timestamps;

  base::TimeDelta capture_delay;

  uint32_t capturer_id = 0;
};

class WebrtcVideoStream::Core : public webrtc::DesktopCapturer::Callback {
 public:
  Core(webrtc::ScreenId screen_id,
       std::unique_ptr<DesktopCapturer> capturer,
       base::WeakPtr<WebrtcVideoStream> video_stream);

  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

  ~Core() override;

  // Called by the owning class to begin capturing and encoding the desktop.
  void Start();

  // webrtc::DesktopCapturer::Callback interface.
  void OnFrameCaptureStart() override;
  void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                       std::unique_ptr<webrtc::DesktopFrame> frame) override;

  // Mimics the remoting::VideoStream interface but only implements the methods
  // the outer class needs to call.
  void SetEventTimestampsSource(
      scoped_refptr<InputEventTimestampsSource> event_timestamps_source);
  void Pause(bool pause);
  void SelectSource(webrtc::ScreenId id);
  void SetComposeEnabled(bool enabled);
  void SetMouseCursor(std::unique_ptr<webrtc::MouseCursor> mouse_cursor);
  void SetMouseCursorPosition(const webrtc::DesktopVector& position);
  void BoostFramerate(base::TimeDelta capture_interval,
                      base::TimeDelta boost_duration);

  // Called by the video track source to set the max frame rate for the stream.
  void SetMaxFramerateFps(int max_framerate_fps);

 private:
  // Called by the |scheduler_|.
  void CaptureNextFrame();

  // The current frame size.
  webrtc::DesktopSize frame_size_;

  // The current frame DPI.
  webrtc::DesktopVector frame_dpi_;

  // Screen ID of the monitor being captured, from the initial value passed to
  // WebrtcVideoStream::Start(), or from SelectSource().
  webrtc::ScreenId screen_id_;

  // Stats of the frame that's being captured.
  std::unique_ptr<FrameStats> current_frame_stats_;

  // Capturer used to capture the screen.
  std::unique_ptr<DesktopCapturer> capturer_;

  // Schedules the next video frame.
  std::unique_ptr<WebrtcFrameScheduler> scheduler_;

  // Provides event timestamps which are used for |current_frame_stats|.
  scoped_refptr<InputEventTimestampsSource> event_timestamps_source_;

  // Points back to the WebrtcVideoStream instance which owns |this|.
  base::WeakPtr<WebrtcVideoStream> video_stream_;

  // Allows Core to post messages to its owner in a thread-safe manner.
  scoped_refptr<base::SingleThreadTaskRunner> video_stream_task_runner_;

  THREAD_CHECKER(thread_checker_);
};

WebrtcVideoStream::Core::Core(webrtc::ScreenId screen_id,
                              std::unique_ptr<DesktopCapturer> capturer,
                              base::WeakPtr<WebrtcVideoStream> video_stream)
    : screen_id_(screen_id),
      capturer_(std::move(capturer)),
      video_stream_(std::move(video_stream)),
      video_stream_task_runner_(
          base::SingleThreadTaskRunner::GetCurrentDefault()) {
  if (capturer_->SupportsFrameCallbacks()) {
    scheduler_ = std::make_unique<NoOpWebrtcFrameScheduler>(capturer_.get());
  } else {
    scheduler_ = std::make_unique<WebrtcFrameSchedulerConstantRate>();
  }
  DETACH_FROM_THREAD(thread_checker_);
}

WebrtcVideoStream::Core::~Core() = default;

void WebrtcVideoStream::Core::Start() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  capturer_->Start(this);
  scheduler_->Start(base::BindRepeating(
      &WebrtcVideoStream::Core::CaptureNextFrame, base::Unretained(this)));
}

void WebrtcVideoStream::Core::OnFrameCaptureStart() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  current_frame_stats_ = std::make_unique<FrameStats>();
  current_frame_stats_->capture_started_time = base::TimeTicks::Now();
  current_frame_stats_->input_event_timestamps =
      event_timestamps_source_->TakeLastEventTimestamps();
  current_frame_stats_->screen_id = screen_id_;
}

void WebrtcVideoStream::Core::OnCaptureResult(
    webrtc::DesktopCapturer::Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  current_frame_stats_->capture_ended_time = base::TimeTicks::Now();
  current_frame_stats_->capture_delay =
      base::Milliseconds(frame ? frame->capture_time_ms() : 0);

  if (!frame || frame->size().is_empty()) {
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
    video_stream_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&WebrtcVideoStream::OnVideoSizeChanged,
                                  video_stream_, frame_size_, frame_dpi_));
  }

  current_frame_stats_->capturer_id = frame->capturer_id();

  scheduler_->OnFrameCaptured(frame.get());

  video_stream_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebrtcVideoStream::SendCapturedFrame, video_stream_,
                     std::move(frame), std::move(current_frame_stats_)));
}

void WebrtcVideoStream::Core::SetEventTimestampsSource(
    scoped_refptr<InputEventTimestampsSource> event_timestamps_source) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  event_timestamps_source_ = event_timestamps_source;
}

void WebrtcVideoStream::Core::Pause(bool pause) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  scheduler_->Pause(pause);
}

void WebrtcVideoStream::Core::SelectSource(webrtc::ScreenId id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  screen_id_ = id;
  VLOG(0) << "SelectSource: id=" << id;
  capturer_->SelectSource(id);
}

void WebrtcVideoStream::Core::SetComposeEnabled(bool enabled) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  capturer_->SetComposeEnabled(enabled);
}
void WebrtcVideoStream::Core::SetMouseCursor(
    std::unique_ptr<webrtc::MouseCursor> mouse_cursor) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  capturer_->SetMouseCursor(std::move(mouse_cursor));
}

void WebrtcVideoStream::Core::SetMouseCursorPosition(
    const webrtc::DesktopVector& position) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  capturer_->SetMouseCursorPosition(position);
}

void WebrtcVideoStream::Core::BoostFramerate(base::TimeDelta capture_interval,
                                             base::TimeDelta boost_duration) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  scheduler_->BoostCaptureRate(capture_interval, boost_duration);
}

void WebrtcVideoStream::Core::SetMaxFramerateFps(int max_framerate_fps) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  scheduler_->SetMaxFramerateFps(max_framerate_fps);
}

void WebrtcVideoStream::Core::CaptureNextFrame() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  current_frame_stats_ = std::make_unique<FrameStats>();
  current_frame_stats_->capture_started_time = base::TimeTicks::Now();
  current_frame_stats_->input_event_timestamps =
      event_timestamps_source_->TakeLastEventTimestamps();
  current_frame_stats_->screen_id = screen_id_;

  capturer_->CaptureFrame();
}

WebrtcVideoStream::WebrtcVideoStream(const SessionOptions& session_options)
    : session_options_(session_options) {
// TODO(joedow): Dig into the threading model on other platforms to see if they
// can also be updated to run on a dedicated thread.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)
  core_task_runner_ = base::ThreadPool::CreateSingleThreadTaskRunner(
      {base::TaskPriority::HIGHEST},
      base::SingleThreadTaskRunnerThreadMode::DEDICATED);
#else
  core_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
#endif
}

WebrtcVideoStream::~WebrtcVideoStream() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (core_) {
    core_task_runner_->DeleteSoon(FROM_HERE, core_.release());
  }

  if (peer_connection_ && transceiver_) {
    // Stop the video-stream before removing it from the peer-connection.
    // Otherwise, it will continue to be listed in
    // peer_connection_->GetSenders(), and may interfere with bandwidth
    // estimation - b/366055325.
    transceiver_->StopStandard();

    // Ignore any errors here, as this may return an error if the
    // peer-connection has been closed.
    peer_connection_->RemoveTrackOrError(transceiver_->sender());
  }
}

void WebrtcVideoStream::Start(
    webrtc::ScreenId screen_id,
    std::unique_ptr<DesktopCapturer> desktop_capturer,
    WebrtcTransport* webrtc_transport,
    WebrtcVideoEncoderFactory* video_encoder_factory) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(desktop_capturer);
  DCHECK(webrtc_transport);
  DCHECK(video_encoder_factory);

  scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory(
      webrtc_transport->peer_connection_factory());
  peer_connection_ = webrtc_transport->peer_connection();
  DCHECK(peer_connection_factory);
  DCHECK(peer_connection_);

  std::string stream_name = StreamNameForId(screen_id);
  video_track_source_ = new rtc::RefCountedObject<WebrtcVideoTrackSource>(
      base::BindRepeating(&WebrtcVideoStream::OnSinkAddedOrUpdated,
                          weak_factory_.GetWeakPtr()));
  rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track =
      peer_connection_factory->CreateVideoTrack(video_track_source_,
                                                stream_name);

  webrtc::RtpTransceiverInit init;
  init.stream_ids = {stream_name};

  // value() DCHECKs if AddTransceiver() fails, which only happens if a track
  // was already added with the stream label.
  transceiver_ = peer_connection_->AddTransceiver(video_track, init).value();

  webrtc_transport->OnVideoTransceiverCreated(transceiver_);

  video_encoder_factory->video_stream_event_router()
      .SetVideoChannelStateObserver(stream_name, weak_factory_.GetWeakPtr());

  core_ = std::make_unique<Core>(screen_id, std::move(desktop_capturer),
                                 weak_factory_.GetWeakPtr());
  core_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(&WebrtcVideoStream::Core::Start,
                                             base::Unretained(core_.get())));
}

void WebrtcVideoStream::SelectSource(webrtc::ScreenId id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Unretained is sound as |core_| is owned by |this| and destroyed on
  // |core_task_runner_|.
  core_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WebrtcVideoStream::Core::SelectSource,
                                base::Unretained(core_.get()), id));
}

void WebrtcVideoStream::SetEventTimestampsSource(
    scoped_refptr<InputEventTimestampsSource> event_timestamps_source) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Unretained is sound as |core_| is owned by |this| and destroyed on
  // |core_task_runner_|.
  core_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebrtcVideoStream::Core::SetEventTimestampsSource,
                     base::Unretained(core_.get()),
                     base::RetainedRef(event_timestamps_source)));
}

void WebrtcVideoStream::Pause(bool pause) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Unretained is sound as |core_| is owned by |this| and destroyed on
  // |core_task_runner_|.
  core_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WebrtcVideoStream::Core::Pause,
                                base::Unretained(core_.get()), pause));
}

void WebrtcVideoStream::SetObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observer_ = observer;
}

void WebrtcVideoStream::SetComposeEnabled(bool enabled) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Unretained is sound as |core_| is owned by |this| and destroyed on
  // |core_task_runner_|.
  core_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WebrtcVideoStream::Core::SetComposeEnabled,
                                base::Unretained(core_.get()), enabled));
}

void WebrtcVideoStream::SetMouseCursor(
    std::unique_ptr<webrtc::MouseCursor> mouse_cursor) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Unretained is sound as |core_| is owned by |this| and destroyed on
  // |core_task_runner_|.
  core_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebrtcVideoStream::Core::SetMouseCursor,
                     base::Unretained(core_.get()), std::move(mouse_cursor)));
}

void WebrtcVideoStream::SetMouseCursorPosition(
    const webrtc::DesktopVector& position) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Unretained is sound as |core_| is owned by |this| and destroyed on
  // |core_task_runner_|.
  core_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebrtcVideoStream::Core::SetMouseCursorPosition,
                     base::Unretained(core_.get()), position));
}

void WebrtcVideoStream::BoostFramerate(base::TimeDelta capture_interval,
                                       base::TimeDelta boost_duration) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Unretained is sound as |core_| is owned by |this| and destroyed on
  // |core_task_runner_|.
  core_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WebrtcVideoStream::Core::BoostFramerate,
                                base::Unretained(core_.get()), capture_interval,
                                boost_duration));
}

void WebrtcVideoStream::SetTargetFramerate(int framerate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_GT(framerate, 0);
  DCHECK_LE(framerate, 1000);
  target_framerate_ = framerate;

  if (!transceiver_) {
    LOG(WARNING) << "No transceiver, can't set updated framerate.";
  }

  auto sender = transceiver_->sender();
  webrtc::RtpParameters parameters = sender->GetParameters();
  if (parameters.encodings.empty()) {
    LOG(ERROR) << "No encodings found for sender " << sender->id();
    return;
  }

  for (auto& encoding : parameters.encodings) {
    encoding.max_framerate = framerate;
  }

  ScopedAllowSyncPrimitivesForWebRtcVideoStream allow_wait;
  webrtc::RTCError result = transceiver_->sender()->SetParameters(parameters);
  DCHECK(result.ok()) << "SetParameters() failed: " << result.message();
}

// static
std::string WebrtcVideoStream::StreamNameForId(webrtc::ScreenId id) {
  if (id == webrtc::kFullDesktopScreenId) {
    // Used in the single-stream case.
    return "screen_stream";
  }

  return "screen_stream_" + base::NumberToString(id);
}

void WebrtcVideoStream::OnEncodedFrameSent(
    webrtc::EncodedImageCallback::Result result,
    const WebrtcVideoEncoder::EncodedFrame& frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (result.error != webrtc::EncodedImageCallback::Result::OK) {
    // TODO(sergeyu): Stop the stream.
    LOG(ERROR) << "Failed to send video frame.";
    return;
  }

  // Exit early if we aren't able to send a FrameStats message.
  if (!video_stats_dispatcher_ || !video_stats_dispatcher_->is_connected()) {
    return;
  }

  // The down-cast is safe, because the |stats| object was originally created
  // by this class and attached to the frame.
  const auto* current_frame_stats =
      static_cast<const FrameStats*>(frame.stats.get());
  DCHECK(current_frame_stats);

  HostFrameStats stats;
  stats.bandwidth_estimate_kbps = current_frame_stats->bandwidth_estimate_kbps;
  stats.rtt_estimate = current_frame_stats->rtt_estimate;
  stats.send_pending_delay = current_frame_stats->send_pending_delay;

  stats.frame_size = frame.data->size();

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
  // VP8/VP9 encoder lies within 0-63, with 0 representing a lossless frame.
  // TODO(crbug.com/41418600): Remove |quantizer| from the WebrtcVideoEncoder
  // interface, and move this logic to the encoders.
  stats.frame_quality = (63 - frame.quantizer) * 100 / 63;

  stats.screen_id = current_frame_stats->screen_id;

  stats.codec = VideoCodecToProtoEnum(frame.codec);
  stats.profile = frame.profile;
  stats.encoded_rect_width = frame.encoded_rect_width;
  stats.encoded_rect_height = frame.encoded_rect_height;

  video_stats_dispatcher_->OnVideoFrameStats(result.frame_id, stats);
}

void WebrtcVideoStream::OnSinkAddedOrUpdated(const rtc::VideoSinkWants& wants) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto framerate = wants.max_framerate_fps;
  VLOG(0) << "WebRTC requested max framerate: " << framerate << " FPS";

  // OnSinkAddedOrUpdated() is called when:
  //   - A new stream is added
  //   - An SDP renegotiation is requested
  //   - A new max framerate is requested
  //   - WebRTC artificially lowers the framerate due to network conditions
  //
  // We need to update the max_framerate for the stream in some of the
  // scenarios but not for the others. In order to determine whether to update
  // the RTPSender, we check the current max_framerate rather than the
  // framerate in |wants|.
  auto sender = transceiver_->sender();
  if (sender) {
    for (auto& encoding : sender->GetParameters().encodings) {
      if (encoding.max_framerate != target_framerate_) {
        VLOG(0) << "Setting target framerate for new sink: "
                << target_framerate_;
        SetTargetFramerate(target_framerate_);
      }
    }
  }

  // Unretained is sound as |core_| is owned by |this| and destroyed on
  // |core_task_runner_|.
  core_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WebrtcVideoStream::Core::SetMaxFramerateFps,
                                base::Unretained(core_.get()), framerate));
}

void WebrtcVideoStream::OnVideoSizeChanged(webrtc::DesktopSize frame_size,
                                           webrtc::DesktopVector frame_dpi) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (observer_) {
    observer_->OnVideoSizeChanged(this, std::move(frame_size),
                                  std::move(frame_dpi));
  }
}

void WebrtcVideoStream::SendCapturedFrame(
    std::unique_ptr<webrtc::DesktopFrame> desktop_frame,
    std::unique_ptr<WebrtcVideoEncoder::FrameStats> frame_stats) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Send the captured frame to the registered sink, if any. WebRTC will route
  // this to the appropriate encoder.
  video_track_source_->SendCapturedFrame(std::move(desktop_frame),
                                         std::move(frame_stats));
}

}  // namespace remoting::protocol
