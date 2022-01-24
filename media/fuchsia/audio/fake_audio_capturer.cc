// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/audio/fake_audio_capturer.h"

#include <string.h>

#include "base/bind.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/threading/thread_task_runner_handle.h"

namespace media {

FakeAudioCapturer::FakeAudioCapturer(
    fidl::InterfaceRequest<fuchsia::media::AudioCapturer> request,
    DataGeneration data_generation)
    : data_generation_(data_generation), binding_(this, std::move(request)) {}

FakeAudioCapturer::~FakeAudioCapturer() = default;

size_t FakeAudioCapturer::GetPacketSize() const {
  return frames_per_packet_ * stream_type_->channels * sizeof(float);
}

void FakeAudioCapturer::SendData(base::TimeTicks timestamp, void* data) {
  CHECK(buffer_vmo_);
  CHECK(is_active_);

  // Find unused packet.
  auto it = std::find(packets_usage_.begin(), packets_usage_.end(), false);

  // Currently tests don't try to send more than 2 packets and the buffer
  // always will have space for at least 2 packets.
  CHECK(it != packets_usage_.end());

  size_t buffer_index = it - packets_usage_.begin();
  size_t buffer_pos = buffer_index * GetPacketSize();

  packets_usage_[buffer_index] = true;

  // Write data to the shared VMO.
  zx_status_t status = buffer_vmo_.write(data, buffer_pos, GetPacketSize());
  ZX_CHECK(status == ZX_OK, status);

  // Send the new packet.
  fuchsia::media::StreamPacket packet;
  packet.payload_buffer_id = kBufferId;
  packet.pts = timestamp.ToZxTime();
  packet.payload_offset = buffer_pos;
  packet.payload_size = GetPacketSize();
  binding_.events().OnPacketProduced(std::move(packet));
}

// fuchsia::media::AudioCapturer implementation.
void FakeAudioCapturer::SetPcmStreamType(
    fuchsia::media::AudioStreamType stream_type) {
  CHECK(!stream_type_.has_value());
  CHECK_EQ(stream_type.sample_format, fuchsia::media::AudioSampleFormat::FLOAT);

  stream_type_ = std::move(stream_type);
}

void FakeAudioCapturer::AddPayloadBuffer(uint32_t id, zx::vmo payload_buffer) {
  CHECK_EQ(id, kBufferId);
  CHECK(!buffer_vmo_);
  CHECK(stream_type_.has_value());

  buffer_vmo_ = std::move(payload_buffer);
  zx_status_t status = buffer_vmo_.get_size(&buffer_size_);
  ZX_CHECK(status == ZX_OK, status);
}

void FakeAudioCapturer::StartAsyncCapture(uint32_t frames_per_packet) {
  CHECK(buffer_vmo_);
  CHECK(!is_active_);

  is_active_ = true;
  frames_per_packet_ = frames_per_packet;
  size_t num_packets = buffer_size_ / GetPacketSize();

  // AudioCapturer protocol requires that we can fit at least 2 packets in the
  // buffer in async mode.
  CHECK_GE(num_packets, 2U);

  packets_usage_.clear();
  packets_usage_.resize(num_packets, false);

  if (data_generation_ == DataGeneration::AUTOMATIC) {
    start_timestamp_ = base::TimeTicks::Now();
    ProducePackets();
  }
}

void FakeAudioCapturer::ReleasePacket(fuchsia::media::StreamPacket packet) {
  CHECK_EQ(packet.payload_buffer_id, kBufferId);
  CHECK_EQ(packet.payload_offset % GetPacketSize(), 0U);
  size_t buffer_index = packet.payload_offset / GetPacketSize();
  CHECK_LT(buffer_index, packets_usage_.size());
  CHECK(packets_usage_[buffer_index]);
  packets_usage_[buffer_index] = false;
}

// No other methods are expected to be called.
void FakeAudioCapturer::NotImplemented_(const std::string& name) {
  NOTREACHED();
}

void FakeAudioCapturer::ProducePackets() {
  if (!binding_.is_bound()) {
    return;
  }
  char data[GetPacketSize()];
  memset(data, 0, GetPacketSize());
  SendData(start_timestamp_ + base::Seconds(1) * packet_index_ *
                                  frames_per_packet_ /
                                  stream_type_->frames_per_second,
           data);
  packet_index_++;
  timer_.Start(FROM_HERE,
               start_timestamp_ +
                   base::Seconds(1) * packet_index_ * frames_per_packet_ /
                       stream_type_->frames_per_second -
                   base::TimeTicks::Now(),
               this, &FakeAudioCapturer::ProducePackets);
}

}  // namespace media
