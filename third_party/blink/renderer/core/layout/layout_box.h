/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2006, 2007 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_BOX_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_BOX_H_

#include <memory>

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/custom/custom_layout_child.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/min_max_size.h"
#include "third_party/blink/renderer/core/layout/overflow_model.h"
#include "third_party/blink/renderer/platform/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/wtf/compiler.h"

namespace blink {

class EventHandler;
class LayoutBlockFlow;
class LayoutMultiColumnSpannerPlaceholder;
struct NGPhysicalBoxStrut;
class ShapeOutsideInfo;

struct PaintInfo;
struct WebScrollIntoViewParams;

enum SizeType { kMainOrPreferredSize, kMinSize, kMaxSize };
enum AvailableLogicalHeightType {
  kExcludeMarginBorderPadding,
  kIncludeMarginBorderPadding
};
// When painting, overlay scrollbars do not take up space and should not affect
// clipping behavior. During hit testing, overlay scrollbars behave like regular
// scrollbars and should change how hit testing is clipped.
enum MarginDirection { kBlockDirection, kInlineDirection };
enum BackgroundRectType { kBackgroundClipRect, kBackgroundKnownOpaqueRect };

enum ShouldComputePreferred { kComputeActual, kComputePreferred };

using SnapAreaSet = HashSet<const LayoutBox*>;

struct LayoutBoxRareData {
  USING_FAST_MALLOC(LayoutBoxRareData);

 public:
  LayoutBoxRareData()
      : spanner_placeholder_(nullptr),
        override_logical_width_(-1),
        override_logical_height_(-1),
        // TODO(rego): We should store these based on physical direction.
        has_override_containing_block_content_logical_width_(false),
        has_override_containing_block_content_logical_height_(false),
        has_override_containing_block_percentage_resolution_logical_height_(
            false),
        has_previous_content_box_rect_and_layout_overflow_rect_(false),
        percent_height_container_(nullptr),
        snap_container_(nullptr),
        snap_areas_(nullptr) {}

  // For spanners, the spanner placeholder that lays us out within the multicol
  // container.
  LayoutMultiColumnSpannerPlaceholder* spanner_placeholder_;

  LayoutUnit override_logical_width_;
  LayoutUnit override_logical_height_;

  bool has_override_containing_block_content_logical_width_ : 1;
  bool has_override_containing_block_content_logical_height_ : 1;
  bool has_override_containing_block_percentage_resolution_logical_height_ : 1;
  bool has_previous_content_box_rect_and_layout_overflow_rect_ : 1;

  LayoutUnit override_containing_block_content_logical_width_;
  LayoutUnit override_containing_block_content_logical_height_;
  LayoutUnit override_containing_block_percentage_resolution_logical_height_;

  LayoutUnit offset_to_next_page_;

  LayoutUnit pagination_strut_;

  LayoutBlock* percent_height_container_;
  // For snap area, the owning snap container.
  LayoutBox* snap_container_;
  // For snap container, the descendant snap areas that contribute snap
  // points.
  std::unique_ptr<SnapAreaSet> snap_areas_;

  SnapAreaSet& EnsureSnapAreas() {
    if (!snap_areas_)
      snap_areas_ = std::make_unique<SnapAreaSet>();

    return *snap_areas_;
  }

  // Used by BoxPaintInvalidator. Stores the previous content box size and
  // layout overflow rect after the last paint invalidation. They are valid if
  // has_previous_content_box_rect_and_layout_overflow_rect_ is true.
  LayoutRect previous_physical_content_box_rect_;
  LayoutRect previous_physical_layout_overflow_rect_;

  // Used by LocalFrameView::ScrollIntoView. When the scroll is sequenced
  // rather than instantly performed, we need the pending_offset_to_scroll
  // to calculate the next rect_to_scroll as if the scroll has been performed.
  // TODO(sunyunjia): We should get rid of this variable and move the next
  // rect_to_scroll calculation into ScrollRectToVisible. crbug.com/741830
  LayoutSize pending_offset_to_scroll_;

  // Used by CSSLayoutDefinition::Instance::Layout. Represents the script
  // object for this box that web developers can query style, and perform
  // layout upon. Only created if IsCustomItem() is true.
  Persistent<CustomLayoutChild> layout_child_;

  DISALLOW_COPY_AND_ASSIGN(LayoutBoxRareData);
};

// LayoutBox implements the full CSS box model.
//
// LayoutBoxModelObject only introduces some abstractions for LayoutInline and
// LayoutBox. The logic for the model is in LayoutBox, e.g. the storage for the
// rectangle and offset forming the CSS box (m_frameRect) and the getters for
// the different boxes.
//
// LayoutBox is also the uppermost class to support scrollbars, however the
// logic is delegated to PaintLayerScrollableArea.
// Per the CSS specification, scrollbars should "be inserted between the inner
// border edge and the outer padding edge".
// (see http://www.w3.org/TR/CSS21/visufx.html#overflow)
// Also the scrollbar width / height are removed from the content box. Taking
// the following example:
//
// <!DOCTYPE html>
// <style>
// ::-webkit-scrollbar {
//     /* Force non-overlay scrollbars */
//     width: 10px;
//     height: 20px;
// }
// </style>
// <div style="overflow:scroll; width: 100px; height: 100px">
//
// The <div>'s content box is not 100x100 as specified in the style but 90x80 as
// we remove the scrollbars from the box.
//
// The presence of scrollbars is determined by the 'overflow' property and can
// be conditioned on having layout overflow (see OverflowModel for more details
// on how we track overflow).
//
// There are 2 types of scrollbars:
// - non-overlay scrollbars take space from the content box.
// - overlay scrollbars don't and just overlay hang off from the border box,
//   potentially overlapping with the padding box's content.
// For more details on scrollbars, see PaintLayerScrollableArea.
//
//
// ***** THE BOX MODEL *****
// The CSS box model is based on a series of nested boxes:
// http://www.w3.org/TR/CSS21/box.html
//
//       |----------------------------------------------------|
//       |                                                    |
//       |                   margin-top                       |
//       |                                                    |
//       |     |-----------------------------------------|    |
//       |     |                                         |    |
//       |     |             border-top                  |    |
//       |     |                                         |    |
//       |     |    |--------------------------|----|    |    |
//       |     |    |                          |    |    |    |
//       |     |    |       padding-top        |####|    |    |
//       |     |    |                          |####|    |    |
//       |     |    |    |----------------|    |####|    |    |
//       |     |    |    |                |    |    |    |    |
//       | ML  | BL | PL |  content box   | PR | SW | BR | MR |
//       |     |    |    |                |    |    |    |    |
//       |     |    |    |----------------|    |    |    |    |
//       |     |    |                          |    |    |    |
//       |     |    |      padding-bottom      |    |    |    |
//       |     |    |--------------------------|----|    |    |
//       |     |    |                      ####|    |    |    |
//       |     |    |     scrollbar height ####| SC |    |    |
//       |     |    |                      ####|    |    |    |
//       |     |    |-------------------------------|    |    |
//       |     |                                         |    |
//       |     |           border-bottom                 |    |
//       |     |                                         |    |
//       |     |-----------------------------------------|    |
//       |                                                    |
//       |                 margin-bottom                      |
//       |                                                    |
//       |----------------------------------------------------|
//
// BL = border-left
// BR = border-right
// ML = margin-left
// MR = margin-right
// PL = padding-left
// PR = padding-right
// SC = scroll corner (contains UI for resizing (see the 'resize' property)
// SW = scrollbar width
//
// Note that the vertical scrollbar (if existing) will be on the left in
// right-to-left direction and horizontal writing-mode. The horizontal scrollbar
// (if existing) is always at the bottom.
//
// Those are just the boxes from the CSS model. Extra boxes are tracked by Blink
// (e.g. the overflows). Thus it is paramount to know which box a function is
// manipulating. Also of critical importance is the coordinate system used (see
// the COORDINATE SYSTEMS section in LayoutBoxModelObject).
class CORE_EXPORT LayoutBox : public LayoutBoxModelObject {
 public:
  explicit LayoutBox(ContainerNode*);

  PaintLayerType LayerTypeRequired() const override;

  bool BackgroundIsKnownToBeOpaqueInRect(
      const LayoutRect& local_rect) const override;

  virtual bool BackgroundShouldAlwaysBeClipped() const { return false; }

  // Use this with caution! No type checking is done!
  LayoutBox* FirstChildBox() const;
  LayoutBox* FirstInFlowChildBox() const;
  LayoutBox* LastChildBox() const;

  int PixelSnappedWidth() const { return frame_rect_.PixelSnappedWidth(); }
  int PixelSnappedHeight() const { return frame_rect_.PixelSnappedHeight(); }

  void SetX(LayoutUnit x) {
    if (x == frame_rect_.X())
      return;
    frame_rect_.SetX(x);
    LocationChanged();
  }
  void SetY(LayoutUnit y) {
    if (y == frame_rect_.Y())
      return;
    frame_rect_.SetY(y);
    LocationChanged();
  }
  void SetWidth(LayoutUnit width) {
    if (width == frame_rect_.Width())
      return;
    frame_rect_.SetWidth(width);
    SizeChanged();
  }
  void SetHeight(LayoutUnit height) {
    if (height == frame_rect_.Height())
      return;
    frame_rect_.SetHeight(height);
    SizeChanged();
  }

  LayoutUnit LogicalLeft() const {
    return StyleRef().IsHorizontalWritingMode() ? frame_rect_.X()
                                                : frame_rect_.Y();
  }
  LayoutUnit LogicalRight() const { return LogicalLeft() + LogicalWidth(); }
  LayoutUnit LogicalTop() const {
    return StyleRef().IsHorizontalWritingMode() ? frame_rect_.Y()
                                                : frame_rect_.X();
  }
  LayoutUnit LogicalBottom() const { return LogicalTop() + LogicalHeight(); }
  LayoutUnit LogicalWidth() const {
    return StyleRef().IsHorizontalWritingMode() ? frame_rect_.Width()
                                                : frame_rect_.Height();
  }
  LayoutUnit LogicalHeight() const {
    return StyleRef().IsHorizontalWritingMode() ? frame_rect_.Height()
                                                : frame_rect_.Width();
  }

