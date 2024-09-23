// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/audio/fake_audio_consumer.h"

#include <lib/vfs/cpp/pseudo_dir.h>
#include <lib/vfs/cpp/service.h>

#include "base/fuchsia/fuchsia_logging.h"

namespace media {

const base::TimeDelta FakeAudioConsumer::kMinLeadTime = base::Milliseconds(100);
const base::TimeDelta FakeAudioConsumer::kMaxLeadTime = base::Milliseconds(500);

FakeAudioConsumer::FakeAudioConsumer(
    uint64_t session_id,
    fidl::InterfaceRequest<fuchsia::media::AudioConsumer> request)
    : session_id_(session_id),
      audio_consumer_binding_(this),
      stream_sink_binding_(this),
      volume_control_binding_(this) {
  audio_consumer_binding_.Bind(std::move(request));
}

FakeAudioConsumer::~FakeAudioConsumer() = default;

base::TimeDelta FakeAudioConsumer::GetMediaPosition() {
  base::TimeDelta result = media_pos_;
  if (state_ == State::kPlaying) {
    result += (base::TimeTicks::Now() - reference_time_) * media_delta_ /
              reference_delta_;
  }
  return result;
}

void FakeAudioConsumer::CreateStreamSink(
    std::vector<zx::vmo> buffers,
    fuchsia::media::AudioStreamType stream_type,
    std::unique_ptr<fuchsia::media::Compression> compression,
    fidl::InterfaceRequest<fuchsia::media::StreamSink> stream_sink_request) {
  num_buffers_ = buffers.size();
  CHECK_GT(num_buffers_, 0U);
  stream_sink_binding_.Bind(std::move(stream_sink_request));
}

void FakeAudioConsumer::Start(fuchsia::media::AudioConsumerStartFlags flags,
                              int64_t reference_time,
                              int64_t media_time) {
  CHECK(state_ == State::kStopped);

  if (reference_time != fuchsia::media::NO_TIMESTAMP) {
    reference_time_ = base::TimeTicks::FromZxTime(reference_time);
  } else {
    reference_time_ = base::TimeTicks::Now() + kMinLeadTime;
  }

  if (media_time != fuchsia::media::NO_TIMESTAMP) {
    media_pos_ = base::TimeDelta::FromZxDuration(media_time);
  } else {
    if (media_pos_.is_min()) {
      media_pos_ = base::TimeDelta();
    }
  }

  state_ = State::kPlaying;

  OnStatusUpdate();
  ScheduleNextStreamPosUpdate();
}

void FakeAudioConsumer::Stop() {
  CHECK(state_ != State::kStopped);

  state_ = State::kStopped;
  OnStatusUpdate();
}

void FakeAudioConsumer::WatchStatus(WatchStatusCallback callback) {
  status_callback_ = std::move(callback);
  if (have_status_update_) {
    CallStatusCallback();
  }
}

void FakeAudioConsumer::SetRate(float rate) {
  // Playback rate must not be negative.
  CHECK_GE(rate, 0.0);

  // Update reference position.
  auto now = base::TimeTicks::Now();
  media_pos_ =
      media_pos_ + (now - reference_time_) * media_delta_ / reference_delta_;
  reference_time_ = now;

  // Approximate the rate as n/1000;
  reference_delta_ = 1000;
  media_delta_ = static_cast<int>(rate * 1000.0);

  OnStatusUpdate();

  if (update_timer_.IsRunning())
    update_timer_.Reset();
  ScheduleNextStreamPosUpdate();
}

void FakeAudioConsumer::BindVolumeControl(
    fidl::InterfaceRequest<fuchsia::media::audio::VolumeControl>
        volume_control_request) {
  volume_control_binding_.Bind(std::move(volume_control_request));
}

void FakeAudioConsumer::SendPacket(fuchsia::media::StreamPacket stream_packet,
                                   SendPacketCallback callback) {
  CHECK_LT(stream_packet.payload_buffer_id, num_buffers_);

  Packet packet;
  if (stream_packet.pts == fuchsia::media::NO_TIMESTAMP) {
    if (media_pos_.is_min()) {
      packet.pts = base::TimeDelta();
    } else {
      packet.pts = media_pos_;
    }
  } else {
    packet.pts = base::TimeDelta::FromZxDuration(stream_packet.pts);
  }
  pending_packets_.push_back(std::move(packet));

  callback();

  ScheduleNextStreamPosUpdate();
}

void FakeAudioConsumer::SendPacketNoReply(fuchsia::media::StreamPacket packet) {
  NOTREACHED_IN_MIGRATION();
}

void FakeAudioConsumer::EndOfStream() {
  Packet packet;
  packet.is_eos = true;
  pending_packets_.push_back(std::move(packet));
}

void FakeAudioConsumer::DiscardAllPackets(DiscardAllPacketsCallback callback) {
  DiscardAllPacketsNoReply();
  std::move(callback)();
}

void FakeAudioConsumer::DiscardAllPacketsNoReply() {
  pending_packets_.clear();
}

void FakeAudioConsumer::SetVolume(float volume) {
  volume_ = volume;
}

void FakeAudioConsumer::SetMute(bool mute) {
  is_muted_ = mute;
}

void FakeAudioConsumer::NotImplemented_(const std::string& name) {
  LOG(FATAL) << "Reached non-implemented " << name;
}

void FakeAudioConsumer::ScheduleNextStreamPosUpdate() {
  if (pending_packets_.empty() || update_timer_.IsRunning() ||
      media_delta_ == 0 || state_ != State::kPlaying) {
    return;
  }
  base::TimeDelta delay;
  if (!pending_packets_.front().is_eos) {
    auto next_packet_time =
        reference_time_ + (pending_packets_.front().pts - media_pos_) *
                              reference_delta_ / media_delta_;
    delay = (next_packet_time - base::TimeTicks::Now());
  }
  update_timer_.Start(FROM_HERE, delay,
                      base::BindOnce(&FakeAudioConsumer::UpdateStreamPos,
                                     base::Unretained(this)));
}

void FakeAudioConsumer::UpdateStreamPos() {
  if (state_ != State::kPlaying)
    return;

  auto now = base::TimeTicks::Now();
  auto new_media_pos =
      media_pos_ + (now - reference_time_) * media_delta_ / reference_delta_;

  // Drop all packets with PTS before the current position.
  while (!pending_packets_.empty()) {
    if (!pending_packets_.front().is_eos &&
        pending_packets_.front().pts > new_media_pos) {
      break;
    }

    Packet packet = pending_packets_.front();
    pending_packets_.pop_front();

    if (packet.is_eos) {
      // No data should be submitted after EOS.
      CHECK(pending_packets_.empty());
      audio_consumer_binding_.events().OnEndOfStream();
      state_ = State::kEndOfStream;
      media_pos_ = new_media_pos;
      reference_time_ = now;
    }
  }

  ScheduleNextStreamPosUpdate();
}

void FakeAudioConsumer::OnStatusUpdate() {
  have_status_update_ = true;
  if (status_callback_) {
    CallStatusCallback();
  }
}

void FakeAudioConsumer::CallStatusCallback() {
  DCHECK(status_callback_);
  DCHECK(have_status_update_);

  fuchsia::media::AudioConsumerStatus status;
  if (state_ == State::kPlaying) {
    fuchsia::media::TimelineFunction timeline;
    timeline.reference_time = reference_time_.ToZxTime();
    timeline.subject_time = media_pos_.ToZxDuration();
    timeline.reference_delta = reference_delta_;
    timeline.subject_delta = media_delta_;

    status.set_presentation_timeline(std::move(timeline));
  }

  status.set_min_lead_time(kMinLeadTime.ToZxDuration());
  status.set_max_lead_time(kMaxLeadTime.ToZxDuration());

  have_status_update_ = false;
  std::move(status_callback_)(std::move(status));
  status_callback_ = {};
}

FakeAudioConsumerService::FakeAudioConsumerService(vfs::PseudoDir* pseudo_dir)
    : binding_(pseudo_dir, this) {}

FakeAudioConsumerService::~FakeAudioConsumerService() {}

void FakeAudioConsumerService::CreateAudioConsumer(
    uint64_t session_id,
    fidl::InterfaceRequest<fuchsia::media::AudioConsumer> request) {
  audio_consumers_.push_back(
      std::make_unique<FakeAudioConsumer>(session_id, std::move(request)));
}

void FakeAudioConsumerService::NotImplemented_(const std::string& name) {
  LOG(FATAL) << "Reached non-implemented " << name;
}

}  // namespace media