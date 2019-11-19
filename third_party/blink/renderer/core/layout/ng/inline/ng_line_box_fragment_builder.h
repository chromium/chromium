// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_LINE_BOX_FRAGMENT_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_LINE_BOX_FRAGMENT_BUILDER_H_

#include "third_party/blink/renderer/core/layout/geometry/logical_offset.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_height_metrics.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_container_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_container_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_positioned_float.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ComputedStyle;
class NGInlineBreakToken;

class CORE_EXPORT NGLineBoxFragmentBuilder final
    : public NGContainerFragmentBuilder {
  STACK_ALLOCATED();

 public:
  NGLineBoxFragmentBuilder(NGInlineNode node,
                           scoped_refptr<const ComputedStyle> style,
                           const NGConstraintSpace* space,
                           WritingMode writing_mode,
                           TextDirection)
      : NGContainerFragmentBuilder(node,
                                   style,
                                   space,
                                   writing_mode,
                                   TextDirection::kLtr),
        line_box_type_(NGPhysicalLineBoxFragment::kNormalLineBox),
        base_direction_(TextDirection::kLtr) {}

  void Reset();

  LayoutUnit LineHeight() const {
    return metrics_.LineHeight().ClampNegativeToZero();
  }

  void SetInlineSize(LayoutUnit inline_size) {
    size_.inline_size = inline_size;
  }

  void SetHangInlineSize(LayoutUnit hang_inline_size) {
    hang_inline_size_ = hang_inline_size;
  }

  // Mark this line box is an "empty" line box. See NGLineBoxType.
  void SetIsEmptyLineBox();

  const NGLineHeightMetrics& Metrics() const { return metrics_; }
  void SetMetrics(const NGLineHeightMetrics& metrics) { metrics_ = metrics; }

  void SetBaseDirection(TextDirection direction) {
    base_direction_ = direction;
  }

  // Set the break token for the fragment to build.
  // A finished break token will be attached if not set.
  void SetBreakToken(scoped_refptr<NGInlineBreakToken> break_token) {
    break_token_ = std::move(break_token);
  }

  // A data struct to keep NGLayoutResult or fragment until the box tree
  // structures and child offsets are finalized.
  struct Child {
    DISALLOW_NEW();

    scoped_refptr<const NGLayoutResult> layout_result;
    scoped_refptr<const NGPhysicalTextFragment> fragment;
    LayoutObject* out_of_flow_positioned_box = nullptr;
    LayoutObject* unpositioned_float = nullptr;
    // The offset of the border box, initially in this child coordinate system.
    // |ComputeInlinePositions()| converts it to the offset within the line box.
    LogicalOffset offset;
    // The offset of a positioned float wrt. the root BFC. This should only be
    // set for positioned floats.
    NGBfcOffset bfc_offset;
    // The inline size of the margin box.
    LayoutUnit inline_size;
    LayoutUnit margin_line_left;
    // The index of |box_data_list_|, used in |PrepareForReorder()| and
    // |UpdateAfterReorder()| to track children of boxes across BiDi reorder.
    unsigned box_data_index = 0;
    // For an inline box, shows the number of descendant |Child|ren, including
    // empty ones. Includes itself, so 1 means no descendants. 0 if not an
    // inline box. Available only after |CreateBoxFragments()|.
    unsigned children_count = 0;
    UBiDiLevel bidi_level = 0xff;
    // The current text direction for OOF positioned items.
    TextDirection container_direction = TextDirection::kLtr;

    // Empty constructor needed for |resize()|.
    Child() = default;
    // Create a placeholder. A placeholder does not have a fragment nor a bidi
    // level.
    Child(LogicalOffset offset) : offset(offset) {}
    // Crete a bidi control. A bidi control does not have a fragment, but has
    // bidi level and affects bidi reordering.
    Child(UBiDiLevel bidi_level) : bidi_level(bidi_level) {}
    // Create an in-flow |NGLayoutResult|.
    Child(scoped_refptr<const NGLayoutResult> layout_result,
          LogicalOffset offset,
          LayoutUnit inline_size,
          UBiDiLevel bidi_level)
        : layout_result(std::move(layout_result)),
          offset(offset),
          inline_size(inline_size),
          bidi_level(bidi_level) {}
    // Create an in-flow |NGPhysicalTextFragment|.
    Child(scoped_refptr<const NGPhysicalTextFragment> fragment,
          LogicalOffset offset,
          LayoutUnit inline_size,
          UBiDiLevel bidi_level)
        : fragment(std::move(fragment)),
          offset(offset),
          inline_size(inline_size),
          bidi_level(bidi_level) {}
    Child(scoped_refptr<const NGPhysicalTextFragment> fragment,
          LayoutUnit block_offset,
          LayoutUnit inline_size,
          UBiDiLevel bidi_level)
        : fragment(std::move(fragment)),
          offset({LayoutUnit(), block_offset}),
          inline_size(inline_size),
          bidi_level(bidi_level) {}
    // Create an out-of-flow positioned object.
    Child(LayoutObject* out_of_flow_positioned_box,
          UBiDiLevel bidi_level,
          TextDirection container_direction)
        : out_of_flow_positioned_box(out_of_flow_positioned_box),
          bidi_level(bidi_level),
          container_direction(container_direction) {}
    // Create an unpositioned float.
    Child(LayoutObject* unpositioned_float, UBiDiLevel bidi_level)
        : unpositioned_float(unpositioned_float), bidi_level(bidi_level) {}
    // Create a positioned float.
    Child(scoped_refptr<const NGLayoutResult> layout_result,
          NGBfcOffset bfc_offset,
          UBiDiLevel bidi_level)
        : layout_result(std::move(layout_result)),
          bfc_offset(bfc_offset),
          bidi_level(bidi_level) {}

    bool HasInFlowFragment() const {
      if (fragment)
        return true;

      if (layout_result && !layout_result->PhysicalFragment().IsFloating())
        return true;

      return false;
    }
    bool HasOutOfFlowFragment() const { return out_of_flow_positioned_box; }
    bool HasFragment() const {
      return HasInFlowFragment() || HasOutOfFlowFragment();
    }
    bool HasBidiLevel() const { return bidi_level != 0xff; }
    bool IsPlaceholder() const { return !HasFragment() && !HasBidiLevel(); }
    bool IsOpaqueToBidiReordering() const {
      if (IsPlaceholder())
        return true;
      // Skip all inline boxes. Fragments for inline boxes maybe created earlier
      // if they have no children.
      if (layout_result) {
        const LayoutObject* layout_object =
            layout_result->PhysicalFragment().GetLayoutObject();
        DCHECK(layout_object);
        if (layout_object->IsLayoutInline())
          return true;
      }
      return false;
    }
    const NGPhysicalFragment* PhysicalFragment() const {
      if (layout_result)
        return &layout_result->PhysicalFragment();
      return fragment.get();
    }
  };

  // A vector of Child.
  // Unlike the fragment builder, chlidren are mutable.
  // Callers can add to the fragment builder in a batch once finalized.
  class ChildList {
    STACK_ALLOCATED();

   public:
    ChildList() = default;
    void operator=(ChildList&& other) {
      children_ = std::move(other.children_);
    }

    Child& operator[](wtf_size_t i) { return children_[i]; }
    const Child& operator[](wtf_size_t i) const { return children_[i]; }

    wtf_size_t size() const { return children_.size(); }
    bool IsEmpty() const { return children_.IsEmpty(); }
    void ReserveInitialCapacity(unsigned capacity) {
      children_.ReserveInitialCapacity(capacity);
    }
    void clear() { children_.resize(0); }
    void resize(wtf_size_t size) { children_.resize(size); }

    using iterator = Vector<Child, 16>::iterator;
    iterator begin() { return children_.begin(); }
    iterator end() { return children_.end(); }
    using const_iterator = Vector<Child, 16>::const_iterator;
    const_iterator begin() const { return children_.begin(); }
    const_iterator end() const { return children_.end(); }
    using reverse_iterator = Vector<Child, 16>::reverse_iterator;
    reverse_iterator rbegin() { return children_.rbegin(); }
    reverse_iterator rend() { return children_.rend(); }
    using const_reverse_iterator = Vector<Child, 16>::const_reverse_iterator;
    const_reverse_iterator rbegin() const { return children_.rbegin(); }
    const_reverse_iterator rend() const { return children_.rend(); }

    Child* FirstInFlowChild();
    Child* LastInFlowChild();

    // Add a child. Accepts all constructor arguments for |Child|.
    template <class... Args>
    void AddChild(Args&&... args) {
      children_.emplace_back(std::forward<Args>(args)...);
    }
    void InsertChild(unsigned index);
    void InsertChild(unsigned index,
                     scoped_refptr<const NGLayoutResult> layout_result,
                     const LogicalOffset& offset,
                     LayoutUnit inline_size,
                     UBiDiLevel bidi_level) {
      children_.insert(index, Child{std::move(layout_result), offset,
                                    inline_size, bidi_level});
    }

    void MoveInInlineDirection(LayoutUnit);
    void MoveInInlineDirection(LayoutUnit, unsigned start, unsigned end);
    void MoveInBlockDirection(LayoutUnit);
    void MoveInBlockDirection(LayoutUnit, unsigned start, unsigned end);

   private:
    Vector<Child, 16> children_;
  };

  // Add all items in ChildList. Skips null Child if any.
  void AddChildren(ChildList&);

  // Propagate data in |ChildList| without adding them to this builder. When
  // adding children as fragment items, they appear in the container, but there
  // are some data that should be propagated through line box fragments.
  void PropagateChildrenData(ChildList&);

  // Creates the fragment. Can only be called once.
  scoped_refptr<const NGLayoutResult> ToLineBoxFragment();

 private:
  NGLineHeightMetrics metrics_;
  LayoutUnit hang_inline_size_;
  NGPhysicalLineBoxFragment::NGLineBoxType line_box_type_;
  TextDirection base_direction_;

  friend class NGLayoutResult;
  friend class NGPhysicalLineBoxFragment;

  DISALLOW_COPY_AND_ASSIGN(NGLineBoxFragmentBuilder);
};

}  // namespace blink

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(
    blink::NGLineBoxFragmentBuilder::Child)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_LINE_BOX_FRAGMENT_BUILDER_H_
