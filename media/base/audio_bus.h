// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef MEDIA_BASE_AUDIO_BUS_H_
#define MEDIA_BASE_AUDIO_BUS_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "media/base/media_export.h"

namespace media {
class AudioParameters;

// Represents a sequence of audio frames containing frames() audio samples for
// each of channels() channels. The data is stored as a set of contiguous
// float arrays with one array per channel. The memory for the arrays is either
// allocated and owned by the AudioBus or it is provided to one of the factory
// methods. AudioBus guarantees that it allocates memory such that float array
// for each channel is aligned by AudioBus::kChannelAlignment bytes and it
// requires the same for memory passed to its Wrap...() factory methods.
class MEDIA_EXPORT AudioBus {
 public:
  using BitstreamData = base::span<uint8_t>;
  using Channel = base::span<float>;
  using ConstChannel = base::span<const float>;
  using ChannelVector = std::vector<Channel>;

  // Guaranteed alignment of each channel's data; use 16-byte alignment for easy
  // SSE optimizations.
  static constexpr size_t kChannelAlignment = 16;

  // Creates a new AudioBus and allocates |channels| of length |frames|.  Uses
  // channels() and frames_per_buffer() from AudioParameters if given.
  static std::unique_ptr<AudioBus> Create(int channels, int frames);
  static std::unique_ptr<AudioBus> Create(const AudioParameters& params);

  // Creates a new AudioBus with the given number of channels, but zero length.
  // Clients are expected to subsequently call SetChannelData() and set_frames()
  // to wrap externally allocated memory.
  static std::unique_ptr<AudioBus> CreateWrapper(int channels);

  // Creates a new AudioBus by wrapping an existing block of memory.  Block must
  // be at least CalculateMemorySize() bytes in size.  |data| must outlive the
  // returned AudioBus.  |data| must be aligned by kChannelAlignment.
  static std::unique_ptr<AudioBus> WrapMemory(int channels,
                                              int frames,
                                              base::span<float> data);
  static std::unique_ptr<AudioBus> WrapMemory(const AudioParameters& params,
                                              base::span<uint8_t> data);
  static std::unique_ptr<AudioBus> WrapMemory(const AudioParameters& params,
                                              base::span<float> data);

  // Based on the given number of channels and frames, calculates the minimum
  // required size in bytes of a contiguous block of memory to be passed to
  // AudioBus for storage of the audio data.
  // Uses channels() and frames_per_buffer() from AudioParameters if given.
  static size_t CalculateMemorySize(int channels, int frames);
  static size_t CalculateMemorySize(const AudioParameters& params);

  // Checks if buffer is properly aligned to be used in `SetChannelData()`
  static bool IsAligned(void* ptr);
  static bool IsAligned(base::span<float> span);

  // Methods that are expected to be called after AudioBus::CreateWrapper() in
  // order to wrap externally allocated memory.
  // To avoid cases where channel sizes and number of frames don't match,
  // `set_frames()` must be called before setting channel data.
  // Note: It is illegal to call these methods when using a factory method other
  // than CreateWrapper().
  void set_frames(int frames);
  void SetChannelData(int channel, Channel data);
  void SetAllChannels(const ChannelVector& channel_data);

  // Method optionally called after AudioBus::CreateWrapper().
  // Runs |deleter| when on |this|' destruction, freeing external data
  // referenced by SetChannelData().
  // Note: It is illegal to call this method when using a factory method other
  // than CreateWrapper().
  void SetWrappedDataDeleter(base::OnceClosure deleter);

  // Methods for compressed bitstream formats. The data size may not be equal to
  // the capacity of the AudioBus. Also, the frame count may not be equal to the
  // capacity of the AudioBus. Thus, we need extra methods to access the real
  // data size and frame count for bitstream formats.
  bool is_bitstream_format() const { return is_bitstream_format_; }
  void set_is_bitstream_format(bool is_bitstream_format) {
    if (is_bitstream_format) {
      // Don't allow bitstreams if we don't have a continuous chunk of memory.
      // This happens for busses created by CreateWrapper() and WrapVector().
      CHECK(!reserved_memory_.empty());
    }
    is_bitstream_format_ = is_bitstream_format;
  }
  void SetBitstreamSize(size_t data_size);
  int GetBitstreamFrames() const;
  void SetBitstreamFrames(size_t frames);

  // Returns the currently used bitstream data.
  BitstreamData bitstream_data() const { return bitstream_data_; }

  // Overwrites the sample values stored in this AudioBus instance with values
  // from a given interleaved |source_buffer| with expected layout
  // [ch0, ch1, ..., chN, ch0, ch1, ...] and sample values in the format
  // corresponding to the given SourceSampleTypeTraits.
  // The sample values are converted to float values by means of the method
  // convert_to_float32() provided by the SourceSampleTypeTraits. For a list of
  // ready-to-use SampleTypeTraits, see file audio_sample_types.h.
  // If |num_frames_to_write| is less than frames(), the remaining frames are
  // zeroed out. If |num_frames_to_write| is more than frames(), this results in
  // undefined behavior.
  template <class SourceSampleTypeTraits>
  void FromInterleaved(
      const typename SourceSampleTypeTraits::ValueType* source_buffer,
      int num_frames_to_write);

