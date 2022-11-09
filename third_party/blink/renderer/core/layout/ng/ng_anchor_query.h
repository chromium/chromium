// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_ANCHOR_QUERY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_ANCHOR_QUERY_H_

#include "third_party/blink/renderer/core/core_export.h"

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/style/scoped_css_name.h"
#include "third_party/blink/renderer/platform/geometry/anchor_query_enums.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

class LayoutObject;
class NGLogicalAnchorQuery;
class NGLogicalAnchorQueryMap;
class NGPhysicalFragment;
class ScopedCSSName;
struct NGLogicalAnchorReference;

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
      const ScopedCSSName& name) const;
  const PhysicalRect* Rect(const ScopedCSSName& name) const;
  const NGPhysicalFragment* Fragment(const ScopedCSSName& name) const;

  using NGPhysicalAnchorReferenceMap =
      HeapHashMap<Member<const ScopedCSSName>,
                  Member<NGPhysicalAnchorReference>>;
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
      const ScopedCSSName& name) const;
  const LogicalRect* Rect(const ScopedCSSName& name) const;
  const NGPhysicalFragment* Fragment(const ScopedCSSName& name) const;

  enum class SetOptions {
    // A valid entry. The call order is in the tree order.
    kValidInOrder,
    // A valid entry but the call order may not be in the tree order.
    kValidOutOfOrder,
    // An invalid entry.
    kInvalid,
  };
  void Set(const ScopedCSSName& name,
           const NGPhysicalFragment& fragment,
           const LogicalRect& rect,
           SetOptions);
  void Set(const ScopedCSSName& name,
           NGLogicalAnchorReference* reference,
           bool maybe_out_of_order = false);
  void SetFromPhysical(const NGPhysicalAnchorQuery& physical_query,
                       const WritingModeConverter& converter,
                       const LogicalOffset& additional_offset,
                       SetOptions);

  // Evaluate the |anchor_name| for the |anchor_value|. Returns |nullopt| if
  // the query is invalid (e.g., no targets or wrong axis.)
  absl::optional<LayoutUnit> EvaluateAnchor(
      const ScopedCSSName& anchor_name,
      AnchorValue anchor_value,
      LayoutUnit available_size,
      const WritingModeConverter& container_converter,
      const PhysicalOffset& offset_to_padding_box,
      bool is_y_axis,
      bool is_right_or_bottom) const;
  absl::optional<LayoutUnit> EvaluateSize(const ScopedCSSName& anchor_name,
                                          AnchorSizeValue anchor_size_value,
                                          WritingMode container_writing_mode,
                                          WritingMode self_writing_mode) const;

  void Trace(Visitor* visitor) const;

 private:
  friend class NGPhysicalAnchorQuery;

  HeapHashMap<Member<const ScopedCSSName>, Member<NGLogicalAnchorReference>>
      anchor_references_;
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
        self_writing_mode_(self_writing_mode) {
    DCHECK(anchor_query_);
  }

  // This constructor takes |NGLogicalAnchorQueryMap| and |containing_block|
  // instead of |NGLogicalAnchorQuery|.
  NGAnchorEvaluatorImpl(const NGLogicalAnchorQueryMap& anchor_queries,
                        const LayoutObject& containing_block,
                        const WritingModeConverter& container_converter,
                        const PhysicalOffset& offset_to_padding_box,
                        WritingMode self_writing_mode)
      : anchor_queries_(&anchor_queries),
        containing_block_(&containing_block),
        container_converter_(container_converter),
        offset_to_padding_box_(offset_to_padding_box),
        self_writing_mode_(self_writing_mode) {
    DCHECK(anchor_queries_);
    DCHECK(containing_block_);
  }

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

  absl::optional<LayoutUnit> Evaluate(
      const CalculationExpressionNode&) const override;

 private:
  const NGLogicalAnchorQuery* AnchorQuery() const;

  absl::optional<LayoutUnit> EvaluateAnchor(const ScopedCSSName& anchor_name,
                                            AnchorValue anchor_value) const;
  absl::optional<LayoutUnit> EvaluateAnchorSize(
      const ScopedCSSName& anchor_name,
      AnchorSizeValue anchor_size_value) const;

  mutable const NGLogicalAnchorQuery* anchor_query_ = nullptr;
  const NGLogicalAnchorQueryMap* anchor_queries_ = nullptr;
  const LayoutObject* containing_block_ = nullptr;
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
