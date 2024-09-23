// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/base/audio_buffer.h"

#include <cmath>

#include "base/bits.h"
#include "base/containers/heap_array.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "media/base/audio_bus.h"
#include "media/base/limits.h"
#include "media/base/timestamp_constants.h"

namespace media {

AudioBuffer::ExternalMemory::ExternalMemory() = default;
AudioBuffer::ExternalMemory::ExternalMemory(base::span<uint8_t> span)
    : span_(span) {}
AudioBuffer::ExternalMemory::~ExternalMemory() = default;
AudioBuffer::ExternalMemory::ExternalMemory(const ExternalMemory&) = default;
AudioBuffer::ExternalMemory::ExternalMemory(ExternalMemory&&) = default;

namespace {

// TODO(crbug.com/41258600): Use vector instructions to speed this up.
template <class SourceSampleTypeTraits>
void CopyConvertFromInterleaved(
    const typename SourceSampleTypeTraits::ValueType* source_buffer,
    int num_frames_to_write,
    const std::vector<float*> dest) {
  const int channels = dest.size();
  for (int ch = 0; ch < channels; ++ch) {
    float* dest_data = dest[ch];
    for (int target_frame_index = 0, read_pos_in_source = ch;
         target_frame_index < num_frames_to_write;
         ++target_frame_index, read_pos_in_source += channels) {
      auto source_value = source_buffer[read_pos_in_source];
      dest_data[target_frame_index] =
          SourceSampleTypeTraits::ToFloat(source_value);
    }
  }
}

class SelfOwnedMemory : public AudioBuffer::ExternalMemory {
 public:
  explicit SelfOwnedMemory(size_t size)
      : heap_array_(
            base::HeapArray<uint8_t, base::AlignedFreeDeleter>::
                FromOwningPointer(
                    static_cast<uint8_t*>(
                        base::AlignedAlloc(size, AudioBus::kChannelAlignment)),
                    size)) {
    span_ = heap_array_.as_span();
  }

