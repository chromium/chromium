// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_ANCHOR_QUERY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_ANCHOR_QUERY_H_

#include "third_party/blink/renderer/core/core_export.h"

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_rect.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/platform/geometry/anchor_query_enums.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

class LayoutBox;
class LayoutObject;
class NGLogicalAnchorQuery;
class NGPhysicalFragment;
struct NGLogicalAnchorReference;
struct NGLogicalLink;
struct NGLogicalOOFNodeForFragmentation;

struct CORE_EXPORT NGPhysicalAnchorReference
    : public GarbageCollected<NGPhysicalAnchorReference> {
  NGPhysicalAnchorReference(const NGLogicalAnchorReference& logical_reference,
                            const WritingModeConverter& converter);

  void Trace(Visitor* visitor) const;

  PhysicalRect rect;
  Member<const NGPhysicalFragment> fragment;
  bool is_invalid = false;
};

class CORE_EXPORT NGPhysicalAnchorQuery {
  DISALLOW_NEW();

 public:
  bool IsEmpty() const { return anchor_references_.empty(); }

  const NGPhysicalAnchorReference* AnchorReference(
      const AtomicString& name) const;
  const PhysicalRect* Rect(const AtomicString& name) const;
  const NGPhysicalFragment* Fragment(const AtomicString& name) const;

  using NGPhysicalAnchorReferenceMap =
      HeapHashMap<AtomicString, Member<NGPhysicalAnchorReference>>;
  NGPhysicalAnchorReferenceMap::const_iterator begin() const {
    return anchor_references_.begin();
  }
  NGPhysicalAnchorReferenceMap::const_iterator end() const {
    return anchor_references_.end();
  }

  void SetFromLogical(const NGLogicalAnchorQuery& logical_query,
                      const WritingModeConverter& converter);

  void Trace(Visitor* visitor) const;

 private:
  friend class NGLogicalAnchorQuery;

  NGPhysicalAnchorReferenceMap anchor_references_;
};

struct CORE_EXPORT NGLogicalAnchorReference
    : public GarbageCollected<NGLogicalAnchorReference> {
  NGLogicalAnchorReference(const NGPhysicalFragment& fragment,
                           const LogicalRect& rect,
                           bool is_invalid)
      : rect(rect), fragment(&fragment), is_invalid(is_invalid) {}

  // Insert |this| into the given singly linked list in the pre-order.
  void InsertInPreOrderInto(Member<NGLogicalAnchorReference>* head_ptr);

  void Trace(Visitor* visitor) const;

  LogicalRect rect;
  Member<const NGPhysicalFragment> fragment;
  // A singly linked list in the order of the pre-order DFS.
  Member<NGLogicalAnchorReference> next;
  bool is_invalid = false;
};

class CORE_EXPORT NGLogicalAnchorQuery
    : public GarbageCollected<NGLogicalAnchorQuery> {
 public:
  // Returns an empty instance.
  static const NGLogicalAnchorQuery& Empty();

  bool IsEmpty() const { return anchor_references_.empty(); }

  const NGLogicalAnchorReference* AnchorReference(
      const AtomicString& name) const;
  const LogicalRect* Rect(const AtomicString& name) const;
  const NGPhysicalFragment* Fragment(const AtomicString& name) const;

  enum class SetOptions {
    // A valid entry. The call order is in the tree order.
    kValidInOrder,
    // A valid entry but the call order may not be in the tree order.
    kValidOutOfOrder,
    // An invalid entry.
    kInvalid,
  };
  void Set(const AtomicString& name,
           const NGPhysicalFragment& fragment,
           const LogicalRect& rect,
           SetOptions);
  void Set(const AtomicString& name,
           NGLogicalAnchorReference* reference,
           bool maybe_out_of_order = false);
  void SetFromPhysical(const NGPhysicalAnchorQuery& physical_query,
                       const WritingModeConverter& converter,
                       const LogicalOffset& additional_offset,
                       SetOptions);

  // Evaluate the |anchor_name| for the |anchor_value|. Returns |nullopt| if
  // the query is invalid (e.g., no targets or wrong axis.)
  absl::optional<LayoutUnit> EvaluateAnchor(
      const AtomicString& anchor_name,
      AnchorValue anchor_value,
      LayoutUnit available_size,
      const WritingModeConverter& container_converter,
      const PhysicalOffset& offset_to_padding_box,
      bool is_y_axis,
      bool is_right_or_bottom) const;
  absl::optional<LayoutUnit> EvaluateSize(const AtomicString& anchor_name,
                                          AnchorSizeValue anchor_size_value,
                                          WritingMode container_writing_mode,
                                          WritingMode self_writing_mode) const;

  void Trace(Visitor* visitor) const;

 private:
  friend class NGPhysicalAnchorQuery;

  HeapHashMap<AtomicString, Member<NGLogicalAnchorReference>>
      anchor_references_;
};

