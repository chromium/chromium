// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLineBoxFragmentBuilder_h
#define NGLineBoxFragmentBuilder_h

#include "third_party/blink/renderer/core/layout/ng/geometry/ng_logical_offset.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_height_metrics.h"
#include "third_party/blink/renderer/core/layout/ng/ng_container_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_positioned_float.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"

namespace blink {

class ComputedStyle;
class NGInlineBreakToken;
class NGPhysicalFragment;
struct NGPositionedFloat;

class CORE_EXPORT NGLineBoxFragmentBuilder final
    : public NGContainerFragmentBuilder {
  STACK_ALLOCATED();

 public:
  NGLineBoxFragmentBuilder(NGInlineNode node,
                           scoped_refptr<const ComputedStyle> style,
                           WritingMode writing_mode,
                           TextDirection)
      : NGContainerFragmentBuilder(style, writing_mode, TextDirection::kLtr),
        node_(node),
        base_direction_(TextDirection::kLtr) {}

  void Reset();

  LayoutUnit LineHeight() const {
    return metrics_.LineHeight().ClampNegativeToZero();
  }

  const NGLineHeightMetrics& Metrics() const { return metrics_; }
  void SetMetrics(const NGLineHeightMetrics& metrics) { metrics_ = metrics; }

  void SetBaseDirection(TextDirection direction) {
    base_direction_ = direction;
  }

  void SwapPositionedFloats(Vector<NGPositionedFloat>* positioned_floats) {
    positioned_floats_.swap(*positioned_floats);
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

    scoped_refptr<NGLayoutResult> layout_result;
    scoped_refptr<const NGPhysicalFragment> fragment;
    LayoutObject* out_of_flow_positioned_box = nullptr;
    LayoutObject* out_of_flow_containing_box = nullptr;
    NGLogicalOffset offset;
    LayoutUnit inline_size;
    unsigned box_data_index = 0;
    UBiDiLevel bidi_level = 0xff;

    // Empty constructor needed for |resize()|.
    Child() = default;
    // Create a placeholder. A placeholder does not have a fragment nor a bidi
    // level.
    Child(NGLogicalOffset offset) : offset(offset) {}
    // Crete a bidi control. A bidi control does not have a fragment, but has
    // bidi level and affects bidi reordering.
    Child(UBiDiLevel bidi_level) : bidi_level(bidi_level) {}
    // Create an in-flow |NGLayoutResult|.
    Child(scoped_refptr<NGLayoutResult> layout_result,
          NGLogicalOffset offset,
          LayoutUnit inline_size,
          UBiDiLevel bidi_level)
        : layout_result(std::move(layout_result)),
          offset(offset),
          inline_size(inline_size),
          bidi_level(bidi_level) {}
    // Create an in-flow |NGPhysicalFragment|.
    Child(scoped_refptr<const NGPhysicalFragment> fragment,
          NGLogicalOffset offset,
          LayoutUnit inline_size,
          UBiDiLevel bidi_level)
        : fragment(std::move(fragment)),
          offset(offset),
          inline_size(inline_size),
          bidi_level(bidi_level) {}
    Child(scoped_refptr<const NGPhysicalFragment> fragment,
          LayoutUnit block_offset,
          LayoutUnit inline_size,
          UBiDiLevel bidi_level)
        : fragment(std::move(fragment)),
          offset({LayoutUnit(), block_offset}),
          inline_size(inline_size),
          bidi_level(bidi_level) {}
    // Create an out-of-flow positioned object.
    Child(LayoutObject* out_of_flow_positioned_box,
          LayoutObject* out_of_flow_containing_box,
          UBiDiLevel bidi_level)
        : out_of_flow_positioned_box(out_of_flow_positioned_box),
          out_of_flow_containing_box(out_of_flow_containing_box),
          bidi_level(bidi_level) {}

    bool HasInFlowFragment() const { return layout_result || fragment; }
    bool HasOutOfFlowFragment() const { return out_of_flow_positioned_box; }
    bool HasFragment() const {
      return HasInFlowFragment() || HasOutOfFlowFragment();
    }
    bool HasBidiLevel() const { return bidi_level != 0xff; }
    bool IsPlaceholder() const { return !HasFragment() && !HasBidiLevel(); }
    const NGPhysicalFragment* PhysicalFragment() const {
      return layout_result ? layout_result->PhysicalFragment().get()
                           : fragment.get();
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
    void InsertChild(unsigned index,
                     scoped_refptr<NGLayoutResult> layout_result,
                     const NGLogicalOffset& offset,
                     LayoutUnit inline_size,
                     UBiDiLevel bidi_level) {
      children_.insert(index, Child{std::move(layout_result), offset,
                                    inline_size, bidi_level});
    }

    void MoveInInlineDirection(LayoutUnit, unsigned start, unsigned end);
    void MoveInBlockDirection(LayoutUnit);
    void MoveInBlockDirection(LayoutUnit, unsigned start, unsigned end);

   private:
    Vector<Child, 16> children_;
  };

  // Add all items in ChildList. Skips null Child if any.
  void AddChildren(ChildList&);

  // Creates the fragment. Can only be called once.
  scoped_refptr<NGLayoutResult> ToLineBoxFragment();

 private:
  NGInlineNode node_;

  NGLineHeightMetrics metrics_;
  Vector<NGPositionedFloat> positioned_floats_;

  TextDirection base_direction_;

  friend class NGLayoutResult;
  friend class NGPhysicalLineBoxFragment;

  DISALLOW_COPY_AND_ASSIGN(NGLineBoxFragmentBuilder);
};

}  // namespace blink

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(
    blink::NGLineBoxFragmentBuilder::Child);

#endif  // NGLineBoxFragmentBuilder
