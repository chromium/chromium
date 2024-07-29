// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/formats/hls/media_segment.h"

#include "base/numerics/byte_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "media/formats/hls/types.h"
#include "url/gurl.h"

namespace media::hls {

MediaSegment::InitializationSegment::InitializationSegment(
    GURL uri,
    std::optional<types::ByteRange> byte_range)
    : uri_(std::move(uri)), byte_range_(byte_range) {}

MediaSegment::InitializationSegment::~InitializationSegment() = default;

MediaSegment::EncryptionData::EncryptionData(
    GURL uri,
    XKeyTagMethod method,
    XKeyTagKeyFormat format,
    MediaSegment::EncryptionData::IVContainer iv)
    : uri_(std::move(uri)),
      method_(method),
      iv_(std::move(iv)),
      format_(format) {}

MediaSegment::EncryptionData::~EncryptionData() = default;

MediaSegment::EncryptionData::IVContainer MediaSegment::EncryptionData::GetIV(
    types::DecimalInteger media_sequence_number) const {
  if (format_ == XKeyTagKeyFormat::kIdentity && !iv_.has_value()) {
    return std::make_tuple(0, media_sequence_number);
  }
  return iv_;
}

std::optional<std::string> MediaSegment::EncryptionData::GetIVStr(
    types::DecimalInteger media_sequence_number) const {
  MediaSegment::EncryptionData::IVContainer iv = GetIV(media_sequence_number);
  if (!iv.has_value()) {
    return std::nullopt;
  }
  std::string str;
  char* write_buffer = base::WriteInto(&str, 17);
  uint64_t msb, lsb;
  std::tie(msb, lsb) = iv.value();
  msb = base::ByteSwap(msb);
  lsb = base::ByteSwap(lsb);
  memcpy(write_buffer, &msb, 8);
  memcpy(&write_buffer[8], &lsb, 8);
  return str;
}

void MediaSegment::EncryptionData::ImportKey(std::string_view key_content) {
  key_ = crypto::SymmetricKey::Import(crypto::SymmetricKey::AES,
                                      std::string(key_content));
}

MediaSegment::MediaSegment(
    base::TimeDelta duration,
    types::DecimalInteger media_sequence_number,
    types::DecimalInteger discontinuity_sequence_number,
    GURL uri,
    scoped_refptr<InitializationSegment> initialization_segment,
    scoped_refptr<EncryptionData> encryption_data,
    std::optional<types::ByteRange> byte_range,
    std::optional<types::DecimalInteger> bitrate,
    bool has_discontinuity,
    bool is_gap,
    bool has_new_init_segment,
    bool has_new_encryption_data)
    : duration_(duration),
      media_sequence_number_(media_sequence_number),
      discontinuity_sequence_number_(discontinuity_sequence_number),
      uri_(std::move(uri)),
      initialization_segment_(std::move(initialization_segment)),
      encryption_data_(std::move(encryption_data)),
      byte_range_(byte_range),
      bitrate_(bitrate),
      has_discontinuity_(has_discontinuity),
      is_gap_(is_gap),
      has_new_init_segment_(has_new_init_segment),
      has_new_encryption_data_(has_new_encryption_data) {}

MediaSegment::~MediaSegment() = default;

}  // namespace media::hls
