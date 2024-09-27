// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/muxers/mp4_box_writer.h"

#include <string_view>

#include "base/big_endian.h"
#include "media/muxers/box_byte_stream.h"
#include "media/muxers/mp4_muxer_context.h"
#include "media/muxers/output_position_tracker.h"

namespace media {

Mp4BoxWriter::Mp4BoxWriter(const Mp4MuxerContext& context)
    : context_(context) {}

Mp4BoxWriter::~Mp4BoxWriter() = default;

size_t Mp4BoxWriter::WriteAndFlush() {
  // It will write itself as well as children boxes.
  BoxByteStream writer;

  return WriteAndFlush(writer);
}

size_t Mp4BoxWriter::WriteAndFlush(BoxByteStream& writer) {
  DCHECK(!writer.has_open_boxes());

  // It will write to input writer as well as children boxes.
  Write(writer);

  // Update the total size on respective boxes.
  std::vector<uint8_t> buffer = writer.Flush();

  // Write the entire boxes to the blob.
  context().GetOutputPositionTracker().WriteSpan(buffer);

  return buffer.size();
}

void Mp4BoxWriter::WriteChildren(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& child_box : child_boxes_) {
    child_box->Write(writer);
  }
}

void Mp4BoxWriter::AddChildBox(std::unique_ptr<Mp4BoxWriter> box_writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  child_boxes_.push_back(std::move(box_writer));
}

}  // namespace media
