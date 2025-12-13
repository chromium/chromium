// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/media_segment.h"

#include "base/containers/span.h"
#include "base/containers/span_writer.h"
#include "base/numerics/byte_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "crypto/aes_cbc.h"
#include "media/formats/hls/types.h"
#include "url/gurl.h"

namespace media::hls {

namespace {

bool KeyIsValidSize(const std::vector<uint8_t>& key) {
  // HLS allows 128- and 256-bit keys, but not 192-bit.
  return key.size() == 16 || key.size() == 32;
}

}  // namespace

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
  base::WriteInto(&str, 17);
  uint64_t msb, lsb;
  std::tie(msb, lsb) = iv.value();

  base::SpanWriter writer(base::as_writable_byte_span(str));
  writer.WriteU64BigEndian(msb);
  writer.WriteU64BigEndian(lsb);
  return str;
}

void MediaSegment::EncryptionData::ImportKey(std::string_view key_content) {
  key_ = std::vector<uint8_t>(key_content.begin(), key_content.end());
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

bool MediaSegment::GetPlaintextStreamSource(base::span<const uint8_t> src,
                                            base::span<const uint8_t>* dest,
                                            std::vector<uint8_t>* mem) const {
  if (!encryption_data_) {
    *dest = src;
    return true;
  }

  switch (encryption_data_->GetMethod()) {
    case hls::XKeyTagMethod::kNone: {
      *dest = src;
      return true;
    }
    case hls::XKeyTagMethod::kAES128:
    case hls::XKeyTagMethod::kAES256: {
      auto maybe_iv = encryption_data_->GetIVStr(media_sequence_number_);
      std::array<uint8_t, crypto::aes_cbc::kBlockSize> iv;
      if (!maybe_iv.has_value()) {
        return false;
      }
      base::span(iv).copy_from(base::as_byte_span(*maybe_iv));
      auto key = encryption_data_->GetKey();
      if (!KeyIsValidSize(key)) {
        return false;
      }
      auto maybe_plaintext = crypto::aes_cbc::Decrypt(key, iv, src);
      if (!maybe_plaintext) {
        return false;
      }
      *mem = std::move(maybe_plaintext).value();
      *dest = *mem;
      return true;
    }
    default:
      return false;
  }
}

}  // namespace media::hls
