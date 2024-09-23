// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

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

namespace {

// Descriptors use a variable length size entry. We've fixed the size to
// 4 bytes to make inline construction simple. The lowest 7 bits encode
// the actual value, an MSB==1 indicates there's another byte to decode,
// and an MSB==0 indicates there are no more bytes to decode.
void EncodeDescriptorSize(size_t size, uint8_t* output) {
  DCHECK_LT(size, (1u << (4u * 7u)));
  for (int i = 3; i > 0; i--) {
    output[3 - i] = (size >> (7 * i)) | 0x80;
  }
  output[3] = size & 0x7F;
}

}  // namespace

// static
bool ESDescriptor::IsAAC(uint8_t object_type) {
  return object_type == kISO_14496_3 || object_type == kISO_13818_7_AAC_LC;
}

// static
std::vector<uint8_t> ESDescriptor::CreateEsds(
    const std::vector<uint8_t>& aac_extra_data) {
#pragma pack(push, 1)
  struct Descriptor {
    uint8_t tag;
    uint8_t size[4];  // Note: Size is variable length, with a 1 in the MSB
                      // signaling another byte remains. Clamping to 4 here
                      // just makes it easier to construct the ESDS in place.
  };
  struct DecoderConfigDescriptor : Descriptor {
    uint8_t aot;
    uint8_t flags;
    uint8_t unused[11];
    Descriptor extra_data;
  };
  struct EsDescriptor : Descriptor {
    uint16_t id;
    uint8_t flags;
    DecoderConfigDescriptor decoder_config;
  };
#pragma pack(pop)

  std::vector<uint8_t> esds_data(sizeof(EsDescriptor) + aac_extra_data.size());
  auto* esds = reinterpret_cast<EsDescriptor*>(esds_data.data());

  esds->tag = kESDescrTag;
  EncodeDescriptorSize(
      sizeof(EsDescriptor) - sizeof(Descriptor) + aac_extra_data.size(),
      esds->size);

  esds->decoder_config.tag = kDecoderConfigDescrTag;
  EncodeDescriptorSize(sizeof(DecoderConfigDescriptor) - sizeof(Descriptor) +
                           aac_extra_data.size(),
                       esds->decoder_config.size);
  esds->decoder_config.aot = kISO_14496_3;  // AAC.
  esds->decoder_config.flags = 0x15;        // AudioStream

  esds->decoder_config.extra_data.tag = kDecoderSpecificInfoTag;
  EncodeDescriptorSize(aac_extra_data.size(),
                       esds->decoder_config.extra_data.size);

  base::ranges::copy(aac_extra_data, esds_data.begin() + sizeof(EsDescriptor));

  DCHECK(ESDescriptor().Parse(esds_data));
  return esds_data;
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