  // Similar to FromInterleaved...(), but overwrites the frames starting at a
  // given offset |write_offset_in_frames| and does not zero out frames that are
  // not overwritten.
  template <class SourceSampleTypeTraits>
  void FromInterleavedPartial(
      const typename SourceSampleTypeTraits::ValueType* source_buffer,
      int write_offset_in_frames,
      int num_frames_to_write);

  // Reads the sample values stored in this AudioBus instance and places them
  // into the given |dest_buffer| in interleaved format using the sample format
  // specified by TargetSampleTypeTraits. For a list of ready-to-use
  // SampleTypeTraits, see file audio_sample_types.h. If |num_frames_to_read| is
  // larger than frames(), this results in undefined behavior.
  template <class TargetSampleTypeTraits>
  void ToInterleaved(
      int num_frames_to_read,
      typename TargetSampleTypeTraits::ValueType* dest_buffer) const;

  // Similar to ToInterleaved(), but reads the frames starting at a given
  // offset |read_offset_in_frames|.
  template <class TargetSampleTypeTraits>
  void ToInterleavedPartial(
      int read_offset_in_frames,
      int num_frames_to_read,
      typename TargetSampleTypeTraits::ValueType* dest_buffer) const;

  // Helper method for copying channel data from one AudioBus to another.  Both
  // AudioBus object must have the same frames() and channels().
  void CopyTo(AudioBus* dest) const;

  // Similar to above, but clips values to [-1, 1] during the copy process.
  void CopyAndClipTo(AudioBus* dest) const;

  // Helper method to copy frames from one AudioBus to another. Both AudioBus
  // objects must have the same number of channels(). |source_start_frame| is
  // the starting offset. |dest_start_frame| is the starting offset in |dest|.
  // |frame_count| is the number of frames to copy.
  void CopyPartialFramesTo(int source_start_frame,
                           int frame_count,
                           int dest_start_frame,
                           AudioBus* dest) const;

  // Returns a raw pointer to the requested channel.  Pointer is guaranteed to
  // have a 16-byte alignment.  Warning: Do not rely on having sane (i.e. not
  // inf, nan, or between [-1.0, 1.0]) values in the channel data.
  // TODO(crbug.com/373960632): Rename `channel_span` to `channel`.
  Channel channel_span(int channel) {
    CHECK(!is_bitstream_format_);
    return channel_data_[channel];
  }
  ConstChannel channel_span(int channel) const {
    CHECK(!is_bitstream_format_);
    return channel_data_[channel];
  }

  // Convenience function to allow range-based for-loops.
  const ChannelVector& AllChannels() const;

  // Returns a copy of `channels_`, with `subspan()` applied to each channel.
  // Note: The returned channels might not be aligned, depending on `offset`.
  ChannelVector AllChannelsSubspan(size_t offset, size_t count) const;

  // Returns the number of channels.
  int channels() const { return static_cast<int>(channel_data_.size()); }
  // Returns the number of frames.
  // Note: for bitstream formats, use GetBitstreamFrames() to get the actual
  // number of encoded frames. However, `frames()` remains useful in determining
  // the amount of `reserved_memory_` this bus has.
  int frames() const { return frames_; }

  // Helper method for zeroing out all channels of audio data.
  void Zero();
  void ZeroFrames(int frames);
  void ZeroFramesPartial(int start_frame, int frames);

  // Checks if all frames are zero.
  bool AreFramesZero() const;

  // Scale internal channel values by |volume| >= 0.  If an invalid value
  // is provided, no adjustment is done.
  void Scale(float volume);

  // Swaps channels identified by |a| and |b|.  The caller needs to make sure
  // the channels are valid.
  void SwapChannels(int a, int b);

  AudioBus(const AudioBus&) = delete;
  AudioBus& operator=(const AudioBus&) = delete;

  virtual ~AudioBus();

 protected:
  AudioBus(int channels, int frames);
  AudioBus(int channels, int frames, base::span<float> data);
  explicit AudioBus(int channels);

 private:
  void ZeroBitstream();

  // Helper method for building |channel_data_| from a block of memory.  |data|
  // must be at least CalculateMemorySize(...) bytes in size.
  void BuildChannelData(int channels, base::span<float> data);

  static void CheckOverflow(int start_frame, int frames, int total_frames);

  template <class SourceSampleTypeTraits>
  static void CopyConvertFromInterleavedSourceToAudioBus(
      const typename SourceSampleTypeTraits::ValueType* source_buffer,
      int write_offset_in_frames,
      int num_frames_to_write,
      AudioBus* dest);

  template <class TargetSampleTypeTraits>
  static void CopyConvertFromAudioBusToInterleavedTarget(
      const AudioBus* source,
      int read_offset_in_frames,
      int num_frames_to_read,
      typename TargetSampleTypeTraits::ValueType* dest_buffer);

  // Contiguous block of channel memory.
  base::AlignedHeapArray<float> data_;

