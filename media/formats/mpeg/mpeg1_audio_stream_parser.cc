// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mpeg/mpeg1_audio_stream_parser.h"

#include <array>

#include "base/compiler_specific.h"
#include "media/base/media_log.h"
#include "media/formats/mpeg/lib.rs.h"

namespace media {

namespace {

constexpr uint32_t kMPEG1StartCodeMask = 0xffe00000;
constexpr int kCodecDelay = 529;

ChannelLayout Mp3ChannelCountToLayout(int channels) {
  return channels == 1 ? CHANNEL_LAYOUT_MONO : CHANNEL_LAYOUT_STEREO;
}

MPEGAudioStreamParserBase::Header ConvertFfiHeader(
    const formats::mpeg::MpegAudioHeaderInfo& ffi_header) {
  MPEGAudioStreamParserBase::Header header;
  header.frame_size = ffi_header.frame_size;
  header.sample_rate = ffi_header.sample_rate;
  header.channel_layout = Mp3ChannelCountToLayout(ffi_header.channels);
  header.sample_count = ffi_header.sample_count;
  header.metadata_frame = ffi_header.metadata_frame;
  return header;
}

}  // namespace

// static
std::optional<MPEG1AudioStreamParser::Header>
MPEG1AudioStreamParser::ParseHeader(base::span<const uint8_t> data) {
  auto rust_data = rust::Slice<const uint8_t>(data);
  auto ffi_res = media::formats::mpeg::parse_mp3_header(rust_data);
  if (ffi_res.frame_size == 0) {
    return std::nullopt;
  }
  return ConvertFfiHeader(ffi_res);
}

MPEG1AudioStreamParser::MPEG1AudioStreamParser()
    : MPEGAudioStreamParserBase(kMPEG1StartCodeMask,
                                AudioCodec::kMP3,
                                kCodecDelay) {}

MPEG1AudioStreamParser::~MPEG1AudioStreamParser() = default;

size_t MPEG1AudioStreamParser::GetMinHeaderSize() const {
  return kHeaderSize;
}

std::optional<MPEG1AudioStreamParser::Header>
MPEG1AudioStreamParser::ParseFrameHeader(base::span<const uint8_t> data) {
  auto header = ParseHeader(data);
  if (!header) {
    LIMITED_MEDIA_LOG(DEBUG, media_log(), mp3_parse_error_limit_, 5)
        << "Invalid MP3 header.";
  }
  return header;
}

MPEGAudioStreamParserBase::Header MPEG1AudioStreamParser::FfiHeaderToHeader(
    const formats::mpeg::MpegAudioHeaderInfo& ffi_header) const {
  return ConvertFfiHeader(ffi_header);
}

}  // namespace media