  // Logical height of the object, including content overflowing the
  // border-after edge.
  virtual LayoutUnit LogicalHeightWithVisibleOverflow() const;

  LayoutUnit ConstrainLogicalWidthByMinMax(LayoutUnit,
                                           LayoutUnit,
                                           LayoutBlock*) const;
  LayoutUnit ConstrainLogicalHeightByMinMax(
      LayoutUnit logical_height,
      LayoutUnit intrinsic_content_height) const;
  LayoutUnit ConstrainContentBoxLogicalHeightByMinMax(
      LayoutUnit logical_height,
      LayoutUnit intrinsic_content_height) const;

  int PixelSnappedLogicalHeight() const {
    return StyleRef().IsHorizontalWritingMode() ? PixelSnappedHeight()
                                                : PixelSnappedWidth();
  }
  int PixelSnappedLogicalWidth() const {
    return StyleRef().IsHorizontalWritingMode() ? PixelSnappedWidth()
                                                : PixelSnappedHeight();
  }

  LayoutUnit MinimumLogicalHeightForEmptyLine() const {
    return BorderAndPaddingLogicalHeight() + ScrollbarLogicalHeight() +
           LineHeight(
               true,
               IsHorizontalWritingMode() ? kHorizontalLine : kVerticalLine,
               kPositionOfInteriorLineBoxes);
  }

  void SetLogicalLeft(LayoutUnit left) {
    if (StyleRef().IsHorizontalWritingMode())
      SetX(left);
    else
      SetY(left);
  }
  void SetLogicalTop(LayoutUnit top) {
    if (StyleRef().IsHorizontalWritingMode())
      SetY(top);
    else
      SetX(top);
  }
  void SetLogicalLocation(const LayoutPoint& location) {
    if (StyleRef().IsHorizontalWritingMode())
      SetLocation(location);
    else
      SetLocation(location.TransposedPoint());
  }
  void SetLogicalWidth(LayoutUnit size) {
    if (StyleRef().IsHorizontalWritingMode())
      SetWidth(size);
    else
      SetHeight(size);
  }
  void SetLogicalHeight(LayoutUnit size) {
    if (StyleRef().IsHorizontalWritingMode())
      SetHeight(size);
    else
      SetWidth(size);
  }

  // See frame_rect_.
  LayoutPoint Location() const { return frame_rect_.Location(); }
  LayoutSize LocationOffset() const {
    return LayoutSize(frame_rect_.X(), frame_rect_.Y());
  }
  LayoutSize Size() const { return frame_rect_.Size(); }
  IntSize PixelSnappedSize() const { return frame_rect_.PixelSnappedSize(); }

  void SetLocation(const LayoutPoint& location) {
    if (location == frame_rect_.Location())
      return;
    frame_rect_.SetLocation(location);
    LocationChanged();
  }

  // The ancestor box that this object's location and physicalLocation are
  // relative to.
  virtual LayoutBox* LocationContainer() const;

  // FIXME: Currently scrollbars are using int geometry and positioned based on
  // pixelSnappedBorderBoxRect whose size may change when location changes
  // because of pixel snapping. This function is used to change location of the
  // LayoutBox outside of LayoutBox::layout(). Will remove when we use
  // LayoutUnits for scrollbars.
  void SetLocationAndUpdateOverflowControlsIfNeeded(const LayoutPoint&);

  void SetSize(const LayoutSize& size) {
    if (size == frame_rect_.Size())
      return;
    frame_rect_.SetSize(size);
    SizeChanged();
  }
  void Move(LayoutUnit dx, LayoutUnit dy) {
    if (!dx && !dy)
      return;
    frame_rect_.Move(dx, dy);
    LocationChanged();
  }

  // See frame_rect_.
  LayoutRect FrameRect() const { return frame_rect_; }
  void SetFrameRect(const LayoutRect& rect) {
    SetLocation(rect.Location());
    SetSize(rect.Size());
  }

  // Note that those functions have their origin at this box's CSS border box.
  // As such their location doesn't account for 'top'/'left'. About its
  // coordinate space, it can be treated as in either physical coordinates
  // or "physical coordinates in flipped block-flow direction", and
  // FlipForWritingMode() will do nothing on it.
  LayoutRect BorderBoxRect() const { return LayoutRect(LayoutPoint(), Size()); }

  // Client rect and padding box rect are the same concept.
  // TODO(crbug.com/877518): Some callers of this method may actually want
  // "physical coordinates in flipped block-flow direction".
  DISABLE_CFI_PERF LayoutRect PhysicalPaddingBoxRect() const {
    return LayoutRect(ClientLeft(), ClientTop(), ClientWidth(), ClientHeight());
  }

  IntRect PixelSnappedBorderBoxRect(
      const LayoutSize& offset = LayoutSize()) const {
    return IntRect(IntPoint(),
                   PixelSnappedIntSize(frame_rect_.Size(),
                                       frame_rect_.Location() + offset));
  }
  IntRect BorderBoundingBox() const final {
    return PixelSnappedBorderBoxRect();
  }

  // The content area of the box (excludes padding - and intrinsic padding for
  // table cells, etc... - and scrollbars and border).
  // TODO(crbug.com/877518): Some callers of this method may actually want
  // "physical coordinates in flipped block-flow direction".
  DISABLE_CFI_PERF LayoutRect PhysicalContentBoxRect() const {
    return LayoutRect(ContentLeft(), ContentTop(), ContentWidth(),
                      ContentHeight());
  }
  // TODO(crbug.com/877518): Some callers of this method may actually want
  // "physical coordinates in flipped block-flow direction".
  LayoutSize PhysicalContentBoxOffset() const {
    return LayoutSize(ContentLeft(), ContentTop());
  }
  // The content box converted to absolute coords (taking transforms into
  // account).
  FloatQuad AbsoluteContentQuad(MapCoordinatesFlags = 0) const;

  // The enclosing rectangle of the background with given opacity requirement.
  // TODO(crbug.com/877518): Some callers of this method may actually want
  // "physical coordinates in flipped block-flow direction".
  LayoutRect PhysicalBackgroundRect(BackgroundRectType) const;

  // This returns the content area of the box (excluding padding and border).
  // The only difference with contentBoxRect is that computedCSSContentBoxRect
  // does include the intrinsic padding in the content box as this is what some
  // callers expect (like getComputedStyle).
  LayoutRect ComputedCSSContentBoxRect() const {
    return LayoutRect(
        BorderLeft() + ComputedCSSPaddingLeft(),
        BorderTop() + ComputedCSSPaddingTop(),
        ClientWidth() - ComputedCSSPaddingLeft() - ComputedCSSPaddingRight(),
        ClientHeight() - ComputedCSSPaddingTop() - ComputedCSSPaddingBottom());
  }

  void AddOutlineRects(Vector<LayoutRect>&,
                       const LayoutPoint& additional_offset,
                       NGOutlineType) const override;

  // Use this with caution! No type checking is done!
  LayoutBox* PreviousSiblingBox() const;
  LayoutBox* PreviousInFlowSiblingBox() const;
  LayoutBox* NextSiblingBox() const;
  LayoutBox* NextInFlowSiblingBox() const;
  LayoutBox* ParentBox() const;

  // Return the previous sibling column set or spanner placeholder. Only to be
  // used on multicol container children.
  LayoutBox* PreviousSiblingMultiColumnBox() const;
  // Return the next sibling column set or spanner placeholder. Only to be used
  // on multicol container children.
  LayoutBox* NextSiblingMultiColumnBox() const;

  bool CanResize() const;

  // Like most of the other box geometries, visual and layout overflow are also
  // in the "physical coordinates in flipped block-flow direction" of the box.
  LayoutRect NoOverflowRect() const;
  LayoutRect LayoutOverflowRect() const {
    return overflow_ ? overflow_->LayoutOverflowRect() : NoOverflowRect();
  }
  LayoutRect PhysicalLayoutOverflowRect() const {
    LayoutRect overflow_rect = LayoutOverflowRect();
    FlipForWritingMode(overflow_rect);
    return overflow_rect;
  }
  IntRect PixelSnappedLayoutOverflowRect() const {
    return PixelSnappedIntRect(LayoutOverflowRect());
  }
  LayoutSize MaxLayoutOverflow() const {
    return LayoutSize(LayoutOverflowRect().MaxX(), LayoutOverflowRect().MaxY());
  }
  LayoutUnit LogicalLeftLayoutOverflow() const {
    return StyleRef().IsHorizontalWritingMode() ? LayoutOverflowRect().X()
                                                : LayoutOverflowRect().Y();
  }
  LayoutUnit LogicalRightLayoutOverflow() const {
    return StyleRef().IsHorizontalWritingMode() ? LayoutOverflowRect().MaxX()
                                                : LayoutOverflowRect().MaxY();
  }

  LayoutRect VisualOverflowRect() const override;
  LayoutRect PhysicalVisualOverflowRect() const {
    LayoutRect overflow_rect = VisualOverflowRect();
    FlipForWritingMode(overflow_rect);
    return overflow_rect;
  }
  LayoutUnit LogicalLeftVisualOverflow() const {
    return StyleRef().IsHorizontalWritingMode() ? VisualOverflowRect().X()
                                                : VisualOverflowRect().Y();
  }
  LayoutUnit LogicalRightVisualOverflow() const {
    return StyleRef().IsHorizontalWritingMode() ? VisualOverflowRect().MaxX()
                                                : VisualOverflowRect().MaxY();
  }

