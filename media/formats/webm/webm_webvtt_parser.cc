// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/webm/webm_webvtt_parser.h"

namespace media {

void WebMWebVTTParser::Parse(const uint8_t* payload,
                             int payload_size,
                             std::string* id,
                             std::string* settings,
                             std::string* content) {
  WebMWebVTTParser parser(payload, payload_size);
  parser.Parse(id, settings, content);
}

WebMWebVTTParser::WebMWebVTTParser(const uint8_t* payload, int payload_size)
    : ptr_(payload), ptr_end_(payload + payload_size) {}

void WebMWebVTTParser::Parse(std::string* id,
                             std::string* settings,
                             std::string* content) {
  ParseLine(id);
  ParseLine(settings);
  content->assign(ptr_.get(), ptr_end_.get());
}

bool WebMWebVTTParser::GetByte(uint8_t* byte) {
  if (ptr_ >= ptr_end_)
    return false;  // indicates end-of-stream

  *byte = *ptr_++;
  return true;
}

void WebMWebVTTParser::UngetByte() {
  --ptr_;
}

void WebMWebVTTParser::ParseLine(std::string* line) {
  line->clear();

  // Consume characters from the stream, until we reach end-of-line.

  // The WebVTT spec states that lines may be terminated in any of the following
  // three ways:
  //  LF
  //  CR
  //  CR LF

  // The spec is here:
  //  http://wiki.webmproject.org/webm-metadata/temporal-metadata/webvtt-in-webm

  enum {
    kLF = '\x0A',
    kCR = '\x0D'
  };

  for (;;) {
    uint8_t byte;

    if (!GetByte(&byte) || byte == kLF)
      return;

    if (byte == kCR) {
      if (GetByte(&byte) && byte != kLF)
        UngetByte();

      return;
    }

    line->push_back(byte);
  }
}

}  // namespace media
