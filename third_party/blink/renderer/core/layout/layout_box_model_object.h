/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2006, 2007, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_BOX_MODEL_OBJECT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_BOX_MODEL_OBJECT_H_

#include <memory>
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/background_bleed_avoidance.h"
#include "third_party/blink/renderer/core/layout/content_change_type.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/text/writing_mode_utils.h"

namespace blink {

class PaintLayer;
class PaintLayerScrollableArea;

enum PaintLayerType {
  kNoPaintLayer,
  kNormalPaintLayer,
  // A forced or overflow clip layer is required for bookkeeping purposes,
  // but does not force a layer to be self painting.
  kOverflowClipPaintLayer,
  kForcedPaintLayer
};

// Modes for some of the line-related functions.
enum LinePositionMode {
  kPositionOnContainingLine,
  kPositionOfInteriorLineBoxes
};
enum LineDirectionMode { kHorizontalLine, kVerticalLine };

// This class is the base class for all CSS objects.
//
// All CSS objects follow the box model object. See THE BOX MODEL section in
// LayoutBox for more information.
//
// This class actually doesn't have the box model but it exposes some common
// functions or concepts that sub-classes can extend upon. For example, there
// are accessors for margins, borders, paddings and borderBoundingBox().
//
// The reason for this partial implementation is that the 2 classes inheriting
// from it (LayoutBox and LayoutInline) have different requirements but need to
// have a PaintLayer. For a full implementation of the box model, see LayoutBox.
//
// An important member of this class is PaintLayer, which is stored in a rare-
// data pattern (see: Layer()). PaintLayers are instantiated for several reasons
// based on the return value of layerTypeRequired().
// Interestingly, most SVG objects inherit from LayoutSVGModelObject and thus
// can't have a PaintLayer. This is an unfortunate artifact of our
// design as it limits code sharing and prevents hardware accelerating SVG
// (the current design require a PaintLayer for compositing).
//
//
// ***** COORDINATE SYSTEMS *****
//
// In order to fully understand LayoutBoxModelObject and the inherited classes,
// we need to introduce the concept of coordinate systems.
// There are 4 coordinate systems:
// - physical coordinates: it is the coordinate system used for painting and
//   correspond to physical direction as seen on the physical display (screen,
//   printed page). In CSS, 'top', 'right', 'bottom', 'left' are all in physical
//   coordinates. The code matches this convention too.
//
// - logical coordinates: this is the coordinate system used for layout. It is
//   determined by 'writing-mode' and 'direction'. Any property using 'before',
//   'after', 'start' or 'end' is in logical coordinates. Those are also named
//   respectively 'logical top', 'logical bottom', 'logical left' and
//   'logical right'.
//
// Example with writing-mode: vertical-rl; direction: ltr;
//
//                    'top' / 'start' side
//
//                     block-flow direction
//           <------------------------------------ |
//           ------------------------------------- |
//           |        c   |          s           | |
// 'left'    |        o   |          o           | |   inline     'right'
//    /      |        n   |          m           | |  direction      /
// 'after'   |        t   |          e           | |              'before'
//  side     |        e   |                      | |                side
//           |        n   |                      | |
//           |        t   |                      | |
//           ------------------------------------- v
//
//                 'bottom' / 'end' side
//
// See https://drafts.csswg.org/css-writing-modes-3/#text-flow for some
// extra details.
//
// - physical coordinates with flipped block-flow direction: those are physical
//   coordinates but we flipped the block direction. Almost all geometries
//   in box layout use this coordinate space, except those having explicit
//   "Logical" or "Physical" prefix in their names, or the name implies logical
//   (e.g. InlineStart, BlockEnd) or physical (e.g. Top, Left), or the return
//   type is PhysicalRect.
//
// - logical coordinates without flipping inline direction: those are "logical
//   block coordinates", without considering text direction. Examples are
//   "LogicalLeft" and "LogicalRight".
//
// For more information, see the following doc about coordinate spaces:
// https://chromium.googlesource.com/chromium/src.git/+/master/third_party/blink/renderer/core/layout/README.md#coordinate-spaces
class CORE_EXPORT LayoutBoxModelObject : public LayoutObject {
 public:
  LayoutBoxModelObject(ContainerNode*);
  ~LayoutBoxModelObject() override;

