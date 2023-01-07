// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINT_STYLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINT_STYLE_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/frame/color_scheme.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/style/applied_text_decoration.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ShadowList;

struct CORE_EXPORT TextPaintStyle {
  DISALLOW_NEW();

 public:
  Color current_color;
  Color fill_color;
  Color stroke_color;
  Color emphasis_mark_color;
  float stroke_width;
  mojom::blink::ColorScheme color_scheme;
  scoped_refptr<const ShadowList> shadow;
  absl::optional<AppliedTextDecoration> selection_text_decoration;

  bool operator==(const TextPaintStyle& other) const {
    return current_color == other.current_color &&
           fill_color == other.fill_color &&
           stroke_color == other.stroke_color &&
           emphasis_mark_color == other.emphasis_mark_color &&
           stroke_width == other.stroke_width &&
           color_scheme == other.color_scheme && shadow == other.shadow &&
           selection_text_decoration == other.selection_text_decoration;
  }
  bool operator!=(const TextPaintStyle& other) const {
    return !(*this == other);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINT_STYLE_H_
