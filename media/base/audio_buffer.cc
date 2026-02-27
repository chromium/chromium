// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_buffer.h"

#include <cmath>

#include "base/bits.h"
#include "base/compiler_specific.h"
#include "base/containers/heap_array.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/types/pass_key.h"
#include "base/types/zip.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_sample_types.h"
#include "media/base/limits.h"
#include "media/base/timestamp_constants.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace media {

AudioBuffer::ExternalMemory::ExternalMemory() = default;
AudioBuffer::ExternalMemory::ExternalMemory(base::span<uint8_t> span)
    : span_(span) {}
AudioBuffer::ExternalMemory::~ExternalMemory() = default;
AudioBuffer::ExternalMemory::ExternalMemory(const ExternalMemory&) = default;
AudioBuffer::ExternalMemory::ExternalMemory(ExternalMemory&&) = default;

namespace {

class SelfOwnedMemory : public AudioBuffer::ExternalMemory {
 public:
  explicit SelfOwnedMemory(size_t size)
      : heap_array_(
            base::AlignedUninit<uint8_t>(size, AudioBus::kChannelAlignment)) {
    span_ = heap_array_.as_span();
  }

 private:
  base::AlignedHeapArray<uint8_t> heap_array_;
};

std::unique_ptr<AudioBuffer::ExternalMemory> AllocateMemory(size_t size) {
  return std::make_unique<SelfOwnedMemory>(size);
}

template <typename T>
base::span<T> CastSpan(base::span<uint8_t> span) {
  CHECK_EQ(span.size() % sizeof(T), 0u);
  CHECK(base::IsAligned(span.data(), alignof(T)));
  // SAFETY: Spanification documentation strongly discourages
  // `reinterpret_cast`, but it is a necessary evil throughout this file, as we
  // store multiple format types as bytes. Checking that the data is aligned
  // and that the size divisible is the best we can do here.
  return UNSAFE_BUFFERS(
      base::span(reinterpret_cast<T*>(span.data()), span.size() / sizeof(T)));
}

template <typename T>
base::span<const T> CastConstSpan(base::span<const uint8_t> span) {
  CHECK_EQ(span.size() % sizeof(T), 0u);
  CHECK(base::IsAligned(span.data(), alignof(T)));
  // SAFETY: See `CastSpan()` comment.
  return UNSAFE_BUFFERS(base::span(reinterpret_cast<const T*>(span.data()),
                                   span.size() / sizeof(T)));
}

template <>
base::span<const uint8_t> CastConstSpan(base::span<const uint8_t> span) {
  return span;
}

template <typename SampleTypeTraits>
void PlanarRead(AudioBus* dest,
                const std::vector<base::raw_span<uint8_t>>& source,
                size_t dest_offset,
                size_t source_offset,
                size_t frames) {
  using SourceValueType = typename SampleTypeTraits::ValueType;

  CHECK_EQ(static_cast<size_t>(dest->channels()), source.size());
  for (auto [dest_ch, source_ch] : base::zip(dest->AllChannels(), source)) {
    auto dest_data = dest_ch.subspan(dest_offset, frames);
    auto source_data = CastConstSpan<SourceValueType>(source_ch).subspan(
        source_offset, frames);

    std::ranges::transform(source_data, dest_data.begin(),
                           SampleTypeTraits::ToFloat);
  }
}

}  // namespace

static base::TimeDelta CalculateDuration(int frames, double sample_rate) {
  DCHECK_GT(sample_rate, 0);
  return base::Microseconds(frames * base::Time::kMicrosecondsPerSecond /
                            sample_rate);
}

AudioBufferMemoryPool::AudioBufferMemoryPool()
    : AudioBufferMemoryPool(AudioBus::kChannelAlignment) {}
AudioBufferMemoryPool::AudioBufferMemoryPool(size_t alignment)
    : alignment_(alignment) {}
AudioBufferMemoryPool::~AudioBufferMemoryPool() = default;

AudioBufferMemoryPool::ExternalMemoryFromPool::ExternalMemoryFromPool(
    ExternalMemoryFromPool&& am) = default;
