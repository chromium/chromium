// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_PHYSICAL_BOX_FRAGMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_PHYSICAL_BOX_FRAGMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_baseline.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_container_fragment.h"
#include "third_party/blink/renderer/platform/graphics/scroll_types.h"
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

  scoped_refptr<const NGLayoutResult> CloneAsHiddenForPaint() const;

  ~NGPhysicalBoxFragment() {
    if (has_fragment_items_)
      ComputeItemsAddress()->~NGFragmentItems();
    for (const NGLink& child : Children())
      child.fragment->Release();
  }

  // Returns |NGFragmentItems| if this fragment has one.
  bool HasItems() const { return has_fragment_items_; }
  const NGFragmentItems* Items() const {
    return has_fragment_items_ ? ComputeItemsAddress() : nullptr;
  }

  base::Optional<LayoutUnit> Baseline(const NGBaselineRequest& request) const {
    return baselines_.Offset(request);
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

  NGPixelSnappedPhysicalBoxStrut PixelSnappedPadding() const {
    if (!has_padding_)
      return NGPixelSnappedPhysicalBoxStrut();
    return ComputePaddingAddress()->SnapToDevicePixels();
  }

  bool HasSelfPaintingLayer() const;
  bool ChildrenInline() const { return children_inline_; }

  PhysicalRect ScrollableOverflow() const;

  // TODO(layout-dev): These three methods delegate to legacy layout for now,
  // update them to use LayoutNG based overflow information from the fragment
  // and change them to use NG geometry types once LayoutNG supports overflow.
  PhysicalRect OverflowClipRect(
      const PhysicalOffset& location,
      OverlayScrollbarClipBehavior = kIgnorePlatformOverlayScrollbarSize) const;
  LayoutSize ScrolledContentOffset() const;
  PhysicalSize ScrollSize() const;

  // Compute visual overflow of this box in the local coordinate.
  PhysicalRect ComputeSelfInkOverflow() const;

  // Fragment offset is this fragment's offset from parent.
  // Needed to compensate for LayoutInline Legacy code offsets.
  void AddSelfOutlineRects(const PhysicalOffset& additional_offset,
                           NGOutlineType include_block_overflows,
                           Vector<PhysicalRect>* outline_rects) const;

  UBiDiLevel BidiLevel() const;

  // Bitmask for border edges, see NGBorderEdges::Physical.
  unsigned BorderEdges() const { return border_edge_; }
  NGPixelSnappedPhysicalBoxStrut BorderWidths() const;

  // Return true if this is the first fragment generated from a node.
  bool IsFirstForNode() const { return is_first_for_node_; }

#if DCHECK_IS_ON()
  void CheckSameForSimplifiedLayout(const NGPhysicalBoxFragment&,
                                    bool check_same_block_size) const;
#endif

 private:
  NGPhysicalBoxFragment(NGBoxFragmentBuilder* builder,
                        const NGPhysicalBoxStrut& borders,
                        const NGPhysicalBoxStrut& padding,
                        WritingMode block_or_line_writing_mode);

  const NGFragmentItems* ComputeItemsAddress() const {
    DCHECK(has_fragment_items_ || has_borders_ || has_padding_);
    const NGLink* children_end = children_ + Children().size();
    return reinterpret_cast<const NGFragmentItems*>(children_end);
  }

  const NGPhysicalBoxStrut* ComputeBordersAddress() const {
    DCHECK(has_borders_ || has_padding_);
    const NGFragmentItems* items = ComputeItemsAddress();
    if (has_fragment_items_)
      ++items;
    return reinterpret_cast<const NGPhysicalBoxStrut*>(items);
  }

  const NGPhysicalBoxStrut* ComputePaddingAddress() const {
    DCHECK(has_padding_);
    const NGPhysicalBoxStrut* address = ComputeBordersAddress();
    return has_borders_ ? address + 1 : address;
  }

  NGBaselineList baselines_;
  NGLink children_[];
  // borders and padding come from after |children_| if they are not zero.
};

template <>
struct DowncastTraits<NGPhysicalBoxFragment> {
  static bool AllowFrom(const NGPhysicalFragment& fragment) {
    return fragment.Type() == NGPhysicalFragment::kFragmentBox ||
           fragment.Type() == NGPhysicalFragment::kFragmentRenderedLegend;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_PHYSICAL_BOX_FRAGMENT_H_
