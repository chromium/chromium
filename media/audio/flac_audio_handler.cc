// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/audio/flac_audio_handler.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_fifo.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/limits.h"
#include "third_party/flac/include/FLAC/ordinals.h"
#include "third_party/flac/include/FLAC/stream_decoder.h"

namespace media {

FlacAudioHandler::FlacAudioHandler(std::string_view data)
    : flac_data_(data), decoder_(FLAC__stream_decoder_new()) {}

FlacAudioHandler::~FlacAudioHandler() = default;

bool FlacAudioHandler::Initialize() {
  DCHECK(decoder_);
  DCHECK(!is_initialized());

  // Initialize the `decoder_`.
  const FLAC__StreamDecoderInitStatus init_status =
      FLAC__stream_decoder_init_stream(
          /*decoder=*/decoder_.get(), /*read_callback=*/ReadCallback,
          /*seek_callback=*/nullptr, /*tell_callback=*/nullptr,
          /*length_callback=*/nullptr, /*eof_callback=*/nullptr,
          /*write_callback=*/WriteCallback, /*metadata_callback=*/MetaCallback,
          /*error_callback=*/ErrorCallback,
          /*client_data=*/static_cast<void*>(this));
  DCHECK_EQ(init_status, FLAC__STREAM_DECODER_INIT_STATUS_OK);

  // Get the metadata.
  if (!FLAC__stream_decoder_process_until_end_of_metadata(decoder_.get())) {
    return false;
  }
  // Avoid that `fifo_` will be unbounded if metadata isn't in the first
  // packet.
  Reset();
  return is_initialized();
}

int FlacAudioHandler::GetNumChannels() const {
  DCHECK(is_initialized());
  return num_channels_;
}

int FlacAudioHandler::GetSampleRate() const {
  DCHECK(is_initialized());
  return sample_rate_;
}

base::TimeDelta FlacAudioHandler::GetDuration() const {
  DCHECK(is_initialized());
  return AudioTimestampHelper::FramesToTime(static_cast<int64_t>(total_frames_),
                                            sample_rate_);
}

bool FlacAudioHandler::AtEnd() const {
  auto state = FLAC__stream_decoder_get_state(decoder_.get());
  return state ==
             FLAC__StreamDecoderState::FLAC__STREAM_DECODER_END_OF_STREAM ||
         state == FLAC__StreamDecoderState::FLAC__STREAM_DECODER_ABORTED ||
         has_error_;
}

bool FlacAudioHandler::CopyTo(AudioBus* bus, size_t* frames_written) {
  DCHECK(bus);
  DCHECK(is_initialized());

  if (AtEnd()) {
    DCHECK_EQ(fifo_->frames(), 0);
    bus->Zero();
    return true;
  }

  DCHECK_EQ(bus->frames(), kDefaultFrameCount);
  DCHECK_EQ(bus->channels(), num_channels_);

  // Records the number of frames copied into `bus`.
  int frames_copied = 0;

  do {
    if (fifo_->frames() == 0 && !AtEnd()) {
      write_callback_called_ = false;
      if (!FLAC__stream_decoder_process_single(decoder_.get())) {
        return false;
      }
    }

    if (fifo_->frames() > 0) {
      const int frames =
          std::min(bus->frames() - frames_copied, fifo_->frames());
      fifo_->Consume(bus, frames_copied, frames);
      frames_copied += frames;
    }
  } while (!AtEnd() && frames_copied < bus->frames());

  *frames_written = frames_copied;
  return true;
}

void FlacAudioHandler::Reset() {
  FLAC__stream_decoder_reset(decoder_.get());
  if (is_initialized()) {
    fifo_->Clear();
  }
  cursor_ = 0;
  has_error_ = false;
}

FLAC__StreamDecoderReadStatus FlacAudioHandler::ReadCallback(
    const FLAC__StreamDecoder* decoder,
    FLAC__byte buffer[],
    size_t* bytes,
    void* client_data) {
  return reinterpret_cast<FlacAudioHandler*>(client_data)
      ->ReadCallbackInternal(buffer, bytes);
}

FLAC__StreamDecoderWriteStatus FlacAudioHandler::WriteCallback(
    const FLAC__StreamDecoder* decoder,
    const FLAC__Frame* frame,
    const FLAC__int32* const buffer[],
    void* client_data) {
  return reinterpret_cast<FlacAudioHandler*>(client_data)
      ->WriteCallbackInternal(frame, buffer);
}

void FlacAudioHandler::MetaCallback(const FLAC__StreamDecoder* decoder,
                                    const FLAC__StreamMetadata* metadata,
                                    void* client_data) {
  reinterpret_cast<FlacAudioHandler*>(client_data)
      ->MetaCallbackInternal(metadata);
}

void FlacAudioHandler::ErrorCallback(const FLAC__StreamDecoder* decoder,
                                     FLAC__StreamDecoderErrorStatus status,
                                     void* client_data) {
  LOG(ERROR) << "Got an error callback: "
             << FLAC__StreamDecoderErrorStatusString[status];
  reinterpret_cast<FlacAudioHandler*>(client_data)->ErrorCallbackInternal();
}

FLAC__StreamDecoderReadStatus FlacAudioHandler::ReadCallbackInternal(
    FLAC__byte buffer[],
    size_t* bytes) {
  // Abort to avoid a deadlock.
  if (*bytes < 0) {
    return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
  }

  DCHECK_LE(cursor_, flac_data_.size());
  // Check if there is enough data to read.
  if (flac_data_.size() - cursor_ < *bytes) {
    *bytes = flac_data_.size() - cursor_;
  }

  memcpy(buffer, flac_data_.data() + cursor_, *bytes);

  // Update `cursor_`.
  cursor_ += *bytes;

  // Check for end of input.
  return cursor_ >= flac_data_.size()
             ? FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM
             : FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

FLAC__StreamDecoderWriteStatus FlacAudioHandler::WriteCallbackInternal(
    const FLAC__Frame* frame,
    const FLAC__int32* const buffer[]) {
  // For some fuzzer cases (b/41495570), a single call of
  // `FLAC__stream_decoder_process_single` will trigger the write callback for
  // multiple times to add silence frames. We don't support the abnormal padding
  // configurations.
  if (has_error_ || write_callback_called_) {
    return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
  }

  // Get the number of channels and the number of samples per channel.
  const int num_channels = frame->header.channels;
  const int num_samples = frame->header.blocksize;

  // Avoid the crash happened in `fifo_`.
  if (num_channels != num_channels_) {
    return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
  }

  // Discard the packet if there are more than the number of `max_blocksize`
  // frames.
  if (num_samples > bus_->frames()) {
    return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
  }

  for (int ch = 0; ch < num_channels; ++ch) {
    float* channel_data = bus_->channel(ch);
    const FLAC__int32* source_data = buffer[ch];
    for (int s = 0; s < num_samples; ++s, ++channel_data, ++source_data) {
      *channel_data = SignedInt16SampleTypeTraits::ToFloat(*source_data);
    }
  }

  fifo_->Push(bus_.get(), num_samples);
  write_callback_called_ = true;

  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void FlacAudioHandler::MetaCallbackInternal(
    const FLAC__StreamMetadata* metadata) {
  DCHECK(metadata);
  if (metadata->type != FLAC__METADATA_TYPE_STREAMINFO) {
    return;
  }

  // Avoid re-initialize the metadata variables due to some bad data.
  if (is_initialized()) {
    return;
  }

  // Stores the metadata for the audio.
  num_channels_ = static_cast<int>(metadata->data.stream_info.channels);
  sample_rate_ = static_cast<int>(metadata->data.stream_info.sample_rate);
  total_frames_ = metadata->data.stream_info.total_samples;
  bits_per_sample_ =
      static_cast<int>(metadata->data.stream_info.bits_per_sample);

  // We will create `fifo_` only when the params are valid.
  if (!AreParamsValid()) {
    LOG(ERROR) << "****Format is invalid. "
               << "num_channels: " << num_channels_ << " "
               << "sample_rate: " << sample_rate_ << " "
               << "bits_per_sample: " << bits_per_sample_ << " "
               << "total_frames_: " << total_frames_;
    return;
  }

  int max_blocksize =
      static_cast<int>(metadata->data.stream_info.max_blocksize);
  if (max_blocksize <= 0) {
    return;
  }

  // There will be at most `max_blocksize` of frames for each call to the
  // `WriteCallback()`. For the client bus in `CopyTo()`, it will always
  // have `kDefaultFrameCount` frames. We want to make sure the `fifo_`
  // can hold enough data to refill for a request. For example, we might
  // have `N
  // - 1` frames in the `fifo_` when a request for `N` frames comes in.
  // Then we need to fill the new coming `N` frames into `fifo_`, as a
  // result it requires a total capacity of `N + N - 1`. Technically, it
  // can be `kDefaultFrameCount + kDefaultFrameCount - 1` and
  // `max_blocksize + kDefaultFrameCount - 1`. 2 is just used for
  // simplicity.
  fifo_ = std::make_unique<AudioFifo>(
      num_channels_, std::max(kDefaultFrameCount * 2, max_blocksize * 2));

  bus_ = AudioBus::Create(num_channels_, max_blocksize);
}

void FlacAudioHandler::ErrorCallbackInternal() {
  has_error_ = true;
}

bool FlacAudioHandler::AreParamsValid() const {
  return (num_channels_ > 0 &&
          num_channels_ <= static_cast<int>(limits::kMaxChannels)) &&
         (bits_per_sample_ == 16) &&
         (sample_rate_ >= static_cast<int>(limits::kMinSampleRate) &&
          sample_rate_ <= static_cast<int>(limits::kMaxSampleRate)) &&
         (total_frames_ != 0);
}

}  // namespace media