AudioBufferMemoryPool::ExternalMemoryFromPool::ExternalMemoryFromPool(
    scoped_refptr<AudioBufferMemoryPool> pool,
    base::AlignedHeapArray<uint8_t> memory)
    : memory_(std::move(memory)), pool_(std::move(pool)) {
  span_ = memory_.as_span();
}

AudioBufferMemoryPool::ExternalMemoryFromPool::~ExternalMemoryFromPool() {
  if (pool_) {
    // Entry is destroyed outside of the pool and the memory needs to be
    // returned to the pool. But we need to unplug the pool pointer first
    // in order to avoid circular dependencies pool<->memory.
    auto pool = std::move(pool_);
    pool->ReturnBuffer(std::move(*this));
  }
}

size_t AudioBufferMemoryPool::GetPoolSizeForTesting() {
  base::AutoLock al(entry_lock_);
  return entries_.size();
}

std::unique_ptr<AudioBufferMemoryPool::ExternalMemoryFromPool>
AudioBufferMemoryPool::CreateBuffer(size_t size) {
  base::AutoLock al(entry_lock_);
  while (!entries_.empty()) {
    ExternalMemoryFromPool entry = std::move(entries_.front());
    entries_.pop_front();
    if (entry.span().size() == size) {
      // Before giving away the memory, set where it should be returned to.
      entry.pool_ = this;
      return std::make_unique<ExternalMemoryFromPool>(std::move(entry));
    }
  }

  // FFmpeg may not always initialize the entire output memory, so just like
  // for VideoFrames we need to zero out the memory. https://crbug.com/1144070.
  auto memory = base::AlignedUninit<uint8_t>(size, GetChannelAlignment());
  std::ranges::fill(memory, 0u);
  return std::make_unique<ExternalMemoryFromPool>(
      ExternalMemoryFromPool(this, std::move(memory)));
}

void AudioBufferMemoryPool::ReturnBuffer(ExternalMemoryFromPool memory) {
  base::AutoLock al(entry_lock_);
  entries_.emplace_back(std::move(memory));
}

