// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_ANCHOR_QUERY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_ANCHOR_QUERY_H_

#include "third_party/blink/renderer/core/core_export.h"

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_rect.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/platform/geometry/anchor_query_enums.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

class NGLogicalAnchorQuery;
class NGPhysicalFragment;
class WritingModeConverter;

struct CORE_EXPORT NGPhysicalAnchorReference
    : public GarbageCollected<NGPhysicalAnchorReference> {
  NGPhysicalAnchorReference(PhysicalRect rect,
                            const NGPhysicalFragment* fragment)
      : rect(rect), fragment(fragment) {}

  void Trace(Visitor* visitor) const;

  PhysicalRect rect;
  Member<const NGPhysicalFragment> fragment;
};

class CORE_EXPORT NGPhysicalAnchorQuery {
  DISALLOW_NEW();

 public:
  bool IsEmpty() const { return anchor_references_.IsEmpty(); }

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

struct CORE_EXPORT NGLogicalAnchorReference {
  STACK_ALLOCATED();

 public:
  LogicalRect rect;
  const NGPhysicalFragment* fragment;
};

class CORE_EXPORT NGLogicalAnchorQuery {
  STACK_ALLOCATED();

 public:
  bool IsEmpty() const { return anchor_references_.IsEmpty(); }

  const NGLogicalAnchorReference* AnchorReference(
      const AtomicString& name) const;
  const LogicalRect* Rect(const AtomicString& name) const;
  const NGPhysicalFragment* Fragment(const AtomicString& name) const;

  void Set(const AtomicString& name, const NGLogicalAnchorReference& reference);
  void SetFromPhysical(const NGPhysicalAnchorQuery& physical_query,
                       const WritingModeConverter& converter,
                       const LogicalOffset& additional_offset);

  // Evaluate the |anchor_name| for the |anchor_value|. Returns |nullopt| if
  // the query is invalid (e.g., no targets or wrong axis.)
  absl::optional<LayoutUnit> EvaluateAnchor(
      const AtomicString& anchor_name,
      AnchorValue anchor_value,
      LayoutUnit available_size,
      const WritingModeConverter& container_converter,
      bool is_y_axis,
      bool is_right_or_bottom) const;
  absl::optional<LayoutUnit> EvaluateSize(const AtomicString& anchor_name,
                                          AnchorSizeValue anchor_size_value,
                                          WritingMode container_writing_mode,
                                          WritingMode self_writing_mode) const;

 private:
  friend class NGPhysicalAnchorQuery;

  HashMap<AtomicString, NGLogicalAnchorReference> anchor_references_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_ANCHOR_QUERY_H_
