// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scroll/snap_event.h"

#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

SnapEvent* SnapEvent::Create(const AtomicString& type,
                             HeapVector<Member<Node>>& targets) {
  return MakeGarbageCollected<SnapEvent>(type, targets);
}

SnapEvent::SnapEvent(const AtomicString& type,
                     HeapVector<Member<Node>>& targets)
    : Event(type, Bubbles::kNo, Cancelable::kNo),
      snap_targets_(StaticNodeList::Adopt(targets)) {}

}  // namespace blink