AudioBuffer::AudioBuffer(base::PassKey<AudioBuffer>,
                         SampleFormat sample_format,
                         ChannelLayout channel_layout,
                         int channel_count,
                         int sample_rate,
                         int frame_count,
                         bool create_buffer,
                         const uint8_t* const* data,
                         const size_t data_size,
                         const base::TimeDelta timestamp,
                         scoped_refptr<AudioBufferMemoryPool> pool)
    : sample_format_(sample_format),
      channel_layout_(channel_layout),
      channel_count_(channel_count),
      sample_rate_(sample_rate),
      adjusted_frame_count_(frame_count),
      end_of_stream_(!create_buffer && !data && !frame_count),
      timestamp_(timestamp),
      duration_(end_of_stream_
                    ? base::TimeDelta()
                    : CalculateDuration(adjusted_frame_count_, sample_rate_)),
      data_size_(data_size),
      pool_(std::move(pool)) {
  CHECK_GE(channel_count_, 0);
  CHECK_LE(channel_count_, limits::kMaxChannels);
  CHECK_GE(frame_count, 0);

  CHECK(channel_layout == CHANNEL_LAYOUT_DISCRETE ||
        ChannelLayoutToChannelCount(channel_layout) == channel_count);

  const size_t bytes_per_channel = SampleFormatToBytesPerChannel(sample_format);
  const size_t channel_alignment =
      pool_ ? pool_->GetChannelAlignment() : AudioBus::kChannelAlignment;
  CHECK_LE(bytes_per_channel, channel_alignment);

  // Empty buffer?
  if (!create_buffer) {
    return;
  }

  absl::Cleanup populate_channel_data_on_exit = [this] {
    PopulateChannelData();
  };

  CHECK_NE(sample_format, kUnknownSampleFormat);

  if (sample_format == kSampleFormatIECDts) {
    // Allocate a contiguous buffer for IEC61937 encapsulated Bitstream.
    const size_t forced_data_size =
        frame_count * bytes_per_channel * channel_count_;
    CHECK_LE(data_size, forced_data_size);
    data_size_ = forced_data_size;
    data_ =
        pool_ ? pool_->CreateBuffer(data_size_) : AllocateMemory(data_size_);
    channel_spans_.push_back(data_->span());

    auto needs_zeroing = data_->span();

    // Copy data
    if (data) {
      // Note: `data_size` is the external data size, not `data_size_`.
      auto [data_portion, zero_portion] = data_->span().split_at(data_size);

      data_portion.copy_from_nonoverlapping(
          UNSAFE_TODO(base::span(data[0], data_size)));
      needs_zeroing = zero_portion;
    }

    std::ranges::fill(needs_zeroing, 0u);
    return;
  }

  const size_t data_size_per_channel = frame_count * bytes_per_channel;
  if (IsPlanar(sample_format)) {
    DCHECK(!IsBitstreamFormat()) << sample_format_;
    // Planar data, so need to allocate buffer for each channel.
    // Determine per channel data size, taking into account alignment.
    const size_t block_size_per_channel =
        base::bits::AlignUp(data_size_per_channel, channel_alignment);
    CHECK_GE(block_size_per_channel, data_size_per_channel);

    // Allocate a contiguous buffer for all the channel data.
    data_size_ = channel_count_ * block_size_per_channel;
    data_ =
        pool_ ? pool_->CreateBuffer(data_size_) : AllocateMemory(data_size_);
    channel_spans_.reserve(channel_count_);

    auto remaining_channels = data_->span();

    // Copy each channel's data into the appropriate spot.
    for (int i = 0; i < channel_count_; ++i) {
      auto [channel, rem] = remaining_channels.split_at(block_size_per_channel);
      if (data) {
        channel.first(data_size_per_channel)
            .copy_from_nonoverlapping(
                UNSAFE_TODO(base::span(data[i], data_size_per_channel)));
      }
      channel_spans_.push_back(channel);
      remaining_channels = rem;
    }

    CHECK(remaining_channels.empty());
    return;
  }

  // Remaining formats are interleaved data.
  CHECK(IsInterleaved(sample_format)) << sample_format_;
  // Allocate our own buffer and copy the supplied data into it. Buffer must
  // contain the data for all channels.
  if (!IsBitstreamFormat()) {
    data_size_ = data_size_per_channel * channel_count_;
  } else {
    DCHECK_GT(data_size_, 0u);
  }

  data_ = pool_ ? pool_->CreateBuffer(data_size_) : AllocateMemory(data_size_);
  channel_spans_.push_back(data_->span());
  if (data) {
    data_->span().copy_from_nonoverlapping(
        UNSAFE_TODO(base::span(data[0], data_size_)));
  }
}

AudioBuffer::AudioBuffer(base::PassKey<AudioBuffer>,
                         SampleFormat sample_format,
                         ChannelLayout channel_layout,
                         int channel_count,
                         int sample_rate,
                         int frame_count,
                         const base::TimeDelta timestamp,
                         std::unique_ptr<ExternalMemory> external_memory)
    : sample_format_(sample_format),
      channel_layout_(channel_layout),
      channel_count_(channel_count),
      sample_rate_(sample_rate),
      adjusted_frame_count_(frame_count),
      end_of_stream_(false),
      timestamp_(timestamp),
      duration_(end_of_stream_
                    ? base::TimeDelta()
                    : CalculateDuration(adjusted_frame_count_, sample_rate_)),
      data_(std::move(external_memory)) {
  CHECK_GE(channel_count_, 0);
  CHECK_LE(channel_count_, limits::kMaxChannels);
  CHECK_GE(frame_count, 0);
  CHECK_NE(sample_format, kUnknownSampleFormat);
  CHECK(data_);

  CHECK(channel_layout == CHANNEL_LAYOUT_DISCRETE ||
        ChannelLayoutToChannelCount(channel_layout) == channel_count);

  if (IsBitstreamFormat()) {
    data_size_ = data_->span().size();
    CHECK_GT(data_size_, 0u);
    return;
  }

  absl::Cleanup populate_channel_data_on_exit = [this] {
    PopulateChannelData();
  };

  const size_t bytes_per_channel = SampleFormatToBytesPerChannel(sample_format);
  const size_t data_size_per_channel = frame_count * bytes_per_channel;

  data_size_ = channel_count_ * data_size_per_channel;
  CHECK_GE(data_->span().size(), data_size_);

  if (IsInterleaved(sample_format)) {
    channel_spans_.push_back(data_->span());
    return;
  }

  if (IsPlanar(sample_format)) {
    // Planar data, so need to set up pointers for each channel.
    channel_spans_.reserve(channel_count_);

    auto remaining_channels = data_->span();
    // Set each channel's data pointer into the appropriate spot.
    for (int i = 0; i < channel_count_; ++i) {
      auto [channel, rem] = remaining_channels.split_at(data_size_per_channel);
      channel_spans_.push_back(channel);
      remaining_channels = rem;
    }
    return;
  }

  NOTREACHED() << sample_format;
}

