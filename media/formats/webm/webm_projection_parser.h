// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_WEBM_WEBM_PROJECTION_PARSER_H_
#define MEDIA_FORMATS_WEBM_WEBM_PROJECTION_PARSER_H_

#include <optional>
#include <vector>

#include "media/base/media_log.h"
#include "media/base/video_spatial_format.h"
#include "media/base/video_transformation.h"
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
  bool OnListEnd(int id) override;

  VideoTransformation GetVideoTransformation() const;
  VideoProjectionType GetProjectionType() const;

 private:
  friend class WebMProjectionParserTest;

  // WebMParserClient implementation.
  bool OnUInt(int id, int64_t val) override;
  bool OnFloat(int id, double val) override;
  bool OnBinary(int id, base::span<const uint8_t> data) override;

  // private data
  const std::unique_ptr<MediaLog> media_log_;
  std::optional<int64_t> projection_type_;
  std::vector<uint8_t> projection_private_;
  std::optional<double> pose_yaw_;    // value must be [-180, 180]
  std::optional<double> pose_roll_;   // value must be [-180, 180]

  // Derived fields, used to store calculated projection type and
  // transformation.
  VideoProjectionType video_projection_type_;
  VideoTransformation video_transformation_;
};

}  // namespace media

#endif  // MEDIA_FORMATS_WEBM_WEBM_PROJECTION_PARSER_H_