  // This is the only way layers should ever be destroyed.
  void DestroyLayer();

  PhysicalOffset RelativePositionOffset() const;
  LayoutSize RelativePositionLogicalOffset() const {
    // TODO(layout-dev): This seems incorrect in flipped blocks writing mode,
    // but seems for legacy layout only.
    auto offset = RelativePositionOffset().ToLayoutSize();
    return StyleRef().IsHorizontalWritingMode() ? offset
                                                : offset.TransposedSize();
  }

  // Populates StickyPositionConstraints, setting the sticky box rect,
  // containing block rect and updating the constraint offsets according to the
  // available space.
  PhysicalRect ComputeStickyConstrainingRect() const;
  void UpdateStickyPositionConstraints() const;
  PhysicalOffset StickyPositionOffset() const;
  bool IsSlowRepaintConstrainedObject() const;

  PhysicalOffset OffsetForInFlowPosition() const;

  // IE extensions. Used to calculate offsetWidth/Height. Overridden by inlines
  // (LayoutInline) to return the remaining width on a given line (and the
  // height of a single line).
  virtual LayoutUnit OffsetLeft(const Element*) const;
  virtual LayoutUnit OffsetTop(const Element*) const;
  virtual LayoutUnit OffsetWidth() const = 0;
  virtual LayoutUnit OffsetHeight() const = 0;

  int PixelSnappedOffsetLeft(const Element* parent) const {
    return RoundToInt(OffsetLeft(parent));
  }
  int PixelSnappedOffsetTop(const Element* parent) const {
    return RoundToInt(OffsetTop(parent));
  }
  virtual int PixelSnappedOffsetWidth(const Element*) const;
  virtual int PixelSnappedOffsetHeight(const Element*) const;

  bool HasSelfPaintingLayer() const;
  PaintLayer* Layer() const { return FirstFragment().Layer(); }
  // The type of PaintLayer to instantiate. Any value returned from this
  // function other than NoPaintLayer will lead to a PaintLayer being created.
  virtual PaintLayerType LayerTypeRequired() const = 0;
  PaintLayerScrollableArea* GetScrollableArea() const;

  virtual void UpdateFromStyle();

  // This will work on inlines to return the bounding box of all of the lines'
  // border boxes.
  virtual IntRect BorderBoundingBox() const = 0;

  virtual PhysicalRect PhysicalVisualOverflowRect() const = 0;

  bool UsesCompositedScrolling() const;

  // Returns which layers backgrounds should be painted into for a overflow
  // scrolling box if it uses composited scrolling.
  BackgroundPaintLocation ComputeBackgroundPaintLocationIfComposited() const;

  // These return the CSS computed padding values.
  LayoutUnit ComputedCSSPaddingTop() const {
    return ComputedCSSPadding(StyleRef().PaddingTop());
  }
  LayoutUnit ComputedCSSPaddingBottom() const {
    return ComputedCSSPadding(StyleRef().PaddingBottom());
  }
  LayoutUnit ComputedCSSPaddingLeft() const {
    return ComputedCSSPadding(StyleRef().PaddingLeft());
  }
  LayoutUnit ComputedCSSPaddingRight() const {
    return ComputedCSSPadding(StyleRef().PaddingRight());
  }
  LayoutUnit ComputedCSSPaddingBefore() const {
    return ComputedCSSPadding(StyleRef().PaddingBefore());
  }
  LayoutUnit ComputedCSSPaddingAfter() const {
    return ComputedCSSPadding(StyleRef().PaddingAfter());
  }
  LayoutUnit ComputedCSSPaddingStart() const {
    return ComputedCSSPadding(StyleRef().PaddingStart());
  }
  LayoutUnit ComputedCSSPaddingEnd() const {
    return ComputedCSSPadding(StyleRef().PaddingEnd());
  }
  LayoutUnit ComputedCSSPaddingOver() const {
    return ComputedCSSPadding(StyleRef().PaddingOver());
  }
  LayoutUnit ComputedCSSPaddingUnder() const {
    return ComputedCSSPadding(StyleRef().PaddingUnder());
  }

