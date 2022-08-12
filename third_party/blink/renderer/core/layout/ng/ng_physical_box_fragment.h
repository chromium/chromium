// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_PHYSICAL_BOX_FRAGMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_PHYSICAL_BOX_FRAGMENT_H_

#include "base/dcheck_is_on.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/box_sides.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items.h"
#include "third_party/blink/renderer/core/layout/ng/mathml/ng_mathml_paint_info.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_borders.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_fragment_data.h"
#include "third_party/blink/renderer/core/style/style_overflow_clip_margin.h"
#include "third_party/blink/renderer/platform/graphics/overlay_scrollbar_clip_behavior.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class NGBoxFragmentBuilder;
enum class NGOutlineType;
struct FrameSetLayoutData;

class CORE_EXPORT NGPhysicalBoxFragment final : public NGPhysicalFragment {
 public:
  static const NGPhysicalBoxFragment* Create(
      NGBoxFragmentBuilder* builder,
      WritingMode block_or_line_writing_mode);

  // Creates a shallow copy of |other|.
  static const NGPhysicalBoxFragment* Clone(const NGPhysicalBoxFragment& other);

  // Creates a shallow copy of |other| but uses the "post-layout" fragments to
  // ensure fragment-tree consistency.
  static const NGPhysicalBoxFragment* CloneWithPostLayoutFragments(
      const NGPhysicalBoxFragment& other,
      const absl::optional<PhysicalRect> updated_layout_overflow =
          absl::nullopt);

  using PassKey = base::PassKey<NGPhysicalBoxFragment>;
  NGPhysicalBoxFragment(PassKey,
                        NGBoxFragmentBuilder* builder,
                        bool has_layout_overflow,
                        const PhysicalRect& layout_overflow,
                        bool has_borders,
                        const NGPhysicalBoxStrut& borders,
                        bool has_padding,
                        const NGPhysicalBoxStrut& padding,
                        const absl::optional<PhysicalRect>& inflow_bounds,
                        bool has_fragment_items,
                        bool has_rare_data,
                        WritingMode block_or_line_writing_mode);

  // Make a shallow copy. The child fragment pointers are just shallowly
  // copied. Fragment *items* are cloned (but not box fragments associated with
  // items), though. Additionally, the copy will set new overflow information,
  // based on the parameters, rather than copying it from the original fragment.
  NGPhysicalBoxFragment(PassKey,
                        const NGPhysicalBoxFragment& other,
                        bool has_layout_overflow,
                        const PhysicalRect& layout_overflow);

  ~NGPhysicalBoxFragment();

#if DCHECK_IS_ON()
  class AllowPostLayoutScope {
    STACK_ALLOCATED();

   public:
    AllowPostLayoutScope();
    ~AllowPostLayoutScope();
    static bool IsAllowed() { return allow_count_; }

   private:
    static unsigned allow_count_;
  };
#endif

  void TraceAfterDispatch(Visitor* visitor) const;

  const NGPhysicalBoxFragment* PostLayout() const;

  // Returns the children of |this|.
  //
  // Note, children in this collection maybe old generations. Items in this
  // collection are safe, but their children (grandchildren of |this|) maybe
  // from deleted nodes or LayoutObjects. Also see |PostLayoutChildren()|.
  base::span<const NGLink> Children() const {
    DCHECK(children_valid_);
    return base::make_span(children_, const_num_children_);
  }

  // Similar to |Children()| but all children are the latest generation of
  // post-layout, and therefore all descendants are safe.
  NGPhysicalFragment::PostLayoutChildLinkList PostLayoutChildren() const {
    DCHECK(children_valid_);
    return PostLayoutChildLinkList(const_num_children_, children_);
  }

  // This exposes a mutable part of the fragment for |NGOutOfFlowLayoutPart|.
  class MutableChildrenForOutOfFlow final {
    STACK_ALLOCATED();

   protected:
    friend class NGOutOfFlowLayoutPart;
    base::span<NGLink> Children() const {
      return base::make_span(buffer_, num_children_);
    }

   private:
    friend class NGPhysicalBoxFragment;
    MutableChildrenForOutOfFlow(const NGLink* buffer, wtf_size_t num_children)
        : buffer_(const_cast<NGLink*>(buffer)), num_children_(num_children) {}

