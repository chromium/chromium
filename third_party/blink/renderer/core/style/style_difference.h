// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_DIFFERENCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_DIFFERENCE_H_

#include <algorithm>
#include <iosfwd>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class StyleDifference {
  STACK_ALLOCATED();

 public:
  void Merge(StyleDifference other) {
    needs_reshape |= other.needs_reshape;
    needs_recompute_visual_overflow |= other.needs_recompute_visual_overflow;
    disable_scroll_anchoring |= other.disable_scroll_anchoring;
    compositing_reasons_changed |= other.compositing_reasons_changed;
    background_color_changed |= other.background_color_changed;
    blend_mode_changed |= other.blend_mode_changed;
    border_radius_changed |= other.border_radius_changed;
    border_shape_changed |= other.border_shape_changed;
    clip_path_changed |= other.clip_path_changed;
    clip_property_changed |= other.clip_property_changed;
    filter_changed |= other.filter_changed;
    mask_changed |= other.mask_changed;
    opacity_changed |= other.opacity_changed;
    only_transform_property_changed |= other.only_transform_property_changed;
    text_decoration_or_color_changed |= other.text_decoration_or_color_changed;
    transform_changed |= other.transform_changed;
    transform_data_changed |= other.transform_data_changed;
    z_index_changed |= other.z_index_changed;
    paint_type_ = std::max(paint_type_, other.paint_type_);
    layout_type_ = std::max(layout_type_, other.layout_type_);
  }

  bool HasDifference() const {
    return needs_reshape || needs_recompute_visual_overflow ||
           disable_scroll_anchoring || compositing_reasons_changed ||
           background_color_changed || blend_mode_changed ||
           border_radius_changed || border_shape_changed || clip_path_changed ||
           clip_property_changed || filter_changed || mask_changed ||
           opacity_changed || only_transform_property_changed ||
           text_decoration_or_color_changed || transform_changed ||
           transform_data_changed || z_index_changed || paint_type_ ||
           layout_type_;
  }

  // For simple paint invalidation, we can directly invalidate the
  // DisplayItemClients during style update, without paint invalidation during
  // PrePaintTreeWalk.
  bool NeedsSimplePaintInvalidation() const {
    return paint_type_ == kSimplePaint;
  }
  bool NeedsNormalPaintInvalidation() const {
    return paint_type_ == kNormalPaint;
  }

  void SetNeedsNormalPaintInvalidation() { paint_type_ = kNormalPaint; }
  void SetNeedsSimplePaintInvalidation() {
    DCHECK(!NeedsNormalPaintInvalidation());
    paint_type_ = kSimplePaint;
  }

  bool NeedsFullLayout() const { return layout_type_ == kFullLayout; }
  bool NeedsPositionedLayout() const {
    return layout_type_ == kPositionedLayout;
  }

  void SetNeedsFullLayout() { layout_type_ = kFullLayout; }
  void SetNeedsPositionedLayout() {
    DCHECK(!NeedsFullLayout());
    layout_type_ = kPositionedLayout;
  }

  unsigned needs_reshape : 1 = false;
  unsigned needs_recompute_visual_overflow : 1 = false;
  unsigned disable_scroll_anchoring : 1 = false;
  unsigned compositing_reasons_changed : 1 = false;
  unsigned background_color_changed : 1 = false;
  unsigned blend_mode_changed : 1 = false;
  unsigned border_radius_changed : 1 = false;
  unsigned border_shape_changed : 1 = false;
  unsigned clip_path_changed : 1 = false;
  unsigned clip_property_changed : 1 = false;
  unsigned filter_changed : 1 = false;
  unsigned mask_changed : 1 = false;
  unsigned opacity_changed : 1 = false;
  unsigned only_transform_property_changed : 1 = false;
  // The object needs to issue paint invalidations if it is affected by text
  // decorations or properties dependent on color (e.g., border or outline).
  // TextDecorationLine changes must also invalidate ink overflow.
  unsigned text_decoration_or_color_changed : 1 = false;
  unsigned transform_changed : 1 = false;
  unsigned transform_data_changed : 1 = false;
  unsigned z_index_changed : 1 = false;

 private:
  enum PaintType { kNoPaint = 0, kSimplePaint, kNormalPaint };
  unsigned paint_type_ : 2 = kNoPaint;

  enum LayoutType { kNoLayout = 0, kPositionedLayout, kFullLayout };
  unsigned layout_type_ : 2 = kNoLayout;

  // This exists only to get the object up to exactly 32 bits, which keeps
  // Clang from making partial writes of it when copying (making two small
  // writes to the stack and then reading the same data back again with a large
  // read can cause store-to-load forward stalls). Feel free to take bits from
  // here if you need them for something else.
  unsigned padding_ [[maybe_unused]] : 10;

  friend CORE_EXPORT std::ostream& operator<<(std::ostream&,
                                              const StyleDifference&);
};
static_assert(sizeof(StyleDifference) == 4, "Remove some padding bits!");

CORE_EXPORT std::ostream& operator<<(std::ostream&, const StyleDifference&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_DIFFERENCE_H_
