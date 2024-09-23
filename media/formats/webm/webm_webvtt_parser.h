// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_WEBM_WEBM_WEBVTT_PARSER_H_
#define MEDIA_FORMATS_WEBM_WEBM_WEBVTT_PARSER_H_

#include <stdint.h>

#include <string>

#include "base/memory/raw_ptr.h"
#include "media/base/media_export.h"

namespace media {

class MEDIA_EXPORT WebMWebVTTParser {
 public:
  WebMWebVTTParser(const WebMWebVTTParser&) = delete;
  WebMWebVTTParser& operator=(const WebMWebVTTParser&) = delete;

  // Utility function to parse the WebVTT cue from a byte stream.
  static void Parse(const uint8_t* payload,
                    int payload_size,
                    std::string* id,
                    std::string* settings,
                    std::string* content);

 private:
  // The payload is the embedded WebVTT cue, stored in a WebM block.
  // The parser treats this as a UTF-8 byte stream.
  WebMWebVTTParser(const uint8_t* payload, int payload_size);

  // Parse the cue identifier, settings, and content from the stream.
  void Parse(std::string* id, std::string* settings, std::string* content);
  // Remove a byte from the stream, advancing the stream pointer.
  // Returns true if a character was returned; false means "end of stream".
  bool GetByte(uint8_t* byte);

  // Backup the stream pointer.
  void UngetByte();

  // Parse a line of text from the stream.
  void ParseLine(std::string* line);

  // Represents the portion of the stream that has not been consumed yet.
  raw_ptr<const uint8_t, AllowPtrArithmetic> ptr_;
  const raw_ptr<const uint8_t> ptr_end_;
};

}  // namespace media

#endif  // MEDIA_FORMATS_WEBM_WEBM_WEBVTT_PARSER_H_
