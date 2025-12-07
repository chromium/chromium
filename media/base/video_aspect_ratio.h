// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_ASPECT_RATIO_H_
#define MEDIA_BASE_VIDEO_ASPECT_RATIO_H_

#include <stdint.h>

#include "media/base/media_export.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace mojo {
template <typename DataViewType, typename T>
struct StructTraits;
}  // namespace mojo

namespace media {

namespace mojom {
class VideoAspectRatioDataView;
}  // namespace mojom

class MEDIA_EXPORT VideoAspectRatio {
 public:
  // Create a pixel aspect ratio (PAR). |width| and |height| describe the
  // shape of a pixel. For example, an anamorphic video has regtangular pixels
  // with a 2:1 PAR, that is they are twice as wide as they are tall.
  //
  // Note that this is also called a sample aspect ratio (SAR), but SAR can also
  // mean storage aspect ratio (which is the coded size).
  static VideoAspectRatio PAR(int width, int height);

  // Create a display aspect ratio (DAR). |width| and |height| describe the
  // shape of the rendered picture. For example a 1920x1080 video with square
  // pixels has a 1920:1080 = 16:9 DAR.
  static VideoAspectRatio DAR(int width, int height);

  // A default-constructed VideoAspectRatio is !IsValid().
  VideoAspectRatio() = default;

  // Create a VideoAspectRatio from a known |natural_size|.
  // TODO(crbug.com/40769111): Remove.
  VideoAspectRatio(const gfx::Rect& visible_rect,
                   const gfx::Size& natural_size);

  bool operator==(const VideoAspectRatio& other) const;

  // An aspect ratio is invalid if it was default constructed, had nonpositive
  // components, or exceeds implementation limits.
  bool IsValid() const;

  // Computes the expected display size for a given visible size.
  // Returns visible_rect.size() if !IsValid().
  gfx::Size GetNaturalSize(const gfx::Rect& visible_rect) const;

 private:
  friend struct mojo::StructTraits<mojom::VideoAspectRatioDataView,
                                   VideoAspectRatio>;

  enum class Type {
    kDisplay,
    kPixel,
  };

  VideoAspectRatio(Type type, int width, int height);

  Type type_ = Type::kDisplay;
  double aspect_ratio_ = 0.0;
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_ASPECT_RATIO_H_
