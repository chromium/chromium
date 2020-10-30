// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_HIGHLIGHT_PAINTING_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_HIGHLIGHT_PAINTING_UTILS_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/paint/paint_phase.h"
#include "third_party/blink/renderer/core/style/applied_text_decoration.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Color;
class Document;
class ComputedStyle;
class Node;
struct TextPaintStyle;
struct PaintInfo;

class CORE_EXPORT HighlightPaintingUtils {
  STATIC_ONLY(HighlightPaintingUtils);

 public:
  static base::Optional<AppliedTextDecoration> HighlightTextDecoration(
      const ComputedStyle& style,
      const ComputedStyle& pseudo_style);
  static Color HighlightBackgroundColor(const Document&,
                                        const ComputedStyle&,
                                        Node*,
                                        PseudoId);
  static Color HighlightForegroundColor(const Document&,
                                        const ComputedStyle&,
                                        Node*,
                                        PseudoId,
                                        const GlobalPaintFlags);
  static Color HighlightEmphasisMarkColor(const Document&,
                                          const ComputedStyle&,
                                          Node*,
                                          PseudoId,
                                          const GlobalPaintFlags);
  static TextPaintStyle HighlightPaintingStyle(const Document&,
                                               const ComputedStyle&,
                                               Node*,
                                               PseudoId,
                                               const TextPaintStyle& text_style,
                                               const PaintInfo&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_HIGHLIGHT_PAINTING_UTILS_H_