    NGLink* buffer_;
    wtf_size_t num_children_;
  };

  MutableChildrenForOutOfFlow GetMutableChildrenForOutOfFlow() const {
    DCHECK(children_valid_);
    return MutableChildrenForOutOfFlow(children_, const_num_children_);
  }

  // Returns |NGFragmentItems| if this fragment has one.
  bool HasItems() const { return const_has_fragment_items_; }
  const NGFragmentItems* Items() const {
    return const_has_fragment_items_ ? ComputeItemsAddress() : nullptr;
  }

  absl::optional<LayoutUnit> Baseline() const {
    if (has_baseline_)
      return baseline_;
    return absl::nullopt;
  }

  absl::optional<LayoutUnit> LastBaseline() const {
    if (has_last_baseline_)
      return last_baseline_;
    return absl::nullopt;
  }

  LogicalRect TableGridRect() const {
    return ComputeRareDataAddress()->table_grid_rect;
  }

  const NGTableFragmentData::ColumnGeometries* TableColumnGeometries() const {
    return &ComputeRareDataAddress()->table_column_geometries;
  }

  const NGTableBorders* TableCollapsedBorders() const {
    return const_has_rare_data_
               ? ComputeRareDataAddress()->table_collapsed_borders.get()
               : nullptr;
  }

  const NGTableFragmentData::CollapsedBordersGeometry*
  TableCollapsedBordersGeometry() const {
    return ComputeRareDataAddress()->table_collapsed_borders_geometry.get();
  }

  wtf_size_t TableCellColumnIndex() const {
    return ComputeRareDataAddress()->table_cell_column_index;
  }

  absl::optional<wtf_size_t> TableSectionStartRowIndex() const {
    DCHECK(IsTableNGSection());
    if (!const_has_rare_data_)
      return absl::nullopt;
    const auto* rare_data = ComputeRareDataAddress();
    if (rare_data->table_section_row_offsets.IsEmpty())
      return absl::nullopt;
    return rare_data->table_section_start_row_index;
  }

  const Vector<LayoutUnit>* TableSectionRowOffsets() const {
    DCHECK(IsTableNGSection());
    return const_has_rare_data_
               ? &ComputeRareDataAddress()->table_section_row_offsets
               : nullptr;
  }

  // Returns the layout-overflow for this fragment.
  const PhysicalRect LayoutOverflow() const {
    if (is_legacy_layout_root_)
      return To<LayoutBox>(GetLayoutObject())->PhysicalLayoutOverflowRect();
    if (!has_layout_overflow_)
      return {{}, Size()};
    return *ComputeLayoutOverflowAddress();
  }

  bool HasLayoutOverflow() const { return has_layout_overflow_; }

  const NGPhysicalBoxStrut Borders() const {
    if (!has_borders_)
      return NGPhysicalBoxStrut();
    return *ComputeBordersAddress();
  }

  const NGPhysicalBoxStrut Padding() const {
    if (!has_padding_)
      return NGPhysicalBoxStrut();
    return *ComputePaddingAddress();
  }

  const PhysicalOffset ContentOffset() const {
    if (!has_borders_ && !has_padding_)
      return PhysicalOffset();
    PhysicalOffset offset;
    if (has_borders_)
      offset += Borders().Offset();
    if (has_padding_)
      offset += Padding().Offset();
    return offset;
  }

  // Returns the bounds of any inflow children for this fragment (specifically
  // no out-of-flow positioned objects). This will return |absl::nullopt| if:
  //  - The fragment is *not* a scroll container.
  //  - The scroll container contains no inflow children.
  // This is normally the union of all inflow children's border-box rects
  // (without relative positioning applied), however for grid layout it is the
  // size and position of the grid instead.
  // This is used for scrollable overflow calculations.
  const absl::optional<PhysicalRect> InflowBounds() const {
    if (!has_inflow_bounds_)
      return absl::nullopt;
    return *ComputeInflowBoundsAddress();
  }

