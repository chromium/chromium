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
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

class WritingModeConverter;

struct NGPhysicalAnchorQuery {
  bool IsEmpty() const { return anchor_references.IsEmpty(); }

  HashMap<AtomicString, PhysicalRect> anchor_references;
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

  HashMap<AtomicString, LogicalRect> anchor_references;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_ANCHOR_QUERY_H_