  LayoutRect SelfVisualOverflowRect() const {
    return overflow_ ? overflow_->SelfVisualOverflowRect() : BorderBoxRect();
  }
  LayoutRect ContentsVisualOverflowRect() const {
    return overflow_ ? overflow_->ContentsVisualOverflowRect() : LayoutRect();
  }

  // These methods don't mean the box *actually* has top/left overflow. They
  // mean that *if* the box overflows, it will overflow to the top/left rather
  // than the bottom/right. This happens when child content is laid out
  // right-to-left (e.g. direction:rtl) or or bottom-to-top (e.g. direction:rtl
  // writing-mode:vertical-rl).
  virtual bool HasTopOverflow() const;
  virtual bool HasLeftOverflow() const;

  void AddLayoutOverflow(const LayoutRect&);
  void AddSelfVisualOverflow(const LayoutRect&);
  void AddContentsVisualOverflow(const LayoutRect&);

  void AddVisualEffectOverflow();
  LayoutRectOutsets ComputeVisualEffectOverflowOutsets();
  void AddVisualOverflowFromChild(const LayoutBox& child) {
    AddVisualOverflowFromChild(child, child.LocationOffset());
  }
  void AddLayoutOverflowFromChild(const LayoutBox& child) {
    AddLayoutOverflowFromChild(child, child.LocationOffset());
  }
  void AddVisualOverflowFromChild(const LayoutBox& child,
                                  const LayoutSize& delta);
  void AddLayoutOverflowFromChild(const LayoutBox& child,
                                  const LayoutSize& delta);
  void ClearLayoutOverflow();
  void ClearAllOverflows() { overflow_.reset(); }

  virtual void UpdateAfterLayout();

  DISABLE_CFI_PERF LayoutUnit ContentLeft() const {
    return ClientLeft() + PaddingLeft();
  }
  DISABLE_CFI_PERF LayoutUnit ContentTop() const {
    return ClientTop() + PaddingTop();
  }
  DISABLE_CFI_PERF LayoutUnit ContentWidth() const {
    // We're dealing with LayoutUnit and saturated arithmetic here, so we need
    // to guard against negative results. The value returned from clientWidth()
    // may in itself be a victim of saturated arithmetic; e.g. if both border
    // sides were sufficiently wide (close to LayoutUnit::max()).  Here we
    // subtract two padding values from that result, which is another source of
    // saturated arithmetic.
    return (ClientWidth() - PaddingLeft() - PaddingRight())
        .ClampNegativeToZero();
  }
  DISABLE_CFI_PERF LayoutUnit ContentHeight() const {
    // We're dealing with LayoutUnit and saturated arithmetic here, so we need
    // to guard against negative results. The value returned from clientHeight()
    // may in itself be a victim of saturated arithmetic; e.g. if both border
    // sides were sufficiently wide (close to LayoutUnit::max()).  Here we
    // subtract two padding values from that result, which is another source of
    // saturated arithmetic.
    return (ClientHeight() - PaddingTop() - PaddingBottom())
        .ClampNegativeToZero();
  }
  LayoutSize ContentSize() const {
    return LayoutSize(ContentWidth(), ContentHeight());
  }
  LayoutUnit ContentLogicalWidth() const {
    return StyleRef().IsHorizontalWritingMode() ? ContentWidth()
                                                : ContentHeight();
  }
  LayoutUnit ContentLogicalHeight() const {
    return StyleRef().IsHorizontalWritingMode() ? ContentHeight()
                                                : ContentWidth();
  }

  // IE extensions. Used to calculate offsetWidth/Height. Overridden by inlines
  // (LayoutFlow) to return the remaining width on a given line (and the height
  // of a single line).
  LayoutUnit OffsetWidth() const final { return frame_rect_.Width(); }
  LayoutUnit OffsetHeight() const final { return frame_rect_.Height(); }

  int PixelSnappedOffsetWidth(const Element*) const final;
  int PixelSnappedOffsetHeight(const Element*) const final;

  DISABLE_CFI_PERF LayoutUnit LeftScrollbarWidth() const {
    return ShouldPlaceVerticalScrollbarOnLeft()
               // See the function for the reason of using it here.
               ? VerticalScrollbarWidthClampedToContentBox()
               : LayoutUnit();
  }
  DISABLE_CFI_PERF LayoutUnit RightScrollbarWidth() const {
    return ShouldPlaceVerticalScrollbarOnLeft()
               ? LayoutUnit()
               // See VerticalScrollbarWidthClampedToContentBox for the reason
               // of not using it here.
               : LayoutUnit(VerticalScrollbarWidth());
  }
  // The horizontal scrollbar is always at the bottom.
  DISABLE_CFI_PERF LayoutUnit BottomScrollbarHeight() const {
    return LayoutUnit(HorizontalScrollbarHeight());
  }

  // This could be
  //   IsHorizontalWritingMode() ? LeftScrollbarWidth() : TopScrollbarWidth(),
  // but LeftScrollbarWidth() is non-zero only in horizontal rtl mode, and we
  // never have scrollbar on the top, so it's just LeftScrollbarWidth().
  DISABLE_CFI_PERF LayoutUnit LogicalLeftScrollbarWidth() const {
    return LeftScrollbarWidth();
  }
  DISABLE_CFI_PERF LayoutUnit LogicalTopScrollbarHeight() const {
    return UNLIKELY(HasFlippedBlocksWritingMode()) ? RightScrollbarWidth()
                                                   : LayoutUnit();
  }

  // Physical client rect (a.k.a. PhysicalPaddingBoxRect(), defined by
  // ClientLeft, ClientTop, ClientWidth and ClientHeight) represents the
  // interior of an object excluding borders and scrollbars.
  DISABLE_CFI_PERF LayoutUnit ClientLeft() const {
    return BorderLeft() + LeftScrollbarWidth();
  }
  DISABLE_CFI_PERF LayoutUnit ClientTop() const { return BorderTop(); }
  LayoutUnit ClientWidth() const;
  LayoutUnit ClientHeight() const;
  DISABLE_CFI_PERF LayoutUnit ClientLogicalWidth() const {
    return IsHorizontalWritingMode() ? ClientWidth() : ClientHeight();
  }
  DISABLE_CFI_PERF LayoutUnit ClientLogicalHeight() const {
    return IsHorizontalWritingMode() ? ClientHeight() : ClientWidth();
  }
  DISABLE_CFI_PERF LayoutUnit ClientLogicalBottom() const {
    return BorderBefore() + LogicalTopScrollbarHeight() + ClientLogicalHeight();
  }

  int PixelSnappedClientWidth() const;
  int PixelSnappedClientHeight() const;

  // scrollWidth/scrollHeight will be the same as clientWidth/clientHeight
  // unless the object has overflow:hidden/scroll/auto specified and also has
  // overflow. scrollLeft/Top return the current scroll position. These methods
  // are virtual so that objects like textareas can scroll shadow content (but
  // pretend that they are the objects that are scrolling).
  virtual LayoutUnit ScrollLeft() const;
  virtual LayoutUnit ScrollTop() const;
  virtual LayoutUnit ScrollWidth() const;
  virtual LayoutUnit ScrollHeight() const;
  int PixelSnappedScrollWidth() const;
  int PixelSnappedScrollHeight() const;
  virtual void SetScrollLeft(LayoutUnit);
  virtual void SetScrollTop(LayoutUnit);

  void ScrollToPosition(const FloatPoint&,
                        ScrollBehavior = kScrollBehaviorInstant);
  void ScrollByRecursively(const ScrollOffset& delta);
  // If makeVisibleInVisualViewport is set, the visual viewport will be scrolled
  // if required to make the rect visible.
  LayoutRect ScrollRectToVisibleRecursive(const LayoutRect&,
                                          const WebScrollIntoViewParams&);

  LayoutRectOutsets MarginBoxOutsets() const { return margin_box_outsets_; }
  LayoutUnit MarginTop() const override { return margin_box_outsets_.Top(); }
  LayoutUnit MarginBottom() const override {
    return margin_box_outsets_.Bottom();
  }
  LayoutUnit MarginLeft() const override { return margin_box_outsets_.Left(); }
  LayoutUnit MarginRight() const override {
    return margin_box_outsets_.Right();
  }
  void SetMargin(const NGPhysicalBoxStrut&);
  void SetMarginTop(LayoutUnit margin) { margin_box_outsets_.SetTop(margin); }
  void SetMarginBottom(LayoutUnit margin) {
    margin_box_outsets_.SetBottom(margin);
  }
  void SetMarginLeft(LayoutUnit margin) { margin_box_outsets_.SetLeft(margin); }
  void SetMarginRight(LayoutUnit margin) {
    margin_box_outsets_.SetRight(margin);
  }

  void SetMarginBefore(LayoutUnit value,
                       const ComputedStyle* override_style = nullptr) {
    LogicalMarginToPhysicalSetter(override_style).SetBefore(value);
  }
  void SetMarginAfter(LayoutUnit value,
                      const ComputedStyle* override_style = nullptr) {
    LogicalMarginToPhysicalSetter(override_style).SetAfter(value);
  }
  void SetMarginStart(LayoutUnit value,
                      const ComputedStyle* override_style = nullptr) {
    LogicalMarginToPhysicalSetter(override_style).SetStart(value);
  }
  void SetMarginEnd(LayoutUnit value,
                    const ComputedStyle* override_style = nullptr) {
    LogicalMarginToPhysicalSetter(override_style).SetEnd(value);
  }

  // The following functions are used to implement collapsing margins.
  // All objects know their maximal positive and negative margins. The formula
  // for computing a collapsed margin is |maxPosMargin| - |maxNegmargin|.
  // For a non-collapsing box, such as a leaf element, this formula will simply
  // return the margin of the element.  Blocks override the maxMarginBefore and
  // maxMarginAfter methods.
  virtual bool IsSelfCollapsingBlock() const { return false; }
  virtual LayoutUnit CollapsedMarginBefore() const { return MarginBefore(); }
  virtual LayoutUnit CollapsedMarginAfter() const { return MarginAfter(); }
  LayoutRectOutsets CollapsedMarginBoxLogicalOutsets() const {
    return LayoutRectOutsets(CollapsedMarginBefore(), LayoutUnit(),
                             CollapsedMarginAfter(), LayoutUnit());
  }

