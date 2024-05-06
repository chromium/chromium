// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scroll/snap_event.h"

#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

SnapEvent* SnapEvent::Create(const AtomicString& type,
                             Bubbles bubbles,
                             Member<Node>& block_target,
                             Member<Node>& inline_target) {
  return MakeGarbageCollected<SnapEvent>(type, bubbles, block_target,
                                         inline_target);
}

SnapEvent::SnapEvent(const AtomicString& type,
                     Bubbles bubbles,
                     Member<Node>& block_target,
                     Member<Node>& inline_target)
    : Event(type, bubbles, Cancelable::kNo),
      snap_target_block_(block_target),
      snap_target_inline_(inline_target) {}

}  // namespace blink
