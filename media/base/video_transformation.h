// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_TRANSFORMATION_H_
#define MEDIA_BASE_VIDEO_TRANSFORMATION_H_

#include <string>

#include "media/base/media_export.h"

namespace media {

// Enumeration to represent 90 degree video rotation for MP4 videos
// where it can be rotated by 90 degree intervals.
enum VideoRotation : int {
  VIDEO_ROTATION_0 = 0,
  VIDEO_ROTATION_90 = 90,
  VIDEO_ROTATION_180 = 180,
  VIDEO_ROTATION_270 = 270,
  VIDEO_ROTATION_MAX = VIDEO_ROTATION_270
};

// Stores frame rotation & mirroring values. These are usually calculated from
// a rotation matrix from a demuxer, and we only support 90 degree rotation
// increments.
struct MEDIA_EXPORT VideoTransformation {
  static VideoTransformation FromFFmpegDisplayMatrix(const int32_t* matrix);

  constexpr VideoTransformation(VideoRotation rotation, bool mirrored)
      : rotation(rotation), mirrored(mirrored) {}
  constexpr VideoTransformation(VideoRotation r)
      : VideoTransformation(r, false) {}
  constexpr VideoTransformation()
      : VideoTransformation(VIDEO_ROTATION_0, false) {}

  // Rotation by angle Θ is represented in the matrix as:
  // [ cos(Θ), -sin(Θ)]
  // [ sin(Θ),  cos(Θ)]
  // A vertical flip is represented by the cosine's having opposite signs
  // and a horizontal flip is represented by the sine's having the same sign.
  VideoTransformation(const int32_t matrix[4]);

  // The video rotation value, in 90 degree steps.
  VideoRotation rotation;

  // Whether the video should be flipped about its Y axis.
  // This transformation takes place _after_ rotation, since they are not
  // commutative.
  bool mirrored;
};

MEDIA_EXPORT bool operator==(const struct VideoTransformation& first,
                             const struct VideoTransformation& second);

constexpr VideoTransformation kNoTransformation = VideoTransformation();

std::string MEDIA_EXPORT VideoRotationToString(VideoRotation rotation);

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_TRANSFORMATION_H_
