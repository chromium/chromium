// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_APPLIED_TEXT_DECORATION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_APPLIED_TEXT_DECORATION_H_

#include <optional>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/style/text_decoration_inset.h"
#include "third_party/blink/renderer/core/style/text_decoration_thickness.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class CORE_EXPORT AppliedTextDecoration {
  DISALLOW_NEW();

 public:
  // Set if the element that applies the decoration has a precomputed :hover
  // style (see `kPseudoIdPrecomputedHover`).
  struct PrecomputedHoverState {
    // True if the element's current style shows a visible decoration, so the
    // decoration is in effect. False if the current style's decoration is not
    // visible, so the decoration isn't in effect until the precomputed style
    // applies.
    bool is_current_visible = true;

    // The DOMNodeId of the element whose precomputed :hover style changes the
    // decoration. That's the element with the style, which isn't necessarily
    // the hovered one (e.g. #a in `#b:hover #a`).
    DOMNodeId source_id = kInvalidDOMNodeId;

    bool operator==(const PrecomputedHoverState&) const = default;
  };

  AppliedTextDecoration(TextDecorationLine,
                        ETextDecorationStyle,
                        Color,
                        TextDecorationThickness,
                        Length,
                        TextDecorationInset,
                        EBoxDecorationBreak);
  AppliedTextDecoration(TextDecorationLine,
                        ETextDecorationStyle,
                        Color unvisited_color,
                        Color visited_color,
                        bool is_visited,
                        TextDecorationThickness,
                        Length,
                        TextDecorationInset,
                        EBoxDecorationBreak);

  TextDecorationLine Lines() const {
    return static_cast<TextDecorationLine>(lines_);
  }
  ETextDecorationStyle Style() const {
    return static_cast<ETextDecorationStyle>(style_);
  }
  Color GetColor() const { return is_visited_ ? visited_color_ : color_; }
  void SetColor(Color color) {
    color_ = color;
    visited_color_ = color;
  }

  TextDecorationThickness Thickness() const { return thickness_; }
  Length UnderlineOffset() const { return underline_offset_; }
  const TextDecorationInset& DecorationInset() const {
    return decoration_inset_;
  }
  EBoxDecorationBreak BoxDecorationBreak() const {
    return static_cast<EBoxDecorationBreak>(box_decoration_break_);
  }

  const std::optional<PrecomputedHoverState>& GetPrecomputedHoverState() const {
    return precomputed_hover_state_;
  }
  void SetPrecomputedHoverState(PrecomputedHoverState hover_state) {
    precomputed_hover_state_ = hover_state;
  }

  // Returns true if the decoration applies now, not only once the precomputed
  // style applies (see `PrecomputedHoverState`). Otherwise, the decoration is
  // hidden until then; if the precomputed style shows it, it has the lines of
  // that style, transparent, so that ink overflow includes them.
  bool IsInEffect() const {
    return !precomputed_hover_state_ ||
           precomputed_hover_state_->is_current_visible;
  }

  bool operator==(const AppliedTextDecoration&) const;

 private:
  unsigned lines_ : kTextDecorationLineBits;
  unsigned style_ : 3;  // ETextDecorationStyle
  unsigned box_decoration_break_ : 1;
  // Whether this decoration was created for a visited link. Deliberately
  // excluded from operator== to avoid leaking :visited state through style
  // diffing or decoration run merging.
  unsigned is_visited_ : 1;
  Color color_;
  Color visited_color_;
  TextDecorationThickness thickness_;
  Length underline_offset_;
  TextDecorationInset decoration_inset_;
  std::optional<PrecomputedHoverState> precomputed_hover_state_;
};

using AppliedTextDecorationVector = GCedHeapVector<AppliedTextDecoration, 1>;

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(blink::AppliedTextDecoration)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_APPLIED_TEXT_DECORATION_H_
