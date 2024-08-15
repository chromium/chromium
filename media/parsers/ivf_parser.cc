// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/parsers/ivf_parser.h"

#include <cstring>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"

namespace media {

IvfParser::IvfParser() : ptr_(nullptr), end_(nullptr) {}

bool IvfParser::Initialize(const uint8_t* stream,
                           size_t size,
                           IvfFileHeader* file_header) {
  DCHECK(stream);
  DCHECK(file_header);
  ptr_ = stream;
  end_ = stream + size;
  CHECK_GE(end_, ptr_);

  if (size < sizeof(IvfFileHeader)) {
    DLOG(ERROR) << "EOF before file header";
    return false;
  }

  auto input =
      // TODO(crbug.com/40284755): Initialize() should receive a span, not a
      // pointer. IvfParser should hold a span, not a pointer.
      UNSAFE_TODO(base::span(ptr_.get(), end_.get()));
  auto [in_header, in_rem] = input.split_at<sizeof(IvfFileHeader)>();

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

  // TODO(crbug.com/40284755): IvfParser should hold a span, not a pointer.
  ptr_ = in_rem.data();

  return true;
}

bool IvfParser::ParseNextFrame(IvfFrameHeader* frame_header,
                               const uint8_t** payload) {
  DCHECK(ptr_);
  DCHECK(payload);
  CHECK_GE(end_, ptr_);

  if (base::checked_cast<size_t>(end_ - ptr_) < sizeof(IvfFrameHeader)) {
    DLOG_IF(ERROR, ptr_ != end_) << "Incomplete frame header";
    return false;
  }

  auto input =
      // TODO(crbug.com/40284755): IvfParser should hold a span, not a pointer.
      UNSAFE_TODO(base::span(ptr_.get(), end_.get()));
  auto [in_header, in_rem] = input.split_at<sizeof(IvfFrameHeader)>();

  // The stream is little-endian encoded, so we can just copy it into place.
  base::byte_span_from_ref(*frame_header).copy_from(in_header);

  // TODO(crbug.com/40284755): IvfParser should hold a span, not a pointer.
  ptr_ = in_rem.data();

  if (base::checked_cast<uint32_t>(end_ - ptr_) < frame_header->frame_size) {
    DLOG(ERROR) << "Not enough frame data";
    return false;
  }

  *payload = ptr_;
  ptr_ += frame_header->frame_size;

  return true;
}

}  // namespace media
