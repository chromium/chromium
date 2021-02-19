// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_DEPTH_ORDERED_LAYOUT_OBJECT_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_DEPTH_ORDERED_LAYOUT_OBJECT_LIST_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class LayoutObject;

// Put data inside a forward-declared struct, to avoid including LayoutObject.h.
struct DepthOrderedLayoutObjectListData;

class DepthOrderedLayoutObjectList {
  DISALLOW_NEW();

 public:
  DepthOrderedLayoutObjectList();
  ~DepthOrderedLayoutObjectList();

  void Add(LayoutObject&);
  void Remove(LayoutObject&);
  void Clear();

  int size() const;
  bool IsEmpty() const;

  struct LayoutObjectWithDepth {
    LayoutObjectWithDepth(LayoutObject* in_object)
        : object(in_object), depth(DetermineDepth(in_object)) {}

    LayoutObjectWithDepth() : object(nullptr), depth(0) {}

    LayoutObject* object;
    unsigned depth;

    LayoutObject& operator*() const { return *object; }
    LayoutObject* operator->() const { return object; }

    bool operator<(const DepthOrderedLayoutObjectList::LayoutObjectWithDepth&
                       other) const {
      return depth > other.depth;
    }

   private:
    static unsigned DetermineDepth(LayoutObject*);
  };

  const HashSet<LayoutObject*>& Unordered() const;
  const Vector<LayoutObjectWithDepth>& Ordered();

 private:
  DepthOrderedLayoutObjectListData* data_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_DEPTH_ORDERED_LAYOUT_OBJECT_LIST_H_