// This computes anchor queries for each containing block for when
// block-fragmented. When block-fragmented, all OOFs are added to their
// fragmentainers instead of their containing blocks, but anchor queries can be
// different for each containing block.
class CORE_EXPORT NGLogicalAnchorQueryForFragmentation {
  STACK_ALLOCATED();

 public:
  bool HasAnchorsOnOutOfFlowObjects() const { return has_anchors_on_oofs_; }
  bool ShouldLayoutByContainingBlock() const {
    return !queries_.empty() || has_anchors_on_oofs_;
  }

  // Get |NGLogicalAnchorQuery| in the stitched coordinate system for the given
  // containing block. If there is no anchor query for the containing block,
  // returns an empty instance.
  const NGLogicalAnchorQuery& StitchedAnchorQuery(
      const LayoutObject& containing_block) const;

  // Update the internal map of anchor queries for containing blocks from the
  // |children| of the fragmentation context.
  void Update(
      const base::span<const NGLogicalLink>& children,
      const base::span<const NGLogicalOOFNodeForFragmentation>& oof_nodes,
      const LayoutBox& root,
      WritingDirectionMode writing_direction);

 private:
  HeapHashMap<const LayoutObject*, Member<NGLogicalAnchorQuery>> queries_;
  bool has_anchors_on_oofs_ = false;
};

class CORE_EXPORT NGAnchorEvaluatorImpl : public Length::AnchorEvaluator {
  STACK_ALLOCATED();

 public:
  // An empty evaluator that always return `nullopt`. This instance can still
  // compute `HasAnchorFunctions()`.
  NGAnchorEvaluatorImpl() = default;

  NGAnchorEvaluatorImpl(const NGLogicalAnchorQuery& anchor_query,
                        const WritingModeConverter& container_converter,
                        const PhysicalOffset& offset_to_padding_box,
                        WritingMode self_writing_mode)
      : anchor_query_(&anchor_query),
        container_converter_(container_converter),
        offset_to_padding_box_(offset_to_padding_box),
        self_writing_mode_(self_writing_mode) {}

  // Returns true if this evaluator was invoked for `anchor()` or
  // `anchor-size()` functions.
  bool HasAnchorFunctions() const { return has_anchor_functions_; }

  // This must be set before evaluating `anchor()` function.
  void SetAxis(bool is_y_axis,
               bool is_right_or_bottom,
               LayoutUnit available_size) {
    available_size_ = available_size;
    is_y_axis_ = is_y_axis;
    is_right_or_bottom_ = is_right_or_bottom;
  }

  absl::optional<LayoutUnit> EvaluateAnchor(
      const AtomicString& anchor_name,
      AnchorValue anchor_value) const override;
  absl::optional<LayoutUnit> EvaluateAnchorSize(
      const AtomicString& anchor_name,
      AnchorSizeValue anchor_size_value) const override;

 private:
  const NGLogicalAnchorQuery* anchor_query_ = nullptr;
  const WritingModeConverter container_converter_{
      {WritingMode::kHorizontalTb, TextDirection::kLtr}};
  PhysicalOffset offset_to_padding_box_;
  WritingMode self_writing_mode_;
  LayoutUnit available_size_;
  bool is_y_axis_ = false;
  bool is_right_or_bottom_ = false;
  mutable bool has_anchor_functions_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_ANCHOR_QUERY_H_
