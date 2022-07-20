// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_ANCHOR_QUERY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_ANCHOR_QUERY_H_

#include "third_party/blink/renderer/core/core_export.h"

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_rect.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/platform/geometry/anchor_query_enums.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

class NGPhysicalFragment;
class WritingModeConverter;

struct NGPhysicalAnchorReference
    : public GarbageCollected<NGPhysicalAnchorReference> {
  NGPhysicalAnchorReference(PhysicalRect rect,
                            const NGPhysicalFragment* fragment)
      : rect(rect), fragment(fragment) {}

  PhysicalRect rect;
  Member<const NGPhysicalFragment> fragment;

  void Trace(Visitor* visitor) const;
};

struct NGPhysicalAnchorQuery {
  bool IsEmpty() const { return anchor_references.IsEmpty(); }

  HeapHashMap<AtomicString, Member<NGPhysicalAnchorReference>>
      anchor_references;

  void Trace(Visitor* visitor) const;
  DISALLOW_NEW();
};

struct NGLogicalAnchorReference {
  LogicalRect rect;
  const NGPhysicalFragment* fragment;

  STACK_ALLOCATED();
};

struct NGLogicalAnchorQuery {
  bool IsEmpty() const { return anchor_references.IsEmpty(); }

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

  HashMap<AtomicString, NGLogicalAnchorReference> anchor_references;

  STACK_ALLOCATED();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_ANCHOR_QUERY_H_
