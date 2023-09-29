// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_SCROLL_CUSTOMIZATION_CALLBACKS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_SCROLL_CUSTOMIZATION_CALLBACKS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class Node;
class ScrollStateCallback;

class CORE_EXPORT ScrollCustomizationCallbacks
    : public GarbageCollected<ScrollCustomizationCallbacks> {
 public:
  ScrollCustomizationCallbacks() = default;
  ScrollCustomizationCallbacks(const ScrollCustomizationCallbacks&) = delete;
  ScrollCustomizationCallbacks& operator=(const ScrollCustomizationCallbacks&) =
      delete;

  void SetDistributeScroll(Node*, ScrollStateCallback*);
  ScrollStateCallback* GetDistributeScroll(Node*);
  void SetApplyScroll(Node*, ScrollStateCallback*);
  void RemoveApplyScroll(Node*);
  ScrollStateCallback* GetApplyScroll(Node*);
  bool InScrollPhase(Node*) const;

  void Trace(Visitor* visitor) const {
    visitor->Trace(apply_scroll_callbacks_);
    visitor->Trace(distribute_scroll_callbacks_);
  }

 private:
  using ScrollStateCallbackList =
      HeapHashMap<WeakMember<Node>, Member<ScrollStateCallback>>;
  ScrollStateCallbackList apply_scroll_callbacks_;
  ScrollStateCallbackList distribute_scroll_callbacks_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_SCROLL_CUSTOMIZATION_CALLBACKS_H_
