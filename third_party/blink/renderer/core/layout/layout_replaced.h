/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2009 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_REPLACED_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_REPLACED_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"

namespace blink {

struct IntrinsicSizingInfo;

// LayoutReplaced is the base class for a replaced element as defined by CSS:
//
// "An element whose content is outside the scope of the CSS formatting model,
// such as an image, embedded document, or applet."
// http://www.w3.org/TR/CSS2/conform.html#defs
//
// Blink consider that replaced elements have an intrinsic sizes (e.g. the
// natural size of an image or a video). The intrinsic size is stored by
// m_intrinsicSize.
//
// The computation sometimes ask for the intrinsic ratio, defined as follow:
//
//                      intrinsicWidth
//   intrinsicRatio = -------------------
//                      intrinsicHeight
//
// The intrinsic ratio is used to keep the same proportion as the intrinsic
// size (thus avoiding visual distortions if width / height doesn't match
// the intrinsic value).
class CORE_EXPORT LayoutReplaced : public LayoutBox {
 public:
  LayoutReplaced(Element*);
  LayoutReplaced(Element*, const LayoutSize& intrinsic_size);
  ~LayoutReplaced() override;

  LayoutUnit ComputeReplacedLogicalWidth(
      ShouldComputePreferred = kComputeActual) const override;
  LayoutUnit ComputeReplacedLogicalHeight(
      LayoutUnit estimated_used_width = LayoutUnit()) const override;

  bool HasReplacedLogicalHeight() const;
  // This function returns the local rect of the replaced content.
  virtual PhysicalRect ReplacedContentRect() const;

  // This is used by a few special elements, e.g. <video>, <iframe> to ensure
  // a persistent sizing under different subpixel offset, because these
  // elements have a high cost to resize. The drawback is that we may overflow
  // or underflow the final content box by 1px.
  static PhysicalRect PreSnappedRectForPersistentSizing(const PhysicalRect&);

  bool NeedsPreferredWidthsRecalculation() const override;

  void RecalcVisualOverflow() override;

  // These values are specified to be 300 and 150 pixels in the CSS 2.1 spec.
  // http://www.w3.org/TR/CSS2/visudet.html#inline-replaced-width
  static const int kDefaultWidth;
  static const int kDefaultHeight;
  bool CanHaveChildren() const override { return false; }
  virtual void PaintReplaced(const PaintInfo&,
                             const PhysicalOffset& paint_offset) const {}

  PhysicalRect LocalSelectionVisualRect() const final;

  bool HasObjectFit() const {
    return StyleRef().GetObjectFit() !=
           ComputedStyleInitialValues::InitialObjectFit();
  }

  void Paint(const PaintInfo&) const override;

  // This function is public only so we can call it when computing
  // intrinsic size in LayoutNG.
  virtual void ComputeIntrinsicSizingInfo(IntrinsicSizingInfo&) const;

  // This callback must be invoked whenever the underlying intrinsic size has
  // changed.
  //
  // The intrinsic size can change due to the network (from the default
  // intrinsic size [see above] to the actual intrinsic size) or to some
  // CSS properties like 'zoom' or 'image-orientation'.
  virtual void IntrinsicSizeChanged();

 protected:
  void WillBeDestroyed() override;

  void UpdateLayout() override;

  LayoutSize IntrinsicSize() const final {
    return LayoutSize(IntrinsicWidth(), IntrinsicHeight());
  }

  LayoutUnit IntrinsicWidth() const {
    if (HasOverrideIntrinsicContentWidth())
      return OverrideIntrinsicContentWidth();
    else if (ShouldApplySizeContainment())
      return LayoutUnit();
    return intrinsic_size_.Width();
  }
  LayoutUnit IntrinsicHeight() const {
    if (HasOverrideIntrinsicContentHeight())
      return OverrideIntrinsicContentHeight();
    else if (ShouldApplySizeContainment())
      return LayoutUnit();
    return intrinsic_size_.Height();
  }

  void ComputePositionedLogicalWidth(
      LogicalExtentComputedValues&) const override;
  void ComputePositionedLogicalHeight(
      LogicalExtentComputedValues&) const override;

  MinMaxSizes ComputeIntrinsicLogicalWidths() const final;

  // This function calculates the placement of the replaced contents. It takes
  // intrinsic size of the replaced contents, stretch to fit CSS content box
  // according to object-fit.
  PhysicalRect ComputeObjectFit(
      const LayoutSize* overridden_intrinsic_size = nullptr) const;

  LayoutUnit IntrinsicContentLogicalHeight() const override {
    return IntrinsicLogicalHeight();
  }

  virtual LayoutUnit MinimumReplacedHeight() const { return LayoutUnit(); }

  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;

  void SetIntrinsicSize(const LayoutSize& intrinsic_size) {
    intrinsic_size_ = intrinsic_size;
  }

  PositionWithAffinity PositionForPoint(const PhysicalOffset&) const override;

  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectLayoutReplaced || LayoutBox::IsOfType(type);
  }

 private:
  MinMaxSizes PreferredLogicalWidths() const final;

  void ComputeIntrinsicSizingInfoForReplacedContent(IntrinsicSizingInfo&) const;
  FloatSize ConstrainIntrinsicSizeToMinMax(const IntrinsicSizingInfo&) const;

  LayoutUnit ComputeConstrainedLogicalWidth(ShouldComputePreferred) const;

  mutable LayoutSize intrinsic_size_;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutReplaced, IsLayoutReplaced());

}  // namespace blink

#endif
