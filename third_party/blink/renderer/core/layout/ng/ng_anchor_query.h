// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_ANCHOR_QUERY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_ANCHOR_QUERY_H_

#include "third_party/blink/renderer/core/core_export.h"

#include "third_party/blink/renderer/core/layout/geometry/logical_rect.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

struct NGPhysicalAnchorQuery {
  bool IsEmpty() const { return anchor_references.IsEmpty(); }

  HashMap<AtomicString, PhysicalRect> anchor_references;
};

struct NGLogicalAnchorQuery {
  bool IsEmpty() const { return anchor_references.IsEmpty(); }

  HashMap<AtomicString, LogicalRect> anchor_references;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_ANCHOR_QUERY_H_