  // Return true if this is either a container that establishes an inline
  // formatting context, or if it's non-atomic inline content participating in
  // one. Empty blocks don't establish an inline formatting context.
  //
  // The return value from this method is undefined and irrelevant if the object
  // establishes a different type of formatting context than block/inline, such
  // as table or flexbox.
  //
  // Example:
  // <div>                                       <!-- false -->
  //   <div>                                     <!-- true -->
  //     <div style="float:left;"></div>         <!-- false -->
  //     <div style="float:left;">               <!-- true -->
  //       xxx                                   <!-- true -->
  //     </div>
  //     <div style="float:left;">               <!-- false -->
  //       <div style="float:left;"></div>       <!-- false -->
  //     </div>
  //     <span>                                  <!-- true -->
  //       xxx                                   <!-- true -->
  //       <span style="display:inline-block;">  <!-- false -->
  //         <div></div>                         <!-- false -->
  //       </span>
  //       <span style="display:inline-block;">  <!-- true -->
  //         xxx                                 <!-- true -->
  //       </span>
  //       <span style="display:inline-flex;">   <!-- N/A -->
  bool IsInlineFormattingContext() const {
    return is_inline_formatting_context_;
  }

  // The |LayoutBox| whose |PhysicalFragments()| contains |this|. This is
  // different from |GetLayoutObject()| if |this.IsColumnBox()|.
  const LayoutBox* OwnerLayoutBox() const;
  LayoutBox* MutableOwnerLayoutBox() const;

  // Returns the offset in the |OwnerLayoutBox| coordinate system. This is only
  // supported for CSS boxes (i.e. not for fragmentainers, for instance).
  PhysicalOffset OffsetFromOwnerLayoutBox() const;

  PhysicalRect ScrollableOverflow(TextHeightType height_type) const;
  PhysicalRect ScrollableOverflowFromChildren(TextHeightType height_type) const;

  OverflowClipAxes GetOverflowClipAxes() const {
    if (const auto* layout_object = GetLayoutObject())
      return layout_object->GetOverflowClipAxes();
    return kNoOverflowClip;
  }

  // TODO(layout-dev): These three methods delegate to legacy layout for now,
  // update them to use LayoutNG based overflow information from the fragment
  // and change them to use NG geometry types once LayoutNG supports overflow.
  PhysicalRect OverflowClipRect(
      const PhysicalOffset& location,
      OverlayScrollbarClipBehavior = kIgnoreOverlayScrollbarSize) const;
  PhysicalRect OverflowClipRect(
      const PhysicalOffset& location,
      const NGBlockBreakToken* incoming_break_token,
      OverlayScrollbarClipBehavior = kIgnoreOverlayScrollbarSize) const;
  gfx::Vector2d PixelSnappedScrolledContentOffset() const;
  PhysicalSize ScrollSize() const;

  NGInkOverflow::Type InkOverflowType() const {
    return static_cast<NGInkOverflow::Type>(ink_overflow_type_);
  }
  bool IsInkOverflowComputed() const {
    return InkOverflowType() != NGInkOverflow::kNotSet &&
           InkOverflowType() != NGInkOverflow::kInvalidated;
  }
  bool HasInkOverflow() const {
    return InkOverflowType() != NGInkOverflow::kNone;
  }

  // 3 types of ink overflows:
  // * |SelfInkOverflow| includes box decorations that are outside of the
  //   border box.
  //   Returns |LocalRect| when there are no overflow.
  // * |ContentsInkOverflow| includes anything that would bleed out of the box
  //   and would be clipped by the overflow clip ('overflow' != visible). This
  //   corresponds to children that overflows their parent.
  //   Returns an empty rect when there are no overflow.
  // * |InkOverflow| includes self and contents ink overflow, unless it has
  //   clipping, in that case it includes self ink overflow only.
  //   Returns |LocalRect| when there are no overflow.
  PhysicalRect InkOverflow() const;
  PhysicalRect SelfInkOverflow() const;
  PhysicalRect ContentsInkOverflow() const;

  // Fast check if |NodeAtPoint| may find a hit.
  bool MayIntersect(const HitTestResult& result,
                    const HitTestLocation& hit_test_location,
                    const PhysicalOffset& accumulated_offset) const;

