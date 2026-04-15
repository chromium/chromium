// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/parsers/ivf_parser.h"

#include <cstring>

#include "base/check.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"

namespace media {

IvfParser::IvfParser() = default;

bool IvfParser::Initialize(base::span<const uint8_t> stream,
                           IvfFileHeader* file_header) {
  DCHECK(stream.data());
  DCHECK(file_header);
  stream_ = stream;

  if (stream.size() < sizeof(IvfFileHeader)) {
    DLOG(ERROR) << "EOF before file header";
    return false;
  }

  auto [in_header, in_rem] = stream_.split_at<sizeof(IvfFileHeader)>();

  // The stream is little-endian encoded, so we can just copy it into place.
  base::byte_span_from_ref(*file_header).copy_from(in_header);

  if (base::as_byte_span(file_header->signature) != kIvfHeaderSignature) {
    DLOG(ERROR) << "IVF signature mismatch";
    return false;
  }
  DLOG_IF(WARNING, file_header->version != 0)
      << "IVF version unknown: " << file_header->version
      << ", the parser may not be able to parse correctly";
  if (file_header->header_size != sizeof(IvfFileHeader)) {
    DLOG(ERROR) << "IVF file header size mismatch";
    return false;
  }

  stream_ = in_rem;

  return true;
}

base::span<const uint8_t> IvfParser::ParseNextFrame(
    IvfFrameHeader* frame_header) {
  DCHECK(stream_.data());

  if (stream_.size() < sizeof(IvfFrameHeader)) {
    DLOG_IF(ERROR, stream_.size() > 0) << "Incomplete frame header";
    return {};
  }

  auto [in_header, in_rem] = stream_.split_at<sizeof(IvfFrameHeader)>();

  // The stream is little-endian encoded, so we can just copy it into place.
  base::byte_span_from_ref(*frame_header).copy_from(in_header);

  stream_ = in_rem;

  if (stream_.size() < frame_header->frame_size) {
    DLOG(ERROR) << "Not enough frame data";
    return {};
  }

  return stream_.take_first(frame_header->frame_size);
}

}  // namespace media
