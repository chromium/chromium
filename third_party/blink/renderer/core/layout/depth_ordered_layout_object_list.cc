// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/depth_ordered_layout_object_list.h"

#include <algorithm>
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"

namespace blink {

class DepthOrderedLayoutObjectListData
    : public GarbageCollected<DepthOrderedLayoutObjectListData> {
 public:
  DepthOrderedLayoutObjectListData() = default;
  void Trace(Visitor* visitor) const {
    visitor->Trace(ordered_objects_);
    visitor->Trace(objects_);
  }

  HeapVector<DepthOrderedLayoutObjectList::LayoutObjectWithDepth>&
  ordered_objects() {
    return ordered_objects_;
  }
  HeapHashSet<Member<LayoutObject>>& objects() { return objects_; }

 private:
  // LayoutObjects sorted by depth (deepest first). This structure is only
  // populated at the beginning of enumerations. See ordered().
  HeapVector<DepthOrderedLayoutObjectList::LayoutObjectWithDepth>
      ordered_objects_;

  // Outside of layout, LayoutObjects can be added and removed as needed such
  // as when style was changed or destroyed. They're kept in this hashset to
  // keep those operations fast.
  HeapHashSet<Member<LayoutObject>> objects_;
};

DepthOrderedLayoutObjectList::DepthOrderedLayoutObjectList()
    : data_(MakeGarbageCollected<DepthOrderedLayoutObjectListData>()) {}

DepthOrderedLayoutObjectList::~DepthOrderedLayoutObjectList() = default;

int DepthOrderedLayoutObjectList::size() const {
  return data_->objects().size();
}

bool DepthOrderedLayoutObjectList::IsEmpty() const {
  return data_->objects().IsEmpty();
}

void DepthOrderedLayoutObjectList::Add(LayoutObject& object) {
  DCHECK(!object.GetFrameView()->IsInPerformLayout());
  data_->objects().insert(&object);
  data_->ordered_objects().clear();
}

void DepthOrderedLayoutObjectList::Remove(LayoutObject& object) {
  auto it = data_->objects().find(&object);
  if (it == data_->objects().end())
    return;
  DCHECK(!object.GetFrameView()->IsInPerformLayout());
  data_->objects().erase(it);
  data_->ordered_objects().clear();
}

void DepthOrderedLayoutObjectList::Clear() {
  data_->objects().clear();
  data_->ordered_objects().clear();
}

void DepthOrderedLayoutObjectList::LayoutObjectWithDepth::Trace(
    Visitor* visitor) const {
  visitor->Trace(object);
}

unsigned DepthOrderedLayoutObjectList::LayoutObjectWithDepth::DetermineDepth(
    LayoutObject* object) {
  unsigned depth = 1;
  for (LayoutObject* parent = object->Parent(); parent;
       parent = parent->Parent())
    ++depth;
  return depth;
}

const HeapHashSet<Member<LayoutObject>>&
DepthOrderedLayoutObjectList::Unordered() const {
  return data_->objects();
}

const HeapVector<DepthOrderedLayoutObjectList::LayoutObjectWithDepth>&
DepthOrderedLayoutObjectList::Ordered() {
  if (data_->objects().IsEmpty() || !data_->ordered_objects().IsEmpty())
    return data_->ordered_objects();

  data_->ordered_objects().clear();
  for (auto layout_object : data_->objects()) {
    data_->ordered_objects().push_back(
        DepthOrderedLayoutObjectList::LayoutObjectWithDepth(layout_object));
  }
  std::sort(data_->ordered_objects().begin(), data_->ordered_objects().end());
  return data_->ordered_objects();
}

void DepthOrderedLayoutObjectList::Trace(Visitor* visitor) const {
  visitor->Trace(data_);
}

}  // namespace blink