AudioBuffer::~AudioBuffer() = default;

// static
scoped_refptr<AudioBuffer> AudioBuffer::CopyFrom(
    SampleFormat sample_format,
    ChannelLayout channel_layout,
    int channel_count,
    int sample_rate,
    int frame_count,
    const uint8_t* const* data,
    const base::TimeDelta timestamp,
    scoped_refptr<AudioBufferMemoryPool> pool) {
  // If you hit this CHECK you likely have a bug in a demuxer. Go fix it.
  CHECK_GT(frame_count, 0);  // Otherwise looks like an EOF buffer.
  CHECK(data[0]);
  return base::MakeRefCounted<AudioBuffer>(
      base::PassKey<AudioBuffer>(), sample_format, channel_layout,
      channel_count, sample_rate, frame_count, true, data, 0, timestamp,
      std::move(pool));
}

// static
scoped_refptr<AudioBuffer> AudioBuffer::CopyFrom(
    ChannelLayout channel_layout,
    int sample_rate,
    const base::TimeDelta timestamp,
    const AudioBus* audio_bus,
    scoped_refptr<AudioBufferMemoryPool> pool) {
  DCHECK(audio_bus->frames());

  const int channel_count = audio_bus->channels();
  DCHECK(channel_count);

  std::vector<const uint8_t*> data(channel_count);
  for (int ch = 0; ch < channel_count; ch++) {
    data[ch] = reinterpret_cast<const uint8_t*>(audio_bus->channel(ch).data());
  }

  return CopyFrom(kSampleFormatPlanarF32, channel_layout, channel_count,
                  sample_rate, audio_bus->frames(), data.data(), timestamp,
                  std::move(pool));
}

// static
scoped_refptr<AudioBuffer> AudioBuffer::CopyFrom(
    int sample_rate,
    const base::TimeDelta timestamp,
    const AudioBus* audio_bus,
    scoped_refptr<AudioBufferMemoryPool> pool) {
  const int channel_count = audio_bus->channels();
  DCHECK(channel_count);

  return CopyFrom(GuessChannelLayout(channel_count), sample_rate, timestamp,
                  audio_bus, std::move(pool));
}

// static
scoped_refptr<AudioBuffer> AudioBuffer::CopyBitstreamFrom(
    SampleFormat sample_format,
    ChannelLayout channel_layout,
    int channel_count,
    int sample_rate,
    int frame_count,
    const uint8_t* const* data,
    const size_t data_size,
    const base::TimeDelta timestamp,
    scoped_refptr<AudioBufferMemoryPool> pool) {
  // If you hit this CHECK you likely have a bug in a demuxer. Go fix it.
  CHECK_GT(frame_count, 0);  // Otherwise looks like an EOF buffer.
  CHECK(data[0]);
  return base::MakeRefCounted<AudioBuffer>(
      base::PassKey<AudioBuffer>(), sample_format, channel_layout,
      channel_count, sample_rate, frame_count, true, data, data_size, timestamp,
      std::move(pool));
}

// static
scoped_refptr<AudioBuffer> AudioBuffer::CreateBuffer(
    SampleFormat sample_format,
    ChannelLayout channel_layout,
    int channel_count,
    int sample_rate,
    int frame_count,
    scoped_refptr<AudioBufferMemoryPool> pool) {
  CHECK_GT(frame_count, 0);  // Otherwise looks like an EOF buffer.
  return base::MakeRefCounted<AudioBuffer>(
      base::PassKey<AudioBuffer>(), sample_format, channel_layout,
      channel_count, sample_rate, frame_count, true, nullptr, 0, kNoTimestamp,
      std::move(pool));
}

