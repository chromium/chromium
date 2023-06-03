// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HIGHLIGHT_HIGHLIGHT_STYLE_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HIGHLIGHT_HIGHLIGHT_STYLE_UTILS_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker.h"
#include "third_party/blink/renderer/core/paint/paint_flags.h"
#include "third_party/blink/renderer/core/style/applied_text_decoration.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class Color;
class CSSProperty;
class Document;
class ComputedStyle;
class Node;
struct TextPaintStyle;
struct PaintInfo;

class CORE_EXPORT HighlightStyleUtils {
  STATIC_ONLY(HighlightStyleUtils);

 public:
  static Color ResolveColor(const Document&,
                            const ComputedStyle& originating_style,
                            const ComputedStyle* pseudo_style,
                            PseudoId pseudo,
                            const CSSProperty& property,
                            absl::optional<Color> previous_layer_color);
  static absl::optional<AppliedTextDecoration> SelectionTextDecoration(
      const Document& document,
      const ComputedStyle& style,
      const ComputedStyle& pseudo_style,
      absl::optional<Color> previous_layer_color);
  static Color HighlightBackgroundColor(
      const Document&,
      const ComputedStyle&,
      Node*,
      absl::optional<Color> previous_layer_color,
      PseudoId,
      const AtomicString& pseudo_argument = g_null_atom);
  static TextPaintStyle HighlightPaintingStyle(
      const Document&,
      const ComputedStyle&,
      Node*,
      PseudoId,
      const TextPaintStyle& previous_layer_text_style,
      const PaintInfo&,
      const AtomicString& pseudo_argument = g_null_atom);
  static absl::optional<Color> HighlightTextDecorationColor(
      const Document&,
      const ComputedStyle&,
      Node*,
      absl::optional<Color> previous_layer_color,
      PseudoId);

  static scoped_refptr<const ComputedStyle> HighlightPseudoStyle(
      Node* node,
      const ComputedStyle& style,
      PseudoId pseudo,
      const AtomicString& pseudo_argument = g_null_atom);

  static bool ShouldInvalidateVisualOverflow(const Node& node,
                                             DocumentMarker::MarkerType type);

  static bool CustomHighlightHasVisualOverflow(
      const Node* node,
      const AtomicString& pseudo_argument = g_null_atom);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HIGHLIGHT_HIGHLIGHT_STYLE_UTILS_H_
