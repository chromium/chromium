// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MUXERS_BOX_BYTE_STREAM_H_
#define MEDIA_MUXERS_BOX_BYTE_STREAM_H_

#include <optional>
#include <string_view>
#include <vector>

#include "base/containers/queue.h"
#include "base/containers/span_writer.h"
#include "base/containers/stack.h"
#include "media/base/media_export.h"
#include "media/formats/mp4/fourccs.h"

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
                    uint8_t version = 0);

  // Writes primitives types in big endian format. If `value` can be larger than
  // the the type being written, methods will `CHECK()` that `value` fits in the
  // type.
  void WriteU8(uint8_t value);
  void WriteU16(uint16_t value);
  void WriteU32(uint32_t value);
  void WriteU64(uint64_t value);
  void WriteBytes(const void* buf, size_t len);
  void WriteString(std::string_view value);

  // Ends a writing session. All pending placeholder values in `size_offsets_`
  // are filled in based on their distance from `position_`.
  std::vector<uint8_t> Flush();

  // Populate a uint32_t place holder offset value with the total size, which
  // is a summation of the box itself with its children.
  void EndBox();

  // Write placeholder for the track data offset of the `trun` box. The data
  // is stored in the `mdat` box so its value will be written during `mdat'
  // box `Write` time.
  void WriteOffsetPlaceholder();

  // Populate the placeholder, which was set by `WriteOffsetPlaceholder`
  // with the current offset. The current offset is a data offset only
  // when the `BoxByteStream` is created with `moof` box.
  void FlushCurrentOffset();

  // Validates whether there is an open box or not.
  bool has_open_boxes() const { return !size_offsets_.empty(); }

  // Returns size of the top level box size until this point.
  // The function is used for `mfro` box where its last property
  // is a total size of the top `mfra' box.
  size_t size() const { return position_; }

  // TODO(crbug.com/40127044): Investigate if this is a reasonable starting
  // size.
  static constexpr int kDefaultBufferLimit = 4096;

  // Test helper method that returns internal size offset vector.
  std::vector<size_t> GetSizeOffsetsForTesting() const { return size_offsets_; }

 private:
  // Expands the capacity of `buffer_` and reinitializes `writer_`.
  void GrowWriter();

  std::vector<size_t> size_offsets_;
  base::queue<size_t> data_offsets_by_track_;
  base::stack<size_t> parent_box_size_offsets_;

  size_t position_ = 0;
  std::vector<uint8_t> buffer_;
  std::optional<base::SpanWriter<uint8_t>> writer_;
};

}  // namespace media

#endif  // MEDIA_MUXERS_BOX_BYTE_STREAM_H_