// static
scoped_refptr<AudioBuffer> AudioBuffer::CreateBitstreamBuffer(
    SampleFormat sample_format,
    ChannelLayout channel_layout,
    int channel_count,
    int sample_rate,
    int frame_count,
    size_t data_size,
    scoped_refptr<AudioBufferMemoryPool> pool) {
  CHECK_GT(frame_count, 0);  // Otherwise looks like an EOF buffer.
  return base::MakeRefCounted<AudioBuffer>(
      base::PassKey<AudioBuffer>(), sample_format, channel_layout,
      channel_count, sample_rate, frame_count, true, nullptr, data_size,
      kNoTimestamp, std::move(pool));
}

// static
scoped_refptr<AudioBuffer> AudioBuffer::CreateEmptyBuffer(
    ChannelLayout channel_layout,
    int channel_count,
    int sample_rate,
    int frame_count,
    const base::TimeDelta timestamp) {
  CHECK_GT(frame_count, 0);  // Otherwise looks like an EOF buffer.
  // Since data == nullptr, format doesn't matter.
  return base::MakeRefCounted<AudioBuffer>(
      base::PassKey<AudioBuffer>(), kSampleFormatF32, channel_layout,
      channel_count, sample_rate, frame_count, false, nullptr, 0, timestamp,
      nullptr);
}

// static
scoped_refptr<AudioBuffer> AudioBuffer::CreateFromExternalMemory(
    SampleFormat sample_format,
    ChannelLayout channel_layout,
    int channel_count,
    int sample_rate,
    int frame_count,
    const base::TimeDelta timestamp,
    std::unique_ptr<AudioBuffer::ExternalMemory> external_memory) {
  CHECK_GT(frame_count, 0);
  return base::MakeRefCounted<AudioBuffer>(
      base::PassKey<AudioBuffer>(), sample_format, channel_layout,
      channel_count, sample_rate, frame_count, timestamp,
      std::move(external_memory));
}

// static
scoped_refptr<AudioBuffer> AudioBuffer::CreateEOSBuffer() {
  return base::MakeRefCounted<AudioBuffer>(
      base::PassKey<AudioBuffer>(), kUnknownSampleFormat, CHANNEL_LAYOUT_NONE,
      0, 0, 0, false, nullptr, 0, kNoTimestamp, nullptr);
}

// static
std::unique_ptr<AudioBus> AudioBuffer::WrapOrCopyToAudioBus(
    scoped_refptr<AudioBuffer> buffer) {
  DCHECK(buffer);

  const int channels = buffer->channel_count();
  const size_t frames = buffer->frame_count();

  CHECK(channels);
  CHECK(frames);

  // `buffer` might already have the right memory layout (aligned floats).
  // Prevent a data copy by wrapping it instead.
  bool audiobus_compatible = false;
  if (buffer->sample_format() == SampleFormat::kSampleFormatPlanarF32) {
    audiobus_compatible = std::ranges::all_of(
        buffer->channel_spans_,
        [](auto channel) { return AudioBus::IsAligned(channel.data()); });
  }

  if (audiobus_compatible) {
    auto audio_bus = AudioBus::CreateWrapper(channels);

    audio_bus->set_frames(frames);

    for (int ch = 0; ch < channels; ++ch) {
      audio_bus->SetChannelData(
          ch, CastSpan<float>(buffer->channel_spans_[ch]).first(frames));
    }

    // Keep `buffer` alive as long as `audio_bus`.
    audio_bus->SetWrappedDataDeleter(
        base::DoNothingWithBoundArgs(std::move(buffer)));

    return audio_bus;
  }

  // `buffer` can't be wrapped directly. Convert and copy it instead.
  auto audio_bus = AudioBus::Create(channels, frames);
  buffer->ReadFrames(frames, 0, 0, audio_bus.get());

  return audio_bus;
}

