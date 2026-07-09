// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/webm/webm_projection_parser.h"

#include "base/check.h"
#include "base/containers/span.h"
#include "base/numerics/byte_conversions.h"
#include "media/formats/webm/webm_constants.h"

namespace {

bool IsValidAngle(double val, double min, double max) {
  return (val >= min && val <= max);
}

// Values for "ProjectionType" are spec'd here:
// https://www.matroska.org/technical/elements.html#ProjectionType
bool IsValidProjectionType(int64_t projection_type_code) {
  const int64_t projection_type_min = media::kWebMProjectionTypeRectangular;
  const int64_t projection_type_max = media::kWebMProjectionTypeMesh;
  return projection_type_code >= projection_type_min &&
         projection_type_code <= projection_type_max;
}
}  // namespace

namespace media {

WebMProjectionParser::WebMProjectionParser(MediaLog* media_log)
    : media_log_(MediaLog::CloneSafely(media_log)) {
  Reset();
}

WebMProjectionParser::~WebMProjectionParser() = default;

void WebMProjectionParser::Reset() {
  projection_type_ = std::nullopt;
  projection_private_.clear();
  pose_yaw_ = std::nullopt;
  pose_roll_ = std::nullopt;
  video_projection_type_ = VideoProjectionType::kNone;
  video_transformation_ = VideoTransformation();
}

// WebMParserClient
bool WebMProjectionParser::OnUInt(int id, int64_t val) {
  if (id != kWebMIdProjectionType) {
    MEDIA_LOG(ERROR, media_log_)
        << "Unexpected id in Projection: 0x" << std::hex << id;
    return false;
  }

  if (projection_type_.has_value()) {
    MEDIA_LOG(ERROR, media_log_)
        << "Multiple values for id: 0x" << std::hex << id << " specified ("
        << projection_type_.value() << " and " << val << ")";
    return false;
  }

  if (!IsValidProjectionType(val)) {
    MEDIA_LOG(ERROR, media_log_)
        << "Unexpected value for ProjectionType: 0x" << std::hex << val;
    return false;
  }

  projection_type_ = val;
  return true;
}

// WebMParserClient
bool WebMProjectionParser::OnBinary(int id, base::span<const uint8_t> data) {
  if (id != kWebMIdProjectionPrivate) {
    MEDIA_LOG(ERROR, media_log_)
        << "Unexpected id in Projection: 0x" << std::hex << id;
    return false;
  }

  if (!projection_private_.empty()) {
    MEDIA_LOG(ERROR, media_log_)
        << "Multiple values for id: 0x" << std::hex << id << " specified";
    return false;
  }

  projection_private_ = std::vector<uint8_t>(data.begin(), data.end());

  return true;
}

// WebMParserClient
bool WebMProjectionParser::OnFloat(int id, double val) {
  std::optional<double>* dst = nullptr;
  bool is_valid = false;

  switch (id) {
    case kWebMIdProjectionPoseYaw:
      dst = &pose_yaw_;
      // Valid range defined:
      // https://www.matroska.org/technical/elements.html#ProjectionPoseYaw
      is_valid = IsValidAngle(val, -180, 180);
      break;
    case kWebMIdProjectionPoseRoll:
      dst = &pose_roll_;
      // Valid range defined:
      // https://www.matroska.org/technical/elements.html#ProjectionPoseRoll
      is_valid = IsValidAngle(val, -180, 180);
      break;
    default:
      MEDIA_LOG(ERROR, media_log_)
          << "Unexpected id in Projection: 0x" << std::hex << id;
      return false;
  }

  if (dst->has_value()) {
    MEDIA_LOG(ERROR, media_log_)
        << "Multiple values for id: 0x" << std::hex << id << " specified ("
        << dst->value() << " and " << val << ")";
    return false;
  }

  if (!is_valid) {
    MEDIA_LOG(ERROR, media_log_) << "Value not within valid range. id: 0x"
                                 << std::hex << id << " val:" << val;
    return false;
  }

  *dst = val;
  return true;
}

bool WebMProjectionParser::OnListEnd(int id) {
  if (id != kWebMIdProjection) {
    return true;
  }

  if (!Validate()) {
    return false;
  }

  int64_t type = projection_type_.value();
  if (type == kWebMProjectionTypeEquirectangular) {
    base::span<const uint8_t> data_span = projection_private_;
    // Skip 4 bytes of ISOBMFF FullBox header if present.
    size_t offset = (data_span.size() == 20) ? 4 : 0;
    base::span<const uint8_t, 16> bounds_span =
        data_span.subspan(offset).first<16>();

    // Equirectangular projection bounds are 0.32 fixed-point values.
    // A 180-degree video has approx 25% (0x40000000) cropped from both
    // the left and right. We use an 18.75% (0x30000000) threshold to
    // distinguish 180-degree from slightly cropped 360-degree video.
    const uint32_t kEquirect180Threshold = 0x30000000;
    uint32_t bounds_left = base::U32FromBigEndian(bounds_span.subspan<8, 4>());
    uint32_t bounds_right =
        base::U32FromBigEndian(bounds_span.subspan<12, 4>());

    if (bounds_left >= kEquirect180Threshold &&
        bounds_right >= kEquirect180Threshold) {
      video_projection_type_ = VideoProjectionType::kEquirect180;
    } else {
      video_projection_type_ = VideoProjectionType::kEquirect360;
    }
  } else {
    video_projection_type_ = VideoProjectionType::kNone;
  }

  double yaw = pose_yaw_.value_or(0.0);
  double roll = pose_roll_.value_or(0.0);
  CHECK_GE(yaw, -180.0);
  CHECK_LE(yaw, 180.0);
  constexpr double kYawMirrorThreshold = 1.0;
  video_transformation_ = media::VideoTransformation(
      roll, std::abs(std::abs(yaw) - 180.0) < kYawMirrorThreshold);

  return true;
}

bool WebMProjectionParser::Validate() const {
  if (!projection_type_.has_value()) {
    MEDIA_LOG(ERROR, media_log_)
        << "Projection element is incomplete; ProjectionType required.";
    return false;
  }

  // According to the EBML/Matroska specification, orientation angle elements
  // (ProjectionPoseYaw and ProjectionPoseRoll) are optional and default to 0.0
  // degrees if omitted by the Muxer. Therefore, we do not require them to be
  // present during validation.

  switch (projection_type_.value()) {
    case kWebMProjectionTypeRectangular:
      if (!projection_private_.empty()) {
        MEDIA_LOG(ERROR, media_log_)
            << "ProjectionPrivate must not be present when ProjectionType is "
               "Rectangular (0).";
        return false;
      }
      break;
    case kWebMProjectionTypeEquirectangular: {
      if (projection_private_.empty()) {
        MEDIA_LOG(ERROR, media_log_)
            << "ProjectionPrivate element required when ProjectionType is "
               "Equirectangular (1).";
        return false;
      }

      // 16 bytes for raw bounds, or 20 bytes if ISOBMFF FullBox header is
      // present.
      if (projection_private_.size() != 16 &&
          projection_private_.size() != 20) {
        MEDIA_LOG(ERROR, media_log_)
            << "ProjectionPrivate element has unexpected size: "
            << projection_private_.size();
        return false;
      }
      break;
    }
    case kWebMProjectionTypeCubemap:
    case kWebMProjectionTypeMesh:
      if (projection_private_.empty()) {
        MEDIA_LOG(ERROR, media_log_)
            << "ProjectionPrivate element required when ProjectionType is "
               "Cubemap (2) or Mesh (3).";
        return false;
      }
      break;
    default:
      return false;
  }

  return true;
}

VideoTransformation WebMProjectionParser::GetVideoTransformation() const {
  return video_transformation_;
}

VideoProjectionType WebMProjectionParser::GetProjectionType() const {
  return video_projection_type_;
}

}  // namespace media
