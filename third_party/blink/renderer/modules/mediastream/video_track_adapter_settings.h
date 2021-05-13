// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_VIDEO_TRACK_ADAPTER_SETTINGS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_VIDEO_TRACK_ADAPTER_SETTINGS_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/optional.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

class MODULES_EXPORT VideoTrackAdapterSettings {
 public:
  // Creates a VideoTrackAdapterSettings with no target resolution or frame rate
  // and without any constraints on the resolution.
  VideoTrackAdapterSettings();
  // Creates a VideoTrackAdapterSettings with a given target resolution and
  // and frame rate, and without any constraints on the resolution.
  VideoTrackAdapterSettings(const gfx::Size& target_size,
                            double max_frame_rate);
  // Creates a VideoTrackAdapterSettings with the specified resolution, frame
  // rate and resolution constraints. If |target_size| is null, it means that
  // no video processing is desired.
  VideoTrackAdapterSettings(base::Optional<gfx::Size> target_size,
                            double min_aspect_ratio,
                            double max_aspect_ratio,
                            double max_frame_rate);
  VideoTrackAdapterSettings(const VideoTrackAdapterSettings& other);
  VideoTrackAdapterSettings& operator=(const VideoTrackAdapterSettings& other);
  bool operator==(const VideoTrackAdapterSettings& other) const;

  const base::Optional<gfx::Size>& target_size() const { return target_size_; }
  int target_width() const {
    DCHECK(target_size_);
    return target_size_->width();
  }
  int target_height() const {
    DCHECK(target_size_);
    return target_size_->height();
  }
  double min_aspect_ratio() const { return min_aspect_ratio_; }
  double max_aspect_ratio() const { return max_aspect_ratio_; }
  double max_frame_rate() const { return max_frame_rate_; }
  void set_max_frame_rate(double max_frame_rate) {
    max_frame_rate_ = max_frame_rate;
  }

 private:
  base::Optional<gfx::Size> target_size_;
  double min_aspect_ratio_;
  double max_aspect_ratio_;
  // A |max_frame_rate| of zero is used to signal that no frame-rate
  // adjustment is necessary.
  // TODO(guidou): Change this to base::Optional. https://crbug.com/734528
  double max_frame_rate_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_VIDEO_TRACK_ADAPTER_SETTINGS_H_
