// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_style_image_value.h"

namespace blink {

double CSSStyleImageValue::intrinsicWidth(bool& is_null) const {
  const base::Optional<IntSize> size = IntrinsicSize();
  if (!size) {
    is_null = true;
    return 0;
  }
  return size.value().Width();
}

double CSSStyleImageValue::intrinsicHeight(bool& is_null) const {
  const base::Optional<IntSize> size = IntrinsicSize();
  if (!size) {
    is_null = true;
    return 0;
  }
  return size.value().Height();
}

double CSSStyleImageValue::intrinsicRatio(bool& is_null) const {
  const base::Optional<IntSize> size = IntrinsicSize();
  if (!size || size.value().Height() == 0) {
    is_null = true;
    return 0;
  }
  return static_cast<double>(size.value().Width()) / size.value().Height();
}

FloatSize CSSStyleImageValue::ElementSize(
    const FloatSize& default_object_size,
    const RespectImageOrientationEnum) const {
  return FloatSize(IntrinsicSize().value_or(IntSize()));
}

}  // namespace blink
