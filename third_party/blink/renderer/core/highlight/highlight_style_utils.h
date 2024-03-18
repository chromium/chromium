// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HIGHLIGHT_HIGHLIGHT_STYLE_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HIGHLIGHT_HIGHLIGHT_STYLE_UTILS_H_

#include <optional>

#include "base/containers/enum_set.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker.h"
#include "third_party/blink/renderer/core/paint/paint_flags.h"
#include "third_party/blink/renderer/core/paint/text_paint_style.h"
#include "third_party/blink/renderer/core/style/applied_text_decoration.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class Color;
class CSSProperty;
class Document;
class ComputedStyle;
class Node;
struct PaintInfo;

class CORE_EXPORT HighlightStyleUtils {
  STATIC_ONLY(HighlightStyleUtils);

 public:
  enum class HighlightColorProperty : unsigned {
    kCurrentColor,
    kFillColor,
    kStrokeColor,
    kEmphasisColor,
    kSelectionDecorationColor,
    kTextDecorationColor,
    // When adding another, update HighlightColorPropertySet below.
  };
  using HighlightColorPropertySet =
      base::EnumSet<HighlightColorProperty,
                    HighlightColorProperty::kCurrentColor,
                    HighlightColorProperty::kTextDecorationColor>;
  struct HighlightTextPaintStyle {
    TextPaintStyle style;
    Color text_decoration_color;
    HighlightColorPropertySet properties_using_current_color;
  };

  static Color ResolveColor(const Document&,
                            const ComputedStyle& originating_style,
                            const ComputedStyle* pseudo_style,
                            PseudoId pseudo,
                            const CSSProperty& property,
                            std::optional<Color> current_color);
  static std::optional<Color> MaybeResolveColor(
      const Document&,
      const ComputedStyle& originating_style,
      const ComputedStyle* pseudo_style,
      PseudoId pseudo,
      const CSSProperty& property);
  static std::optional<AppliedTextDecoration> SelectionTextDecoration(
      const Document& document,
      const ComputedStyle& style,
      const ComputedStyle& pseudo_style);
  static Color HighlightBackgroundColor(
      const Document&,
      const ComputedStyle&,
      Node*,
      std::optional<Color>,
      PseudoId,
      const AtomicString& pseudo_argument = g_null_atom);
  static HighlightTextPaintStyle HighlightPaintingStyle(
      const Document&,
      const ComputedStyle&,
      Node*,
      PseudoId,
      const TextPaintStyle&,
      const PaintInfo&,
      const AtomicString& pseudo_argument = g_null_atom);
  static const ComputedStyle* HighlightPseudoStyle(
      Node* node,
      const ComputedStyle& style,
      PseudoId pseudo,
      const AtomicString& pseudo_argument = g_null_atom);

  static void ResolveColorsFromPreviousLayer(
      HighlightTextPaintStyle& text_style,
      const HighlightTextPaintStyle& previous_layer_style);

  static bool ShouldInvalidateVisualOverflow(const Node& node,
                                             DocumentMarker::MarkerType type);

  static bool CustomHighlightHasVisualOverflow(
      const Node& node,
      const AtomicString& pseudo_argument = g_null_atom);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HIGHLIGHT_HIGHLIGHT_STYLE_UTILS_H_
