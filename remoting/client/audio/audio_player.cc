// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/client/audio/audio_player.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"

// If queue grows bigger than 150ms we start dropping packets.
const int kMaxQueueLatencyMs = 150;

namespace remoting {

// TODO(nicholss): Update legacy audio player to use new audio buffer code.
AudioPlayer::AudioPlayer()
    : sampling_rate_(AudioPacket::SAMPLING_RATE_INVALID),
      start_failed_(false),
      queued_bytes_(0),
      bytes_consumed_(0) {}

AudioPlayer::~AudioPlayer() = default;

void AudioPlayer::ProcessAudioPacket(std::unique_ptr<AudioPacket> packet,
                                     base::OnceClosure done) {
  CHECK_EQ(1, packet->data_size());
  DCHECK_EQ(AudioPacket::ENCODING_RAW, packet->encoding());
  DCHECK_NE(AudioPacket::SAMPLING_RATE_INVALID, packet->sampling_rate());
  DCHECK_EQ(kSampleSizeBytes, static_cast<int>(packet->bytes_per_sample()));
  DCHECK_EQ(kChannels, static_cast<int>(packet->channels()));
  DCHECK_EQ(packet->data(0).size() % (kChannels * kSampleSizeBytes), 0u);

  base::ScopedClosureRunner done_runner(std::move(done));

  // No-op if the Pepper player won't start.
  if (start_failed_) {
    return;
  }

  // Start the Pepper audio player if this is the first packet.
  if (sampling_rate_ != packet->sampling_rate()) {
    // Drop all packets currently in the queue, since they are sampled at the
    // wrong rate.
    {
      base::AutoLock auto_lock(lock_);
      ResetQueue();
    }

    sampling_rate_ = packet->sampling_rate();
    bool success = ResetAudioPlayer(sampling_rate_);
    if (!success) {
      start_failed_ = true;
      return;
    }
  }

  base::AutoLock auto_lock(lock_);

  queued_bytes_ += packet->data(0).size();
  queued_packets_.push_back(std::move(packet));

  int max_buffer_size_ = kMaxQueueLatencyMs * sampling_rate_ *
                         kSampleSizeBytes * kChannels /
                         base::Time::kMillisecondsPerSecond;
  while (queued_bytes_ > max_buffer_size_) {
    queued_bytes_ -= queued_packets_.front()->data(0).size() - bytes_consumed_;
    DCHECK_GE(queued_bytes_, 0);
    queued_packets_.pop_front();
    bytes_consumed_ = 0;
  }
}

// static
void AudioPlayer::AudioPlayerCallback(void* samples,
                                      uint32_t buffer_size,
                                      void* data) {
  AudioPlayer* audio_player = static_cast<AudioPlayer*>(data);
  audio_player->FillWithSamples(samples, buffer_size);
}

void AudioPlayer::ResetQueue() {
  lock_.AssertAcquired();
  queued_packets_.clear();
  queued_bytes_ = 0;
  bytes_consumed_ = 0;
}

void AudioPlayer::FillWithSamples(void* samples, uint32_t buffer_size) {
  base::AutoLock auto_lock(lock_);

  const size_t bytes_needed =
      kChannels * kSampleSizeBytes * GetSamplesPerFrame();

  // Make sure we don't overrun the buffer.
  CHECK_EQ(buffer_size, bytes_needed);

  char* next_sample = static_cast<char*>(samples);
  size_t bytes_extracted = 0;

  while (bytes_extracted < bytes_needed) {
    // Check if we've run out of samples for this packet.
    if (queued_packets_.empty()) {
      memset(next_sample, 0, bytes_needed - bytes_extracted);
      return;
    }

    // Pop off the packet if we've already consumed all its bytes.
    if (queued_packets_.front()->data(0).size() == bytes_consumed_) {
      queued_packets_.pop_front();
      bytes_consumed_ = 0;
      continue;
    }

    const std::string& packet_data = queued_packets_.front()->data(0);
    size_t bytes_to_copy = std::min(packet_data.size() - bytes_consumed_,
                                    bytes_needed - bytes_extracted);
    memcpy(next_sample, packet_data.data() + bytes_consumed_, bytes_to_copy);

    next_sample += bytes_to_copy;
    bytes_consumed_ += bytes_to_copy;
    bytes_extracted += bytes_to_copy;
    queued_bytes_ -= bytes_to_copy;
    DCHECK_GE(queued_bytes_, 0);
  }
}

}  // namespace remoting