 private:
  base::HeapArray<uint8_t, base::AlignedFreeDeleter> heap_array_;
};

std::unique_ptr<AudioBuffer::ExternalMemory> AllocateMemory(size_t size) {
  return std::make_unique<SelfOwnedMemory>(size);
}

}  // namespace

static base::TimeDelta CalculateDuration(int frames, double sample_rate) {
  DCHECK_GT(sample_rate, 0);
  return base::Microseconds(frames * base::Time::kMicrosecondsPerSecond /
                            sample_rate);
}

AudioBufferMemoryPool::AudioBufferMemoryPool(int alignment)
    : alignment_(alignment) {}
AudioBufferMemoryPool::~AudioBufferMemoryPool() = default;

AudioBufferMemoryPool::ExternalMemoryFromPool::ExternalMemoryFromPool(
    ExternalMemoryFromPool&& am) = default;
AudioBufferMemoryPool::ExternalMemoryFromPool::ExternalMemoryFromPool(
    scoped_refptr<AudioBufferMemoryPool> pool,
    std::unique_ptr<uint8_t, base::AlignedFreeDeleter> memory,
    size_t size)
    : memory_(std::move(memory)), pool_(pool) {
  span_ = {memory_.get(), size};
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
  auto memory = std::unique_ptr<uint8_t, base::AlignedFreeDeleter>(
      static_cast<uint8_t*>(base::AlignedAlloc(size, GetChannelAlignment())));
  memset(memory.get(), 0, size);
  return std::make_unique<ExternalMemoryFromPool>(
      ExternalMemoryFromPool(this, std::move(memory), size));
}

void AudioBufferMemoryPool::ReturnBuffer(ExternalMemoryFromPool memory) {
  base::AutoLock al(entry_lock_);
  entries_.emplace_back(std::move(memory));
}

AudioBuffer::AudioBuffer(SampleFormat sample_format,
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

  DCHECK(channel_layout == CHANNEL_LAYOUT_DISCRETE ||
         ChannelLayoutToChannelCount(channel_layout) == channel_count);

  const int bytes_per_channel = SampleFormatToBytesPerChannel(sample_format);
  const int channel_alignment =
      pool_ ? pool_->GetChannelAlignment() : AudioBus::kChannelAlignment;
  CHECK_LE(bytes_per_channel, channel_alignment);

  // Empty buffer?
  if (!create_buffer) {
    return;
  }

  CHECK_NE(sample_format, kUnknownSampleFormat);

  if (sample_format == kSampleFormatIECDts) {
    // Allocate a contiguous buffer for IEC61937 encapsulated Bitstream.
    data_size_ = frame_count * bytes_per_channel * channel_count_;
    data_ =
        pool_ ? pool_->CreateBuffer(data_size_) : AllocateMemory(data_size_);
    channel_data_.push_back(data_->span().data());

    // Copy data
    if (data) {
      memcpy(channel_data_[0], data[0], data_size);
    }
    return;
  }
  int data_size_per_channel = frame_count * bytes_per_channel;
  if (IsPlanar(sample_format)) {
    DCHECK(!IsBitstreamFormat()) << sample_format_;
    // Planar data, so need to allocate buffer for each channel.
    // Determine per channel data size, taking into account alignment.
    int block_size_per_channel = base::bits::AlignUpDeprecatedDoNotUse(
        data_size_per_channel, channel_alignment);
    DCHECK_GE(block_size_per_channel, data_size_per_channel);

    // Allocate a contiguous buffer for all the channel data.
    data_size_ = channel_count_ * block_size_per_channel;
    data_ =
        pool_ ? pool_->CreateBuffer(data_size_) : AllocateMemory(data_size_);
    channel_data_.reserve(channel_count_);

    // Copy each channel's data into the appropriate spot.
    for (int i = 0; i < channel_count_; ++i) {
      channel_data_.push_back(data_->span().data() +
                              i * block_size_per_channel);
      if (data) {
        memcpy(channel_data_[i], data[i], data_size_per_channel);
      }
    }
    return;
  }

  // Remaining formats are interleaved data.
  DCHECK(IsInterleaved(sample_format)) << sample_format_;
  // Allocate our own buffer and copy the supplied data into it. Buffer must
  // contain the data for all channels.
  if (!IsBitstreamFormat()) {
    data_size_ = data_size_per_channel * channel_count_;
  } else {
    DCHECK_GT(data_size_, 0u);
  }

  data_ = pool_ ? pool_->CreateBuffer(data_size_) : AllocateMemory(data_size_);
  channel_data_.push_back(data_->span().data());
  if (data) {
    memcpy(data_->span().data(), data[0], data_size_);
  }
}

AudioBuffer::AudioBuffer(SampleFormat sample_format,
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

  DCHECK(channel_layout == CHANNEL_LAYOUT_DISCRETE ||
         ChannelLayoutToChannelCount(channel_layout) == channel_count);

  if (IsBitstreamFormat()) {
    data_size_ = data_->span().size();
    DCHECK_GT(data_size_, 0u);
    return;
  }

  int bytes_per_channel = SampleFormatToBytesPerChannel(sample_format);
  int data_size_per_channel = frame_count * bytes_per_channel;

  data_size_ = channel_count_ * data_size_per_channel;
  CHECK_GE(data_->span().size(), data_size_);

  if (IsInterleaved(sample_format)) {
    channel_data_.push_back(data_->span().data());
  } else if (IsPlanar(sample_format)) {
    // Planar data, so need to set up pointers for each channel.
    channel_data_.reserve(channel_count_);
    // Set each channel's data pointer into the appropriate spot.
    for (int i = 0; i < channel_count_; ++i) {
      channel_data_.push_back(data_->span().data() + i * data_size_per_channel);
      CHECK_LE(data_->span().data(), channel_data_.back());
      CHECK_GE(data_->span().data() + data_->span().size(),
               channel_data_.back() + data_size_per_channel);
    }
  } else {
    NOTREACHED() << sample_format;
  }
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
  return base::WrapRefCounted(
      new AudioBuffer(sample_format, channel_layout, channel_count, sample_rate,
                      frame_count, true, data, 0, timestamp, std::move(pool)));
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
  for (int ch = 0; ch < channel_count; ch++)
    data[ch] = reinterpret_cast<const uint8_t*>(audio_bus->channel(ch));

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
  return base::WrapRefCounted(new AudioBuffer(
      sample_format, channel_layout, channel_count, sample_rate, frame_count,
      true, data, data_size, timestamp, std::move(pool)));
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
  return base::WrapRefCounted(new AudioBuffer(
      sample_format, channel_layout, channel_count, sample_rate, frame_count,
      true, nullptr, 0, kNoTimestamp, std::move(pool)));
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
  return base::WrapRefCounted(new AudioBuffer(
      sample_format, channel_layout, channel_count, sample_rate, frame_count,
      true, nullptr, data_size, kNoTimestamp, std::move(pool)));
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
  return base::WrapRefCounted(new AudioBuffer(
      kSampleFormatF32, channel_layout, channel_count, sample_rate, frame_count,
      false, nullptr, 0, timestamp, nullptr));
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
  return base::WrapRefCounted(
      new AudioBuffer(sample_format, channel_layout, channel_count, sample_rate,
                      frame_count, timestamp, std::move(external_memory)));
}

// static
scoped_refptr<AudioBuffer> AudioBuffer::CreateEOSBuffer() {
  return base::WrapRefCounted(
      new AudioBuffer(kUnknownSampleFormat, CHANNEL_LAYOUT_NONE, 0, 0, 0, false,
                      nullptr, 0, kNoTimestamp, nullptr));
}

// static
std::unique_ptr<AudioBus> AudioBuffer::WrapOrCopyToAudioBus(
    scoped_refptr<AudioBuffer> buffer) {
  DCHECK(buffer);

  const int channels = buffer->channel_count();
  const int frames = buffer->frame_count();

  DCHECK(channels);
  DCHECK(frames);

  // `buffer` might already have the right memory layout (aligned floats).
  // Prevent a data copy by wrapping it instead.
  bool audiobus_compatible =
      buffer->sample_format() == SampleFormat::kSampleFormatPlanarF32;
  for (int ch = 0; (ch < channels) && audiobus_compatible; ++ch) {
    audiobus_compatible &= AudioBus::IsAligned(buffer->channel_data()[ch]);
  }

  if (audiobus_compatible) {
    auto audio_bus = AudioBus::CreateWrapper(channels);

    for (int ch = 0; ch < channels; ++ch) {
      audio_bus->SetChannelData(
          ch, reinterpret_cast<float*>(buffer->channel_data()[ch]));
    }

    audio_bus->set_frames(frames);

    // Keep |buffer| alive as long as |audio_bus|.
    audio_bus->SetWrappedDataDeleter(
        base::DoNothingWithBoundArgs(std::move(buffer)));

    return audio_bus;
  }

  // |buffer|'s memory can't be wrapped directly. Convert and copy it instead.
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
    DCHECK(!source_frame_offset);
    DCHECK(!dest_frame_offset ||
           dest_frame_offset == dest->GetBitstreamFrames());

    size_t bitstream_size =
        dest_frame_offset ? dest->GetBitstreamDataSize() : 0;
    uint8_t* dest_data =
        reinterpret_cast<uint8_t*>(dest->channel(0)) + bitstream_size;

    memcpy(dest_data, channel_data_[0], data_size());
    dest->SetBitstreamDataSize(bitstream_size + data_size());
    dest->SetBitstreamFrames(dest_frame_offset + frame_count());
    return;
  }