  // Chunk of binary data for bitstream formats.
  // This might point towards external memory, or `data_`.
  // TODO(crbug.com/385028986): Convert to `base::raw_span`
  RAW_PTR_EXCLUSION base::span<uint8_t> reserved_memory_;

  // View over `reserved_memory_`, which represents the chunk of memory which
  // is actively reserved to hold bitstream data. The size of this memory can
  // be adjusted using SetBitstreamDataSize().
  RAW_PTR_EXCLUSION BitstreamData bitstream_data_;

  // Whether the data is compressed bitstream or not.
  bool is_bitstream_format_ = false;
  // The PCM frame count for a compressed bitstream.
  size_t bitstream_frames_ = 0;

  // One float pointer per channel pointing to a contiguous block of memory for
  // that channel. If the memory is owned by this instance, this will
  // point to the memory in |data_|. Otherwise, it may point to memory provided
  // by the client.
  // TODO(crbug.com/385028986): Convert to `base::raw_span`
  RAW_PTR_EXCLUSION ChannelVector channel_data_;

  size_t frames_ = 0u;

  // Protect SetChannelData(), set_frames() and SetWrappedDataDeleter() for use
  // by CreateWrapper().
  const bool is_wrapper_ = false;

  // Run on destruction. Frees memory to the data set via SetChannelData().
  // Only used with CreateWrapper().
  base::OnceClosure wrapped_data_deleter_cb_;
};

// Delegates to FromInterleavedPartial()
template <class SourceSampleTypeTraits>
void AudioBus::FromInterleaved(
    const typename SourceSampleTypeTraits::ValueType* source_buffer,
    int num_frames_to_write) {
  FromInterleavedPartial<SourceSampleTypeTraits>(source_buffer, 0,
                                                 num_frames_to_write);
  // Zero any remaining frames.
  ZeroFramesPartial(num_frames_to_write, frames_ - num_frames_to_write);
}

template <class SourceSampleTypeTraits>
void AudioBus::FromInterleavedPartial(
    const typename SourceSampleTypeTraits::ValueType* source_buffer,
    int write_offset_in_frames,
    int num_frames_to_write) {
  CheckOverflow(write_offset_in_frames, num_frames_to_write, frames_);
  CopyConvertFromInterleavedSourceToAudioBus<SourceSampleTypeTraits>(
      source_buffer, write_offset_in_frames, num_frames_to_write, this);
}

// Delegates to ToInterleavedPartial()
template <class TargetSampleTypeTraits>
void AudioBus::ToInterleaved(
    int num_frames_to_read,
    typename TargetSampleTypeTraits::ValueType* dest_buffer) const {
  ToInterleavedPartial<TargetSampleTypeTraits>(0, num_frames_to_read,
                                               dest_buffer);
}

template <class TargetSampleTypeTraits>
void AudioBus::ToInterleavedPartial(
    int read_offset_in_frames,
    int num_frames_to_read,
    typename TargetSampleTypeTraits::ValueType* dest) const {
  CheckOverflow(read_offset_in_frames, num_frames_to_read, frames_);
  CopyConvertFromAudioBusToInterleavedTarget<TargetSampleTypeTraits>(
      this, read_offset_in_frames, num_frames_to_read, dest);
}

// TODO(chfremer): Consider using vector instructions to speed this up,
//                 https://crbug.com/619628
template <class SourceSampleTypeTraits>
void AudioBus::CopyConvertFromInterleavedSourceToAudioBus(
    const typename SourceSampleTypeTraits::ValueType* source_buffer,
    int write_offset_in_frames,
    int num_frames_to_write,
    AudioBus* dest) {
  const int channels = dest->channels();
  for (int ch = 0; ch < channels; ++ch) {
    AudioBus::Channel channel_data = dest->channel_span(ch);
    for (int target_frame_index = write_offset_in_frames,
             read_pos_in_source = ch;
         target_frame_index < write_offset_in_frames + num_frames_to_write;
         ++target_frame_index, read_pos_in_source += channels) {
      auto source_value = source_buffer[read_pos_in_source];
      channel_data[target_frame_index] =
          SourceSampleTypeTraits::ToFloat(source_value);
    }
  }
}

// TODO(chfremer): Consider using vector instructions to speed this up,
//                 https://crbug.com/619628
template <class TargetSampleTypeTraits>
void AudioBus::CopyConvertFromAudioBusToInterleavedTarget(
    const AudioBus* source,
    int read_offset_in_frames,
    int num_frames_to_read,
    typename TargetSampleTypeTraits::ValueType* dest_buffer) {
  const int channels = source->channels();
  for (int ch = 0; ch < channels; ++ch) {
    AudioBus::ConstChannel channel_data = source->channel_span(ch);
    for (int source_frame_index = read_offset_in_frames, write_pos_in_dest = ch;
         source_frame_index < read_offset_in_frames + num_frames_to_read;
         ++source_frame_index, write_pos_in_dest += channels) {
      float sourceSampleValue = channel_data[source_frame_index];
      dest_buffer[write_pos_in_dest] =
          TargetSampleTypeTraits::FromFloat(sourceSampleValue);
    }
  }
}

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_BUS_H_