  // These functions are used during layout.
  // - Table cells override them to include the intrinsic padding (see
  //   explanations in LayoutTableCell).
  // - Table override them to exclude padding with collapsing borders.
  virtual LayoutUnit PaddingTop() const { return ComputedCSSPaddingTop(); }
  virtual LayoutUnit PaddingBottom() const {
    return ComputedCSSPaddingBottom();
  }
  virtual LayoutUnit PaddingLeft() const { return ComputedCSSPaddingLeft(); }
  virtual LayoutUnit PaddingRight() const { return ComputedCSSPaddingRight(); }

  LayoutUnit PaddingBefore() const {
    return PhysicalPaddingToLogical().Before();
  }
  LayoutUnit PaddingAfter() const { return PhysicalPaddingToLogical().After(); }
  LayoutUnit PaddingStart() const { return PhysicalPaddingToLogical().Start(); }
  LayoutUnit PaddingEnd() const { return PhysicalPaddingToLogical().End(); }
  LayoutUnit PaddingOver() const { return PhysicalPaddingToLogical().Over(); }
  LayoutUnit PaddingUnder() const { return PhysicalPaddingToLogical().Under(); }

  virtual LayoutUnit BorderTop() const {
    return LayoutUnit(StyleRef().BorderTopWidth());
  }
  virtual LayoutUnit BorderBottom() const {
    return LayoutUnit(StyleRef().BorderBottomWidth());
  }
  virtual LayoutUnit BorderLeft() const {
    return LayoutUnit(StyleRef().BorderLeftWidth());
  }
  virtual LayoutUnit BorderRight() const {
    return LayoutUnit(StyleRef().BorderRightWidth());
  }

  LayoutUnit BorderBefore() const { return PhysicalBorderToLogical().Before(); }
  LayoutUnit BorderAfter() const { return PhysicalBorderToLogical().After(); }
  LayoutUnit BorderStart() const { return PhysicalBorderToLogical().Start(); }
  LayoutUnit BorderEnd() const { return PhysicalBorderToLogical().End(); }
  LayoutUnit BorderOver() const { return PhysicalBorderToLogical().Over(); }
  LayoutUnit BorderUnder() const { return PhysicalBorderToLogical().Under(); }

  LayoutUnit BorderWidth() const { return BorderLeft() + BorderRight(); }
  LayoutUnit BorderHeight() const { return BorderTop() + BorderBottom(); }

  LayoutRectOutsets BorderBoxOutsets() const {
    return LayoutRectOutsets(BorderTop(), BorderRight(), BorderBottom(),
                             BorderLeft());
  }

  LayoutRectOutsets PaddingOutsets() const {
    return LayoutRectOutsets(PaddingTop(), PaddingRight(), PaddingBottom(),
                             PaddingLeft());
  }

  // Insets from the border box to the inside of the border.
  LayoutRectOutsets BorderInsets() const {
    return LayoutRectOutsets(-BorderTop(), -BorderRight(), -BorderBottom(),
                             -BorderLeft());
  }

  LayoutUnit BorderAndPaddingStart() const {
    return BorderStart() + PaddingStart();
  }
  DISABLE_CFI_PERF LayoutUnit BorderAndPaddingBefore() const {
    return BorderBefore() + PaddingBefore();
  }
  DISABLE_CFI_PERF LayoutUnit BorderAndPaddingAfter() const {
    return BorderAfter() + PaddingAfter();
  }
  LayoutUnit BorderAndPaddingOver() const {
    return BorderOver() + PaddingOver();
  }
  LayoutUnit BorderAndPaddingUnder() const {
    return BorderUnder() + PaddingUnder();
  }