  void AbsoluteRects(Vector<IntRect>&,
                     const LayoutPoint& accumulated_offset) const override;
  void AbsoluteQuads(Vector<FloatQuad>&,
                     MapCoordinatesFlags mode = 0) const override;
  FloatRect LocalBoundingBoxRectForAccessibility() const final;

  void UpdateLayout() override;
  void Paint(const PaintInfo&) const override;

  virtual bool IsInSelfHitTestingPhase(HitTestAction hit_test_action) const {
    return hit_test_action == kHitTestForeground;
  }

  bool HitTestAllPhases(HitTestResult&,
                        const HitTestLocation& location_in_container,
                        const LayoutPoint& accumulated_offset,
                        HitTestFilter = kHitTestAll) final;
  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation& location_in_container,
                   const LayoutPoint& accumulated_offset,
                   HitTestAction) override;

  LayoutUnit MinPreferredLogicalWidth() const override;
  LayoutUnit MaxPreferredLogicalWidth() const override;

  LayoutUnit OverrideLogicalHeight() const;
  LayoutUnit OverrideLogicalWidth() const;
  bool HasOverrideLogicalHeight() const;
  bool HasOverrideLogicalWidth() const;
  void SetOverrideLogicalHeight(LayoutUnit);
  void SetOverrideLogicalWidth(LayoutUnit);
  void ClearOverrideLogicalHeight();
  void ClearOverrideLogicalWidth();
  void ClearOverrideSize();

  LayoutUnit OverrideContentLogicalWidth() const;
  LayoutUnit OverrideContentLogicalHeight() const;

  LayoutUnit OverrideContainingBlockContentWidth() const override;
  LayoutUnit OverrideContainingBlockContentHeight() const override;
  bool HasOverrideContainingBlockContentWidth() const override;
  bool HasOverrideContainingBlockContentHeight() const override;
  LayoutUnit OverrideContainingBlockContentLogicalWidth() const;
  LayoutUnit OverrideContainingBlockContentLogicalHeight() const;
  bool HasOverrideContainingBlockContentLogicalWidth() const;
  bool HasOverrideContainingBlockContentLogicalHeight() const;
  void SetOverrideContainingBlockContentLogicalWidth(LayoutUnit);
  void SetOverrideContainingBlockContentLogicalHeight(LayoutUnit);
  void ClearOverrideContainingBlockContentSize();

  LayoutUnit OverrideContainingBlockPercentageResolutionLogicalHeight() const;
  bool HasOverrideContainingBlockPercentageResolutionLogicalHeight() const;
  void SetOverrideContainingBlockPercentageResolutionLogicalHeight(LayoutUnit);
  void ClearOverrideContainingBlockPercentageResolutionLogicalHeight();

  LayoutUnit AdjustBorderBoxLogicalWidthForBoxSizing(float width) const;
  LayoutUnit AdjustBorderBoxLogicalHeightForBoxSizing(float height) const;
  LayoutUnit AdjustContentBoxLogicalWidthForBoxSizing(float width) const;
  LayoutUnit AdjustContentBoxLogicalHeightForBoxSizing(float height) const;

  // ComputedMarginValues holds the actual values for margins. It ignores
  // margin collapsing as they are handled in LayoutBlockFlow.
  // The margins are stored in logical coordinates (see COORDINATE
  // SYSTEMS in LayoutBoxModel) for use during layout.
  struct ComputedMarginValues {
    DISALLOW_NEW();
    ComputedMarginValues() = default;

    LayoutUnit before_;
    LayoutUnit after_;
    LayoutUnit start_;
    LayoutUnit end_;
  };

  // LogicalExtentComputedValues is used both for the
  // block-flow and inline-direction axis.
  struct LogicalExtentComputedValues {
    STACK_ALLOCATED();

   public:
    LogicalExtentComputedValues() = default;

    // This is the dimension in the measured direction
    // (logical height or logical width).
    LayoutUnit extent_;

    // This is the offset in the measured direction
    // (logical top or logical left).
    LayoutUnit position_;

    // |m_margins| represents the margins in the measured direction.
    // Note that ComputedMarginValues has also the margins in
    // the orthogonal direction to have clearer names but they are
    // ignored in the code.
    ComputedMarginValues margins_;
  };

  // Resolve auto margins in the chosen direction of the containing block so
  // that objects can be pushed to the start, middle or end of the containing
  // block.
  void ComputeMarginsForDirection(MarginDirection for_direction,
                                  const LayoutBlock* containing_block,
                                  LayoutUnit container_width,
                                  LayoutUnit child_width,
                                  LayoutUnit& margin_start,
                                  LayoutUnit& margin_end,
                                  Length margin_start_length,
                                  Length margin_start_end) const;

  // Used to resolve margins in the containing block's block-flow direction.
  void ComputeAndSetBlockDirectionMargins(const LayoutBlock* containing_block);

  LayoutUnit OffsetFromLogicalTopOfFirstPage() const;

  // The block offset from the logical top of this object to the end of the
  // first fragmentainer it lives in. If it only lives in one fragmentainer, 0
  // is returned.
  LayoutUnit OffsetToNextPage() const {
    return rare_data_ ? rare_data_->offset_to_next_page_ : LayoutUnit();
  }
  void SetOffsetToNextPage(LayoutUnit);

  LayoutSize PendingOffsetToScroll() const {
    return rare_data_ ? rare_data_->pending_offset_to_scroll_ : LayoutSize();
  }
  void SetPendingOffsetToScroll(LayoutSize);

  // Specify which page or column to associate with an offset, if said offset is
  // exactly at a page or column boundary.
  enum PageBoundaryRule { kAssociateWithFormerPage, kAssociateWithLatterPage };
  LayoutUnit PageLogicalHeightForOffset(LayoutUnit) const;
  bool IsPageLogicalHeightKnown() const;
  LayoutUnit PageRemainingLogicalHeightForOffset(LayoutUnit,
                                                 PageBoundaryRule) const;

  bool CrossesPageBoundary(LayoutUnit offset, LayoutUnit logical_height) const;

  // Calculate the strut to insert in order fit content of size
  // |content_logical_height|. Usually this will merely return the distance to
  // the next fragmentainer. However, in cases where the next fragmentainer
  // isn't tall enough to fit the content, and there's a likelihood of taller
  // fragmentainers further ahead, we'll search for one and return the distance
  // to the first fragmentainer that can fit this piece of content.
  LayoutUnit CalculatePaginationStrutToFitContent(
      LayoutUnit offset,
      LayoutUnit content_logical_height) const;

  void PositionLineBox(InlineBox*);
  void MoveWithEdgeOfInlineContainerIfNecessary(bool is_horizontal);

  virtual InlineBox* CreateInlineBox();
  void DirtyLineBoxes(bool full_layout);

  // For atomic inline elements, this function returns the inline box that
  // contains us. Enables the atomic inline LayoutObject to quickly determine
  // what line it is contained on and to easily iterate over structures on the
  // line.
  //
  // InlineBoxWrapper() and FirstInlineFragment() are mutually exclusive,
  // depends on IsInLayoutNGInlineFormattingContext().
  InlineBox* InlineBoxWrapper() const;
  void SetInlineBoxWrapper(InlineBox*);
  void DeleteLineBoxWrapper();

  NGPaintFragment* FirstInlineFragment() const final;
  void SetFirstInlineFragment(NGPaintFragment*) final;

  void SetSpannerPlaceholder(LayoutMultiColumnSpannerPlaceholder&);
  void ClearSpannerPlaceholder();
  LayoutMultiColumnSpannerPlaceholder* SpannerPlaceholder() const final {
    return rare_data_ ? rare_data_->spanner_placeholder_ : nullptr;
  }

  // A pagination strut is the amount of space needed to push an in-flow block-
  // level object (or float) to the logical top of the next page or column. It
  // will be set both for forced breaks (e.g. page-break-before:always) and soft
  // breaks (when there's not enough space in the current page / column for the
  // object). The strut is baked into the logicalTop() of the object, so that
  // logicalTop() - paginationStrut() == the original position in the previous
  // column before deciding to break.
  //
  // Pagination struts are either set in front of a block-level box (here) or
  // before a line (RootInlineBox::paginationStrut()).
  LayoutUnit PaginationStrut() const {
    return rare_data_ ? rare_data_->pagination_strut_ : LayoutUnit();
  }
  void SetPaginationStrut(LayoutUnit);
  void ResetPaginationStrut() {
    if (rare_data_)
      rare_data_->pagination_strut_ = LayoutUnit();
  }

  // Is the specified break-before or break-after value supported on this
  // object? It needs to be in-flow all the way up to a fragmentation context
  // that supports the specified value.
  bool IsBreakBetweenControllable(EBreakBetween) const;

  // Is the specified break-inside value supported on this object? It needs to
  // be contained by a fragmentation context that supports the specified value.
  bool IsBreakInsideControllable(EBreakInside) const;

  virtual EBreakBetween BreakAfter() const;
  virtual EBreakBetween BreakBefore() const;
  EBreakInside BreakInside() const;

  static bool IsForcedFragmentainerBreakValue(EBreakBetween);

  EBreakBetween ClassABreakPointValue(
      EBreakBetween previous_break_after_value) const;

  // Return true if we should insert a break in front of this box. The box needs
  // to start at a valid class A break point in order to allow a forced break.
  // To determine whether or not to break, we also need to know the break-after
  // value of the previous in-flow sibling.
  bool NeedsForcedBreakBefore(EBreakBetween previous_break_after_value) const;