  if (!data_) {
    // Special case for an empty buffer.
    dest->ZeroFramesPartial(dest_frame_offset, frames_to_copy);
    return;
  }

  // Note: The conversion steps below will clip values to [1.0, -1.0f].

  if (sample_format_ == kSampleFormatPlanarF32) {
    for (int ch = 0; ch < channel_count_; ++ch) {
      float* dest_data = dest->channel(ch) + dest_frame_offset;
      const float* source_data =
          reinterpret_cast<const float*>(channel_data_[ch]) +
          source_frame_offset;
      for (int i = 0; i < frames_to_copy; ++i)
        dest_data[i] = Float32SampleTypeTraits::FromFloat(source_data[i]);
    }
    return;
  }
  if (sample_format_ == kSampleFormatPlanarU8) {
    // Format is planar unsigned 8. Convert each value into float and insert
    // into output channel data.
    for (int ch = 0; ch < channel_count_; ++ch) {
      const uint8_t* source_data = channel_data_[ch] + source_frame_offset;
      float* dest_data = dest->channel(ch) + dest_frame_offset;
      for (int i = 0; i < frames_to_copy; ++i)
        dest_data[i] = UnsignedInt8SampleTypeTraits::ToFloat(source_data[i]);
    }
    return;
  }

  if (sample_format_ == kSampleFormatPlanarS16) {
    // Format is planar signed16. Convert each value into float and insert into
    // output channel data.
    for (int ch = 0; ch < channel_count_; ++ch) {
      const int16_t* source_data =
          reinterpret_cast<const int16_t*>(channel_data_[ch]) +
          source_frame_offset;
      float* dest_data = dest->channel(ch) + dest_frame_offset;
      for (int i = 0; i < frames_to_copy; ++i)
        dest_data[i] = SignedInt16SampleTypeTraits::ToFloat(source_data[i]);
    }
    return;
  }

