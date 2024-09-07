// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_BUFFER_H_
#define MEDIA_BASE_AUDIO_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/raw_span.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/base/channel_layout.h"
#include "media/base/media_export.h"
#include "media/base/sample_format.h"

namespace mojo {
template <typename T, typename U>
struct TypeConverter;
template <typename T>
class StructPtr;
}  // namespace mojo

namespace media {
class AudioBufferMemoryPool;

namespace mojom {
class AudioBuffer;
}

// An audio buffer that takes a copy of the data passed to it, holds it, and
// copies it into an AudioBus when needed. Also supports an end of stream
// marker.
class MEDIA_EXPORT AudioBuffer
    : public base::RefCountedThreadSafe<AudioBuffer> {
 public:
  struct MEDIA_EXPORT ExternalMemory {
   public:
    explicit ExternalMemory(base::span<uint8_t> span);
    virtual ~ExternalMemory();
    ExternalMemory(const ExternalMemory&);
    ExternalMemory(ExternalMemory&&);

    base::span<uint8_t> span() { return span_; }

   protected:
    ExternalMemory();
    base::raw_span<uint8_t, DanglingUntriaged> span_;
  };

  // Create an AudioBuffer whose channel data is copied from |data|. For
  // interleaved data, only the first buffer is used. For planar data, the
  // number of buffers must be equal to |channel_count|. |frame_count| is the
  // number of frames in each buffer. |data| must not be null and |frame_count|
  // must be >= 0. For optimal efficiency when many buffers are being created, a
  // AudioBufferMemoryPool can be provided to avoid thrashing memory.
  static scoped_refptr<AudioBuffer> CopyFrom(
      SampleFormat sample_format,
      ChannelLayout channel_layout,
      int channel_count,
      int sample_rate,
      int frame_count,
      const uint8_t* const* data,
      const base::TimeDelta timestamp,
      scoped_refptr<AudioBufferMemoryPool> pool = nullptr);

  // Create an AudioBuffer from a copy of the data in |audio_bus| and a given
  // |channel_layout|. For optimal efficiency when many buffers are being
  // created, a AudioBufferMemoryPool can be provided to avoid thrashing memory.
  static scoped_refptr<AudioBuffer> CopyFrom(
      ChannelLayout channel_layout,
      int sample_rate,
      const base::TimeDelta timestamp,
      const AudioBus* audio_bus,
      scoped_refptr<AudioBufferMemoryPool> pool = nullptr);

  // Create an AudioBuffer from a copy of the data in |audio_bus|.
  // For optimal efficiency when many buffers are being created, a
  // AudioBufferMemoryPool can be provided to avoid thrashing memory.
  static scoped_refptr<AudioBuffer> CopyFrom(
      int sample_rate,
      const base::TimeDelta timestamp,
      const AudioBus* audio_bus,
      scoped_refptr<AudioBufferMemoryPool> pool = nullptr);

  // Create an AudioBuffer for compressed bitstream. Its channel data is copied
  // from |data|, and the size is |data_size|. |data| must not be null and
  // |frame_count| must be >= 0.
  static scoped_refptr<AudioBuffer> CopyBitstreamFrom(
      SampleFormat sample_format,
      ChannelLayout channel_layout,
      int channel_count,
      int sample_rate,
      int frame_count,
      const uint8_t* const* data,
      const size_t data_size,
      const base::TimeDelta timestamp,
      scoped_refptr<AudioBufferMemoryPool> pool = nullptr);

  // Create an AudioBuffer with |frame_count| frames. Buffer is allocated, but
  // not initialized. Timestamp and duration are set to kNoTimestamp. For
  // optimal efficiency when many buffers are being created, a
  // AudioBufferMemoryPool can be provided to avoid thrashing memory.
  static scoped_refptr<AudioBuffer> CreateBuffer(
      SampleFormat sample_format,
      ChannelLayout channel_layout,
      int channel_count,
      int sample_rate,
      int frame_count,
      scoped_refptr<AudioBufferMemoryPool> pool = nullptr);

  // Create an AudioBuffer for compressed bitstream. Buffer is allocated, but
  // not initialized. Timestamp and duration are set to kNoTimestamp.
  static scoped_refptr<AudioBuffer> CreateBitstreamBuffer(
      SampleFormat sample_format,
      ChannelLayout channel_layout,
      int channel_count,
      int sample_rate,
      int frame_count,
      size_t data_size,
      scoped_refptr<AudioBufferMemoryPool> pool = nullptr);

  // Create an empty AudioBuffer with |frame_count| frames.
  static scoped_refptr<AudioBuffer> CreateEmptyBuffer(
      ChannelLayout channel_layout,
      int channel_count,
      int sample_rate,
      int frame_count,
      const base::TimeDelta timestamp);

  // Creates a AudioBuffer with ExternalMemory.
  // `external_memory` is owned by AudioBuffer until it is destroyed.
  // This method CHECK() fails
  //   1. `external_memory` isn't large enough to fit all the data described
  //      by `frame_count` and `channel_count`.
  //   2. `external_memory` is not aligned to `sample_format` requirements.
  static scoped_refptr<AudioBuffer> CreateFromExternalMemory(
      SampleFormat sample_format,
      ChannelLayout channel_layout,
      int channel_count,
      int sample_rate,
      int frame_count,
      const base::TimeDelta timestamp,
      std::unique_ptr<ExternalMemory> external_memory);

  // Helper function that creates a new AudioBus which wraps |audio_buffer| and
  // takes a reference on it, if the memory layout (e.g. |sample_format_|) is
  // compatible with wrapping. Otherwise, this copies |audio_buffer| to a new
  // AudioBus, using ReadFrames().
  static std::unique_ptr<AudioBus> WrapOrCopyToAudioBus(
      scoped_refptr<AudioBuffer> audio_buffer);

  // Create an AudioBuffer indicating we've reached end of stream.
  // Calling any method other than end_of_stream() on the resulting buffer
  // is disallowed.
  static scoped_refptr<AudioBuffer> CreateEOSBuffer();

  AudioBuffer() = delete;
  AudioBuffer(const AudioBuffer&) = delete;
  AudioBuffer& operator=(const AudioBuffer&) = delete;

  // Update sample rate and computed duration.
  // TODO(chcunningham): Remove this upon patching FFmpeg's AAC decoder to
  // provide the correct sample rate at the boundary of an implicit config
  // change.
  void AdjustSampleRate(int sample_rate);

  // Copy frames into |dest|. |frames_to_copy| is the number of frames to copy.
  // |source_frame_offset| specifies how many frames in the buffer to skip
  // first. |dest_frame_offset| is the frame offset in |dest|. The frames are
  // converted and clipped from their source format into planar float32 data
  // (which is all that AudioBus handles).
  void ReadFrames(int frames_to_copy,
                  int source_frame_offset,
                  int dest_frame_offset,
                  AudioBus* dest) const;

  // Trim an AudioBuffer by removing |frames_to_trim| frames from the start.
  // Timestamp and duration are adjusted to reflect the fewer frames.
  // Note that repeated calls to TrimStart() may result in timestamp() and
  // duration() being off by a few microseconds due to rounding issues.
  void TrimStart(int frames_to_trim);

  // Trim an AudioBuffer by removing |frames_to_trim| frames from the end.
  // Duration is adjusted to reflect the fewer frames.
  void TrimEnd(int frames_to_trim);

  // Trim an AudioBuffer by removing |end - start| frames from [|start|, |end|).
  // Even if |start| is zero, timestamp() is not adjusted, only duration().
  void TrimRange(int start, int end);

  // Return true if the buffer contains compressed bitstream.
  bool IsBitstreamFormat() const;

  // Return the number of channels.
  int channel_count() const { return channel_count_; }

  // Return the number of frames held.
  int frame_count() const { return adjusted_frame_count_; }

  // Return the sample rate.
  int sample_rate() const { return sample_rate_; }

  // Return the sample format of the internal buffer, not that of what is
  // returned by ReadFrames().
  SampleFormat sample_format() const { return sample_format_; }

  // Return the channel layout.
  ChannelLayout channel_layout() const { return channel_layout_; }

  base::TimeDelta timestamp() const { return timestamp_; }
  base::TimeDelta duration() const { return duration_; }
  void set_timestamp(base::TimeDelta timestamp) { timestamp_ = timestamp; }

  // If there's no data in this buffer, it represents end of stream.
  bool end_of_stream() const { return end_of_stream_; }

  // Access to the raw buffer for ffmpeg and Android MediaCodec decoders to
  // write directly to. For planar formats the vector elements correspond to
  // the channels. For interleaved formats the resulting vector has exactly
  // one element which contains the buffer pointer.
  const std::vector<uint8_t*>& channel_data() const { return channel_data_; }

  // The size of allocated data memory block. For planar formats channels go
  // sequentially in this block.
  size_t data_size() const { return data_size_; }

 private:
  friend class base::RefCountedThreadSafe<AudioBuffer>;

  // mojo::TypeConverter added as a friend so that AudioBuffer can be
  // transferred across a mojo connection.
  friend struct mojo::TypeConverter<mojo::StructPtr<mojom::AudioBuffer>,
                                    AudioBuffer>;

  // Allocates aligned contiguous buffer to hold all channel data (1 block for
  // interleaved data, |channel_count| blocks for planar data), copies
  // [data,data+data_size) to the allocated buffer(s). If |data| is null, no
  // data is copied. If |create_buffer| is false, no data buffer is created (or
  // copied to).
  AudioBuffer(SampleFormat sample_format,
              ChannelLayout channel_layout,
              int channel_count,
              int sample_rate,
              int frame_count,
              bool create_buffer,
              const uint8_t* const* data,
              const size_t data_size,
              const base::TimeDelta timestamp,
              scoped_refptr<AudioBufferMemoryPool> pool);

  // Takes ownership over a contiguous buffer to hold all channel data
  // (1 block for interleaved data, |channel_count| blocks for planar data).
  AudioBuffer(SampleFormat sample_format,
              ChannelLayout channel_layout,
              int channel_count,
              int sample_rate,
              int frame_count,
              const base::TimeDelta timestamp,
              std::unique_ptr<ExternalMemory> external_memory);

  virtual ~AudioBuffer();

  const SampleFormat sample_format_;
  const ChannelLayout channel_layout_;
  const int channel_count_;
  int sample_rate_;
  int adjusted_frame_count_;
  const bool end_of_stream_;
  base::TimeDelta timestamp_;
  base::TimeDelta duration_;

  // Contiguous block of channel data.
  size_t data_size_ = 0;
  std::unique_ptr<ExternalMemory> data_;

  // For planar data, points to each channels data.
  std::vector<uint8_t*> channel_data_;

  // Allows recycling of memory data to avoid repeated allocations.
  scoped_refptr<AudioBufferMemoryPool> pool_;
};

// Basic memory pool for reusing AudioBuffer internal memory to avoid thrashing.
//
// The pool is managed in a last-in-first-out manner, returned buffers are put
// at the back of the queue. When a new buffer is requested by AudioBuffer, we
// will scan from the front to find a matching buffer. All non-matching buffers
// are dropped. The assumption is that when we reach steady-state all buffers
// will have the same sized allocation. At most the pool will be equal in size
// to the maximum number of concurrent AudioBuffer instances.
//
// Each AudioBuffer instance created with an AudioBufferMemoryPool will take a
// ref on the pool instance so that it may return buffers in the future.
class MEDIA_EXPORT AudioBufferMemoryPool
    : public base::RefCountedThreadSafe<AudioBufferMemoryPool> {
 public:
  explicit AudioBufferMemoryPool(int alignment = AudioBus::kChannelAlignment);
  AudioBufferMemoryPool(const AudioBufferMemoryPool&) = delete;
  AudioBufferMemoryPool& operator=(const AudioBufferMemoryPool&) = delete;

  size_t GetPoolSizeForTesting();
  int GetChannelAlignment() { return alignment_; }

  struct ExternalMemoryFromPool : public AudioBuffer::ExternalMemory {
   public:
    ExternalMemoryFromPool(
        scoped_refptr<AudioBufferMemoryPool> pool,
        std::unique_ptr<uint8_t, base::AlignedFreeDeleter> memory,
        size_t size);
    ExternalMemoryFromPool(ExternalMemoryFromPool&&);
    ~ExternalMemoryFromPool() override;

    std::unique_ptr<uint8_t, base::AlignedFreeDeleter> memory_;
    scoped_refptr<AudioBufferMemoryPool> pool_;
  };

 private:
  friend class AudioBuffer;
  friend class base::RefCountedThreadSafe<AudioBufferMemoryPool>;

  ~AudioBufferMemoryPool();

  std::unique_ptr<ExternalMemoryFromPool> CreateBuffer(size_t size);
  void ReturnBuffer(ExternalMemoryFromPool memory);

  const int alignment_;
  base::Lock entry_lock_;
  std::list<ExternalMemoryFromPool> entries_ GUARDED_BY(entry_lock_);
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_BUFFER_H_
