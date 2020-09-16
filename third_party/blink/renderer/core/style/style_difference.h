// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_DIFFERENCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_DIFFERENCE_H_

#include <algorithm>
#include <iosfwd>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

class StyleDifference {
  STACK_ALLOCATED();

 public:
  enum PropertyDifference {
    kTransformChanged = 1 << 0,
    kOpacityChanged = 1 << 1,
    kZIndexChanged = 1 << 2,
    kFilterChanged = 1 << 3,
    kBackdropFilterChanged = 1 << 4,
    kCSSClipChanged = 1 << 5,
    // The object needs to issue paint invalidations if it is affected by text
    // decorations or properties dependent on color (e.g., border or outline).
    // TextDecorationis changes must also invalidate ink overflow.
    kTextDecorationOrColorChanged = 1 << 6,
    kBlendModeChanged = 1 << 7,
    kMaskChanged = 1 << 8,
    // Whether background-color changed alpha to or from 1.
    kHasAlphaChanged = 1 << 9,
    // If you add a value here, be sure to update kPropertyDifferenceCount.
  };

  StyleDifference()
      : needs_paint_invalidation_(false),
        layout_type_(kNoLayout),
        needs_reshape_(false),
        recompute_visual_overflow_(false),
        visual_rect_update_(false),
        property_specific_differences_(0),
        scroll_anchor_disabling_property_changed_(false),
        compositing_reasons_changed_(false) {}

  void Merge(StyleDifference other) {
    needs_paint_invalidation_ |= other.needs_paint_invalidation_;
    layout_type_ = std::max(layout_type_, other.layout_type_);
    needs_reshape_ |= other.needs_reshape_;
    recompute_visual_overflow_ |= other.recompute_visual_overflow_;
    visual_rect_update_ |= other.visual_rect_update_;
    property_specific_differences_ |= other.property_specific_differences_;
    scroll_anchor_disabling_property_changed_ |=
        other.scroll_anchor_disabling_property_changed_;
    compositing_reasons_changed_ |= other.compositing_reasons_changed_;
  }

  bool HasDifference() const {
    return needs_paint_invalidation_ || layout_type_ || needs_reshape_ ||
           property_specific_differences_ || recompute_visual_overflow_ ||
           visual_rect_update_ || scroll_anchor_disabling_property_changed_ ||
           compositing_reasons_changed_;
  }

  bool HasAtMostPropertySpecificDifferences(
      unsigned property_differences) const {
    return !needs_paint_invalidation_ && !layout_type_ &&
           !(property_specific_differences_ & ~property_differences);
  }

  bool NeedsPaintInvalidation() const { return needs_paint_invalidation_; }
  void SetNeedsPaintInvalidation() { needs_paint_invalidation_ = true; }

  bool NeedsLayout() const { return layout_type_ != kNoLayout; }
  void ClearNeedsLayout() { layout_type_ = kNoLayout; }

  // The offset of this positioned object has been updated.
  bool NeedsPositionedMovementLayout() const {
    return layout_type_ == kPositionedMovement;
  }
  void SetNeedsPositionedMovementLayout() {
    DCHECK(!NeedsFullLayout());
    layout_type_ = kPositionedMovement;
  }

  bool NeedsFullLayout() const { return layout_type_ == kFullLayout; }
  void SetNeedsFullLayout() { layout_type_ = kFullLayout; }

  bool NeedsReshape() const { return needs_reshape_; }
  void SetNeedsReshape() { needs_reshape_ = true; }

  bool NeedsRecomputeVisualOverflow() const {
    return recompute_visual_overflow_;
  }
  void SetNeedsRecomputeVisualOverflow() { recompute_visual_overflow_ = true; }

  bool NeedsVisualRectUpdate() const { return visual_rect_update_; }
  void SetNeedsVisualRectUpdate() { visual_rect_update_ = true; }

  bool TransformChanged() const {
    return property_specific_differences_ & kTransformChanged;
  }
  void SetTransformChanged() {
    property_specific_differences_ |= kTransformChanged;
  }

  bool OpacityChanged() const {
    return property_specific_differences_ & kOpacityChanged;
  }
  void SetOpacityChanged() {
    property_specific_differences_ |= kOpacityChanged;
  }

  bool ZIndexChanged() const {
    return property_specific_differences_ & kZIndexChanged;
  }
  void SetZIndexChanged() { property_specific_differences_ |= kZIndexChanged; }

  bool FilterChanged() const {
    return property_specific_differences_ & kFilterChanged;
  }
  void SetFilterChanged() { property_specific_differences_ |= kFilterChanged; }

  bool BackdropFilterChanged() const {
    return property_specific_differences_ & kBackdropFilterChanged;
  }
  void SetBackdropFilterChanged() {
    property_specific_differences_ |= kBackdropFilterChanged;
  }

  bool CssClipChanged() const {
    return property_specific_differences_ & kCSSClipChanged;
  }
  void SetCSSClipChanged() {
    property_specific_differences_ |= kCSSClipChanged;
  }

  bool BlendModeChanged() const {
    return property_specific_differences_ & kBlendModeChanged;
  }
  void SetBlendModeChanged() {
    property_specific_differences_ |= kBlendModeChanged;
  }

  bool TextDecorationOrColorChanged() const {
    return property_specific_differences_ & kTextDecorationOrColorChanged;
  }
  void SetTextDecorationOrColorChanged() {
    property_specific_differences_ |= kTextDecorationOrColorChanged;
  }

  bool MaskChanged() const {
    return property_specific_differences_ & kMaskChanged;
  }
  void SetMaskChanged() { property_specific_differences_ |= kMaskChanged; }

  bool HasAlphaChanged() const {
    return property_specific_differences_ & kHasAlphaChanged;
  }
  void SetHasAlphaChanged() {
    property_specific_differences_ |= kHasAlphaChanged;
  }

  bool ScrollAnchorDisablingPropertyChanged() const {
    return scroll_anchor_disabling_property_changed_;
  }
  void SetScrollAnchorDisablingPropertyChanged() {
    scroll_anchor_disabling_property_changed_ = true;
  }
  bool CompositingReasonsChanged() const {
    return compositing_reasons_changed_;
  }
  void SetCompositingReasonsChanged() { compositing_reasons_changed_ = true; }

 private:
  static constexpr int kPropertyDifferenceCount = 10;

  friend CORE_EXPORT std::ostream& operator<<(std::ostream&,
                                              const StyleDifference&);

  unsigned needs_paint_invalidation_ : 1;

  enum LayoutType { kNoLayout = 0, kPositionedMovement, kFullLayout };
  unsigned layout_type_ : 2;
  unsigned needs_reshape_ : 1;
  unsigned recompute_visual_overflow_ : 1;
  unsigned visual_rect_update_ : 1;
  unsigned property_specific_differences_ : kPropertyDifferenceCount;
  unsigned scroll_anchor_disabling_property_changed_ : 1;
  unsigned compositing_reasons_changed_ : 1;
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, const StyleDifference&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_DIFFERENCE_H_
