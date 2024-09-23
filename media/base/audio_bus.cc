// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/base/audio_bus.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <utility>

#include "base/check_op.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/audio_parameters.h"
#include "media/base/limits.h"
#include "media/base/vector_math.h"

namespace media {

// In order to guarantee that the memory block for each channel starts at an
// aligned address when splitting a contiguous block of memory into one block
// per channel, we may have to make these blocks larger than otherwise needed.
// We do this by allocating space for potentially more frames than requested.
// This method returns the required size for the contiguous memory block
// in bytes and outputs the adjusted number of frames via |out_aligned_frames|.
static int CalculateMemorySizeInternal(int channels,
                                       int frames,
                                       int* out_aligned_frames) {
  // Since our internal sample format is float, we can guarantee the alignment
  // by making the number of frames an integer multiple of
  // AudioBus::kChannelAlignment / sizeof(float).
  int aligned_frames =
      ((frames * sizeof(float) + AudioBus::kChannelAlignment - 1) &
       ~(AudioBus::kChannelAlignment - 1)) / sizeof(float);

  if (out_aligned_frames)
    *out_aligned_frames = aligned_frames;

  return sizeof(float) * channels * aligned_frames;
}

static void ValidateConfig(int channels, int frames) {
  CHECK_GT(frames, 0);
  CHECK_GT(channels, 0);
  CHECK_LE(channels, static_cast<int>(limits::kMaxChannels));
}

void AudioBus::CheckOverflow(int start_frame, int frames, int total_frames) {
  CHECK_GE(start_frame, 0);
  CHECK_GE(frames, 0);
  CHECK_GT(total_frames, 0);
  int sum = start_frame + frames;
  CHECK_LE(sum, total_frames);
  CHECK_GE(sum, 0);
}

AudioBus::AudioBus(int channels, int frames)
    : frames_(frames), is_wrapper_(false) {
  ValidateConfig(channels, frames_);

  int aligned_frames = 0;
  int size = CalculateMemorySizeInternal(channels, frames, &aligned_frames);

  data_.reset(static_cast<float*>(base::AlignedAlloc(
      size, AudioBus::kChannelAlignment)));

  BuildChannelData(channels, aligned_frames, data_.get());
}

AudioBus::AudioBus(int channels, int frames, float* data)
    : frames_(frames), is_wrapper_(false) {
  // Since |data| may have come from an external source, ensure it's valid.
  CHECK(data);
  ValidateConfig(channels, frames_);

  int aligned_frames = 0;
  CalculateMemorySizeInternal(channels, frames, &aligned_frames);

  BuildChannelData(channels, aligned_frames, data);
}

AudioBus::AudioBus(int frames, const std::vector<float*>& channel_data)
    : channel_data_(channel_data), frames_(frames), is_wrapper_(false) {
  ValidateConfig(
      base::checked_cast<int>(channel_data_.size()), frames_);

  // Sanity check wrapped vector for alignment and channel count.
  for (size_t i = 0; i < channel_data_.size(); ++i)
    DCHECK(IsAligned(channel_data_[i]));
}

AudioBus::AudioBus(int channels)
    : channel_data_(channels), frames_(0), is_wrapper_(true) {
  CHECK_GT(channels, 0);
  for (size_t i = 0; i < channel_data_.size(); ++i)
    channel_data_[i] = nullptr;
}

AudioBus::~AudioBus() {
  if (wrapped_data_deleter_cb_)
    std::move(wrapped_data_deleter_cb_).Run();
}

std::unique_ptr<AudioBus> AudioBus::Create(int channels, int frames) {
  return base::WrapUnique(new AudioBus(channels, frames));
}

std::unique_ptr<AudioBus> AudioBus::Create(const AudioParameters& params) {
  return base::WrapUnique(
      new AudioBus(params.channels(), params.frames_per_buffer()));
}

std::unique_ptr<AudioBus> AudioBus::CreateWrapper(int channels) {
  return base::WrapUnique(new AudioBus(channels));
}

std::unique_ptr<AudioBus> AudioBus::WrapVector(
    int frames,
    const std::vector<float*>& channel_data) {
  return base::WrapUnique(new AudioBus(frames, channel_data));
}

std::unique_ptr<AudioBus> AudioBus::WrapMemory(int channels,
                                               int frames,
                                               void* data) {
  // |data| must be aligned by AudioBus::kChannelAlignment.
  CHECK(IsAligned(data));
  return base::WrapUnique(
      new AudioBus(channels, frames, static_cast<float*>(data)));
}

std::unique_ptr<AudioBus> AudioBus::WrapMemory(const AudioParameters& params,
                                               void* data) {
  // |data| must be aligned by AudioBus::kChannelAlignment.
  CHECK(IsAligned(data));
  return base::WrapUnique(new AudioBus(params.channels(),
                                       params.frames_per_buffer(),
                                       static_cast<float*>(data)));
}

std::unique_ptr<const AudioBus> AudioBus::WrapReadOnlyMemory(int channels,
                                                             int frames,
                                                             const void* data) {
  // Note: const_cast is generally dangerous but is used in this case since
  // AudioBus accommodates both read-only and read/write use cases. A const
  // AudioBus object is returned to ensure no one accidentally writes to the
  // read-only data.
  return WrapMemory(channels, frames, const_cast<void*>(data));
}

std::unique_ptr<const AudioBus> AudioBus::WrapReadOnlyMemory(
    const AudioParameters& params,
    const void* data) {
  // Note: const_cast is generally dangerous but is used in this case since
  // AudioBus accommodates both read-only and read/write use cases. A const
  // AudioBus object is returned to ensure no one accidentally writes to the
  // read-only data.
  return WrapMemory(params, const_cast<void*>(data));
}

void AudioBus::SetChannelData(int channel, float* data) {
  CHECK(is_wrapper_);
  CHECK(data);
  CHECK_GE(channel, 0);
  CHECK_LT(static_cast<size_t>(channel), channel_data_.size());
  DCHECK(IsAligned(data));
  channel_data_[channel] = data;
}

void AudioBus::set_frames(int frames) {
  CHECK(is_wrapper_);
  ValidateConfig(static_cast<int>(channel_data_.size()), frames);
  frames_ = frames;
}

void AudioBus::SetWrappedDataDeleter(base::OnceClosure deleter) {
  CHECK(is_wrapper_);
  DCHECK(!wrapped_data_deleter_cb_);
  wrapped_data_deleter_cb_ = std::move(deleter);
}

size_t AudioBus::GetBitstreamDataSize() const {
  DCHECK(is_bitstream_format_);
  return bitstream_data_size_;
}

void AudioBus::SetBitstreamDataSize(size_t data_size) {
  DCHECK(is_bitstream_format_);
  bitstream_data_size_ = data_size;
}

int AudioBus::GetBitstreamFrames() const {
  DCHECK(is_bitstream_format_);
  return bitstream_frames_;
}

void AudioBus::SetBitstreamFrames(int frames) {
  DCHECK(is_bitstream_format_);
  bitstream_frames_ = frames;
}

void AudioBus::ZeroFramesPartial(int start_frame, int frames) {
  CheckOverflow(start_frame, frames, frames_);

  if (frames <= 0)
    return;

  if (is_bitstream_format_) {
    // No need to clean unused region for bitstream formats.
    if (start_frame >= bitstream_frames_)
      return;

    // Cannot clean partial frames.
    DCHECK_EQ(start_frame, 0);
    DCHECK(frames >= bitstream_frames_);

    // For compressed bitstream, zeroed buffer is not valid and would be
    // discarded immediately. It is faster and makes more sense to reset
    // |bitstream_data_size_| and |is_bitstream_format_| so that the buffer
    // contains no data instead of zeroed data.
    SetBitstreamDataSize(0);
    SetBitstreamFrames(0);
    return;
  }

  for (size_t i = 0; i < channel_data_.size(); ++i) {
    memset(channel_data_[i] + start_frame, 0,
           frames * sizeof(*channel_data_[i]));
  }
}

void AudioBus::ZeroFrames(int frames) {
  ZeroFramesPartial(0, frames);
}

void AudioBus::Zero() {
  ZeroFrames(frames_);
}

bool AudioBus::AreFramesZero() const {
  DCHECK(!is_bitstream_format_);
  for (const auto* channel : channel_data_) {
    for (int j = 0; j < frames_; ++j) {
      if (channel[j]) {
        return false;
      }
    }
  }
  return true;
}

// static
int AudioBus::CalculateMemorySize(const AudioParameters& params) {
  return CalculateMemorySizeInternal(
      params.channels(), params.frames_per_buffer(), NULL);
}

// static
int AudioBus::CalculateMemorySize(int channels, int frames) {
  return CalculateMemorySizeInternal(channels, frames, NULL);
}

// static
bool AudioBus::IsAligned(void* ptr) {
  return base::IsAligned(ptr, kChannelAlignment);
}

void AudioBus::BuildChannelData(int channels, int aligned_frames, float* data) {
  DCHECK(!is_bitstream_format_);
  DCHECK(IsAligned(data));
  DCHECK_EQ(channel_data_.size(), 0U);
  // Initialize |channel_data_| with pointers into |data|.
  channel_data_.reserve(channels);
  for (int i = 0; i < channels; ++i)
    channel_data_.push_back(data + i * aligned_frames);
}

void AudioBus::CopyTo(AudioBus* dest) const {
  dest->set_is_bitstream_format(is_bitstream_format());
  if (is_bitstream_format()) {
    dest->SetBitstreamDataSize(GetBitstreamDataSize());
    dest->SetBitstreamFrames(GetBitstreamFrames());
    memcpy(dest->channel(0), channel(0), GetBitstreamDataSize());
    return;
  }

  CopyPartialFramesTo(0, frames(), 0, dest);
}

void AudioBus::CopyAndClipTo(AudioBus* dest) const {
  DCHECK(!is_bitstream_format_);
  CHECK_EQ(channels(), dest->channels());
  CHECK_LE(frames(), dest->frames());
  for (int i = 0; i < channels(); ++i) {
    float* dest_ptr = dest->channel(i);
    const float* source_ptr = channel(i);
    for (int j = 0; j < frames(); ++j)
      dest_ptr[j] = Float32SampleTypeTraits::FromFloat(source_ptr[j]);
  }
}

void AudioBus::CopyPartialFramesTo(int source_start_frame,
                                   int frame_count,
                                   int dest_start_frame,
                                   AudioBus* dest) const {
  DCHECK(!is_bitstream_format_);
  CHECK_EQ(channels(), dest->channels());
  CHECK_LE(source_start_frame + frame_count, frames());
  CHECK_LE(dest_start_frame + frame_count, dest->frames());

  // Since we don't know if the other AudioBus is wrapped or not (and we don't
  // want to care), just copy using the public channel() accessors.
  for (int i = 0; i < channels(); ++i) {
    memcpy(dest->channel(i) + dest_start_frame,
           channel(i) + source_start_frame,
           sizeof(*channel(i)) * frame_count);
  }
}

void AudioBus::Scale(float volume) {
  DCHECK(!is_bitstream_format_);
  if (volume > 0 && volume != 1) {
    for (int i = 0; i < channels(); ++i)
      vector_math::FMUL(channel(i), volume, frames(), channel(i));
  } else if (volume == 0) {
    Zero();
  }
}

void AudioBus::SwapChannels(int a, int b) {
  DCHECK(!is_bitstream_format_);
  DCHECK(a < channels() && a >= 0);
  DCHECK(b < channels() && b >= 0);
  DCHECK_NE(a, b);
  std::swap(channel_data_[a], channel_data_[b]);
}

}  // namespace media
