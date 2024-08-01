// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/audio/fake_audio_capturer.h"

#include <string.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/types/fixed_array.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

FakeAudioCapturer::FakeAudioCapturer(
    fidl::InterfaceRequest<fuchsia::media::AudioCapturer> request)
    : binding_(this) {
  if (request)
    Bind(std::move(request));
}

FakeAudioCapturer::~FakeAudioCapturer() = default;

void FakeAudioCapturer::Bind(
    fidl::InterfaceRequest<fuchsia::media::AudioCapturer> request) {
  binding_.Bind(std::move(request));
}

size_t FakeAudioCapturer::GetPacketSize() const {
  return frames_per_packet_ * stream_type_->channels * sizeof(float);
}

void FakeAudioCapturer::SetDataGeneration(DataGeneration data_generation) {
  EXPECT_TRUE(!is_active());
  data_generation_ = data_generation;
}

void FakeAudioCapturer::SendData(base::TimeTicks timestamp, void* data) {
  EXPECT_TRUE(buffer_vmo_);
  EXPECT_TRUE(is_active_);

  // Find unused packet.
  auto it = base::ranges::find(packets_usage_, false);

  // Currently tests don't try to send more than 2 packets and the buffer
  // always will have space for at least 2 packets.
  EXPECT_TRUE(it != packets_usage_.end());

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
  EXPECT_TRUE(!stream_type_.has_value());
  EXPECT_EQ(stream_type.sample_format,
            fuchsia::media::AudioSampleFormat::FLOAT);

  stream_type_ = std::move(stream_type);
}

void FakeAudioCapturer::AddPayloadBuffer(uint32_t id, zx::vmo payload_buffer) {
  EXPECT_EQ(id, kBufferId);
  EXPECT_TRUE(!buffer_vmo_);
  EXPECT_TRUE(stream_type_.has_value());

  buffer_vmo_ = std::move(payload_buffer);
  zx_status_t status = buffer_vmo_.get_size(&buffer_size_);
  ZX_CHECK(status == ZX_OK, status);
}

void FakeAudioCapturer::StartAsyncCapture(uint32_t frames_per_packet) {
  EXPECT_TRUE(buffer_vmo_);
  EXPECT_TRUE(!is_active_);

  is_active_ = true;
  frames_per_packet_ = frames_per_packet;
  size_t num_packets = buffer_size_ / GetPacketSize();

  // AudioCapturer protocol requires that we can fit at least 2 packets in the
  // buffer in async data_generation.
  EXPECT_GE(num_packets, 2U);

  packets_usage_.clear();
  packets_usage_.resize(num_packets, false);

  if (data_generation_ == DataGeneration::AUTOMATIC) {
    start_timestamp_ = base::TimeTicks::Now();
    ProducePackets();
  }
}

void FakeAudioCapturer::StopAsyncCaptureNoReply() {
  is_active_ = false;
  timer_.Stop();
}

void FakeAudioCapturer::ReleasePacket(fuchsia::media::StreamPacket packet) {
  EXPECT_EQ(packet.payload_buffer_id, kBufferId);
  EXPECT_EQ(packet.payload_offset % GetPacketSize(), 0U);
  size_t buffer_index = packet.payload_offset / GetPacketSize();
  EXPECT_LT(buffer_index, packets_usage_.size());
  EXPECT_TRUE(packets_usage_[buffer_index]);
  packets_usage_[buffer_index] = false;
}

void FakeAudioCapturer::NotImplemented_(const std::string& name) {
  ADD_FAILURE() << "Unexpected FakeAudioCapturer call: " << name;
}

void FakeAudioCapturer::ProducePackets() {
  if (!binding_.is_bound()) {
    return;
  }
  base::FixedArray<char> data(GetPacketSize());
  memset(data.data(), 0, data.memsize());
  SendData(start_timestamp_ + base::Seconds(1) * packet_index_ *
                                  frames_per_packet_ /
                                  stream_type_->frames_per_second,
           data.data());
  packet_index_++;
  timer_.Start(FROM_HERE,
               start_timestamp_ +
                   base::Seconds(1) * packet_index_ * frames_per_packet_ /
                       stream_type_->frames_per_second -
                   base::TimeTicks::Now(),
               this, &FakeAudioCapturer::ProducePackets);
}

FakeAudioCapturerFactory::FakeAudioCapturerFactory(
    sys::OutgoingDirectory* outgoing_directory)
    : binding_(outgoing_directory, this) {}

FakeAudioCapturerFactory::~FakeAudioCapturerFactory() = default;

std::unique_ptr<FakeAudioCapturer> FakeAudioCapturerFactory::TakeCapturer() {
  if (capturers_.empty())
    return nullptr;
  auto result = std::move(capturers_.front());
  capturers_.pop_front();
  return result;
}

void FakeAudioCapturerFactory::CreateAudioCapturer(
    fidl::InterfaceRequest<fuchsia::media::AudioCapturer> request,
    bool loopback) {
  capturers_.push_back(std::make_unique<FakeAudioCapturer>());
  capturers_.back()->Bind(std::move(request));
}

void FakeAudioCapturerFactory::NotImplemented_(const std::string& name) {
  ADD_FAILURE() << "Unexpected FakeAudioCapturerFactory call: " << name;
}

}  // namespace media
