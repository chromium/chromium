// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_PHYSICAL_BOX_FRAGMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_PHYSICAL_BOX_FRAGMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/box_sides.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items.h"
#include "third_party/blink/renderer/core/layout/ng/mathml/ng_mathml_paint_info.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_container_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_borders.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_fragment_data.h"
#include "third_party/blink/renderer/platform/graphics/overlay_scrollbar_clip_behavior.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class NGBoxFragmentBuilder;
enum class NGOutlineType;

class CORE_EXPORT NGPhysicalBoxFragment final
    : public NGPhysicalContainerFragment {
 public:
  static scoped_refptr<const NGPhysicalBoxFragment> Create(
      NGBoxFragmentBuilder* builder,
      WritingMode block_or_line_writing_mode);

  using PassKey = util::PassKey<NGPhysicalBoxFragment>;
  NGPhysicalBoxFragment(PassKey,
                        NGBoxFragmentBuilder* builder,
                        const NGPhysicalBoxStrut& borders,
                        const NGPhysicalBoxStrut& padding,
                        bool has_rare_data,
                        WritingMode block_or_line_writing_mode);

  scoped_refptr<const NGLayoutResult> CloneAsHiddenForPaint() const;

  ~NGPhysicalBoxFragment() {
    if (has_fragment_items_)
      ComputeItemsAddress()->~NGFragmentItems();
    if (has_rare_data_)
      ComputeRareDataAddress()->~RareData();
    for (const NGLink& child : Children())
      child.fragment->Release();
  }

  const NGPhysicalBoxFragment* PostLayout() const;

  // Returns |NGFragmentItems| if this fragment has one.
  bool HasItems() const { return has_fragment_items_; }
  const NGFragmentItems* Items() const {
    return has_fragment_items_ ? ComputeItemsAddress() : nullptr;
  }

  base::Optional<LayoutUnit> Baseline() const {
    if (has_baseline_)
      return baseline_;
    return base::nullopt;
  }

  base::Optional<LayoutUnit> LastBaseline() const {
    if (has_last_baseline_)
      return last_baseline_;
    return base::nullopt;
  }

  PhysicalRect TableGridRect() const {
    return ComputeRareDataAddress()->table_grid_rect;
  }

  const NGTableFragmentData::ColumnGeometries* TableColumnGeometries() const {
    return &ComputeRareDataAddress()->table_column_geometries;
  }

  const NGTableBorders* TableCollapsedBorders() const {
    return ComputeRareDataAddress()->table_collapsed_borders.get();
  }

  const NGTableFragmentData::CollapsedBordersGeometry*
  TableCollapsedBordersGeometry() const {
    auto& table_collapsed_borders_geometry =
        ComputeRareDataAddress()->table_collapsed_borders_geometry;
    if (!table_collapsed_borders_geometry)
      return nullptr;
    return table_collapsed_borders_geometry.get();
  }

  wtf_size_t TableCellColumnIndex() const {
    return ComputeRareDataAddress()->table_cell_column_index;
  }

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

  bool HasOutOfFlowPositionedFragmentainerDescendants() const {
    if (!has_rare_data_)
      return false;

    return !ComputeRareDataAddress()
                ->oof_positioned_fragmentainer_descendants.IsEmpty();
  }

  base::span<NGPhysicalOutOfFlowPositionedNode>
  OutOfFlowPositionedFragmentainerDescendants() const {
    if (!has_rare_data_)
      return base::span<NGPhysicalOutOfFlowPositionedNode>();
    Vector<NGPhysicalOutOfFlowPositionedNode>& descendants =
        const_cast<Vector<NGPhysicalOutOfFlowPositionedNode>&>(
            ComputeRareDataAddress()->oof_positioned_fragmentainer_descendants);
    return {descendants.data(), descendants.size()};
  }

  NGPixelSnappedPhysicalBoxStrut PixelSnappedPadding() const {
    if (!has_padding_)
      return NGPixelSnappedPhysicalBoxStrut();
    return ComputePaddingAddress()->SnapToDevicePixels();
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

  PhysicalRect ScrollableOverflow(TextHeightType height_type) const;
  PhysicalRect ScrollableOverflowFromChildren(TextHeightType height_type) const;

  // TODO(layout-dev): These three methods delegate to legacy layout for now,
  // update them to use LayoutNG based overflow information from the fragment
  // and change them to use NG geometry types once LayoutNG supports overflow.
  PhysicalRect OverflowClipRect(
      const PhysicalOffset& location,
      OverlayScrollbarClipBehavior = kIgnoreOverlayScrollbarSize) const;
  LayoutSize PixelSnappedScrolledContentOffset() const;
  PhysicalSize ScrollSize() const;

  // Compute visual overflow of this box in the local coordinate.
  PhysicalRect ComputeSelfInkOverflow() const;

  // Contents ink overflow includes anything that would bleed out of the box and
  // would be clipped by the overflow clip ('overflow' != visible). This
  // corresponds to children that overflows their parent.
  PhysicalRect ContentsInkOverflow() const {
    // TODO(layout-dev): Implement box fragment overflow.
    return LocalRect();
  }

  // Fragment offset is this fragment's offset from parent.
  // Needed to compensate for LayoutInline Legacy code offsets.
  void AddSelfOutlineRects(const PhysicalOffset& additional_offset,
                           NGOutlineType include_block_overflows,
                           Vector<PhysicalRect>* outline_rects) const;

  UBiDiLevel BidiLevel() const;

  PhysicalBoxSides SidesToInclude() const {
    return PhysicalBoxSides(include_border_top_, include_border_right_,
                            include_border_bottom_, include_border_left_);
  }
  NGPixelSnappedPhysicalBoxStrut BorderWidths() const;

  // Return true if this is the first fragment generated from a node.
  bool IsFirstForNode() const { return is_first_for_node_; }

  // Returns true if we have a descendant within this formatting context, which
  // is potentially above our block-start edge.
  bool MayHaveDescendantAboveBlockStart() const {
    return may_have_descendant_above_block_start_;
  }

#if DCHECK_IS_ON()
  void CheckSameForSimplifiedLayout(const NGPhysicalBoxFragment&,
                                    bool check_same_block_size) const;
#endif

  bool HasExtraMathMLPainting() const {
    if (IsMathMLFraction())
      return true;

    if (has_rare_data_ && ComputeRareDataAddress()->mathml_paint_info)
      return true;

    return false;
  }
  const NGMathMLPaintInfo& GetMathMLPaintInfo() const {
    return *ComputeRareDataAddress()->mathml_paint_info;
  }

 private:
  struct RareData {
    RareData(NGBoxFragmentBuilder*, PhysicalSize size);

    Vector<NGPhysicalOutOfFlowPositionedNode>
        oof_positioned_fragmentainer_descendants;
    const std::unique_ptr<NGMathMLPaintInfo> mathml_paint_info;

    // TablesNG rare data.
    PhysicalRect table_grid_rect;
    NGTableFragmentData::ColumnGeometries table_column_geometries;
    scoped_refptr<const NGTableBorders> table_collapsed_borders;
    std::unique_ptr<NGTableFragmentData::CollapsedBordersGeometry>
        table_collapsed_borders_geometry;
    wtf_size_t table_cell_column_index;
  };

  const NGFragmentItems* ComputeItemsAddress() const {
    DCHECK(has_fragment_items_ || has_borders_ || has_padding_ ||
           has_rare_data_);
    const NGLink* children_end = children_ + Children().size();
    return reinterpret_cast<const NGFragmentItems*>(children_end);
  }

  const NGPhysicalBoxStrut* ComputeBordersAddress() const {
    DCHECK(has_borders_ || has_padding_ || has_rare_data_);
    const NGFragmentItems* items = ComputeItemsAddress();
    if (!has_fragment_items_)
      return reinterpret_cast<const NGPhysicalBoxStrut*>(items);
    return reinterpret_cast<const NGPhysicalBoxStrut*>(
        reinterpret_cast<const uint8_t*>(items) + items->ByteSize());
  }

  const NGPhysicalBoxStrut* ComputePaddingAddress() const {
    DCHECK(has_padding_ || has_rare_data_);
    const NGPhysicalBoxStrut* address = ComputeBordersAddress();
    return has_borders_ ? address + 1 : address;
  }

  const RareData* ComputeRareDataAddress() const {
    DCHECK(has_rare_data_);
    NGPhysicalBoxStrut* address =
        const_cast<NGPhysicalBoxStrut*>(ComputePaddingAddress());
    return has_padding_ ? reinterpret_cast<RareData*>(address + 1)
                        : reinterpret_cast<RareData*>(address);
  }

#if DCHECK_IS_ON()
  void CheckIntegrity() const;
#endif

  LayoutUnit baseline_;
  LayoutUnit last_baseline_;
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