  DISABLE_CFI_PERF LayoutUnit BorderAndPaddingHeight() const {
    return BorderTop() + BorderBottom() + PaddingTop() + PaddingBottom();
  }
  DISABLE_CFI_PERF LayoutUnit BorderAndPaddingWidth() const {
    return BorderLeft() + BorderRight() + PaddingLeft() + PaddingRight();
  }
  DISABLE_CFI_PERF LayoutUnit BorderAndPaddingLogicalHeight() const {
    return (StyleRef().HasBorder() || StyleRef().MayHavePadding())
               ? BorderAndPaddingBefore() + BorderAndPaddingAfter()
               : LayoutUnit();
  }
  DISABLE_CFI_PERF LayoutUnit BorderAndPaddingLogicalWidth() const {
    return BorderStart() + BorderEnd() + PaddingStart() + PaddingEnd();
  }
  DISABLE_CFI_PERF LayoutUnit BorderAndPaddingLogicalLeft() const {
    return StyleRef().IsHorizontalWritingMode() ? BorderLeft() + PaddingLeft()
                                                : BorderTop() + PaddingTop();
  }

  LayoutUnit BorderLogicalLeft() const {
    return LayoutUnit(StyleRef().IsHorizontalWritingMode() ? BorderLeft()
                                                           : BorderTop());
  }
  LayoutUnit BorderLogicalRight() const {
    return LayoutUnit(StyleRef().IsHorizontalWritingMode() ? BorderRight()
                                                           : BorderBottom());
  }

  LayoutUnit PaddingLogicalWidth() const {
    return PaddingStart() + PaddingEnd();
  }
  LayoutUnit PaddingLogicalHeight() const {
    return PaddingBefore() + PaddingAfter();
  }

  LayoutUnit CollapsedBorderAndCSSPaddingLogicalWidth() const {
    return ComputedCSSPaddingStart() + ComputedCSSPaddingEnd() + BorderStart() +
           BorderEnd();
  }
  LayoutUnit CollapsedBorderAndCSSPaddingLogicalHeight() const {
    return ComputedCSSPaddingBefore() + ComputedCSSPaddingAfter() +
           BorderBefore() + BorderAfter();
  }

  virtual LayoutUnit MarginTop() const = 0;
  virtual LayoutUnit MarginBottom() const = 0;
  virtual LayoutUnit MarginLeft() const = 0;
  virtual LayoutUnit MarginRight() const = 0;

  LayoutUnit MarginBefore(const ComputedStyle* other_style = nullptr) const {
    return PhysicalMarginToLogical(other_style).Before();
  }
  LayoutUnit MarginAfter(const ComputedStyle* other_style = nullptr) const {
    return PhysicalMarginToLogical(other_style).After();
  }
  LayoutUnit MarginStart(const ComputedStyle* other_style = nullptr) const {
    return PhysicalMarginToLogical(other_style).Start();
  }
  LayoutUnit MarginEnd(const ComputedStyle* other_style = nullptr) const {
    return PhysicalMarginToLogical(other_style).End();
  }
  LayoutUnit MarginLineLeft() const {
    return PhysicalMarginToLogical(nullptr).LineLeft();
  }
  LayoutUnit MarginLineRight() const {
    return PhysicalMarginToLogical(nullptr).LineRight();
  }
  LayoutUnit MarginOver() const {
    return PhysicalMarginToLogical(nullptr).Over();
  }
  LayoutUnit MarginUnder() const {
    return PhysicalMarginToLogical(nullptr).Under();
  }

  DISABLE_CFI_PERF LayoutUnit MarginHeight() const {
    return MarginTop() + MarginBottom();
  }
  DISABLE_CFI_PERF LayoutUnit MarginWidth() const {
    return MarginLeft() + MarginRight();
  }
  DISABLE_CFI_PERF LayoutUnit MarginLogicalHeight() const {
    return MarginBefore() + MarginAfter();
  }
  DISABLE_CFI_PERF LayoutUnit MarginLogicalWidth() const {
    return MarginStart() + MarginEnd();
  }

  bool HasInlineDirectionBordersPaddingOrMargin() const {
    return HasInlineDirectionBordersOrPadding() || MarginStart() || MarginEnd();
  }
  bool HasInlineDirectionBordersOrPadding() const {
    return BorderStart() || BorderEnd() || PaddingStart() || PaddingEnd();
  }

  virtual LayoutUnit ContainingBlockLogicalWidthForContent() const;

  virtual void ChildBecameNonInline(LayoutObject* /*child*/) {}

