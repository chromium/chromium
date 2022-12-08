// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_ANCHOR_QUERY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_ANCHOR_QUERY_H_

#include "third_party/blink/renderer/core/core_export.h"

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
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
struct NGLogicalAnchorReference;

using NGAnchorKey = absl::variant<const ScopedCSSName*, const LayoutObject*>;

// This class is conceptually a concatenation of two hash maps with different
// key types but the same value type. To save memory, we don't implement it as
// one hash map with a unified key type; Otherwise, the size of each key will be
// increased by at least one pointer, which is undesired.
template <typename NGAnchorReference>
class NGAnchorQueryBase : public GarbageCollectedMixin {
  using NamedAnchorMap =
      HeapHashMap<Member<const ScopedCSSName>, Member<NGAnchorReference>>;
  using ImplicitAnchorMap =
      HeapHashMap<Member<const LayoutObject>, Member<NGAnchorReference>>;

 public:
  bool IsEmpty() const {
    return named_anchors_.empty() && implicit_anchors_.empty();
  }

  const NGAnchorReference* AnchorReference(const NGAnchorKey& key) const {
    if (const ScopedCSSName* const* name =
            absl::get_if<const ScopedCSSName*>(&key)) {
      return AnchorReference(named_anchors_, *name);
    }
    return AnchorReference(implicit_anchors_,
                           absl::get<const LayoutObject*>(key));
  }

  struct AddResult {
    Member<NGAnchorReference>* stored_value;
    bool is_new_entry;
    STACK_ALLOCATED();
  };
  AddResult insert(const NGAnchorKey& key, NGAnchorReference* reference) {
    if (const ScopedCSSName* const* name =
            absl::get_if<const ScopedCSSName*>(&key)) {
      return insert(named_anchors_, *name, reference);
    }
    return insert(implicit_anchors_, absl::get<const LayoutObject*>(key),
                  reference);
  }

  class Iterator {
    STACK_ALLOCATED();

    using NamedAnchorMap = typename NGAnchorQueryBase::NamedAnchorMap;
    using ImplicitAnchorMap = typename NGAnchorQueryBase::ImplicitAnchorMap;

   public:
    Iterator(const NGAnchorQueryBase* anchor_query,
             typename NamedAnchorMap::const_iterator named_map_iterator,
             typename ImplicitAnchorMap::const_iterator implicit_map_iterator)
        : anchor_query_(anchor_query),
          named_map_iterator_(named_map_iterator),
          implicit_map_iterator_(implicit_map_iterator) {}

    struct Entry {
      NGAnchorKey key;
      NGAnchorReference* value;
      STACK_ALLOCATED();
    };
    Entry operator*() const {
      if (named_map_iterator_ != anchor_query_->named_anchors_.end())
        return Entry{named_map_iterator_->key, named_map_iterator_->value};
      return Entry{implicit_map_iterator_->key, implicit_map_iterator_->value};
    }

    bool operator==(const Iterator& other) const {
      return named_map_iterator_ == other.named_map_iterator_ &&
             implicit_map_iterator_ == other.implicit_map_iterator_;
    }
    bool operator!=(const Iterator& other) const { return !operator==(other); }

    Iterator& operator++() {
      if (named_map_iterator_ != anchor_query_->named_anchors_.end())
        ++named_map_iterator_;
      else
        ++implicit_map_iterator_;
      return *this;
    }

   private:
    const NGAnchorQueryBase* anchor_query_;
    typename NamedAnchorMap::const_iterator named_map_iterator_;
    typename ImplicitAnchorMap::const_iterator implicit_map_iterator_;
  };
  Iterator begin() const {
    return Iterator{this, named_anchors_.begin(), implicit_anchors_.begin()};
  }
  Iterator end() const {
    return Iterator{this, named_anchors_.end(), implicit_anchors_.end()};
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(named_anchors_);
    visitor->Trace(implicit_anchors_);
  }

 private:
  friend class Iterator;

  template <typename AnchorMapType, typename KeyType>
  static const NGAnchorReference* AnchorReference(const AnchorMapType& anchors,
                                                  const KeyType& key) {
    auto it = anchors.find(key);
    return it != anchors.end() ? it->value : nullptr;
  }

  template <typename AnchorMapType, typename KeyType>
  static AddResult insert(AnchorMapType& anchors,
                          const KeyType& key,
                          NGAnchorReference* reference) {
    auto result = anchors.insert(key, reference);
    return AddResult{&result.stored_value->value, result.is_new_entry};
  }

  NamedAnchorMap named_anchors_;
  ImplicitAnchorMap implicit_anchors_;
};

struct CORE_EXPORT NGPhysicalAnchorReference
    : public GarbageCollected<NGPhysicalAnchorReference> {
  NGPhysicalAnchorReference(const NGLogicalAnchorReference& logical_reference,
                            const WritingModeConverter& converter);

  void Trace(Visitor* visitor) const;

  PhysicalRect rect;
  Member<const NGPhysicalFragment> fragment;
  bool is_invalid = false;
};

