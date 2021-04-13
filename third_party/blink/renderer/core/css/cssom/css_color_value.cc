// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_color_value.h"
#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/css/cssom/css_rgb.h"

namespace blink {

CSSRGB* CSSColorValue::toRGB() const {
  return MakeGarbageCollected<CSSRGB>(ToColor());
}

const CSSValue* CSSColorValue::ToCSSValue() const {
  return cssvalue::CSSColor::Create(ToColor().Rgb());
}

}  // namespace blink
