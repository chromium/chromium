// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/alps_decoder.h"

namespace net {
namespace {

bool ReadUint16PrefixedStringPiece(base::StringPiece* payload,
                                   base::StringPiece* output) {
  if (payload->size() < 2) {
    return false;
  }
  const uint16_t length = (static_cast<uint16_t>((*payload)[0]) << 8) +
                          (static_cast<uint8_t>((*payload)[1]));
  payload->remove_prefix(2);

  if (payload->size() < length) {
    return false;
  }
  *output = payload->substr(0, length);
  payload->remove_prefix(length);

  return true;
}

}  // anonymous namespace

AlpsDecoder::AlpsDecoder() {
  decoder_adapter_.set_visitor(&settings_parser_);
  decoder_adapter_.set_extension_visitor(&accept_ch_parser_);
}

AlpsDecoder::~AlpsDecoder() = default;

AlpsDecoder::Error AlpsDecoder::Decode(base::span<const char> data) {
  decoder_adapter_.ProcessInput(data.data(), data.size());

  if (decoder_adapter_.HasError()) {
    return Error::kFramingError;
  }

  if (settings_parser_.forbidden_frame_received()) {
    return Error::kForbiddenFrame;
  }

  if (settings_parser_.settings_ack_received()) {
    return Error::kSettingsWithAck;
  }

  if (decoder_adapter_.state() !=
      http2::Http2DecoderAdapter::SPDY_READY_FOR_FRAME) {
    return Error::kNotOnFrameBoundary;
  }

  return accept_ch_parser_.error();
}

int AlpsDecoder::settings_frame_count() const {
  return settings_parser_.settings_frame_count();
}

AlpsDecoder::SettingsParser::SettingsParser() = default;
AlpsDecoder::SettingsParser::~SettingsParser() = default;

void AlpsDecoder::SettingsParser::OnCommonHeader(
    spdy::SpdyStreamId /*stream_id*/,
    size_t /*length*/,
    uint8_t type,
    uint8_t /*flags*/) {
  if (type == static_cast<uint8_t>(http2::Http2FrameType::DATA) ||
      type == static_cast<uint8_t>(http2::Http2FrameType::HEADERS) ||
      type == static_cast<uint8_t>(http2::Http2FrameType::PRIORITY) ||
      type == static_cast<uint8_t>(http2::Http2FrameType::RST_STREAM) ||
      type == static_cast<uint8_t>(http2::Http2FrameType::PUSH_PROMISE) ||
      type == static_cast<uint8_t>(http2::Http2FrameType::PING) ||
      type == static_cast<uint8_t>(http2::Http2FrameType::GOAWAY) ||
      type == static_cast<uint8_t>(http2::Http2FrameType::WINDOW_UPDATE) ||
      type == static_cast<uint8_t>(http2::Http2FrameType::CONTINUATION)) {
    forbidden_frame_received_ = true;
  }
}

void AlpsDecoder::SettingsParser::OnSettings() {
  settings_frame_count_++;
}
void AlpsDecoder::SettingsParser::OnSetting(spdy::SpdySettingsId id,
                                            uint32_t value) {
  settings_[id] = value;
}

void AlpsDecoder::SettingsParser::OnSettingsAck() {
  settings_ack_received_ = true;
}

AlpsDecoder::AcceptChParser::AcceptChParser() = default;
AlpsDecoder::AcceptChParser::~AcceptChParser() = default;

bool AlpsDecoder::AcceptChParser::OnFrameHeader(spdy::SpdyStreamId stream_id,
                                                size_t length,
                                                uint8_t type,
                                                uint8_t flags) {
  if (type != static_cast<uint8_t>(spdy::SpdyFrameType::ACCEPT_CH) ||
      error_ != Error::kNoError) {
    // Ignore every frame except for ACCEPT_CH.
    // Ignore data after an error has occurred.
    // Returning false causes Http2DecoderAdapter not to call OnFramePayload().
    return false;
  }
  if (stream_id != 0) {
    error_ = Error::kAcceptChInvalidStream;
    return false;
  }
  if (flags != 0) {
    error_ = Error::kAcceptChWithFlags;
    return false;
  }

  return true;
}

void AlpsDecoder::AcceptChParser::OnFramePayload(const char* data, size_t len) {
  DCHECK_EQ(Error::kNoError, error_);

  base::StringPiece payload(data, len);

  while (!payload.empty()) {
    base::StringPiece origin;
    base::StringPiece value;
    if (!ReadUint16PrefixedStringPiece(&payload, &origin) ||
        !ReadUint16PrefixedStringPiece(&payload, &value)) {
      error_ = Error::kAcceptChMalformed;
      return;
    }
    accept_ch_.push_back(
        spdy::AcceptChOriginValuePair{std::string(origin), std::string(value)});
  }
}

}  // namespace net
