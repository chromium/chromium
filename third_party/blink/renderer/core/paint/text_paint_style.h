// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINT_STYLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINT_STYLE_H_

#include "third_party/blink/public/common/css/color_scheme.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ShadowList;

struct CORE_EXPORT TextPaintStyle {
  STACK_ALLOCATED();

 public:
  Color current_color;
  Color fill_color;
  Color stroke_color;
  Color emphasis_mark_color;
  float stroke_width;
  ColorScheme color_scheme;
  const ShadowList* shadow;

  bool operator==(const TextPaintStyle& other) const {
    return current_color == other.current_color &&
           fill_color == other.fill_color &&
           stroke_color == other.stroke_color &&
           emphasis_mark_color == other.emphasis_mark_color &&
           stroke_width == other.stroke_width &&
           color_scheme == other.color_scheme && shadow == other.shadow;
  }
  bool operator!=(const TextPaintStyle& other) const {
    return !(*this == other);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINT_STYLE_H_