void AudioBuffer::AdjustSampleRate(int sample_rate) {
  DCHECK(!end_of_stream_);
  sample_rate_ = sample_rate;
  duration_ = CalculateDuration(adjusted_frame_count_, sample_rate_);
}

void AudioBuffer::ReadFrames(int frames_to_copy,
                             int source_frame_offset,
                             int dest_frame_offset,
                             AudioBus* dest) const {
  // Deinterleave each channel (if necessary) and convert to 32bit
  // floating-point with nominal range -1.0 -> +1.0 (if necessary).

  // |dest| must have the same number of channels, and the number of frames
  // specified must be in range.
  DCHECK(!end_of_stream());
  CHECK_EQ(dest->channels(), channel_count_);
  CHECK_LE(source_frame_offset + frames_to_copy, adjusted_frame_count_);
  CHECK_LE(dest_frame_offset + frames_to_copy, dest->frames());

  dest->set_is_bitstream_format(IsBitstreamFormat());

  if (IsBitstreamFormat()) {
    // For bitstream formats, we only support 2 modes: 1) Overwrite the data to
    // the beginning of the destination buffer. 2) Append new data to the end of
    // the existing data.
    CHECK(!source_frame_offset);
    CHECK(!dest_frame_offset ||
          dest_frame_offset == dest->GetBitstreamFrames());

    const bool append_data = dest_frame_offset == dest->GetBitstreamFrames();
    const size_t dest_size = append_data ? dest->bitstream_data().size() : 0u;
    const size_t new_dest_size = dest_size + data_size();

    dest->SetBitstreamSize(new_dest_size);

    auto dest_span = dest->bitstream_data().subspan(dest_size, data_size());
    dest_span.copy_from_nonoverlapping(channel_spans_[0].first(data_size()));

    dest->SetBitstreamFrames(dest_frame_offset + frame_count());
    return;
  }

  if (!data_) {
    // Special case for an empty buffer.
    dest->ZeroFramesPartial(dest_frame_offset, frames_to_copy);
    return;
  }

  const size_t dest_offset = base::checked_cast<size_t>(dest_frame_offset);
  const size_t source_offset = base::checked_cast<size_t>(source_frame_offset);
  const size_t frames = base::checked_cast<size_t>(frames_to_copy);

  // Note: The conversion steps below will clip values to [1.0, -1.0f].

  if (sample_format_ == kSampleFormatPlanarF32) {
    PlanarRead<Float32SampleTypeTraits>(dest, channel_spans_, dest_offset,
                                        source_offset, frames);
    return;
  }
  if (sample_format_ == kSampleFormatPlanarU8) {
    PlanarRead<UnsignedInt8SampleTypeTraits>(dest, channel_spans_, dest_offset,
                                             source_offset, frames);
    return;
  }
  if (sample_format_ == kSampleFormatPlanarS16) {
    PlanarRead<SignedInt16SampleTypeTraits>(dest, channel_spans_, dest_offset,
                                            source_offset, frames);
    return;
  }
  if (sample_format_ == kSampleFormatPlanarS32) {
    PlanarRead<SignedInt32SampleTypeTraits>(dest, channel_spans_, dest_offset,
                                            source_offset, frames);
    return;
  }

  const size_t bytes_per_channel =
      SampleFormatToBytesPerChannel(sample_format_);
  const size_t frame_size = channel_count_ * bytes_per_channel;
  base::span<const uint8_t> source_data =
      data_->span().subspan(source_offset * frame_size, frames * frame_size);

  if (sample_format_ == kSampleFormatF32) {
    dest->FromInterleavedPartial<Float32SampleTypeTraits>(
        CastConstSpan<float>(source_data), dest_offset);
  } else if (sample_format_ == kSampleFormatU8) {
    dest->FromInterleavedPartial<UnsignedInt8SampleTypeTraits>(source_data,
                                                               dest_offset);
  } else if (sample_format_ == kSampleFormatS16) {
    dest->FromInterleavedPartial<SignedInt16SampleTypeTraits>(
        CastConstSpan<int16_t>(source_data), dest_offset);
  } else if (sample_format_ == kSampleFormatS24 ||
             sample_format_ == kSampleFormatS32) {
    dest->FromInterleavedPartial<SignedInt32SampleTypeTraits>(
        CastConstSpan<int32_t>(source_data), dest_offset);
  } else {
    NOTREACHED() << "Unsupported audio sample type: " << sample_format_;
  }
}

