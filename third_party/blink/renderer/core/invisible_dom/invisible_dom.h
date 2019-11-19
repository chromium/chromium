// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INVISIBLE_DOM_INVISIBLE_DOM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INVISIBLE_DOM_INVISIBLE_DOM_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Node;
class Element;

class CORE_EXPORT InvisibleDOM {
  STATIC_ONLY(InvisibleDOM);

 public:
  static bool IsInsideInvisibleSubtree(const Node& node);

  // Highest inclusive ancestor that has the invisible attribute.
  // Will always return non-null value if IsInsideInvisibleSubtree() is true.
  static Element* InvisibleRoot(const Node&);

  // Activates all the nodes within |range|. Returns true if at least one
  // node gets activated.
  static bool ActivateRangeIfNeeded(const EphemeralRangeInFlatTree& range);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INVISIBLE_DOM_INVISIBLE_DOM_H_