  // Overridden by subclasses to determine line height and baseline position.
  virtual LayoutUnit LineHeight(
      bool first_line,
      LineDirectionMode,
      LinePositionMode = kPositionOnContainingLine) const = 0;
  virtual LayoutUnit BaselinePosition(
      FontBaseline,
      bool first_line,
      LineDirectionMode,
      LinePositionMode = kPositionOnContainingLine) const = 0;

  const LayoutObject* PushMappingToContainer(
      const LayoutBoxModelObject* ancestor_to_stop_at,
      LayoutGeometryMap&) const override;

  void ContentChanged(ContentChangeType);

  // Returns true if the background is painted opaque in the given rect.
  // The query rect is given in local coordinate system.
  virtual bool BackgroundIsKnownToBeOpaqueInRect(const PhysicalRect&) const {
    return false;
  }
  // Returns true if all text in the paint-order subtree will be painted on
  // opaque background.
  virtual bool TextIsKnownToBeOnOpaqueBackground() const { return false; }

  // This object's background is transferred to its LayoutView if:
  // 1. it's the document element, or
  // 2. it's the first <body> if the document element is <html> and doesn't have
  //    a background. http://www.w3.org/TR/css3-background/#body-background
  // If it's the case, the used background should be the initial value (i.e.
  // transparent). The first condition is actually an implementation detail
  // because we paint the view background in ViewPainter instead of the painter
  // of the layout object of the document element.
  bool BackgroundTransfersToView(
      const ComputedStyle* document_element_style = nullptr) const;

  void AbsoluteQuads(Vector<FloatQuad>& quads,
                     MapCoordinatesFlags mode = 0) const override;

  virtual LayoutUnit OverrideContainingBlockContentWidth() const {
    NOTREACHED();
    return LayoutUnit(-1);
  }
  virtual LayoutUnit OverrideContainingBlockContentHeight() const {
    NOTREACHED();
    return LayoutUnit(-1);
  }
  virtual bool HasOverrideContainingBlockContentWidth() const { return false; }
  virtual bool HasOverrideContainingBlockContentHeight() const { return false; }

  // Returns the continuation associated with |this|.
  // Returns nullptr if no continuation is associated with |this|.
  //
  // See the section about CONTINUATIONS AND ANONYMOUS LAYOUTBLOCKFLOWS in
  // LayoutInline for more details about them.
  //
  // Our implementation uses a HashMap to store them to avoid paying the cost
  // for each LayoutBoxModelObject (|continuationMap| in the cpp file).
  // public only for NGOutOfFlowLayoutPart, otherwise protected.
  LayoutBoxModelObject* Continuation() const;

 protected:
  // Compute absolute quads for |this|, but not any continuations. May only be
  // called for objects which can be or have continuations, i.e. LayoutInline or
  // LayoutBlockFlow.
  virtual void AbsoluteQuadsForSelf(Vector<FloatQuad>& quads,
                                    MapCoordinatesFlags mode = 0) const;

  void WillBeDestroyed() override;

  PhysicalOffset AdjustedPositionRelativeTo(const PhysicalOffset&,
                                            const Element*) const;

  // Set the next link in the continuation chain.
  //
  // See continuation above for more details.
  void SetContinuation(LayoutBoxModelObject*);

  virtual PhysicalOffset AccumulateRelativePositionOffsets() const {
    return PhysicalOffset();
  }

  LayoutRect LocalCaretRectForEmptyElement(LayoutUnit width,
                                           LayoutUnit text_indent_offset) const;

  enum RegisterPercentageDescendant {
    kDontRegisterPercentageDescendant,
    kRegisterPercentageDescendant,
  };
  bool HasAutoHeightOrContainingBlockWithAutoHeight(
      RegisterPercentageDescendant = kRegisterPercentageDescendant) const;
  LayoutBlock* ContainingBlockForAutoHeightDetection(
      const Length& logical_height) const;

  void AddOutlineRectsForNormalChildren(Vector<PhysicalRect>&,
                                        const PhysicalOffset& additional_offset,
                                        NGOutlineType) const;
  void AddOutlineRectsForDescendant(const LayoutObject& descendant,
                                    Vector<PhysicalRect>&,
                                    const PhysicalOffset& additional_offset,
                                    NGOutlineType) const;

