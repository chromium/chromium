// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_WEBM_WEBM_INFO_PARSER_H_
#define MEDIA_FORMATS_WEBM_WEBM_INFO_PARSER_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/formats/webm/webm_parser.h"

namespace media {

// Parser for WebM Info element.
class MEDIA_EXPORT WebMInfoParser : public WebMParserClient {
 public:
  WebMInfoParser();

  WebMInfoParser(const WebMInfoParser&) = delete;
  WebMInfoParser& operator=(const WebMInfoParser&) = delete;

  ~WebMInfoParser() override;

  // Parses a WebM Info element in |buf|.
  //
  // Returns -1 if the parse fails.
  // Returns 0 if more data is needed.
  // Returns the number of bytes parsed on success.
  int Parse(const uint8_t* buf, int size);

  int64_t timecode_scale_ns() const { return timecode_scale_ns_; }
  double duration() const { return duration_; }
  base::Time date_utc() const { return date_utc_; }

 private:
  // WebMParserClient methods
  WebMParserClient* OnListStart(int id) override;
  bool OnListEnd(int id) override;
  bool OnUInt(int id, int64_t val) override;
  bool OnFloat(int id, double val) override;
  bool OnBinary(int id, const uint8_t* data, int size) override;
  bool OnString(int id, const std::string& str) override;

  int64_t timecode_scale_ns_;
  double duration_;
  base::Time date_utc_;
};

}  // namespace media

#endif  // MEDIA_FORMATS_WEBM_WEBM_INFO_PARSER_H_
