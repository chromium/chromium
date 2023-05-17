// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MUXERS_BOX_BYTE_STREAM_H_
#define MEDIA_MUXERS_BOX_BYTE_STREAM_H_

#include <vector>

#include "base/big_endian.h"
#include "media/base/media_export.h"
#include "media/formats/mp4/fourccs.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media {

// Helper class for writing big endian ISO-BMFF boxes. ISO-BMFF boxes always
// have the size at the front of the box, so a placeholder value be written
// and filled in later based on distance from the final box size. Class is
// not thread safe.
class MEDIA_EXPORT BoxByteStream {
 public:
  // Constructs a `BoxByteStream` and prepares it for writing. `Flush()` must
  // be called prior to destruction even if nothing is written.
  BoxByteStream();
  ~BoxByteStream();

  // Writes a uint32_t placeholder value that `EndBox()` or `Flush()` will
  // fill in later.
  // Only works if the current position is the start of a new box.
  void StartBox(mp4::FourCC fourcc);
  void StartFullBox(mp4::FourCC fourcc,
                    uint32_t flags = 0,
                    // Chromium MP4 Muxer supports 64 bits as a default, but the
                    // individual box can override it as needed.
                    uint8_t version = 1);

  // Writes primitives types in big endian format. If `value` can be larger than
  // the the type being written, methods will `CHECK()` that `value` fits in the
  // type.
  void WriteU8(uint8_t value);
  void WriteU16(uint16_t value);
  void WriteU24(uint32_t value);
  void WriteU32(uint32_t value);
  void WriteU64(uint64_t value);
  void WriteBytes(const void* buf, size_t len);
  void WriteString(base::StringPiece value);

  // Ends a writing session. All pending placeholder values in `size_offsets_`
  // are filled in based on their distance from `position_`.
  std::vector<uint8_t> Flush();

  // Populate a uint32_t place holder offset value with the total size, which
  // is a summation of the box itself with its children.
  void EndBox();

  // TODO(crbug.com/1072056): Investigate if this is a reasonable starting size.
  static constexpr int kDefaultBufferLimit = 4096;

  // Test helper method that returns internal size offset vector.
  std::vector<size_t> GetSizeOffsetsForTesting() const { return size_offsets_; }

 private:
  // Expands the capacity of `buffer_` and reinitializes `writer_`.
  void GrowWriter();

  std::vector<size_t> size_offsets_;
  size_t position_ = 0;
  std::vector<uint8_t> buffer_;
  absl::optional<base::BigEndianWriter> writer_;
};

}  // namespace media

#endif  // MEDIA_MUXERS_BOX_BYTE_STREAM_H_