  void StyleWillChange(StyleDifference,
                       const ComputedStyle& new_style) override;
  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;

  void InvalidateStickyConstraints();

 public:
  // These functions are only used internally to manipulate the layout tree
  // structure via remove/insert/appendChildNode.
  // Since they are typically called only to move objects around within
  // anonymous blocks (which only have layers in the case of column spans), the
  // default for fullRemoveInsert is false rather than true.
  void MoveChildTo(LayoutBoxModelObject* to_box_model_object,
                   LayoutObject* child,
                   LayoutObject* before_child,
                   bool full_remove_insert = false);
  void MoveChildTo(LayoutBoxModelObject* to_box_model_object,
                   LayoutObject* child,
                   bool full_remove_insert = false) {
    MoveChildTo(to_box_model_object, child, nullptr, full_remove_insert);
  }
  void MoveAllChildrenTo(LayoutBoxModelObject* to_box_model_object,
                         bool full_remove_insert = false) {
    MoveAllChildrenTo(to_box_model_object, nullptr, full_remove_insert);
  }
  void MoveAllChildrenTo(LayoutBoxModelObject* to_box_model_object,
                         LayoutObject* before_child,
                         bool full_remove_insert = false) {
    MoveChildrenTo(to_box_model_object, SlowFirstChild(), nullptr, before_child,
                   full_remove_insert);
  }
  // Move all of the kids from |startChild| up to but excluding |endChild|. 0
  // can be passed as the |endChild| to denote that all the kids from
  // |startChild| onwards should be moved.
  void MoveChildrenTo(LayoutBoxModelObject* to_box_model_object,
                      LayoutObject* start_child,
                      LayoutObject* end_child,
                      bool full_remove_insert = false) {
    MoveChildrenTo(to_box_model_object, start_child, end_child, nullptr,
                   full_remove_insert);
  }
  virtual void MoveChildrenTo(LayoutBoxModelObject* to_box_model_object,
                              LayoutObject* start_child,
                              LayoutObject* end_child,
                              LayoutObject* before_child,
                              bool full_remove_insert = false);

 private:
  void CreateLayerAfterStyleChange();

  LayoutUnit ComputedCSSPadding(const Length&) const;
  bool IsBoxModelObject() const final { return true; }

  PhysicalToLogicalGetter<LayoutUnit, LayoutBoxModelObject>
  PhysicalPaddingToLogical() const {
    return PhysicalToLogicalGetter<LayoutUnit, LayoutBoxModelObject>(
        StyleRef().GetWritingMode(), StyleRef().Direction(), *this,
        &LayoutBoxModelObject::PaddingTop, &LayoutBoxModelObject::PaddingRight,
        &LayoutBoxModelObject::PaddingBottom,
        &LayoutBoxModelObject::PaddingLeft);
  }

  PhysicalToLogicalGetter<LayoutUnit, LayoutBoxModelObject>
  PhysicalMarginToLogical(const ComputedStyle* other_style) const {
    const auto& style = other_style ? *other_style : StyleRef();
    return PhysicalToLogicalGetter<LayoutUnit, LayoutBoxModelObject>(
        style.GetWritingMode(), style.Direction(), *this,
        &LayoutBoxModelObject::MarginTop, &LayoutBoxModelObject::MarginRight,
        &LayoutBoxModelObject::MarginBottom, &LayoutBoxModelObject::MarginLeft);
  }

  PhysicalToLogicalGetter<LayoutUnit, LayoutBoxModelObject>
  PhysicalBorderToLogical() const {
    return PhysicalToLogicalGetter<LayoutUnit, LayoutBoxModelObject>(
        StyleRef().GetWritingMode(), StyleRef().Direction(), *this,
        &LayoutBoxModelObject::BorderTop, &LayoutBoxModelObject::BorderRight,
        &LayoutBoxModelObject::BorderBottom, &LayoutBoxModelObject::BorderLeft);
  }
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutBoxModelObject, IsBoxModelObject());

template <>
struct DowncastTraits<LayoutBoxModelObject> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsBoxModelObject();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_BOX_MODEL_OBJECT_H_
