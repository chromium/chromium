// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_LENGTH_RESOLVER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_LENGTH_RESOLVER_H_

#include <limits>

#include "base/check_op.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

class CORE_EXPORT CSSLengthResolver {
 public:
  explicit CSSLengthResolver(float zoom) : zoom_(zoom) {}

  // Font-relative must be pre-zoomed.
  virtual float EmFontSize() const = 0;
  virtual float RemFontSize() const = 0;
  virtual float ExFontSize() const = 0;
  virtual float ChFontSize() const = 0;

  // Other sizes must not be pre-zoomed.
  virtual double ViewportWidth() const = 0;
  virtual double ViewportHeight() const = 0;
  virtual double SmallViewportWidth() const = 0;
  virtual double SmallViewportHeight() const = 0;
  virtual double LargeViewportWidth() const = 0;
  virtual double LargeViewportHeight() const = 0;
  virtual double DynamicViewportWidth() const = 0;
  virtual double DynamicViewportHeight() const = 0;
  virtual double ContainerWidth() const = 0;
  virtual double ContainerHeight() const = 0;

  virtual WritingMode GetWritingMode() const = 0;

  float Zoom() const { return zoom_; }
  void SetZoom(float zoom) {
    DCHECK(std::isfinite(zoom));
    DCHECK_GT(zoom, 0);
    zoom_ = zoom;
  }

  double ZoomedComputedPixels(double value, CSSPrimitiveValue::UnitType) const;

 private:
  bool IsHorizontalWritingMode() const {
    return blink::IsHorizontalWritingMode(GetWritingMode());
  }
  double ViewportWidthPercent() const;
  double ViewportHeightPercent() const;
  double ViewportInlineSizePercent() const;
  double ViewportBlockSizePercent() const;
  double ViewportMinPercent() const;
  double ViewportMaxPercent() const;
  double SmallViewportWidthPercent() const;
  double SmallViewportHeightPercent() const;
  double SmallViewportInlineSizePercent() const;
  double SmallViewportBlockSizePercent() const;
  double SmallViewportMinPercent() const;
  double SmallViewportMaxPercent() const;
  double LargeViewportWidthPercent() const;
  double LargeViewportHeightPercent() const;
  double LargeViewportInlineSizePercent() const;
  double LargeViewportBlockSizePercent() const;
  double LargeViewportMinPercent() const;
  double LargeViewportMaxPercent() const;
  double DynamicViewportWidthPercent() const;
  double DynamicViewportHeightPercent() const;
  double DynamicViewportInlineSizePercent() const;
  double DynamicViewportBlockSizePercent() const;
  double DynamicViewportMinPercent() const;
  double DynamicViewportMaxPercent() const;
  double ContainerWidthPercent() const;
  double ContainerHeightPercent() const;
  double ContainerInlineSizePercent() const;
  double ContainerBlockSizePercent() const;
  double ContainerMinPercent() const;
  double ContainerMaxPercent() const;

  float zoom_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_LENGTH_RESOLVER_H_
