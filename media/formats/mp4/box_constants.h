// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP4_BOX_CONSTANTS_H_
#define MEDIA_FORMATS_MP4_BOX_CONSTANTS_H_

#include "media/formats/mp4/box_reader.h"

namespace media {

// ISO/IEC 14496-12.
// A transformation matrix for the video.
// Video frames are not scaled, rotated, or skewed, and are displayed at
// their original size with no zoom or depth applied.

// The value 0x00010000 in the top-left and middle element of the
// matrix specifies the horizontal and vertical scaling factor,
// respectively. This means that the video frames are not scaled and
// are displayed at their original size.

// The bottom-right element of the matrix, with a value of 0x40000000,
// specifies the fixed-point value of the zoom or depth of the video frames.
// This value is equal to 1.0 in decimal notation, meaning that there
// is no zoom or depth applied to the video frames.
inline constexpr mp4::DisplayMatrix kDisplayIdentityMatrix = {
    0x00010000, 0, 0, 0, 0x00010000, 0, 0, 0, 0x40000000};

}  // namespace media

#endif  // MEDIA_FORMATS_MP4_BOX_CONSTANTS_H_
