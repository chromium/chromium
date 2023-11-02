// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_WEBM_WEBM_PROJECTION_PARSER_H_
#define MEDIA_FORMATS_WEBM_WEBM_PROJECTION_PARSER_H_

#include "base/memory/raw_ptr.h"
#include "media/base/media_log.h"
#include "media/formats/webm/webm_parser.h"

namespace media {

// Parser for WebM Projection element:
class MEDIA_EXPORT WebMProjectionParser : public WebMParserClient {
 public:
  explicit WebMProjectionParser(MediaLog* media_log);

  WebMProjectionParser(const WebMProjectionParser&) = delete;
  WebMProjectionParser& operator=(const WebMProjectionParser&) = delete;

  ~WebMProjectionParser() override;

  void Reset();
  bool Validate() const;

 private:
  friend class WebMProjectionParserTest;

  // WebMParserClient implementation.
  bool OnUInt(int id, int64_t val) override;
  bool OnFloat(int id, double val) override;

  // private data
  raw_ptr<MediaLog> media_log_;
  int64_t projection_type_;
  double pose_yaw_;    // value must be [-180, 180]
  double pose_pitch_;  // value must be [-90, 90]
  double pose_roll_;   // value must be [-180, 180]
};

}  // namespace media

#endif  // MEDIA_FORMATS_WEBM_WEBM_PROJECTION_PARSER_H_