void AudioBuffer::TrimStart(int frames_to_trim) {
  CHECK_GE(frames_to_trim, 0);
  CHECK_LE(frames_to_trim, adjusted_frame_count_);

  if (IsBitstreamFormat()) {
    DLOG(ERROR) << "Not allowed to trim an audio bitstream buffer.";
    return;
  }

  TrimRange(0, frames_to_trim);
}

void AudioBuffer::TrimEnd(int frames_to_trim) {
  CHECK_GE(frames_to_trim, 0);
  CHECK_LE(frames_to_trim, adjusted_frame_count_);

  if (IsBitstreamFormat()) {
    DLOG(ERROR) << "Not allowed to trim an audio bitstream buffer.";
    return;
  }

  // Adjust the number of frames and duration for this buffer.
  adjusted_frame_count_ -= frames_to_trim;
  duration_ = CalculateDuration(adjusted_frame_count_, sample_rate_);
}

void AudioBuffer::TrimRange(int start, int end) {
  CHECK_GE(start, 0);
  CHECK_LE(end, adjusted_frame_count_);
  CHECK_LE(start, end);

  if (IsBitstreamFormat()) {
    DLOG(ERROR) << "Not allowed to trim an audio bitstream buffer.";
    return;
  }

  const size_t frames_to_trim = end - start;
  CHECK_LE(frames_to_trim, static_cast<size_t>(adjusted_frame_count_));

  const size_t bytes_per_channel =
      SampleFormatToBytesPerChannel(sample_format_);
  // Empty buffers do not have frames to copy backed by `data_`.
  const size_t frames_to_copy = data_ ? adjusted_frame_count_ - end : 0u;
  if (frames_to_copy > 0) {
    switch (sample_format_) {
      case kSampleFormatPlanarU8:
      case kSampleFormatPlanarS16:
      case kSampleFormatPlanarF32:
      case kSampleFormatPlanarS32: {
        const size_t memory_size = bytes_per_channel * frames_to_copy;
        const size_t start_offset = start * bytes_per_channel;
        const size_t end_offset = end * bytes_per_channel;
        for (auto channel : channel_spans_) {
          // Note: do not use `copy_from_non_overlapping` here.
          channel.subspan(start_offset, memory_size)
              .copy_from(channel.subspan(end_offset, memory_size));
        }
        break;
      }
      case kSampleFormatU8:
      case kSampleFormatS16:
      case kSampleFormatS24:
      case kSampleFormatS32:
      case kSampleFormatF32: {
        // Interleaved data can be shifted all at once.
        const size_t frame_size = channel_count_ * bytes_per_channel;
        const size_t copy_size = frames_to_copy * frame_size;
        auto interleaved_data = channel_spans_[0];
        // Note: do not use `copy_from_non_overlapping` here.
        interleaved_data.subspan(start * frame_size, copy_size)
            .copy_from(interleaved_data.subspan(end * frame_size, copy_size));
        break;
      }
      case kUnknownSampleFormat:
      case kSampleFormatAc3:
      case kSampleFormatEac3:
      case kSampleFormatMpegHAudio:
      case kSampleFormatDts:
      case kSampleFormatDtsxP2:
      case kSampleFormatIECDts:
      case kSampleFormatDtse:
        NOTREACHED() << "Invalid sample format!";
    }
  } else {
    CHECK_EQ(frames_to_copy, 0u);
  }

  // Trim the leftover data off the end of the buffer and update duration.
  TrimEnd(frames_to_trim);
}

bool AudioBuffer::IsBitstreamFormat() const {
  return IsBitstream(sample_format_);
}

void AudioBuffer::PopulateChannelData() {
  CHECK(channel_data_.empty());

  channel_data_.reserve(channel_spans_.size());

  std::ranges::transform(channel_spans_, std::back_inserter(channel_data_),
                         [](base::span<uint8_t> span) { return span.data(); });
}

}  // namespace media