  // In order to paint united outline rectangles, the "owner" fragment paints
  // outlines for non-owner fragments.
  bool IsOutlineOwner() const {
    return !IsInlineBox() || InlineContainerFragmentIfOutlineOwner();
  }
  const NGPhysicalBoxFragment* InlineContainerFragmentIfOutlineOwner() const;

  // Fragment offset is this fragment's offset from parent.
  // Needed to compensate for LayoutInline Legacy code offsets.
  void AddSelfOutlineRects(const PhysicalOffset& additional_offset,
                           NGOutlineType include_block_overflows,
                           Vector<PhysicalRect>* outline_rects,
                           LayoutObject::OutlineInfo* info) const;
  // Same as |AddSelfOutlineRects|, except when |this.IsInlineBox()|, in which
  // case the coordinate system is relative to the inline formatting context.
  void AddOutlineRects(const PhysicalOffset& additional_offset,
                       NGOutlineType include_block_overflows,
                       Vector<PhysicalRect>* outline_rects) const;

  PositionWithAffinity PositionForPoint(PhysicalOffset) const;

  // The outsets to apply to the border-box of this fragment for
  // |overflow-clip-margin|.
  NGPhysicalBoxStrut OverflowClipMarginOutsets() const;

  PhysicalBoxSides SidesToInclude() const {
    return PhysicalBoxSides(include_border_top_, include_border_right_,
                            include_border_bottom_, include_border_left_);
  }

  const NGBlockBreakToken* BreakToken() const {
    return To<NGBlockBreakToken>(NGPhysicalFragment::BreakToken());
  }

  // Return true if this is the first fragment generated from a node.
  bool IsFirstForNode() const { return is_first_for_node_; }

  // Return true if this is the only fragment generated from a node.
  bool IsOnlyForNode() const { return IsFirstForNode() && !BreakToken(); }

  bool HasDescendantsForTablePart() const {
    DCHECK(IsTableNGPart() || IsTableNGCell());
    return has_descendants_for_table_part_;
  }

  bool IsFragmentationContextRoot() const {
    return is_fragmentation_context_root_;
  }

#if DCHECK_IS_ON()
  void CheckSameForSimplifiedLayout(const NGPhysicalBoxFragment&,
                                    bool check_same_block_size) const;
#endif

  const FrameSetLayoutData* GetFrameSetLayoutData() const {
    return ComputeRareDataAddress()->frame_set_layout_data.get();
  }

  bool HasExtraMathMLPainting() const {
    if (IsMathMLFraction())
      return true;

    if (const_has_rare_data_ && ComputeRareDataAddress()->mathml_paint_info)
      return true;

    return false;
  }
  const NGMathMLPaintInfo& GetMathMLPaintInfo() const {
    return *ComputeRareDataAddress()->mathml_paint_info;
  }

  // Painters can use const methods only, except for these explicitly declared
  // methods.
  class MutableForPainting {
    STACK_ALLOCATED();

   public:
    void RecalcInkOverflow() { fragment_.RecalcInkOverflow(); }
    void RecalcInkOverflow(const PhysicalRect& contents) {
      fragment_.RecalcInkOverflow(contents);
    }
#if DCHECK_IS_ON()
    void InvalidateInkOverflow() { fragment_.InvalidateInkOverflow(); }
#endif

   private:
    friend class NGPhysicalBoxFragment;
    explicit MutableForPainting(const NGPhysicalBoxFragment& fragment)
        : fragment_(const_cast<NGPhysicalBoxFragment&>(fragment)) {}

    NGPhysicalBoxFragment& fragment_;
  };
  MutableForPainting GetMutableForPainting() const {
    return MutableForPainting(*this);
  }

  class MutableForCloning {
    STACK_ALLOCATED();
    friend class NGFragmentRepeater;
    friend class NGPhysicalBoxFragment;

   public:
    void ClearIsFirstForNode() { fragment_.is_first_for_node_ = false; }
    void ClearPropagatedOOFs() { fragment_.ClearOutOfFlowData(); }
    void SetBreakToken(const NGBlockBreakToken* token) {
      fragment_.break_token_ = token;
    }
    base::span<NGLink> Children() const {
      DCHECK(fragment_.children_valid_);
      return base::make_span(fragment_.children_,
                             fragment_.const_num_children_);
    }

