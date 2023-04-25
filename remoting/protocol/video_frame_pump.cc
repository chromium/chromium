// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/video_frame_pump.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "remoting/base/constants.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/desktop_capturer.h"
#include "remoting/protocol/video_stub.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting::protocol {

// Interval between empty keep-alive frames. These frames are sent only when the
// stream is paused or inactive for some other reason (e.g. when blocked on
// capturer). To prevent PseudoTCP from resetting congestion window this value
// must be smaller than the minimum RTO used in PseudoTCP, which is 250ms.
static const int kKeepAlivePacketIntervalMs = 200;

VideoFramePump::FrameTimestamps::FrameTimestamps() = default;
VideoFramePump::FrameTimestamps::~FrameTimestamps() = default;

VideoFramePump::PacketWithTimestamps::PacketWithTimestamps(
    std::unique_ptr<VideoPacket> packet,
    std::unique_ptr<FrameTimestamps> timestamps)
    : packet(std::move(packet)), timestamps(std::move(timestamps)) {}

VideoFramePump::PacketWithTimestamps::~PacketWithTimestamps() = default;

VideoFramePump::VideoFramePump(
    scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner,
    std::unique_ptr<DesktopCapturer> capturer,
    std::unique_ptr<VideoEncoder> encoder,
    protocol::VideoStub* video_stub)
    : encode_task_runner_(encode_task_runner),
      capturer_(std::move(capturer)),
      encoder_(std::move(encoder)),
      video_stub_(video_stub),
      keep_alive_timer_(
          FROM_HERE,
          base::Milliseconds(kKeepAlivePacketIntervalMs),
          base::BindRepeating(&VideoFramePump::SendKeepAlivePacket,
                              base::Unretained(this))),
      capture_scheduler_(base::BindRepeating(&VideoFramePump::CaptureNextFrame,
                                             base::Unretained(this))) {
  DCHECK(encoder_);
  DCHECK(video_stub_);

  capturer_->Start(this);
  capture_scheduler_.Start();
}

VideoFramePump::~VideoFramePump() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  encode_task_runner_->DeleteSoon(FROM_HERE, encoder_.release());
}

void VideoFramePump::SetEventTimestampsSource(
    scoped_refptr<InputEventTimestampsSource> event_timestamps_source) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  event_timestamps_source_ = event_timestamps_source;
}

void VideoFramePump::Pause(bool pause) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  capture_scheduler_.Pause(pause);
}

void VideoFramePump::SetObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observer_ = observer;
}

void VideoFramePump::SelectSource(webrtc::ScreenId id) {}

void VideoFramePump::SetComposeEnabled(bool enabled) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  capturer_->SetComposeEnabled(enabled);
}
void VideoFramePump::SetMouseCursor(
    std::unique_ptr<webrtc::MouseCursor> mouse_cursor) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  capturer_->SetMouseCursor(std::move(mouse_cursor));
}

void VideoFramePump::SetMouseCursorPosition(
    const webrtc::DesktopVector& position) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  capturer_->SetMouseCursorPosition(position);
}

void VideoFramePump::SetTargetFramerate(int framerate) {}

void VideoFramePump::OnCaptureResult(
    webrtc::DesktopCapturer::Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  capture_scheduler_.OnCaptureCompleted();

  captured_frame_timestamps_->capture_ended_time = base::TimeTicks::Now();

  if (frame) {
    webrtc::DesktopVector dpi =
        frame->dpi().is_zero() ? webrtc::DesktopVector(kDefaultDpi, kDefaultDpi)
                               : frame->dpi();
    if (!frame_size_.equals(frame->size()) || !frame_dpi_.equals(dpi)) {
      frame_size_ = frame->size();
      frame_dpi_ = dpi;
      if (observer_) {
        observer_->OnVideoSizeChanged(this, frame_size_, frame_dpi_);
      }
    }
  }

  // Even when |frame| is nullptr we still need to post it to the encode thread
  // to make sure frames are freed in the same order they are received and
  // that we don't start capturing frame n+2 before frame n is freed.
  encode_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&VideoFramePump::EncodeFrame, encoder_.get(),
                     std::move(frame), std::move(captured_frame_timestamps_)),
      base::BindOnce(&VideoFramePump::OnFrameEncoded,
                     weak_factory_.GetWeakPtr()));
}