  bool PaintedOutputOfObjectHasNoEffectRegardlessOfSize() const override;
  bool MapToVisualRectInAncestorSpaceInternal(
      const LayoutBoxModelObject* ancestor,
      TransformState&,
      VisualRectFlags = kDefaultVisualRectFlags) const override;

  LayoutUnit ContainingBlockLogicalHeightForGetComputedStyle() const;

  LayoutUnit ContainingBlockLogicalWidthForContent() const override;
  LayoutUnit ContainingBlockLogicalHeightForContent(
      AvailableLogicalHeightType) const;

  LayoutUnit ContainingBlockAvailableLineWidth() const;
  LayoutUnit PerpendicularContainingBlockLogicalHeight() const;

  virtual void UpdateLogicalWidth();
  void UpdateLogicalHeight();
  void ComputeLogicalHeight(LogicalExtentComputedValues&) const;
  virtual void ComputeLogicalHeight(LayoutUnit logical_height,
                                    LayoutUnit logical_top,
                                    LogicalExtentComputedValues&) const;
  // This function will compute the logical border-box height, without laying
  // out the box. This means that the result is only "correct" when the height
  // is explicitly specified. This function exists so that intrinsic width
  // calculations have a way to deal with children that have orthogonal flows.
  // When there is no explicit height, this function assumes a content height of
  // zero (and returns just border+padding).
  LayoutUnit ComputeLogicalHeightWithoutLayout() const;

  void ComputeLogicalWidth(LogicalExtentComputedValues&) const;

  bool StretchesToViewport() const {
    return GetDocument().InQuirksMode() && StretchesToViewportInQuirksMode();
  }

  virtual LayoutSize IntrinsicSize() const { return LayoutSize(); }
  LayoutUnit IntrinsicLogicalWidth() const {
    return StyleRef().IsHorizontalWritingMode() ? IntrinsicSize().Width()
                                                : IntrinsicSize().Height();
  }
  LayoutUnit IntrinsicLogicalHeight() const {
    return StyleRef().IsHorizontalWritingMode() ? IntrinsicSize().Height()
                                                : IntrinsicSize().Width();
  }
  virtual LayoutUnit IntrinsicContentLogicalHeight() const {
    return intrinsic_content_logical_height_;
  }

  // Whether or not the element shrinks to its intrinsic width (rather than
  // filling the width of a containing block). HTML4 buttons, <select>s,
  // <input>s, legends, and floating/compact elements do this.
  bool SizesLogicalWidthToFitContent(const Length& logical_width) const;

  LayoutUnit ShrinkLogicalWidthToAvoidFloats(LayoutUnit child_margin_start,
                                             LayoutUnit child_margin_end,
                                             const LayoutBlockFlow* cb) const;

  LayoutUnit ComputeLogicalWidthUsing(
      SizeType,
      const Length& logical_width,
      LayoutUnit available_logical_width,
      const LayoutBlock* containing_block) const;
  LayoutUnit ComputeLogicalHeightUsing(
      SizeType,
      const Length& height,
      LayoutUnit intrinsic_content_height) const;
  LayoutUnit ComputeContentLogicalHeight(
      SizeType,
      const Length& height,
      LayoutUnit intrinsic_content_height) const;
  LayoutUnit ComputeContentAndScrollbarLogicalHeightUsing(
      SizeType,
      const Length& height,
      LayoutUnit intrinsic_content_height) const;
  LayoutUnit ComputeReplacedLogicalWidthUsing(SizeType,
                                              const Length& width) const;
  LayoutUnit ComputeReplacedLogicalWidthRespectingMinMaxWidth(
      LayoutUnit logical_width,
      ShouldComputePreferred = kComputeActual) const;
  LayoutUnit ComputeReplacedLogicalHeightUsing(SizeType,
                                               const Length& height) const;
  LayoutUnit ComputeReplacedLogicalHeightRespectingMinMaxHeight(
      LayoutUnit logical_height) const;

  virtual LayoutUnit ComputeReplacedLogicalWidth(
      ShouldComputePreferred = kComputeActual) const;
  virtual LayoutUnit ComputeReplacedLogicalHeight(
      LayoutUnit estimated_used_width = LayoutUnit()) const;

  virtual bool ShouldComputeSizeAsReplaced() const {
    return IsAtomicInlineLevel() && !IsInlineBlockOrInlineTable();
  }

  // Returns the size that percentage logical heights of this box should be
  // resolved against. This function will walk the ancestor chain of this
  // object to determine this size.
  //  - out_cb returns the LayoutBlock which provided the size.
  //  - out_skipped_auto_height_containing_block returns if any auto height
  //    blocks were skipped to obtain out_cb.
  LayoutUnit ContainingBlockLogicalHeightForPercentageResolution(
      LayoutBlock** out_cb = nullptr,
      bool* out_skipped_auto_height_containing_block = nullptr) const;

  bool PercentageLogicalHeightIsResolvable() const;
  LayoutUnit ComputePercentageLogicalHeight(const Length& height) const;

  // Block flows subclass availableWidth/Height to handle multi column layout
  // (shrinking the width/height available to children when laying out.)
  LayoutUnit AvailableLogicalWidth() const { return ContentLogicalWidth(); }
  LayoutUnit AvailableLogicalHeight(AvailableLogicalHeightType) const;
  LayoutUnit AvailableLogicalHeightUsing(const Length&,
                                         AvailableLogicalHeightType) const;

  // There are a few cases where we need to refer specifically to the available
  // physical width and available physical height. Relative positioning is one
  // of those cases, since left/top offsets are physical.
  LayoutUnit AvailableWidth() const {
    return StyleRef().IsHorizontalWritingMode()
               ? AvailableLogicalWidth()
               : AvailableLogicalHeight(kIncludeMarginBorderPadding);
  }
  LayoutUnit AvailableHeight() const {
    return StyleRef().IsHorizontalWritingMode()
               ? AvailableLogicalHeight(kIncludeMarginBorderPadding)
               : AvailableLogicalWidth();
  }

  int VerticalScrollbarWidth() const;
  int HorizontalScrollbarHeight() const;
  int ScrollbarLogicalWidth() const {
    return StyleRef().IsHorizontalWritingMode() ? VerticalScrollbarWidth()
                                                : HorizontalScrollbarHeight();
  }
  int ScrollbarLogicalHeight() const {
    return StyleRef().IsHorizontalWritingMode() ? HorizontalScrollbarHeight()
                                                : VerticalScrollbarWidth();
  }

  bool CanBeScrolledAndHasScrollableArea() const;
  virtual bool CanBeProgramaticallyScrolled() const;
  virtual void Autoscroll(const IntPoint&);
  bool CanAutoscroll() const;
  IntSize CalculateAutoscrollDirection(
      const IntPoint& point_in_root_frame) const;
  static LayoutBox* FindAutoscrollable(LayoutObject*);
  virtual void StopAutoscroll() {}
  virtual void MayUpdateHoverWhenContentUnderMouseChanged(EventHandler&);

  DISABLE_CFI_PERF bool HasAutoVerticalScrollbar() const {
    return HasOverflowClip() && StyleRef().HasAutoVerticalScroll();
  }
  DISABLE_CFI_PERF bool HasAutoHorizontalScrollbar() const {
    return HasOverflowClip() && StyleRef().HasAutoHorizontalScroll();
  }
  DISABLE_CFI_PERF bool ScrollsOverflow() const {
    return HasOverflowClip() && StyleRef().ScrollsOverflow();
  }
  // We place block-direction scrollbar on the left only if the writing-mode
  // is horizontal, so ShouldPlaceVerticalScrollbarOnLeft() is the same as
  // ShouldPlaceBlockDirectionScrollbarOnLogicalLeft(). The two forms can be
  // used in different contexts, e.g. the former for physical coordinate
  // contexts, and the later for logical coordinate contexts.
  bool ShouldPlaceVerticalScrollbarOnLeft() const {
    return ShouldPlaceBlockDirectionScrollbarOnLogicalLeft();
  }
  virtual bool ShouldPlaceBlockDirectionScrollbarOnLogicalLeft() const {
    return StyleRef().ShouldPlaceBlockDirectionScrollbarOnLogicalLeft();
  }

  bool HasScrollableOverflowX() const {
    return ScrollsOverflowX() &&
           PixelSnappedScrollWidth() != PixelSnappedClientWidth();
  }
  bool HasScrollableOverflowY() const {
    return ScrollsOverflowY() &&
           PixelSnappedScrollHeight() != PixelSnappedClientHeight();
  }
  virtual bool ScrollsOverflowX() const {
    return HasOverflowClip() && StyleRef().ScrollsOverflowX();
  }
  virtual bool ScrollsOverflowY() const {
    return HasOverflowClip() && StyleRef().ScrollsOverflowY();
  }

  // Elements such as the <input> field override this to specify that they are
  // scrollable outside the context of the CSS overflow style
  virtual bool IsIntrinsicallyScrollable(
      ScrollbarOrientation orientation) const {
    return false;
  }

  bool HasUnsplittableScrollingOverflow() const;

  // Page / column breakability inside block-level objects.
  enum PaginationBreakability {
    kAllowAnyBreaks,  // No restrictions on breaking. May examine children to
                      // find possible break points.
    kForbidBreaks,  // Forbid breaks inside this object. Content cannot be split
                    // nicely into smaller pieces.
    kAvoidBreaks  // Preferably avoid breaks. If not possible, examine children
                  // to find possible break points.
  };
  virtual PaginationBreakability GetPaginationBreakability() const;

  LayoutRect LocalCaretRect(
      const InlineBox*,
      int caret_offset,
      LayoutUnit* extra_width_to_end_of_line = nullptr) const override;

  // Returns the intersection of all overflow clips which apply.
  virtual LayoutRect OverflowClipRect(
      const LayoutPoint& location,
      OverlayScrollbarClipBehavior = kIgnorePlatformOverlayScrollbarSize) const;
  LayoutRect ClipRect(const LayoutPoint& location) const;

