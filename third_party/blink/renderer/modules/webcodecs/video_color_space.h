// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_COLOR_SPACE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_COLOR_SPACE_H_

#include "media/base/video_color_space.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_color_primaries.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_matrix_coefficients.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_transfer_characteristics.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "ui/gfx/color_space.h"

namespace blink {

class VideoColorSpaceInit;

class MODULES_EXPORT VideoColorSpace final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static VideoColorSpace* Create(const VideoColorSpaceInit*);

  explicit VideoColorSpace() = default;
  explicit VideoColorSpace(const VideoColorSpaceInit*);
  explicit VideoColorSpace(const gfx::ColorSpace&);
  explicit VideoColorSpace(const media::VideoColorSpace&);

  gfx::ColorSpace ToGfxColorSpace() const;
  media::VideoColorSpace ToMediaColorSpace() const;

  absl::optional<V8VideoColorPrimaries> primaries() const { return primaries_; }
  absl::optional<V8VideoTransferCharacteristics> transfer() const {
    return transfer_;
  }
  absl::optional<V8VideoMatrixCoefficients> matrix() const { return matrix_; }
  absl::optional<bool> fullRange() const { return full_range_; }

  VideoColorSpaceInit* toJSON() const;

 private:
  absl::optional<V8VideoColorPrimaries> primaries_;
  absl::optional<V8VideoTransferCharacteristics> transfer_;
  absl::optional<V8VideoMatrixCoefficients> matrix_;
  absl::optional<bool> full_range_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_COLOR_SPACE_H_
