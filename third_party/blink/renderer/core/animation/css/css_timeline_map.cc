// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css/css_timeline_map.h"

#include "third_party/blink/renderer/core/animation/deferred_timeline.h"

namespace blink {

void CSSDeferredTimelineMap::Trace(blink::Visitor* visitor) const {
  visitor->Trace(map_);
}

DeferredTimeline* CSSDeferredTimelineMap::Find(Document& document,
                                               const AtomicString& name) const {
  if (filter_.IsNone()) {
    return nullptr;
  }
  if (!filter_.Names().Contains(name) && !filter_.IsAll()) {
    return nullptr;
  }

  const InnerMap::AddResult& result = map_.insert(name, nullptr);

  if (result.is_new_entry) {
    result.stored_value->value =
        MakeGarbageCollected<DeferredTimeline>(&document);
  }

  return result.stored_value->value;
}

}  // namespace blink