  // Returns the combination of overflow clip, contain: paint clip and CSS clip
  // for this object.
  LayoutRect ClippingRect(const LayoutPoint& location) const;

  virtual void PaintBoxDecorationBackground(
      const PaintInfo&,
      const LayoutPoint& paint_offset) const;
  virtual void PaintMask(const PaintInfo&,
                         const LayoutPoint& paint_offset) const;
  void ImageChanged(WrappedImagePtr, CanDeferInvalidation) override;
  ResourcePriority ComputeResourcePriority() const final;

  void LogicalExtentAfterUpdatingLogicalWidth(const LayoutUnit& logical_top,
                                              LogicalExtentComputedValues&);

  PositionWithAffinity PositionForPoint(const LayoutPoint&) const override;

  void RemoveFloatingOrPositionedChildFromBlockLists();

  PaintLayer* EnclosingFloatPaintingLayer() const;

  virtual LayoutUnit FirstLineBoxBaseline() const { return LayoutUnit(-1); }
  virtual LayoutUnit InlineBlockBaseline(LineDirectionMode) const {
    return LayoutUnit(-1);
  }  // Returns -1 if we should skip this box when computing the baseline of an
     // inline-block.

  bool ShrinkToAvoidFloats() const;
  virtual bool AvoidsFloats() const;
  bool ShouldBeConsideredAsReplaced() const;

  void UpdateFragmentationInfoForChild(LayoutBox&);
  bool ChildNeedsRelayoutForPagination(const LayoutBox&) const;
  void MarkChildForPaginationRelayoutIfNeeded(LayoutBox&, SubtreeLayoutScope&);

  bool IsWritingModeRoot() const {
    return !Parent() ||
           Parent()->StyleRef().GetWritingMode() != StyleRef().GetWritingMode();
  }
  bool IsOrthogonalWritingModeRoot() const {
    return Parent() &&
           Parent()->IsHorizontalWritingMode() != IsHorizontalWritingMode();
  }
  void MarkOrthogonalWritingModeRoot();
  void UnmarkOrthogonalWritingModeRoot();

  bool IsCustomItem() const;
  bool IsCustomItemShrinkToFit() const;

  bool IsDeprecatedFlexItem() const {
    return !IsInline() && !IsFloatingOrOutOfFlowPositioned() && Parent() &&
           Parent()->IsDeprecatedFlexibleBox();
  }
  bool IsFlexItemIncludingDeprecated() const {
    return !IsInline() && !IsFloatingOrOutOfFlowPositioned() && Parent() &&
           Parent()->IsFlexibleBoxIncludingDeprecated();
  }
  bool IsFlexItem() const {
    return !IsInline() && !IsFloatingOrOutOfFlowPositioned() && Parent() &&
           Parent()->IsFlexibleBox();
  }

  bool IsGridItem() const { return Parent() && Parent()->IsLayoutGrid(); }

  LayoutUnit LineHeight(
      bool first_line,
      LineDirectionMode,
      LinePositionMode = kPositionOnContainingLine) const override;
  LayoutUnit BaselinePosition(
      FontBaseline,
      bool first_line,
      LineDirectionMode,
      LinePositionMode = kPositionOnContainingLine) const override;

  LayoutPoint OffsetPoint(const Element* parent) const;
  LayoutUnit OffsetLeft(const Element*) const final;
  LayoutUnit OffsetTop(const Element*) const final;

  LayoutPoint FlipForWritingModeForChild(const LayoutBox* child,
                                         const LayoutPoint&) const;

  WARN_UNUSED_RESULT LayoutUnit FlipForWritingMode(LayoutUnit position) const {
    // The offset is in the block direction (y for horizontal writing modes, x
    // for vertical writing modes).
    if (!UNLIKELY(HasFlippedBlocksWritingMode()))
      return position;
    DCHECK(!IsHorizontalWritingMode());
    return frame_rect_.Width() - position;
  }
  WARN_UNUSED_RESULT LayoutPoint
  FlipForWritingMode(const LayoutPoint& position) const {
    if (!UNLIKELY(HasFlippedBlocksWritingMode()))
      return position;
    DCHECK(!IsHorizontalWritingMode());
    return LayoutPoint(frame_rect_.Width() - position.X(), position.Y());
  }
  WARN_UNUSED_RESULT LayoutSize
  FlipForWritingMode(const LayoutSize& offset) const {
    if (!UNLIKELY(HasFlippedBlocksWritingMode()))
      return offset;
    DCHECK(!IsHorizontalWritingMode());
    return LayoutSize(frame_rect_.Width() - offset.Width(), offset.Height());
  }
  void FlipForWritingMode(LayoutRect& rect) const {
    if (!UNLIKELY(HasFlippedBlocksWritingMode()))
      return;
    DCHECK(!IsHorizontalWritingMode());
    rect.SetX(frame_rect_.Width() - rect.MaxX());
  }
  WARN_UNUSED_RESULT FloatPoint
  FlipForWritingMode(const FloatPoint& position) const {
    if (!UNLIKELY(HasFlippedBlocksWritingMode()))
      return position;
    DCHECK(!IsHorizontalWritingMode());
    return FloatPoint(frame_rect_.Width() - position.X(), position.Y());
  }
  void FlipForWritingMode(FloatRect& rect) const {
    if (!UNLIKELY(HasFlippedBlocksWritingMode()))
      return;
    DCHECK(!IsHorizontalWritingMode());
    rect.SetX(frame_rect_.Width() - rect.MaxX());
  }

  // Passing |container| causes flipped-block flipping w.r.t. that container,
  // or containingBlock() otherwise.
  LayoutPoint PhysicalLocation(
      const LayoutBox* flipped_blocks_container = nullptr) const;
  LayoutSize PhysicalLocationOffset() const {
    return ToLayoutSize(PhysicalLocation());
  }

  // Convert a local rect in this box's blocks direction into parent's blocks
  // direction, for parent to accumulate layout or visual overflow.
  LayoutRect RectForOverflowPropagation(const LayoutRect&) const;

  LayoutRect LogicalVisualOverflowRectForPropagation() const;
  LayoutRect VisualOverflowRectForPropagation() const {
    return RectForOverflowPropagation(VisualOverflowRect());
  }
  LayoutRect LogicalLayoutOverflowRectForPropagation(
      LayoutObject* container) const;
  LayoutRect LayoutOverflowRectForPropagation(LayoutObject* container) const;

  bool HasOverflowModel() const { return overflow_.get(); }
  bool HasSelfVisualOverflow() const {
    return overflow_ &&
           !BorderBoxRect().Contains(overflow_->SelfVisualOverflowRect());
  }
  bool HasVisualOverflow() const {
    return overflow_ && !BorderBoxRect().Contains(VisualOverflowRect());
  }

  // Return true if re-laying out the containing block of this object means that
  // we need to recalculate the preferred min/max logical widths of this object.
  //
  // Calculating min/max widths for an object should ideally only take itself
  // and its children as input. However, some objects don't adhere strictly to
  // this rule, and also take input from their containing block to figure out
  // their min/max widths. This is the case for e.g. shrink-to-fit containers
  // with percentage inline-axis padding. This isn't good practise, but that's
  // how it is and how it's going to stay, unless we want to undertake a
  // substantial maintenance task of the min/max preferred widths machinery.
  virtual bool NeedsPreferredWidthsRecalculation() const;

  // See README.md for an explanation of scroll origin.
  IntSize OriginAdjustmentForScrollbars() const;
  IntPoint ScrollOrigin() const;
  IntSize ScrolledContentOffset() const;

  // Maps from scrolling contents space to box space and apply overflow
  // clip if needed. Returns true if no clipping applied or the flattened quad
  // bounds actually intersects the clipping region. If edgeInclusive is true,
  // then this method may return true even if the resulting rect has zero area.
  //
  // When applying offsets and not clips, the TransformAccumulation is
  // respected. If there is a clip, the TransformState is flattened first.
  bool MapContentsRectToBoxSpace(
      TransformState&,
      TransformState::TransformAccumulation,
      const LayoutObject& contents,
      VisualRectFlags = kDefaultVisualRectFlags) const;

  // True if the contents scroll relative to this object. |this| must be a
  // containing block for |contents|.
  bool ContainedContentsScroll(const LayoutObject& contents) const;

  // Applies the box clip. This is like mapScrollingContentsRectToBoxSpace,
  // except it does not apply scroll.
  bool ApplyBoxClips(TransformState&,
                     TransformState::TransformAccumulation,
                     VisualRectFlags) const;

  // Maps the visual rect state |transformState| from this box into its
  // container, applying adjustments for the given container offset,
  // scrolling, container clipping, and transform (including container
  // perspective).
  bool MapVisualRectToContainer(const LayoutObject* container_bject,
                                const LayoutPoint& container_offset,
                                const LayoutObject* ancestor,
                                VisualRectFlags,
                                TransformState&) const;

  bool HasRelativeLogicalWidth() const;
  bool HasRelativeLogicalHeight() const;

  bool HasHorizontalLayoutOverflow() const {
    if (!overflow_)
      return false;

    LayoutRect layout_overflow_rect = overflow_->LayoutOverflowRect();
    LayoutRect no_overflow_rect = NoOverflowRect();
    return layout_overflow_rect.X() < no_overflow_rect.X() ||
           layout_overflow_rect.MaxX() > no_overflow_rect.MaxX();
  }

  bool HasVerticalLayoutOverflow() const {
    if (!overflow_)
      return false;

    LayoutRect layout_overflow_rect = overflow_->LayoutOverflowRect();
    LayoutRect no_overflow_rect = NoOverflowRect();
    return layout_overflow_rect.Y() < no_overflow_rect.Y() ||
           layout_overflow_rect.MaxY() > no_overflow_rect.MaxY();
  }

