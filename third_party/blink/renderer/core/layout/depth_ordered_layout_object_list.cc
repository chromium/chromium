// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/depth_ordered_layout_object_list.h"

#include <algorithm>
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/legacy_layout_tree_walking.h"

namespace blink {

class DepthOrderedLayoutObjectListData
    : public GarbageCollected<DepthOrderedLayoutObjectListData> {
 public:
  DepthOrderedLayoutObjectListData() = default;
  void Trace(Visitor* visitor) const {
    visitor->Trace(ordered_objects_);
    visitor->Trace(objects_);
  }

  HeapVector<LayoutObjectWithDepth>& ordered_objects() {
    return ordered_objects_;
  }
  HeapHashSet<Member<LayoutObject>>& objects() { return objects_; }

  // LayoutObjects sorted by depth (deepest first). This structure is only
  // populated at the beginning of enumerations. See ordered().
  HeapVector<LayoutObjectWithDepth> ordered_objects_;

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
  return data_->objects().empty();
}

namespace {

bool ListModificationAllowedFor(const LayoutObject& object) {
  if (!object.GetFrameView()->IsInPerformLayout())
    return true;
  // We are allowed to insert/remove orthogonal writing mode roots during
  // layout for interleaved style recalcs, but only when these roots are fully
  // managed by LayoutNG.
  return object.GetDocument().GetStyleEngine().InContainerQueryStyleRecalc();
}

}  // namespace

void DepthOrderedLayoutObjectList::Add(LayoutObject& object) {
  DCHECK(ListModificationAllowedFor(object));
  data_->objects().insert(&object);
  data_->ordered_objects().clear();
}

void DepthOrderedLayoutObjectList::Remove(LayoutObject& object) {
  auto it = data_->objects().find(&object);
  if (it == data_->objects().end())
    return;
  DCHECK(ListModificationAllowedFor(object));
  data_->objects().erase(it);
  data_->ordered_objects().clear();
}

void DepthOrderedLayoutObjectList::Clear() {
  data_->objects().clear();
  data_->ordered_objects().clear();
}

void LayoutObjectWithDepth::Trace(Visitor* visitor) const {
  visitor->Trace(object);
}

unsigned LayoutObjectWithDepth::DetermineDepth(LayoutObject* object) {
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

const HeapVector<LayoutObjectWithDepth>&
DepthOrderedLayoutObjectList::Ordered() {
  if (data_->objects_.empty() || !data_->ordered_objects_.empty())
    return data_->ordered_objects_;

  data_->ordered_objects_.assign(data_->objects_);
  std::sort(data_->ordered_objects_.begin(), data_->ordered_objects_.end());
  return data_->ordered_objects_;
}

void DepthOrderedLayoutObjectList::Trace(Visitor* visitor) const {
  visitor->Trace(data_);
}

}  // namespace blink
