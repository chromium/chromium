// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_FORMAT_COLOR_SPACE_H_
#define MEDIA_BASE_ANDROID_MEDIA_FORMAT_COLOR_SPACE_H_

#include "media/base/media_export.h"
#include "media/base/video_color_space.h"
#include "ui/gfx/color_space.h"

namespace media {

// A C++ struct that represents the integer values accepted by Android's
// MediaFormat for color information.
struct MEDIA_EXPORT MediaFormatColorSpace {
  MediaFormatColorSpace() = default;

  static constexpr int kUnknown = -1;
  int standard = kUnknown;
  int transfer = kUnknown;
  int range = kUnknown;

  bool operator==(const MediaFormatColorSpace& other) const {
    return standard == other.standard && transfer == other.transfer &&
           range == other.range;
  }
  bool operator!=(const MediaFormatColorSpace& other) const {
    return !(*this == other);
  }

  // This will set `standard`, `transfer`, or `range` to kUnknown if there is no
  // clear translation.
  explicit MediaFormatColorSpace(const VideoColorSpace& color_space);
  static MediaFormatColorSpace MakeRec709();
  static MediaFormatColorSpace MakeHdr10();

  // This will return the default (invalid) color space if any of `standard`,
  // `transfer`, or `range` are unrecognized.
  gfx::ColorSpace ToGfxColorSpace() const;
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_FORMAT_COLOR_SPACE_H_