  if (sample_format_ == kSampleFormatPlanarS32) {
    // Format is planar signed32. Convert each value into float and insert into
    // output channel data.
    for (int ch = 0; ch < channel_count_; ++ch) {
      const int32_t* source_data =
          reinterpret_cast<const int32_t*>(channel_data_[ch]) +
          source_frame_offset;
      float* dest_data = dest->channel(ch) + dest_frame_offset;
      for (int i = 0; i < frames_to_copy; ++i)
        dest_data[i] = SignedInt32SampleTypeTraits::ToFloat(source_data[i]);
    }
    return;
  }

  const int bytes_per_channel = SampleFormatToBytesPerChannel(sample_format_);
  const int frame_size = channel_count_ * bytes_per_channel;
  const uint8_t* source_data =
      data_->span().data() + source_frame_offset * frame_size;

  if (sample_format_ == kSampleFormatF32) {
    dest->FromInterleavedPartial<Float32SampleTypeTraits>(
        reinterpret_cast<const float*>(source_data), dest_frame_offset,
        frames_to_copy);
  } else if (sample_format_ == kSampleFormatU8) {
    dest->FromInterleavedPartial<UnsignedInt8SampleTypeTraits>(
        source_data, dest_frame_offset, frames_to_copy);
  } else if (sample_format_ == kSampleFormatS16) {
    dest->FromInterleavedPartial<SignedInt16SampleTypeTraits>(
        reinterpret_cast<const int16_t*>(source_data), dest_frame_offset,
        frames_to_copy);
  } else if (sample_format_ == kSampleFormatS24 ||
             sample_format_ == kSampleFormatS32) {
    dest->FromInterleavedPartial<SignedInt32SampleTypeTraits>(
        reinterpret_cast<const int32_t*>(source_data), dest_frame_offset,
        frames_to_copy);
  } else {
    NOTREACHED_IN_MIGRATION()
        << "Unsupported audio sample type: " << sample_format_;
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

  if (IsBitstreamFormat()) {
    DLOG(ERROR) << "Not allowed to trim an audio bitstream buffer.";
    return;
  }

  const int frames_to_trim = end - start;
  CHECK_GE(frames_to_trim, 0);
  CHECK_LE(frames_to_trim, adjusted_frame_count_);

  const int bytes_per_channel = SampleFormatToBytesPerChannel(sample_format_);
  // Empty buffers do not have frames to copy backed by data_.
  const int frames_to_copy = data_ ? adjusted_frame_count_ - end : 0;
  if (frames_to_copy > 0) {
    switch (sample_format_) {
      case kSampleFormatPlanarU8:
      case kSampleFormatPlanarS16:
      case kSampleFormatPlanarF32:
      case kSampleFormatPlanarS32:
        // Planar data must be shifted per channel.
        for (int ch = 0; ch < channel_count_; ++ch) {
          memmove(channel_data_[ch] + start * bytes_per_channel,
                  channel_data_[ch] + end * bytes_per_channel,
                  bytes_per_channel * frames_to_copy);
        }
        break;
      case kSampleFormatU8:
      case kSampleFormatS16:
      case kSampleFormatS24:
      case kSampleFormatS32:
      case kSampleFormatF32: {
        // Interleaved data can be shifted all at once.
        const int frame_size = channel_count_ * bytes_per_channel;
        memmove(channel_data_[0] + start * frame_size,
                channel_data_[0] + end * frame_size,
                frame_size * frames_to_copy);
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
        NOTREACHED_IN_MIGRATION() << "Invalid sample format!";
    }
  } else {
    CHECK_EQ(frames_to_copy, 0);
  }

  // Trim the leftover data off the end of the buffer and update duration.
  TrimEnd(frames_to_trim);
}

bool AudioBuffer::IsBitstreamFormat() const {
  return IsBitstream(sample_format_);
}

}  // namespace media