   private:
    explicit MutableForCloning(const NGPhysicalBoxFragment& fragment)
        : fragment_(const_cast<NGPhysicalBoxFragment&>(fragment)) {}

    NGPhysicalBoxFragment& fragment_;
  };
  friend class MutableForCloning;
  MutableForCloning GetMutableForCloning() const {
    return MutableForCloning(*this);
  }

  // Returns if this fragment can compute ink overflow.
  bool CanUseFragmentsForInkOverflow() const { return !IsLegacyLayoutRoot(); }
  // Recalculates and updates |*InkOverflow|.
  void RecalcInkOverflow();
  // |RecalcInkOverflow| using the given contents ink overflow rect.
  void RecalcInkOverflow(const PhysicalRect& contents);

#if DCHECK_IS_ON()
  void InvalidateInkOverflow();
  void AssertFragmentTreeSelf() const;
  void AssertFragmentTreeChildren(bool allow_destroyed_or_moved = false) const;
#endif

 protected:
  friend class NGPhysicalFragment;
  void Dispose();

 private:
  static size_t AdditionalByteSize(wtf_size_t num_fragment_items,
                                   wtf_size_t num_children,
                                   bool has_layout_overflow,
                                   bool has_borders,
                                   bool has_padding,
                                   bool has_inflow_bounds,
                                   bool has_rare_data);

  struct RareData {
    DISALLOW_NEW();

   public:
    RareData(const RareData&);
    explicit RareData(NGBoxFragmentBuilder*);
    void Trace(Visitor*) const;

    const std::unique_ptr<const FrameSetLayoutData> frame_set_layout_data;
    const std::unique_ptr<const NGMathMLPaintInfo> mathml_paint_info;

    // Table rare-data.
    LogicalRect table_grid_rect;
    NGTableFragmentData::ColumnGeometries table_column_geometries;
    scoped_refptr<const NGTableBorders> table_collapsed_borders;
    std::unique_ptr<NGTableFragmentData::CollapsedBordersGeometry>
        table_collapsed_borders_geometry;

    // Table-cell rare-data.
    wtf_size_t table_cell_column_index;

    // Table-section rare-data.
    wtf_size_t table_section_start_row_index;
    Vector<LayoutUnit> table_section_row_offsets;
  };

  const NGFragmentItems* ComputeItemsAddress() const {
    DCHECK(const_has_fragment_items_ || has_layout_overflow_ || has_borders_ ||
           has_padding_ || has_inflow_bounds_ || const_has_rare_data_);
    const NGLink* children_end = children_ + const_num_children_;
    return reinterpret_cast<const NGFragmentItems*>(
        base::bits::AlignUp(reinterpret_cast<const uint8_t*>(children_end),
                            alignof(NGFragmentItems)));
  }

  const PhysicalRect* ComputeLayoutOverflowAddress() const {
    DCHECK(has_layout_overflow_ || has_borders_ || has_padding_ ||
           has_inflow_bounds_ || const_has_rare_data_);
    const NGFragmentItems* items = ComputeItemsAddress();
    const uint8_t* uint8_t_items = reinterpret_cast<const uint8_t*>(items);
    if (const_has_fragment_items_)
      uint8_t_items += items->ByteSize();

    return reinterpret_cast<const PhysicalRect*>(
        base::bits::AlignUp(uint8_t_items, alignof(PhysicalRect)));
  }

  const NGPhysicalBoxStrut* ComputeBordersAddress() const {
    DCHECK(has_borders_ || has_padding_ || has_inflow_bounds_ ||
           const_has_rare_data_);
    const PhysicalRect* address = ComputeLayoutOverflowAddress();
    const uint8_t* unaligned_border_address =
        has_layout_overflow_ ? reinterpret_cast<const uint8_t*>(address + 1)
                             : reinterpret_cast<const uint8_t*>(address);
    return reinterpret_cast<const NGPhysicalBoxStrut*>(base::bits::AlignUp(
        unaligned_border_address, alignof(NGPhysicalBoxStrut)));
  }

