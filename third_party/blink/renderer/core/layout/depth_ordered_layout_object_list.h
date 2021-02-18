// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_DEPTH_ORDERED_LAYOUT_OBJECT_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_DEPTH_ORDERED_LAYOUT_OBJECT_LIST_H_

#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/vector_traits.h"

namespace blink {

class LayoutObject;

// Put data inside a forward-declared struct, to avoid including LayoutObject.h.
class DepthOrderedLayoutObjectListData;

class DepthOrderedLayoutObjectList {
  DISALLOW_NEW();

 public:
  DepthOrderedLayoutObjectList();
  ~DepthOrderedLayoutObjectList();
  void Trace(Visitor*) const;

  void Add(LayoutObject&);
  void Remove(LayoutObject&);
  void Clear();

  int size() const;
  bool IsEmpty() const;

  struct LayoutObjectWithDepth {
    DISALLOW_NEW();
    explicit LayoutObjectWithDepth(LayoutObject* in_object)
        : object(in_object), depth(DetermineDepth(in_object)) {}

    LayoutObjectWithDepth() : object(nullptr) {}

    void Trace(Visitor*) const;

    Member<LayoutObject> object;
    unsigned depth = 0;

    LayoutObject& operator*() const { return *object; }
    LayoutObject* operator->() const { return object; }

    bool operator<(const DepthOrderedLayoutObjectList::LayoutObjectWithDepth&
                       other) const {
      return depth > other.depth;
    }

   private:
    static unsigned DetermineDepth(LayoutObject*);
  };

  const HeapHashSet<Member<LayoutObject>>& Unordered() const;
  const HeapVector<LayoutObjectWithDepth>& Ordered();

 private:
  Member<DepthOrderedLayoutObjectListData> data_;
};

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(
    blink::DepthOrderedLayoutObjectList::LayoutObjectWithDepth)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_DEPTH_ORDERED_LAYOUT_OBJECT_LIST_H_
