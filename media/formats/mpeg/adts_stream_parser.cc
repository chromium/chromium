// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mpeg/adts_stream_parser.h"

#include <stddef.h>

#include "build/build_config.h"
#include "media/base/channel_layout.h"
#include "media/base/media_log.h"
#include "media/formats/mp4/aac.h"
#include "media/formats/mpeg/adts_constants.h"
#include "media/formats/mpeg/lib.rs.h"

namespace media {

namespace {

ChannelLayout AdtsChannelCountToLayout(int channels) {
  switch (channels) {
    case 1:
      return CHANNEL_LAYOUT_MONO;
    case 2:
      return CHANNEL_LAYOUT_STEREO;
    case 3:
      return CHANNEL_LAYOUT_SURROUND;
    case 4:
      return CHANNEL_LAYOUT_4_0;
    case 5:
      return CHANNEL_LAYOUT_5_0_BACK;
    case 6:
      return CHANNEL_LAYOUT_5_1_BACK;
    case 8:
      return CHANNEL_LAYOUT_7_1;
    default:
      return CHANNEL_LAYOUT_NONE;
  }
}

MPEGAudioStreamParserBase::Header ConvertFfiHeader(
    const formats::mpeg::MpegAudioHeaderInfo& ffi_header) {
  MPEGAudioStreamParserBase::Header header;
  header.frame_size = ffi_header.frame_size;
  header.sample_rate = ffi_header.sample_rate;
  header.channel_layout = AdtsChannelCountToLayout(ffi_header.channels);
  header.sample_count = ffi_header.sample_count;
  header.extra_data.push_back(ffi_header.esds >> 8);
  header.extra_data.push_back(ffi_header.esds & 0xFF);
  return header;
}

}  // namespace

// static
std::optional<ADTSStreamParser::Header> ADTSStreamParser::ParseHeader(
    base::span<const uint8_t> data) {
  auto rust_data = rust::Slice<const uint8_t>(data);
  auto ffi_res = media::formats::mpeg::parse_adts_header(rust_data);
  if (ffi_res.frame_size == 0) {
    return std::nullopt;
  }
  return ConvertFfiHeader(ffi_res);
}

constexpr uint32_t kADTSStartCodeMask = 0xfff00000;

ADTSStreamParser::ADTSStreamParser()
    : MPEGAudioStreamParserBase(kADTSStartCodeMask, AudioCodec::kAAC, 0) {}

ADTSStreamParser::~ADTSStreamParser() = default;

size_t ADTSStreamParser::GetMinHeaderSize() const {
  return kADTSHeaderMinSize;
}

std::optional<ADTSStreamParser::Header> ADTSStreamParser::ParseFrameHeader(
    base::span<const uint8_t> data) {
  auto header = ParseHeader(data);
  if (!header) {
    LIMITED_MEDIA_LOG(DEBUG, media_log(), adts_parse_error_limit_, 5)
        << "Invalid ADTS header.";
  }
  return header;
}

MPEGAudioStreamParserBase::Header ADTSStreamParser::FfiHeaderToHeader(
    const formats::mpeg::MpegAudioHeaderInfo& ffi_header) const {
  return ConvertFfiHeader(ffi_header);
}

}  // namespace media