  virtual LayoutBox* CreateAnonymousBoxWithSameTypeAs(
      const LayoutObject*) const {
    NOTREACHED();
    return nullptr;
  }

  bool HasSameDirectionAs(const LayoutBox* object) const {
    return StyleRef().Direction() == object->StyleRef().Direction();
  }

  ShapeOutsideInfo* GetShapeOutsideInfo() const;

  void MarkShapeOutsideDependentsForLayout() {
    if (IsFloating())
      RemoveFloatingOrPositionedChildFromBlockLists();
  }

  void SetIntrinsicContentLogicalHeight(
      LayoutUnit intrinsic_content_logical_height) const {
    intrinsic_content_logical_height_ = intrinsic_content_logical_height;
  }

  bool CanRenderBorderImage() const;

  void MapLocalToAncestor(
      const LayoutBoxModelObject* ancestor,
      TransformState&,
      MapCoordinatesFlags = kApplyContainerFlip) const override;
  void MapAncestorToLocal(const LayoutBoxModelObject*,
                          TransformState&,
                          MapCoordinatesFlags) const override;

  void ClearPreviousVisualRects() override;

  LayoutBlock* PercentHeightContainer() const {
    return rare_data_ ? rare_data_->percent_height_container_ : nullptr;
  }
  void SetPercentHeightContainer(LayoutBlock*);
  void RemoveFromPercentHeightContainer();
  void ClearPercentHeightDescendants();
  // For snap areas, returns the snap container that owns us.
  LayoutBox* SnapContainer() const;
  void SetSnapContainer(LayoutBox*);
  // For snap containers, returns all associated snap areas.
  SnapAreaSet* SnapAreas() const;
  void ClearSnapAreas();

  // CustomLayoutChild only exists if this LayoutBox is a IsCustomItem (aka. a
  // child of a LayoutCustom). This is created/destroyed when this LayoutBox is
  // inserted/removed from the layout tree.
  CustomLayoutChild* GetCustomLayoutChild() const;
  void AddCustomLayoutChildIfNeeded();
  void ClearCustomLayoutChild();

  bool HitTestClippedOutByBorder(const HitTestLocation& location_in_container,
                                 const LayoutPoint& border_box_location) const;

  // Returns true if the box intersects the viewport visible to the user.
  bool IntersectsVisibleViewport() const;

  bool HasNonCompositedScrollbars() const final;

  void EnsureIsReadyForPaintInvalidation() override;

  virtual bool HasControlClip() const { return false; }

  class MutableForPainting : public LayoutObject::MutableForPainting {
   public:
    void SavePreviousSize() {
      GetLayoutBox().previous_size_ = GetLayoutBox().Size();
    }
    void SavePreviousContentBoxRectAndLayoutOverflowRect();
    void ClearPreviousContentBoxRectAndLayoutOverflowRect() {
      if (!GetLayoutBox().rare_data_)
        return;
      GetLayoutBox()
          .rare_data_->has_previous_content_box_rect_and_layout_overflow_rect_ =
          false;
    }

   protected:
    friend class LayoutBox;
    MutableForPainting(const LayoutBox& box)
        : LayoutObject::MutableForPainting(box) {}
    LayoutBox& GetLayoutBox() {
      return static_cast<LayoutBox&>(layout_object_);
    }
  };

  MutableForPainting GetMutableForPainting() const {
    return MutableForPainting(*this);
  }

  LayoutSize PreviousSize() const { return previous_size_; }
  LayoutRect PreviousPhysicalContentBoxRect() const {
    return rare_data_ &&
                   rare_data_
                       ->has_previous_content_box_rect_and_layout_overflow_rect_
               ? rare_data_->previous_physical_content_box_rect_
               : LayoutRect(LayoutPoint(), PreviousSize());
  }
  LayoutRect PreviousPhysicalLayoutOverflowRect() const {
    return rare_data_ &&
                   rare_data_
                       ->has_previous_content_box_rect_and_layout_overflow_rect_
               ? rare_data_->previous_physical_layout_overflow_rect_
               : LayoutRect(LayoutPoint(), PreviousSize());
  }

  // This function calculates the preferred widths for an object.
  //
  // This function is only expected to be called if
  // the boolean preferredLogicalWidthsDirty is true. It also MUST clear the
  // boolean before returning.
  //
  // See INTRINSIC SIZES / PREFERRED LOGICAL WIDTHS in layout_object.h for more
  // details about those widths.
  //
  // This function is public only for use by LayoutNG. Other callers should go
  // through MinPreferredLogicalWidth/MaxPreferredLogicalWidth.
  virtual void ComputePreferredLogicalWidths() {
    ClearPreferredLogicalWidthsDirty();
  }

  // LayoutNG can use this function to update our cache of preferred logical
  // widths when the layout object is managed by NG. Should not be called by
  // regular code.
  // Also clears the "dirty" flag for preferred widths.
  void SetPreferredLogicalWidthsFromNG(MinMaxSize sizes) {
    min_preferred_logical_width_ = sizes.min_size;
    max_preferred_logical_width_ = sizes.max_size;
    ClearPreferredLogicalWidthsDirty();
  }

  // Calculates the intrinsic(https://drafts.csswg.org/css-sizing-3/#intrinsic)
  // logical widths for this layout box.
  //
  // intrinsicWidth is defined as:
  //     intrinsic size of content (without our border and padding) +
  //     scrollbarWidth.
  //
  // preferredWidth is defined as:
  //     fixedWidth OR (intrinsicWidth plus border and padding).
  //     Note: fixedWidth includes border and padding and scrollbarWidth.
  //
  // This is public only for use by LayoutNG. Do not call this elsewhere.
  virtual void ComputeIntrinsicLogicalWidths(
      LayoutUnit& min_logical_width,
      LayoutUnit& max_logical_width) const;

 protected:
  ~LayoutBox() override;

  virtual bool ComputeShouldClipOverflow() const;
  virtual LayoutRect ControlClipRect(const LayoutPoint&) const {
    return LayoutRect();
  }

  void WillBeDestroyed() override;

  void InsertedIntoTree() override;
  void WillBeRemovedFromTree() override;

  void StyleWillChange(StyleDifference,
                       const ComputedStyle& new_style) override;
  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;
  void UpdateFromStyle() override;

  void InLayoutNGInlineFormattingContextWillChange(bool) final;

  virtual ItemPosition SelfAlignmentNormalBehavior(
      const LayoutBox* child = nullptr) const {
    DCHECK(!child);
    return ItemPosition::kStretch;
  }

  // Returns false if it could not cheaply compute the extent (e.g. fixed
  // background), in which case the returned rect may be incorrect.
  // FIXME: make this a const method once the LayoutBox reference in BoxPainter
  // is const.
  bool GetBackgroundPaintedExtent(LayoutRect&) const;
  virtual bool ForegroundIsKnownToBeOpaqueInRect(
      const LayoutRect& local_rect,
      unsigned max_depth_to_test) const;
  bool ComputeBackgroundIsKnownToBeObscured() const override;

  virtual void ComputePositionedLogicalWidth(
      LogicalExtentComputedValues&) const;

  LayoutUnit ComputeIntrinsicLogicalWidthUsing(
      const Length& logical_width_length,
      LayoutUnit available_logical_width,
      LayoutUnit border_and_padding) const;
  virtual LayoutUnit ComputeIntrinsicLogicalContentHeightUsing(
      const Length& logical_height_length,
      LayoutUnit intrinsic_content_height,
      LayoutUnit border_and_padding) const;

  LayoutObject* SplitAnonymousBoxesAroundChild(LayoutObject* before_child);

  virtual bool HitTestOverflowControl(HitTestResult&,
                                      const HitTestLocation&,
                                      const LayoutPoint&) {
    return false;
  }
  virtual bool HitTestChildren(HitTestResult&,
                               const HitTestLocation& location_in_container,
                               const LayoutPoint& accumulated_offset,
                               HitTestAction);
  void AddLayerHitTestRects(
      LayerHitTestRects&,
      const PaintLayer* current_composited_layer,
      const LayoutPoint& layer_offset,
      TouchAction supported_fast_actions,
      const LayoutRect& container_rect,
      TouchAction container_whitelisted_touch_action) const override;
  void ComputeSelfHitTestRects(Vector<LayoutRect>&,
                               const LayoutPoint& layer_offset) const override;

  void InvalidatePaint(const PaintInvalidatorContext&) const override;

  bool ColumnFlexItemHasStretchAlignment() const;
  bool IsStretchingColumnFlexItem() const;
  bool HasStretchedLogicalWidth() const;

  void ExcludeScrollbars(
      LayoutRect&,
      OverlayScrollbarClipBehavior = kIgnorePlatformOverlayScrollbarSize) const;

  LayoutUnit ContainingBlockLogicalWidthForPositioned(
      const LayoutBoxModelObject* containing_block,
      bool check_for_perpendicular_writing_mode = true) const;
  LayoutUnit ContainingBlockLogicalHeightForPositioned(
      const LayoutBoxModelObject* containing_block,
      bool check_for_perpendicular_writing_mode = true) const;

  static void ComputeBlockStaticDistance(
      Length& logical_top,
      Length& logical_bottom,
      const LayoutBox* child,
      const LayoutBoxModelObject* container_block);
  static void ComputeInlineStaticDistance(
      Length& logical_left,
      Length& logical_right,
      const LayoutBox* child,
      const LayoutBoxModelObject* container_block,
      LayoutUnit container_logical_width);
  static void ComputeLogicalLeftPositionedOffset(
      LayoutUnit& logical_left_pos,
      const LayoutBox* child,
      LayoutUnit logical_width_value,
      const LayoutBoxModelObject* container_block,
      LayoutUnit container_logical_width);
  static void ComputeLogicalTopPositionedOffset(
      LayoutUnit& logical_top_pos,
      const LayoutBox* child,
      LayoutUnit logical_height_value,
      const LayoutBoxModelObject* container_block,
      LayoutUnit container_logical_height);
  bool SkipContainingBlockForPercentHeightCalculation(
      const LayoutBox* containing_block) const;

