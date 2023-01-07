// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp4/es_descriptor.h"

#include <stddef.h>

#include "media/base/bit_reader.h"
#include "media/formats/mp4/rcheck.h"

// The elementary stream size is specific by up to 4 bytes.
// The MSB of a byte indicates if there are more bytes for the size.
static bool ReadESSize(media::BitReader* reader, uint32_t* size) {
  uint8_t msb;
  uint8_t byte;

  *size = 0;

  for (size_t i = 0; i < 4; ++i) {
    RCHECK(reader->ReadBits(1, &msb));
    RCHECK(reader->ReadBits(7, &byte));
    *size = (*size << 7) + byte;

    if (msb == 0)
      break;
  }

  return true;
}

namespace media {

namespace mp4 {

// static
bool ESDescriptor::IsAAC(uint8_t object_type) {
  return object_type == kISO_14496_3 || object_type == kISO_13818_7_AAC_LC;
}

ESDescriptor::ESDescriptor()
    : object_type_(kForbidden) {
}

ESDescriptor::~ESDescriptor() = default;

bool ESDescriptor::Parse(const std::vector<uint8_t>& data) {
  BitReader reader(&data[0], data.size());
  uint8_t tag;
  uint32_t size;
  uint8_t stream_dependency_flag;
  uint8_t url_flag;
  uint8_t ocr_stream_flag;
  uint16_t dummy;

  RCHECK(reader.ReadBits(8, &tag));
  RCHECK(tag == kESDescrTag);
  RCHECK(ReadESSize(&reader, &size));

  RCHECK(reader.ReadBits(16, &dummy));  // ES_ID
  RCHECK(reader.ReadBits(1, &stream_dependency_flag));
  RCHECK(reader.ReadBits(1, &url_flag));
  RCHECK(!url_flag);  // We don't support url flag
  RCHECK(reader.ReadBits(1, &ocr_stream_flag));
  RCHECK(reader.ReadBits(5, &dummy));  // streamPriority

  if (stream_dependency_flag)
    RCHECK(reader.ReadBits(16, &dummy));  // dependsOn_ES_ID
  if (ocr_stream_flag)
    RCHECK(reader.ReadBits(16, &dummy));  // OCR_ES_Id

  RCHECK(ParseDecoderConfigDescriptor(&reader));

  return true;
}

uint8_t ESDescriptor::object_type() const {
  return object_type_;
}

const std::vector<uint8_t>& ESDescriptor::decoder_specific_info() const {
  return decoder_specific_info_;
}

bool ESDescriptor::ParseDecoderConfigDescriptor(BitReader* reader) {
  uint8_t tag;
  uint32_t size;
  uint64_t dummy;

  RCHECK(reader->ReadBits(8, &tag));
  RCHECK(tag == kDecoderConfigDescrTag);
  RCHECK(ReadESSize(reader, &size));

  RCHECK(reader->ReadBits(8, &object_type_));
  RCHECK(reader->ReadBits(64, &dummy));
  RCHECK(reader->ReadBits(32, &dummy));
  RCHECK(ParseDecoderSpecificInfo(reader));

  return true;
}

bool ESDescriptor::ParseDecoderSpecificInfo(BitReader* reader) {
  uint8_t tag;
  uint32_t size;

  RCHECK(reader->ReadBits(8, &tag));
  RCHECK(tag == kDecoderSpecificInfoTag);
  RCHECK(ReadESSize(reader, &size));

  decoder_specific_info_.resize(size);
  for (uint32_t i = 0; i < size; ++i)
    RCHECK(reader->ReadBits(8, &decoder_specific_info_[i]));

  return true;
}

}  // namespace mp4

}  // namespace media
