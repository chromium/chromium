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

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_size.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

struct BoxLayoutExtraInput;
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
  LayoutReplaced(Element*, const PhysicalSize& intrinsic_size);
  ~LayoutReplaced() override;

  // This function returns the local rect of the replaced content. The rectangle
  // is in the coordinate space of the element's physical border-box and assumes
  // no clipping.
  PhysicalRect ReplacedContentRect() const;
  virtual PhysicalRect ReplacedContentRectFrom(
      const PhysicalRect& base_content_rect) const;

  // This is used by a few special elements, e.g. <video>, <iframe> to ensure
  // a persistent sizing under different subpixel offset, because these
  // elements have a high cost to resize. The drawback is that we may overflow
  // or underflow the final content box by 1px.
  static PhysicalRect PreSnappedRectForPersistentSizing(const PhysicalRect&);

  void AddVisualEffectOverflow();
  void RecalcVisualOverflow() override;

  // These values are specified to be 300 and 150 pixels in the CSS 2.1 spec.
  // http://www.w3.org/TR/CSS2/visudet.html#inline-replaced-width
  static const int kDefaultWidth;
  static const int kDefaultHeight;
  bool CanHaveChildren() const override {
    NOT_DESTROYED();
    return false;
  }
  virtual bool DrawsBackgroundOntoContentLayer() const {
    NOT_DESTROYED();
    return false;
  }
  virtual void PaintReplaced(const PaintInfo&,
                             const PhysicalOffset& paint_offset) const {
    NOT_DESTROYED();
  }

  PhysicalRect LocalSelectionVisualRect() const final;

  bool HasObjectFit() const {
    NOT_DESTROYED();
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

  bool RespectsCSSOverflow() const override;

  // Returns true if the content is guarenteed to be clipped to the element's
  // content box.
  bool ClipsToContentBox() const;

  void SetNewContentRect(const PhysicalRect* new_content_rect) {
    NOT_DESTROYED();
    new_content_rect_ = new_content_rect;
  }

  // This returns a local rectangle excluding borders and padding from
  // FrameRect().
  //
  // This is a variant of LayoutBox::PhysicalContentBoxRect().
  // - Supports BoxLayoutExtraInput
  // - Doesn't support scrollbars
  PhysicalRect PhysicalContentBoxRectFromNG() const;

 protected:
  virtual bool CanApplyObjectViewBox() const {
    NOT_DESTROYED();
    return true;
  }

  bool IsInSelfHitTestingPhase(HitTestPhase phase) const override {
    NOT_DESTROYED();
    if (LayoutBox::IsInSelfHitTestingPhase(phase))
      return true;

    auto* element = DynamicTo<Element>(GetNode());
    return element && element->IsReplacedElementRespectingCSSOverflow() &&
           phase == HitTestPhase::kSelfBlockBackground;
  }

  void WillBeDestroyed() override;

  void UpdateLayout() override;

  PhysicalSize IntrinsicSize() const {
    NOT_DESTROYED();
    auto width_override = IntrinsicWidthOverride();
    auto height_override = IntrinsicHeightOverride();
    return PhysicalSize(width_override.value_or(intrinsic_size_.width),
                        height_override.value_or(intrinsic_size_.height));
  }

  // This function calculates the placement of the replaced contents. It takes
  // intrinsic size of the replaced contents, stretch to fit CSS content box
  // according to object-fit, object-position and object-view-box.
  PhysicalRect ComputeReplacedContentRect(
      const PhysicalRect& base_content_rect,
      const PhysicalSize* overridden_intrinsic_size = nullptr) const;

  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;

  void SetIntrinsicSize(const PhysicalSize& intrinsic_size) {
    NOT_DESTROYED();
    intrinsic_size_ = intrinsic_size;
  }

  PositionWithAffinity PositionForPoint(const PhysicalOffset&) const override;

  bool IsOfType(LayoutObjectType type) const override {
    NOT_DESTROYED();
    return type == kLayoutObjectReplaced || LayoutBox::IsOfType(type);
  }

  // The intrinsic size for a replaced element is based on its content's natural
  // size. This computes the size including the modification from
  // object-view-box for layout.
  // Note that the intrinsic size for the element can be independent of its
  // content's natural size. For example, if contain-intrinsic-size is
  // specified. Returns null for these cases.
  absl::optional<gfx::SizeF> ComputeObjectViewBoxSizeForIntrinsicSizing() const;

  // ReplacedPainter doesn't support CompositeBackgroundAttachmentFixed yet.
  bool ComputeCanCompositeBackgroundAttachmentFixed() const override {
    NOT_DESTROYED();
    return false;
  }

 private:
  // Computes a rect, relative to the element's content's natural size, that
  // should be used as the content source when rendering this element. This
  // value is used as the input for object-fit/object-position during painting.
  absl::optional<PhysicalRect> ComputeObjectViewBoxRect(
      const PhysicalSize* overridden_intrinsic_size = nullptr) const;

  PhysicalRect ComputeObjectFitAndPositionRect(
      const PhysicalRect& base_content_rect,
      const PhysicalSize* overridden_intrinsic_size) const;

  absl::optional<LayoutUnit> IntrinsicWidthOverride() const {
    NOT_DESTROYED();
    if (HasOverrideIntrinsicContentWidth())
      return OverrideIntrinsicContentWidth();
    else if (ShouldApplySizeContainment())
      return LayoutUnit();
    return absl::nullopt;
  }
  absl::optional<LayoutUnit> IntrinsicHeightOverride() const {
    NOT_DESTROYED();
    if (HasOverrideIntrinsicContentHeight())
      return OverrideIntrinsicContentHeight();
    else if (ShouldApplySizeContainment())
      return LayoutUnit();
    return absl::nullopt;
  }

  // The natural/intrinsic size for this replaced element based on the natural
  // size for the element's contents.
  mutable PhysicalSize intrinsic_size_;

  // The new content rect for SVG roots. This is set during layout, and cleared
  // afterwards. Always nullptr when this object isn't in the process of being
  // laid out.
  const PhysicalRect* new_content_rect_ = nullptr;
};

template <>
struct DowncastTraits<LayoutReplaced> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsLayoutReplaced();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_REPLACED_H_