class CORE_EXPORT NGPhysicalAnchorQuery
    : public NGAnchorQueryBase<NGPhysicalAnchorReference> {
  DISALLOW_NEW();

 public:
  using Base = NGAnchorQueryBase<NGPhysicalAnchorReference>;

  const NGPhysicalAnchorReference* AnchorReference(
      const NGAnchorKey&,
      bool can_use_invalid_anchors) const;
  const NGPhysicalFragment* Fragment(const NGAnchorKey&,
                                     bool can_use_invalid_anchors) const;

  void SetFromLogical(const NGLogicalAnchorQuery& logical_query,
                      const WritingModeConverter& converter);
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
    : public GarbageCollected<NGLogicalAnchorQuery>,
      public NGAnchorQueryBase<NGLogicalAnchorReference> {
 public:
  using Base = NGAnchorQueryBase<NGLogicalAnchorReference>;

  // Returns an empty instance.
  static const NGLogicalAnchorQuery& Empty();

  const NGLogicalAnchorReference* AnchorReference(
      const NGAnchorKey&,
      bool can_use_invalid_anchor) const;

  enum class SetOptions {
    // A valid entry. The call order is in the tree order.
    kValidInOrder,
    // A valid entry but the call order may not be in the tree order.
    kValidOutOfOrder,
    // An invalid entry.
    kInvalid,
  };
  void Set(const NGAnchorKey&,
           const NGPhysicalFragment& fragment,
           const LogicalRect& rect,
           SetOptions);
  void Set(const NGAnchorKey&,
           NGLogicalAnchorReference* reference,
           bool maybe_out_of_order = false);
  void SetFromPhysical(const NGPhysicalAnchorQuery& physical_query,
                       const WritingModeConverter& converter,
                       const LogicalOffset& additional_offset,
                       SetOptions);

  // Evaluate the |anchor_value| for the given reference. Returns |nullopt| if
  // the query is invalid (due to wrong axis).
  absl::optional<LayoutUnit> EvaluateAnchor(
      const NGLogicalAnchorReference& reference,
      AnchorValue anchor_value,
      float percentage,
      LayoutUnit available_size,
      const WritingModeConverter& container_converter,
      WritingDirectionMode self_writing_direction,
      const PhysicalOffset& offset_to_padding_box,
      bool is_y_axis,
      bool is_right_or_bottom) const;
  LayoutUnit EvaluateSize(const NGLogicalAnchorReference& reference,
                          AnchorSizeValue anchor_size_value,
                          WritingMode container_writing_mode,
                          WritingMode self_writing_mode) const;
};

class CORE_EXPORT NGAnchorEvaluatorImpl : public Length::AnchorEvaluator {
  STACK_ALLOCATED();

 public:
  // An empty evaluator that always return `nullopt`. This instance can still
  // compute `HasAnchorFunctions()`.
  NGAnchorEvaluatorImpl() = default;

  NGAnchorEvaluatorImpl(const NGLogicalAnchorQuery& anchor_query,
                        const LayoutObject* implicit_anchor,
                        const WritingModeConverter& container_converter,
                        WritingDirectionMode self_writing_direction,
                        const PhysicalOffset& offset_to_padding_box,
                        bool is_in_top_layer)
      : anchor_query_(&anchor_query),
        implicit_anchor_(implicit_anchor),
        container_converter_(container_converter),
        self_writing_direction_(self_writing_direction),
        offset_to_padding_box_(offset_to_padding_box),
        is_in_top_layer_(is_in_top_layer) {
    DCHECK(anchor_query_);
  }

  // This constructor takes |NGLogicalAnchorQueryMap| and |containing_block|
  // instead of |NGLogicalAnchorQuery|.
  NGAnchorEvaluatorImpl(const NGLogicalAnchorQueryMap& anchor_queries,
                        const LayoutObject* implicit_anchor,
                        const LayoutObject& containing_block,
                        const WritingModeConverter& container_converter,
                        WritingDirectionMode self_writing_direction,
                        const PhysicalOffset& offset_to_padding_box,
                        bool is_in_top_layer)
      : anchor_queries_(&anchor_queries),
        implicit_anchor_(implicit_anchor),
        containing_block_(&containing_block),
        container_converter_(container_converter),
        self_writing_direction_(self_writing_direction),
        offset_to_padding_box_(offset_to_padding_box),
        is_in_top_layer_(is_in_top_layer) {
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

  // Evaluates the given anchor query. Returns nullopt if the query invalid
  // (e.g., no target or wrong axis).
  absl::optional<LayoutUnit> Evaluate(
      const CalculationExpressionNode&) const override;

 private:
  const NGLogicalAnchorQuery* AnchorQuery() const;

  absl::optional<LayoutUnit> EvaluateAnchor(const ScopedCSSName* anchor_name,
                                            AnchorValue anchor_value,
                                            float percentage) const;
  absl::optional<LayoutUnit> EvaluateAnchorSize(
      const ScopedCSSName* anchor_name,
      AnchorSizeValue anchor_size_value) const;

  mutable const NGLogicalAnchorQuery* anchor_query_ = nullptr;
  const NGLogicalAnchorQueryMap* anchor_queries_ = nullptr;
  const LayoutObject* implicit_anchor_ = nullptr;
  const LayoutObject* containing_block_ = nullptr;
  const WritingModeConverter container_converter_{
      {WritingMode::kHorizontalTb, TextDirection::kLtr}};
  WritingDirectionMode self_writing_direction_{WritingMode::kHorizontalTb,
                                               TextDirection::kLtr};
  PhysicalOffset offset_to_padding_box_;
  LayoutUnit available_size_;
  bool is_y_axis_ = false;
  bool is_right_or_bottom_ = false;
  bool is_in_top_layer_ = false;
  mutable bool has_anchor_functions_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_ANCHOR_QUERY_H_
