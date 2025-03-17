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

#include <algorithm>
#include <limits>
#include <utility>

#include "base/bits.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/types/zip.h"
#include "media/base/audio_parameters.h"
#include "media/base/limits.h"
#include "media/base/vector_math.h"

namespace media {

// Returns `frames` rounded up to the nearest number which allows full rows of
// SIMD instructions.
static size_t AlignFramesUp(size_t frames) {
  // Since our internal sample format is float, we can guarantee the alignment
  // by making the number of frames an integer multiple of
  // AudioBus::kChannelAlignment / sizeof(float).
  return base::bits::AlignUp(frames * sizeof(float),
                             AudioBus::kChannelAlignment) /
         sizeof(float);
}

// In order to guarantee that the memory block for each channel starts at an
// aligned address when splitting a contiguous block of memory into one block
// per channel, we may have to make these blocks larger than otherwise needed.
// We do this by allocating space for potentially more frames than requested.
// This method returns the required size for the contiguous memory block
// in bytes and outputs the adjusted number of frames via |out_aligned_frames|.
static size_t CalculateMemorySizeInternal(int channels, size_t frames) {
  return sizeof(float) * channels * AlignFramesUp(frames);
}

static bool IsValidChannelCount(int channels) {
  CHECK_GT(channels, 0);
  return base::checked_cast<size_t>(channels) <=
         static_cast<size_t>(limits::kMaxChannels);
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
    : frames_(base::checked_cast<size_t>(frames)) {
  CHECK(IsValidChannelCount(channels));

  // Over-allocate memory to make sure each channel can start at an aligned
  // location.
  size_t total_samples = channels * AlignFramesUp(frames_);

  data_ =
      base::AlignedUninit<float>(total_samples, AudioBus::kChannelAlignment);

  reserved_memory_ =
      base::as_writable_bytes(base::allow_nonunique_obj, data_.as_span());

  BuildChannelData(channels, data_);
}

AudioBus::AudioBus(int channels, int frames, float* data)
    : AudioBus(
          channels,
          frames,
          // Per interface contract, `data` must have a size of at least
          // CalculateMemorySizeInternal().
          base::span(data, CalculateMemorySizeInternal(channels, frames))) {}

AudioBus::AudioBus(int channels, int frames, base::span<float> data)
    : frames_(base::checked_cast<size_t>(frames)) {
  CHECK(IsValidChannelCount(channels));

  // Since |data| may have come from an external source, ensure it's valid.
  CHECK(!data.empty());
  CHECK(IsAligned(data));

  reserved_memory_ =
      base::as_writable_byte_span(base::allow_nonunique_obj, data);

  // `data` must be at least CalculateMemorySizeInternal(), per interface
  // contract.
  BuildChannelData(channels, data);
}

AudioBus::AudioBus(int frames, const std::vector<float*>& channel_data)
    : frames_(base::checked_cast<size_t>(frames)) {
  CHECK(IsValidChannelCount(channel_data.size()));
  channel_data_.reserve(channel_data.size());

  for (float* data : channel_data) {
    CHECK(IsAligned(data));
    channel_data_.emplace_back(data, frames_);
  }
}

AudioBus::AudioBus(int channels) : channel_data_(channels), is_wrapper_(true) {
  CHECK(IsValidChannelCount(channels));
}

AudioBus::~AudioBus() {
  if (wrapped_data_deleter_cb_) {
    std::move(wrapped_data_deleter_cb_).Run();
  }
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

void AudioBus::SetChannelData(int channel, Channel data) {
  CHECK(is_wrapper_);

  // Make sure `set_frames()` was called first.
  CHECK(frames_);
  CHECK_EQ(data.size(), frames_);

  CHECK_GE(channel, 0);
  CHECK_LT(static_cast<size_t>(channel), channel_data_.size());
  CHECK(IsAligned(data));
  channel_data_[channel] = data;
}

void AudioBus::SetAllChannels(const ChannelVector& channels) {
  CHECK(is_wrapper_);

  // Make sure `set_frames()` was called first.
  CHECK(frames_);

  CHECK(!channels.empty());
  CHECK_EQ(channels.size(), channel_data_.size());

  for (auto channel : channels) {
    CHECK(IsAligned(channel));
    CHECK_EQ(channel.size(), frames_);
  }

  channel_data_ = channels;
}

void AudioBus::set_frames(int frames) {
  CHECK(is_wrapper_);
  frames_ = base::checked_cast<size_t>(frames);
}

void AudioBus::SetWrappedDataDeleter(base::OnceClosure deleter) {
  CHECK(is_wrapper_);
  DCHECK(!wrapped_data_deleter_cb_);
  wrapped_data_deleter_cb_ = std::move(deleter);
}

void AudioBus::SetBitstreamSize(size_t data_size) {
  CHECK(is_bitstream_format_);
  bitstream_data_ = reserved_memory_.first(data_size);
}

int AudioBus::GetBitstreamFrames() const {
  CHECK(is_bitstream_format_);
  return bitstream_frames_;
}

void AudioBus::SetBitstreamFrames(size_t frames) {
  CHECK(is_bitstream_format_);
  bitstream_frames_ = frames;
}

void AudioBus::ZeroFramesPartial(int start, int count) {
  CHECK(!is_bitstream_format_);

  // TODO(crbug.com/373960632): Update the parameters to be `size_t`.
  size_t start_frame = base::checked_cast<size_t>(start);
  size_t frames = base::checked_cast<size_t>(count);

  if (!frames) {
    // Nothing to do.
    return;
  }

  for (auto channel : channel_data_) {
    std::ranges::fill(channel.subspan(start_frame, frames), 0);
  }
}

void AudioBus::ZeroFrames(int frames) {
  CHECK(!bitstream_frames_);

  ZeroFramesPartial(0, frames);
}

void AudioBus::Zero() {
  if (is_bitstream_format_) {
    ZeroBitstream();
    return;
  }

  ZeroFrames(frames_);
}

void AudioBus::ZeroBitstream() {
  CHECK(is_bitstream_format_);
  SetBitstreamSize(0u);
  SetBitstreamFrames(0u);
}

bool AudioBus::AreFramesZero() const {
  CHECK(!is_bitstream_format_);
  for (Channel channel : channel_data_) {
    if (std::ranges::any_of(channel, [](float frame) { return frame != 0; })) {
      return false;
    }
  }
  return true;
}

// static
size_t AudioBus::CalculateMemorySize(const AudioParameters& params) {
  return CalculateMemorySizeInternal(
      params.channels(),
      base::checked_cast<size_t>(params.frames_per_buffer()));
}

// static
size_t AudioBus::CalculateMemorySize(int channels, int frames) {
  return CalculateMemorySizeInternal(channels,
                                     base::checked_cast<size_t>(frames));
}

// static
bool AudioBus::IsAligned(void* ptr) {
  return base::IsAligned(ptr, kChannelAlignment);
}

// static
bool AudioBus::IsAligned(base::span<float> span) {
  return IsAligned(span.data());
}

void AudioBus::BuildChannelData(int channels, base::span<float> data) {
  CHECK(!is_bitstream_format_);
  CHECK(frames_);
  CHECK(IsValidChannelCount(channels));
  CHECK_GE(data.size_bytes(), CalculateMemorySizeInternal(channels, frames_));
  CHECK(IsAligned(data));
  CHECK(channel_data_.empty());

  // Initialize |channel_data_| with pointers into |data|.
  channel_data_.reserve(channels);
  const size_t frames_per_channel = AlignFramesUp(frames_);

  for (int i = 0; i < channels; ++i) {
    // We might have over-allocated memory for alignment purposes, but we only
    // want to expose the first `frames_`.
    channel_data_.push_back(data.subspan(i * frames_per_channel, frames_));
    CHECK(IsAligned(channel_data_.back().data()));
  }
}

void AudioBus::CopyTo(AudioBus* dest) const {
  dest->set_is_bitstream_format(is_bitstream_format());
  if (is_bitstream_format()) {
    dest->SetBitstreamSize(bitstream_data_.size());
    dest->SetBitstreamFrames(bitstream_frames_);

    dest->bitstream_data().copy_from_nonoverlapping(bitstream_data_);
    return;
  }

  CopyPartialFramesTo(0, frames(), 0, dest);
}

void AudioBus::CopyAndClipTo(AudioBus* dest) const {
  DCHECK(!is_bitstream_format_);
  CHECK_EQ(channels(), dest->channels());
  CHECK_LE(frames(), dest->frames());
  for (auto [src_ch, dest_ch] : base::zip(channel_data_, dest->AllChannels())) {
    vector_math::FCLAMP(src_ch, dest_ch);
  }
}

void AudioBus::CopyPartialFramesTo(int source_start_frame,
                                   int frame_count,
                                   int dest_start_frame,
                                   AudioBus* dest) const {
  DCHECK(!is_bitstream_format_);
  CHECK_EQ(channels(), dest->channels());

  const size_t source_offset = base::checked_cast<size_t>(source_start_frame);
  const size_t dest_offset = base::checked_cast<size_t>(dest_start_frame);
  const size_t count = base::checked_cast<size_t>(frame_count);

  ChannelVector src_channels = AllChannelsSubspan(source_offset, count);
  ChannelVector dest_channels = dest->AllChannelsSubspan(dest_offset, count);

  // Since we don't know if the other AudioBus is wrapped or not (and we don't
  // want to care), just copy using the channel accessors.
  for (auto [src_span, dest_span] : base::zip(src_channels, dest_channels)) {
    dest_span.copy_from_nonoverlapping(src_span);
  }
}

void AudioBus::Scale(float volume) {
  DCHECK(!is_bitstream_format_);
  if (volume > 0 && volume != 1) {
    for (auto channel : channel_data_) {
      vector_math::FMUL(channel, volume, channel);
    }
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

const AudioBus::ChannelVector& AudioBus::AllChannels() const {
  return channel_data_;
}

AudioBus::ChannelVector AudioBus::AllChannelsSubspan(size_t offset,
                                                     size_t count) const {
  ChannelVector sub_channels;

  for (Channel channel : channel_data_) {
    sub_channels.push_back(channel.subspan(offset, count));
  }

  return sub_channels;
}

}  // namespace media
