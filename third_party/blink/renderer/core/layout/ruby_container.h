// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_RUBY_CONTAINER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_RUBY_CONTAINER_H_

#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class LayoutBoxModelObject;
class LayoutObject;

// RubyContainer is a helper for LayoutRuby.
class RubyContainer : public GarbageCollected<RubyContainer> {
 public:
  explicit RubyContainer(LayoutBoxModelObject& ruby);
  void Trace(Visitor* visitor) const;

  void AddChild(LayoutObject* child, LayoutObject* before_child);
  void DidRemoveChildFromColumn(LayoutObject& child);

 private:
  // Returns `true` if we need to call Repair().
  bool InsertChildAt(LayoutObject* child, wtf_size_t index);
  void Repair();
  void MergeAnonymousBases(wtf_size_t index);

  Member<LayoutBoxModelObject> ruby_object_;

  // This list contains ruby base boxes and ruby annotation boxes, and
  // represents children of `ruby_object_` in the document order.
  // Children with neither display:ruby-base nor display:ruby-text are wrapped
  // by anonymous ruby base boxes.
  HeapVector<Member<LayoutObject>> content_list_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_RUBY_CONTAINER_H_
