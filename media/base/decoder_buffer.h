// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DECODER_BUFFER_H_
#define MEDIA_BASE_DECODER_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/decoder_buffer_side_data.h"
#include "media/base/decrypt_config.h"
#include "media/base/media_export.h"
#include "media/base/timestamp_constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media {

// A specialized buffer for interfacing with audio / video decoders.
//
// Also includes decoder specific functionality for decryption.
//
// NOTE: It is illegal to call any method when end_of_stream() is true.
class MEDIA_EXPORT DecoderBuffer
    : public base::RefCountedThreadSafe<DecoderBuffer> {
 public:
  // ExternalMemory wraps a class owning a buffer and expose the data interface
  // through |span|. This class is derived by a class that owns the class owning
  // the buffer owner class. It is generally better to add the buffer class to
  // DecoderBuffer. ExternalMemory is for a class that cannot be added; for
  // instance, rtc::scoped_refptr<webrtc::EncodedImageBufferInterface>, webrtc
  // class cannot be included in //media/base.
  struct MEDIA_EXPORT ExternalMemory {
   public:
    explicit ExternalMemory(base::span<const uint8_t> span) : span_(span) {}
    virtual ~ExternalMemory() = default;
    const base::span<const uint8_t>& span() const { return span_; }

   protected:
    ExternalMemory() = default;
    base::span<const uint8_t> span_;
  };

  using DiscardPadding = std::pair<base::TimeDelta, base::TimeDelta>;

  struct MEDIA_EXPORT TimeInfo {
    TimeInfo();
    ~TimeInfo();
    TimeInfo(const TimeInfo&);
    TimeInfo& operator=(const TimeInfo&);

    // Presentation time of the frame.
    base::TimeDelta timestamp;

    // Presentation duration of the frame.
    base::TimeDelta duration;

    // Duration of (audio) samples from the beginning and end of this frame
    // which should be discarded after decoding. A value of kInfiniteDuration
    // for the first value indicates the entire frame should be discarded; the
    // second value must be base::TimeDelta() in this case.
    DiscardPadding discard_padding;
  };

  // Allocates buffer with |size| >= 0. |is_key_frame_| will default to false.
  explicit DecoderBuffer(size_t size);

  DecoderBuffer(const DecoderBuffer&) = delete;
  DecoderBuffer& operator=(const DecoderBuffer&) = delete;

  // Create a DecoderBuffer whose |data_| is copied from |data|. |data| must not
  // be NULL and |size| >= 0. The buffer's |is_key_frame_| will default to
  // false.
  static scoped_refptr<DecoderBuffer> CopyFrom(const uint8_t* data,
                                               size_t size);

  // Create a DecoderBuffer where data() of |size| bytes resides within the heap
  // as byte array. The buffer's |is_key_frame_| will default to false.
  //
  // Ownership of |data| is transferred to the buffer.
  static scoped_refptr<DecoderBuffer> FromArray(std::unique_ptr<uint8_t[]> data,
                                                size_t size);

  // Create a DecoderBuffer where data() of |size| bytes resides within the
  // memory referred to by |region| at non-negative offset |offset|. The
  // buffer's |is_key_frame_| will default to false.
  //
  // The shared memory will be mapped read-only.
  //
  // If mapping fails, nullptr will be returned.
  static scoped_refptr<DecoderBuffer> FromSharedMemoryRegion(
      base::UnsafeSharedMemoryRegion region,
      uint64_t offset,
      size_t size);

  // Create a DecoderBuffer where data() of |size| bytes resides within the
  // ReadOnlySharedMemoryRegion referred to by |mapping| at non-negative offset
  // |offset|. The buffer's |is_key_frame_| will default to false.
  //
  // Ownership of |region| is transferred to the buffer.
  static scoped_refptr<DecoderBuffer> FromSharedMemoryRegion(
      base::ReadOnlySharedMemoryRegion region,
      uint64_t offset,
      size_t size);

  // Creates a DecoderBuffer with ExternalMemory. The buffer accessed through
  // the created DecoderBuffer is |span| of |external_memory||.
  // |external_memory| is owned by DecoderBuffer until it is destroyed.
  static scoped_refptr<DecoderBuffer> FromExternalMemory(
      std::unique_ptr<ExternalMemory> external_memory);

  // Create a DecoderBuffer indicating we've reached end of stream.
  //
  // Calling any method other than end_of_stream() on the resulting buffer
  // is disallowed.
  static scoped_refptr<DecoderBuffer> CreateEOSBuffer();

  // Method to verify if subsamples of a DecoderBuffer match.
  static bool DoSubsamplesMatch(const DecoderBuffer& buffer);

  const TimeInfo& time_info() const {
    DCHECK(!end_of_stream());
    return time_info_;
  }

  base::TimeDelta timestamp() const {
    DCHECK(!end_of_stream());
    return time_info_.timestamp;
  }

  // TODO(dalecurtis): This should be renamed at some point, but to avoid a yak
  // shave keep as a virtual with hacker_style() for now.
  virtual void set_timestamp(base::TimeDelta timestamp);

  base::TimeDelta duration() const {
    DCHECK(!end_of_stream());
    return time_info_.duration;
  }

  void set_duration(base::TimeDelta duration) {
    DCHECK(!end_of_stream());
    DCHECK(duration == kNoTimestamp ||
           (duration >= base::TimeDelta() && duration != kInfiniteDuration))
        << duration.InSecondsF();
    time_info_.duration = duration;
  }

  const uint8_t* data() const {
    DCHECK(!end_of_stream());
    if (read_only_mapping_.IsValid())
      return read_only_mapping_.GetMemoryAs<const uint8_t>();
    if (writable_mapping_.IsValid())
      return writable_mapping_.GetMemoryAs<const uint8_t>();
    if (external_memory_)
      return external_memory_->span().data();
    return data_.get();
  }

  // TODO(sandersd): Remove writable_data(). https://crbug.com/834088
  uint8_t* writable_data() const {
    DCHECK(!end_of_stream());
    DCHECK(!read_only_mapping_.IsValid());
    DCHECK(!writable_mapping_.IsValid());
    DCHECK(!external_memory_);
    return data_.get();
  }

  size_t data_size() const {
    DCHECK(!end_of_stream());
    return size_;
  }

  const DiscardPadding& discard_padding() const {
    DCHECK(!end_of_stream());
    return time_info_.discard_padding;
  }

  void set_discard_padding(const DiscardPadding& discard_padding) {
    DCHECK(!end_of_stream());
    time_info_.discard_padding = discard_padding;
  }

  // Returns DecryptConfig associated with |this|. Returns null iff |this| is
  // not encrypted.
  const DecryptConfig* decrypt_config() const {
    DCHECK(!end_of_stream());
    return decrypt_config_.get();
  }

  void set_decrypt_config(std::unique_ptr<DecryptConfig> decrypt_config) {
    DCHECK(!end_of_stream());
    decrypt_config_ = std::move(decrypt_config);
  }

  // If there's no data in this buffer, it represents end of stream.
  bool end_of_stream() const {
    return !read_only_mapping_.IsValid() && !writable_mapping_.IsValid() &&
           !external_memory_ && !data_;
  }

  bool is_key_frame() const {
    DCHECK(!end_of_stream());
    return is_key_frame_;
  }

  bool is_encrypted() const {
    DCHECK(!end_of_stream());
    return decrypt_config() && decrypt_config()->encryption_scheme() !=
                                   EncryptionScheme::kUnencrypted;
  }

  void set_is_key_frame(bool is_key_frame) {
    DCHECK(!end_of_stream());
    is_key_frame_ = is_key_frame;
  }

  bool has_side_data() const { return side_data_.has_value(); }
  const absl::optional<DecoderBufferSideData>& side_data() const {
    return side_data_;
  }
  DecoderBufferSideData& WritableSideData();
  void set_side_data(const absl::optional<DecoderBufferSideData>& side_data) {
    side_data_ = side_data;
  }

  // Returns true if all fields in |buffer| matches this buffer including
  // |data_|.
  bool MatchesForTesting(const DecoderBuffer& buffer) const;

  // As above, except that |data_| is not compared.
  bool MatchesMetadataForTesting(const DecoderBuffer& buffer) const;

  // Returns a human-readable string describing |*this|.
  std::string AsHumanReadableString(bool verbose = false) const;

 protected:
  friend class base::RefCountedThreadSafe<DecoderBuffer>;

  // Allocates a buffer of size |size| >= 0 and copies |data| into it. If |data|
  // is NULL then |data_| is set to NULL and |buffer_size_| to 0.
  // |is_key_frame_| will default to false.
  DecoderBuffer(const uint8_t* data, size_t size);

  DecoderBuffer(std::unique_ptr<uint8_t[]> data, size_t size);

  DecoderBuffer(base::ReadOnlySharedMemoryMapping mapping, size_t size);

  DecoderBuffer(base::WritableSharedMemoryMapping mapping, size_t size);

  explicit DecoderBuffer(std::unique_ptr<ExternalMemory> external_memory);

  virtual ~DecoderBuffer();

  // Encoded data, if it is stored on the heap.
  std::unique_ptr<uint8_t[]> data_;

 private:
  TimeInfo time_info_;

  // Size of the encoded data.
  size_t size_;

  // Structured side data.
  absl::optional<DecoderBufferSideData> side_data_;

  // Encoded data, if it is stored in a read-only shared memory mapping.
  base::ReadOnlySharedMemoryMapping read_only_mapping_;

  // Encoded data, if it is stored in a writable shared memory mapping.
  base::WritableSharedMemoryMapping writable_mapping_;

  std::unique_ptr<ExternalMemory> external_memory_;

  // Encryption parameters for the encoded data.
  std::unique_ptr<DecryptConfig> decrypt_config_;

  // Whether the frame was marked as a keyframe in the container.
  bool is_key_frame_ = false;

  // Constructor helper method for memory allocations.
  void Initialize();
};

}  // namespace media

#endif  // MEDIA_BASE_DECODER_BUFFER_H_