  LayoutRect LocalVisualRectIgnoringVisibility() const override;

  LayoutSize OffsetFromContainerInternal(
      const LayoutObject*,
      bool ignore_scroll_offset) const override;

  // For atomic inlines, returns its resolved direction in text flow. Not to be
  // confused with the CSS property 'direction'.
  // Returns the CSS 'direction' property value when it is not atomic inline.
  TextDirection ResolvedDirection() const;

 private:
  void UpdateShapeOutsideInfoAfterStyleChange(const ComputedStyle&,
                                              const ComputedStyle* old_style);
  void UpdateGridPositionAfterStyleChange(const ComputedStyle*);
  void UpdateScrollSnapMappingAfterStyleChange(const ComputedStyle*,
                                               const ComputedStyle* old_style);
  void ClearScrollSnapMapping();
  void AddScrollSnapMapping();

  bool AutoWidthShouldFitContent() const;
  LayoutUnit ShrinkToFitLogicalWidth(LayoutUnit available_logical_width,
                                     LayoutUnit borders_plus_padding) const;

  bool StretchesToViewportInQuirksMode() const;

  virtual void ComputePositionedLogicalHeight(
      LogicalExtentComputedValues&) const;
  void ComputePositionedLogicalWidthUsing(
      SizeType,
      Length logical_width,
      const LayoutBoxModelObject* container_block,
      TextDirection container_direction,
      LayoutUnit container_logical_width,
      LayoutUnit borders_plus_padding,
      const Length& logical_left,
      const Length& logical_right,
      const Length& margin_logical_left,
      const Length& margin_logical_right,
      LogicalExtentComputedValues&) const;
  void ComputePositionedLogicalHeightUsing(
      SizeType,
      Length logical_height_length,
      const LayoutBoxModelObject* container_block,
      LayoutUnit container_logical_height,
      LayoutUnit borders_plus_padding,
      LayoutUnit logical_height,
      const Length& logical_top,
      const Length& logical_bottom,
      const Length& margin_logical_top,
      const Length& margin_logical_bottom,
      LogicalExtentComputedValues&) const;

  LayoutUnit FillAvailableMeasure(LayoutUnit available_logical_width) const;
  LayoutUnit FillAvailableMeasure(LayoutUnit available_logical_width,
                                  LayoutUnit& margin_start,
                                  LayoutUnit& margin_end) const;

  LayoutBoxRareData& EnsureRareData() {
    if (!rare_data_)
      rare_data_ = std::make_unique<LayoutBoxRareData>();
    return *rare_data_.get();
  }

  bool LogicalHeightComputesAsNone(SizeType) const;

  bool IsBox() const =
      delete;  // This will catch anyone doing an unnecessary check.

  void LocationChanged();
  void SizeChanged();

  void UpdateBackgroundAttachmentFixedStatusAfterStyleChange();

  void InflateVisualRectForFilter(TransformState&) const;
  void InflateVisualRectForFilterUnderContainer(
      TransformState&,
      const LayoutObject& container,
      const LayoutBoxModelObject* ancestor_to_stop_at) const;

  LayoutRectOutsets margin_box_outsets_;

  void AddSnapArea(const LayoutBox&);
  void RemoveSnapArea(const LayoutBox&);

  // Returns true when the current recursive scroll into visible could propagate
  // to parent frame.
  bool AllowedToPropagateRecursiveScrollToParentFrame(
      const WebScrollIntoViewParams&);

  LayoutRect DebugRect() const override;

  float VisualRectOutsetForRasterEffects() const override;

  // Return the width of the vertical scrollbar, unless it's larger than the
  // logical width of the content box, in which case we'll return that instead.
  // Scrollbar handling is quite bad in such situations, and this method here
  // is just to make sure that left-hand scrollbars don't mess up
  // scrollWidth. For the full story, visit http://crbug.com/724255.
  LayoutUnit VerticalScrollbarWidthClampedToContentBox() const;

  // The CSS border box rect for this box.
  //
  // The rectangle is in LocationContainer's physical coordinates in flipped
  // block-flow direction of LocationContainer (see the COORDINATE SYSTEMS
  // section in LayoutBoxModelObject). The location is the distance from this
  // object's border edge to the LocationContainer's border edge. Thus it
  // includes any logical top/left along with this box's margins. It doesn't
  // include transforms, relative position offsets etc.
  LayoutRect frame_rect_;

  // Previous size of m_frameRect, updated after paint invalidation.
  LayoutSize previous_size_;

  // Our intrinsic height, used for min-height: min-content etc. Maintained by
  // updateLogicalHeight. This is logicalHeight() before it is clamped to
  // min/max.
  mutable LayoutUnit intrinsic_content_logical_height_;

 protected:
  // The logical width of the element if it were to break its lines at every
  // possible opportunity.
  //
  // See LayoutObject::minPreferredLogicalWidth() for more details.
  LayoutUnit min_preferred_logical_width_;

  // The logical width of the element if it never breaks any lines at all.
  //
  // See LayoutObject::maxPreferredLogicalWidth() for more details.
  LayoutUnit max_preferred_logical_width_;

  // Our overflow information.
  std::unique_ptr<BoxOverflowModel> overflow_;

 private:
  LogicalToPhysicalSetter<LayoutUnit, LayoutBox> LogicalMarginToPhysicalSetter(
      const ComputedStyle* override_style) {
    const auto& style = override_style ? *override_style : StyleRef();
    return LogicalToPhysicalSetter<LayoutUnit, LayoutBox>(
        style.GetWritingMode(), style.Direction(), *this,
        &LayoutBox::SetMarginTop, &LayoutBox::SetMarginRight,
        &LayoutBox::SetMarginBottom, &LayoutBox::SetMarginLeft);
  }

  union {
    // The inline box containing this LayoutBox, for atomic inline elements.
    // Valid only when !IsInLayoutNGInlineFormattingContext().
    InlineBox* inline_box_wrapper_;
    // The first fragment of the inline box containing this LayoutBox, for
    // atomic inline elements. Valid only when
    // IsInLayoutNGInlineFormattingContext().
    NGPaintFragment* first_paint_fragment_;
  };

  std::unique_ptr<LayoutBoxRareData> rare_data_;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutBox, IsBox());

inline LayoutBox* LayoutBox::PreviousSiblingBox() const {
  return ToLayoutBox(PreviousSibling());
}

inline LayoutBox* LayoutBox::PreviousInFlowSiblingBox() const {
  LayoutBox* previous = PreviousSiblingBox();
  while (previous && previous->IsOutOfFlowPositioned())
    previous = previous->PreviousSiblingBox();
  return previous;
}

inline LayoutBox* LayoutBox::NextSiblingBox() const {
  return ToLayoutBox(NextSibling());
}

inline LayoutBox* LayoutBox::NextInFlowSiblingBox() const {
  LayoutBox* next = NextSiblingBox();
  while (next && next->IsOutOfFlowPositioned())
    next = next->NextSiblingBox();
  return next;
}

inline LayoutBox* LayoutBox::ParentBox() const {
  return ToLayoutBox(Parent());
}

inline LayoutBox* LayoutBox::FirstInFlowChildBox() const {
  LayoutBox* first = FirstChildBox();
  return (first && first->IsOutOfFlowPositioned())
             ? first->NextInFlowSiblingBox()
             : first;
}

inline LayoutBox* LayoutBox::FirstChildBox() const {
  return ToLayoutBox(SlowFirstChild());
}

inline LayoutBox* LayoutBox::LastChildBox() const {
  return ToLayoutBox(SlowLastChild());
}

inline LayoutBox* LayoutBox::PreviousSiblingMultiColumnBox() const {
  DCHECK(IsLayoutMultiColumnSpannerPlaceholder() || IsLayoutMultiColumnSet());
  LayoutBox* previous_box = PreviousSiblingBox();
  if (previous_box->IsLayoutFlowThread())
    return nullptr;
  return previous_box;
}

inline LayoutBox* LayoutBox::NextSiblingMultiColumnBox() const {
  DCHECK(IsLayoutMultiColumnSpannerPlaceholder() || IsLayoutMultiColumnSet());
  return NextSiblingBox();
}

inline InlineBox* LayoutBox::InlineBoxWrapper() const {
  return IsInLayoutNGInlineFormattingContext() ? nullptr : inline_box_wrapper_;
}

inline void LayoutBox::SetInlineBoxWrapper(InlineBox* box_wrapper) {
  CHECK(!IsInLayoutNGInlineFormattingContext());

  if (box_wrapper) {
    DCHECK(!inline_box_wrapper_);
    // m_inlineBoxWrapper should already be nullptr. Deleting it is a safeguard
    // against security issues. Otherwise, there will two line box wrappers
    // keeping the reference to this layoutObject, and only one will be notified
    // when the layoutObject is getting destroyed. The second line box wrapper
    // will keep a stale reference.
    if (UNLIKELY(inline_box_wrapper_ != nullptr))
      DeleteLineBoxWrapper();
  }

  inline_box_wrapper_ = box_wrapper;
}

inline NGPaintFragment* LayoutBox::FirstInlineFragment() const {
  return IsInLayoutNGInlineFormattingContext() ? first_paint_fragment_
                                               : nullptr;
}

inline bool LayoutBox::IsForcedFragmentainerBreakValue(
    EBreakBetween break_value) {
  return break_value == EBreakBetween::kColumn ||
         break_value == EBreakBetween::kLeft ||
         break_value == EBreakBetween::kPage ||
         break_value == EBreakBetween::kRecto ||
         break_value == EBreakBetween::kRight ||
         break_value == EBreakBetween::kVerso;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_BOX_H_
