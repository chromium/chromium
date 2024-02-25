// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_style_image_value.h"

namespace blink {

double CSSStyleImageValue::intrinsicWidth(bool& is_null) const {
  const std::optional<gfx::Size> size = IntrinsicSize();
  if (!size) {
    is_null = true;
    return 0;
  }
  return size.value().width();
}

double CSSStyleImageValue::intrinsicHeight(bool& is_null) const {
  const std::optional<gfx::Size> size = IntrinsicSize();
  if (!size) {
    is_null = true;
    return 0;
  }
  return size.value().height();
}

double CSSStyleImageValue::intrinsicRatio(bool& is_null) const {
  const std::optional<gfx::Size> size = IntrinsicSize();
  if (!size || size.value().height() == 0) {
    is_null = true;
    return 0;
  }
  return static_cast<double>(size.value().width()) / size.value().height();
}

gfx::SizeF CSSStyleImageValue::ElementSize(
    const gfx::SizeF& default_object_size,
    const RespectImageOrientationEnum) const {
  return gfx::SizeF(IntrinsicSize().value_or(gfx::Size()));
}

}  // namespace blink