  const NGPhysicalBoxStrut* ComputePaddingAddress() const {
    DCHECK(has_padding_ || has_inflow_bounds_ || const_has_rare_data_);
    const NGPhysicalBoxStrut* address = ComputeBordersAddress();
    const uint8_t* unaligned_address =
        has_borders_ ? reinterpret_cast<const uint8_t*>(address + 1)
                     : reinterpret_cast<const uint8_t*>(address);
    return reinterpret_cast<const NGPhysicalBoxStrut*>(
        base::bits::AlignUp(unaligned_address, alignof(NGPhysicalBoxStrut)));
  }

  const PhysicalRect* ComputeInflowBoundsAddress() const {
    DCHECK(has_inflow_bounds_ || const_has_rare_data_);
    NGPhysicalBoxStrut* address =
        const_cast<NGPhysicalBoxStrut*>(ComputePaddingAddress());
    const uint8_t* unaligned_address =
        has_padding_ ? reinterpret_cast<const uint8_t*>(address + 1)
                     : reinterpret_cast<const uint8_t*>(address);
    return reinterpret_cast<const PhysicalRect*>(
        base::bits::AlignUp(unaligned_address, alignof(PhysicalRect)));
  }

  const RareData* ComputeRareDataAddress() const {
    DCHECK(const_has_rare_data_);
    PhysicalRect* address =
        const_cast<PhysicalRect*>(ComputeInflowBoundsAddress());
    const uint8_t* unaligned_address =
        has_inflow_bounds_ ? reinterpret_cast<const uint8_t*>(address + 1)
                           : reinterpret_cast<const uint8_t*>(address);
    return reinterpret_cast<const RareData*>(
        base::bits::AlignUp(unaligned_address, alignof(RareData)));
  }

  void SetInkOverflow(const PhysicalRect& self, const PhysicalRect& contents);
  PhysicalRect RecalcContentsInkOverflow();
  PhysicalRect ComputeSelfInkOverflow() const;

  void AddOutlineRects(const PhysicalOffset& additional_offset,
                       NGOutlineType include_block_overflows,
                       bool inline_container_relative,
                       Vector<PhysicalRect>* outline_rects) const;
  void AddOutlineRectsForInlineBox(PhysicalOffset additional_offset,
                                   NGOutlineType include_block_overflows,
                                   bool inline_container_relative,
                                   Vector<PhysicalRect>* outline_rects) const;

  PositionWithAffinity PositionForPointByClosestChild(
      PhysicalOffset point_in_contents) const;

  PositionWithAffinity PositionForPointInBlockFlowDirection(
      PhysicalOffset point_in_contents) const;

  PositionWithAffinity PositionForPointInTable(
      PhysicalOffset point_in_contents) const;

  PositionWithAffinity PositionForPointRespectingEditingBoundaries(
      const NGPhysicalBoxFragment& child,
      PhysicalOffset point_in_child) const;

#if DCHECK_IS_ON()
  void CheckIntegrity() const;
#endif

  unsigned is_inline_formatting_context_ : 1;
  const unsigned const_has_fragment_items_ : 1;
  unsigned include_border_top_ : 1;
  unsigned include_border_right_ : 1;
  unsigned include_border_bottom_ : 1;
  unsigned include_border_left_ : 1;
  unsigned has_layout_overflow_ : 1;
  unsigned ink_overflow_type_ : NGInkOverflow::kTypeBits;
  unsigned has_borders_ : 1;
  unsigned has_padding_ : 1;
  unsigned has_inflow_bounds_ : 1;
  const unsigned const_has_rare_data_ : 1;
  unsigned is_first_for_node_ : 1;
  unsigned has_descendants_for_table_part_ : 1;
  unsigned is_fragmentation_context_root_ : 1;

  const wtf_size_t const_num_children_;

  LayoutUnit baseline_;
  LayoutUnit last_baseline_;
  NGInkOverflow ink_overflow_;
  NGLink children_[];
  // fragment_items, borders, padding, and rare_data are after |children_| if
  // they are not empty/initial.
};

template <>
struct DowncastTraits<NGPhysicalBoxFragment> {
  static bool AllowFrom(const NGPhysicalFragment& fragment) {
    return fragment.Type() == NGPhysicalFragment::kFragmentBox;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_PHYSICAL_BOX_FRAGMENT_H_
