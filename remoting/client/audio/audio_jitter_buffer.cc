// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/client/audio/audio_jitter_buffer.h"

#include <algorithm>
#include <string>

#include "base/check_op.h"

namespace {

// AudioJitterBuffer maintains a list of AudioPackets whose total playback
// duration <= |kMaxQueueLatency|.
// Once the buffer has run out of AudioPackets (latency reaches 0), it waits
// until the total latency reaches |kUnderrunRecoveryLatency| before it starts
// feeding the get-data requests. This helps reduce the frequency of stopping
// when the buffer underruns.
// If the total latency has reached |kMaxQueueLatency|, the oldest packets
// will get dropped until the latency is reduced to no more than
// |kOverrunRecoveryLatency|. This helps reduce the number of glitches when
// the buffer overruns.
// Otherwise the total latency can freely fluctuate between 0 and
// |kMaxQueueLatency|.

constexpr base::TimeDelta kMaxQueueLatency = base::Milliseconds(150);
constexpr base::TimeDelta kUnderrunRecoveryLatency = base::Milliseconds(60);
constexpr base::TimeDelta kOverrunRecoveryLatency = base::Milliseconds(90);

}  // namespace

namespace remoting {

AudioJitterBuffer::AudioJitterBuffer(
    OnFormatChangedCallback on_format_changed) {
  DETACH_FROM_THREAD(thread_checker_);
  on_format_changed_ = std::move(on_format_changed);
}

AudioJitterBuffer::~AudioJitterBuffer() = default;

void AudioJitterBuffer::AddAudioPacket(std::unique_ptr<AudioPacket> packet) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK_EQ(1, packet->data_size());
  DCHECK_EQ(AudioPacket::ENCODING_RAW, packet->encoding());
  DCHECK_NE(AudioPacket::SAMPLING_RATE_INVALID, packet->sampling_rate());

  AudioStreamFormat stream_format;
  stream_format.bytes_per_sample = packet->bytes_per_sample();
  stream_format.channels = packet->channels();
  stream_format.sample_rate = packet->sampling_rate();

  DCHECK_GT(stream_format.bytes_per_sample, 0);
  DCHECK_GT(stream_format.channels, 0);
  DCHECK_GT(stream_format.sample_rate, 0);
  DCHECK_EQ(packet->data(0).size() %
                (stream_format.channels * stream_format.bytes_per_sample),
            0u);

  if (!stream_format_ || *stream_format_ != stream_format) {
    ResetBuffer(stream_format);
  }

  // Push the new data to the back of the queue.
  queued_bytes_ += packet->data(0).size();
  queued_packets_.push_back(std::move(packet));

  if (underrun_protection_mode_ &&
      queued_bytes_ > GetBufferSizeFromTime(kUnderrunRecoveryLatency)) {
    // The buffer has enough data to start feeding the requests.
    underrun_protection_mode_ = false;
  }

  DropOverrunPackets();
  ProcessGetDataRequests();
}

void AudioJitterBuffer::AsyncGetData(std::unique_ptr<GetDataRequest> request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(stream_format_);
  DCHECK_EQ(request->bytes_needed % stream_format_->bytes_per_sample, 0u);
  queued_requests_.push_back(std::move(request));
  ProcessGetDataRequests();
}

void AudioJitterBuffer::ClearGetDataRequests() {
  queued_requests_.clear();
}

void AudioJitterBuffer::ResetBuffer(const AudioStreamFormat& new_format) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  queued_packets_.clear();
  queued_bytes_ = 0;
  first_packet_offset_ = 0;
  ClearGetDataRequests();
  stream_format_ = std::make_unique<AudioStreamFormat>(new_format);
  underrun_protection_mode_ = true;
  if (on_format_changed_) {
    on_format_changed_.Run(*stream_format_);
  }
}

void AudioJitterBuffer::ProcessGetDataRequests() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (underrun_protection_mode_) {
    return;
  }

  // Get the active request if there is one.
  while (!queued_requests_.empty() && !queued_packets_.empty()) {
    auto& active_request = queued_requests_.front();

    // Copy any available data into the active request up to as much requested.
    while (active_request->bytes_extracted < active_request->bytes_needed &&
           !queued_packets_.empty()) {
      uint8_t* next_data = static_cast<uint8_t*>(active_request->data) +
                           active_request->bytes_extracted;

      const std::string& packet_data = queued_packets_.front()->data(0);
      size_t bytes_to_copy = std::min(
          packet_data.size() - first_packet_offset_,
          active_request->bytes_needed - active_request->bytes_extracted);

      memcpy(next_data, packet_data.data() + first_packet_offset_,
             bytes_to_copy);

      first_packet_offset_ += bytes_to_copy;
      active_request->bytes_extracted += bytes_to_copy;
      queued_bytes_ -= bytes_to_copy;
      DCHECK_GE(queued_bytes_, 0u);
      // Pop off the packet if we've already consumed all its bytes.
      if (queued_packets_.front()->data(0).size() == first_packet_offset_) {
        queued_packets_.pop_front();
        first_packet_offset_ = 0;
      }
    }

    // If this request is fulfilled, call the callback and pop it off the queue.
    if (active_request->bytes_extracted == active_request->bytes_needed) {
      active_request->OnDataFilled();
      queued_requests_.pop_front();
    }
  }

  if (queued_packets_.empty()) {
    // Buffer overrun.
    underrun_protection_mode_ = true;
  }
}

size_t AudioJitterBuffer::GetBufferSizeFromTime(
    base::TimeDelta duration) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(stream_format_);
  return duration.InMilliseconds() * stream_format_->sample_rate *
         stream_format_->bytes_per_sample * stream_format_->channels /
         base::Time::kMillisecondsPerSecond;
}

void AudioJitterBuffer::DropOverrunPackets() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (queued_bytes_ <= GetBufferSizeFromTime(kMaxQueueLatency)) {
    return;
  }

  size_t new_size = GetBufferSizeFromTime(kOverrunRecoveryLatency);
  while (queued_bytes_ > new_size) {
    queued_bytes_ -=
        queued_packets_.front()->data(0).size() - first_packet_offset_;
    DCHECK_GE(queued_bytes_, 0u);
    queued_packets_.pop_front();
    first_packet_offset_ = 0;
  }
}

}  // namespace remoting