void VideoFramePump::CaptureNextFrame() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  captured_frame_timestamps_ = std::make_unique<FrameTimestamps>();
  captured_frame_timestamps_->capture_started_time = base::TimeTicks::Now();

  if (event_timestamps_source_) {
    captured_frame_timestamps_->input_event_timestamps =
        event_timestamps_source_->TakeLastEventTimestamps();
  }

  capturer_->CaptureFrame();
}

// static
std::unique_ptr<VideoFramePump::PacketWithTimestamps>
VideoFramePump::EncodeFrame(VideoEncoder* encoder,
                            std::unique_ptr<webrtc::DesktopFrame> frame,
                            std::unique_ptr<FrameTimestamps> timestamps) {
  timestamps->encode_started_time = base::TimeTicks::Now();

  std::unique_ptr<VideoPacket> packet;
  // If |frame| is non-NULL then let the encoder process it.
  if (frame) {
    packet = encoder->Encode(*frame);
  }

  // If |frame| is NULL, or the encoder returned nothing, return an empty
  // packet.
  if (!packet) {
    packet = std::make_unique<VideoPacket>();
  }

  if (frame) {
    packet->set_capture_time_ms(frame->capture_time_ms());
  }

  timestamps->encode_ended_time = base::TimeTicks::Now();
  packet->set_encode_time_ms(
      (timestamps->encode_ended_time - timestamps->encode_started_time)
          .InMilliseconds());

  return std::make_unique<PacketWithTimestamps>(std::move(packet),
                                                std::move(timestamps));
}

void VideoFramePump::OnFrameEncoded(
    std::unique_ptr<PacketWithTimestamps> packet) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  capture_scheduler_.OnFrameEncoded(packet->packet.get());

  if (send_pending_) {
    pending_packets_.push_back(std::move(packet));
  } else {
    SendPacket(std::move(packet));
  }
}

void VideoFramePump::SendPacket(std::unique_ptr<PacketWithTimestamps> packet) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!send_pending_);

  packet->timestamps->can_send_time = base::TimeTicks::Now();
  UpdateFrameTimers(packet->packet.get(), packet->timestamps.get());

  send_pending_ = true;
  video_stub_->ProcessVideoPacket(
      std::move(packet->packet),
      base::BindOnce(&VideoFramePump::OnVideoPacketSent,
                     weak_factory_.GetWeakPtr()));
}

void VideoFramePump::UpdateFrameTimers(VideoPacket* packet,
                                       FrameTimestamps* timestamps) {
  if (!timestamps->input_event_timestamps.is_null()) {
    packet->set_capture_pending_time_ms(
        (timestamps->capture_started_time -
         timestamps->input_event_timestamps.host_timestamp)
            .InMilliseconds());
    packet->set_latest_event_timestamp(
        timestamps->input_event_timestamps.client_timestamp.ToInternalValue());
  }

  packet->set_capture_overhead_time_ms(
      (timestamps->capture_ended_time - timestamps->capture_started_time)
          .InMilliseconds() -
      packet->capture_time_ms());

  packet->set_encode_pending_time_ms(
      (timestamps->encode_started_time - timestamps->capture_ended_time)
          .InMilliseconds());

  packet->set_send_pending_time_ms(
      (timestamps->can_send_time - timestamps->encode_ended_time)
          .InMilliseconds());
}

void VideoFramePump::OnVideoPacketSent() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  send_pending_ = false;
  capture_scheduler_.OnFrameSent();
  keep_alive_timer_.Reset();

  // Send next packet if any.
  if (!pending_packets_.empty()) {
    std::unique_ptr<PacketWithTimestamps> next =
        std::move(pending_packets_.front());
    pending_packets_.erase(pending_packets_.begin());
    SendPacket(std::move(next));
  }
}

void VideoFramePump::SendKeepAlivePacket() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  video_stub_->ProcessVideoPacket(
      std::make_unique<VideoPacket>(),
      base::BindOnce(&VideoFramePump::OnKeepAlivePacketSent,
                     weak_factory_.GetWeakPtr()));
}

void VideoFramePump::OnKeepAlivePacketSent() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  keep_alive_timer_.Reset();
}

}  // namespace remoting::protocol
