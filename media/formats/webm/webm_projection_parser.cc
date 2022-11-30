// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/webm/webm_projection_parser.h"

#include "base/check.h"
#include "base/logging.h"
#include "media/formats/webm/webm_constants.h"

namespace {
int64_t INVALID_PROJECTION_TYPE = -1;
double INVALID_ANGLE = -1000;

bool IsValidAngle(double val, double min, double max) {
  return (val >= min && val <= max);
}

// Values for "ProjectionType" are spec'd here:
// https://www.matroska.org/technical/elements.html#ProjectionType
bool IsValidProjectionType(int64_t projection_type_code) {
  const int64_t projection_type_min = 0;  // rectangular
  const int64_t projection_type_max = 3;  // mesh
  return projection_type_code >= projection_type_min &&
         projection_type_code <= projection_type_max;
}
}  // namespace

namespace media {

WebMProjectionParser::WebMProjectionParser(MediaLog* media_log)
    : media_log_(media_log) {
  Reset();
}

WebMProjectionParser::~WebMProjectionParser() = default;

void WebMProjectionParser::Reset() {
  projection_type_ = INVALID_PROJECTION_TYPE;
  pose_yaw_ = INVALID_ANGLE;
  pose_pitch_ = INVALID_ANGLE;
  pose_roll_ = INVALID_ANGLE;
}

// WebMParserClient
bool WebMProjectionParser::OnUInt(int id, int64_t val) {
  if (id != kWebMIdProjectionType) {
    MEDIA_LOG(ERROR, media_log_)
        << "Unexpected id in Projection: 0x" << std::hex << id;
    return false;
  }

  if (projection_type_ != INVALID_PROJECTION_TYPE) {
    MEDIA_LOG(ERROR, media_log_)
        << "Multiple values for id: 0x" << std::hex << id << " specified ("
        << projection_type_ << " and " << val << ")";
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
bool WebMProjectionParser::OnFloat(int id, double val) {
  double* dst = NULL;
  bool is_valid = false;

  switch (id) {
    case kWebMIdProjectionPoseYaw:
      dst = &pose_yaw_;
      // Valid range defined:
      // https://www.matroska.org/technical/elements.html#ProjectionPoseYaw
      is_valid = IsValidAngle(val, -180, 180);
      break;
    case kWebMIdProjectionPosePitch:
      dst = &pose_pitch_;
      // Valid range defined:
      // https://www.matroska.org/technical/elements.html#ProjectionPosePitch
      is_valid = IsValidAngle(val, -90, 90);
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

  if (*dst != INVALID_ANGLE) {
    MEDIA_LOG(ERROR, media_log_)
        << "Multiple values for id: 0x" << std::hex << id << " specified ("
        << *dst << " and " << val << ")";
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

bool WebMProjectionParser::Validate() const {
  if (projection_type_ == INVALID_PROJECTION_TYPE) {
    MEDIA_LOG(ERROR, media_log_)
        << "Projection element is incomplete; ProjectionType required.";
    return false;
  }

  if (pose_yaw_ == INVALID_ANGLE) {
    MEDIA_LOG(ERROR, media_log_)
        << "Projection element is incomplete; ProjectionPoseYaw required.";
    return false;
  }

  if (pose_pitch_ == INVALID_ANGLE) {
    MEDIA_LOG(ERROR, media_log_)
        << "Projection element is incomplete; ProjectionPosePitch required.";
    return false;
  }

  if (pose_roll_ == INVALID_ANGLE) {
    MEDIA_LOG(ERROR, media_log_)
        << "Projection element is incomplete; ProjectionPoseRoll required.";
    return false;
  }

  return true;
}

}  // namespace media