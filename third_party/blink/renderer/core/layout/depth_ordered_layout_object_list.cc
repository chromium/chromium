// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/depth_ordered_layout_object_list.h"

#include <algorithm>
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/ng/legacy_layout_tree_walking.h"

namespace blink {

struct DepthOrderedLayoutObjectListData {
  Vector<LayoutObjectWithDepth>& ordered_objects() { return ordered_objects_; }
  HashSet<LayoutObject*>& objects() { return objects_; }

  // LayoutObjects sorted by depth (deepest first). This structure is only
  // populated at the beginning of enumerations. See ordered().
  Vector<LayoutObjectWithDepth> ordered_objects_;

  // Outside of layout, LayoutObjects can be added and removed as needed such
  // as when style was changed or destroyed. They're kept in this hashset to
  // keep those operations fast.
  HashSet<LayoutObject*> objects_;
};

DepthOrderedLayoutObjectList::DepthOrderedLayoutObjectList()
    : data_(new DepthOrderedLayoutObjectListData) {}

DepthOrderedLayoutObjectList::~DepthOrderedLayoutObjectList() {
  delete data_;
}

int DepthOrderedLayoutObjectList::size() const {
  return data_->objects_.size();
}

bool DepthOrderedLayoutObjectList::IsEmpty() const {
  return data_->objects_.IsEmpty();
}

namespace {

bool ListModificationAllowedFor(const LayoutObject& object) {
  if (!object.GetFrameView()->IsInPerformLayout())
    return true;
  // We are allowed to insert/remove orthogonal writing mode roots during
  // layout for interleaved style recalcs, but only when these roots are fully
  // managed by LayoutNG.
  return object.GetDocument().GetStyleEngine().InContainerQueryStyleRecalc() &&
         IsManagedByLayoutNG(object);
}

}  // namespace

void DepthOrderedLayoutObjectList::Add(LayoutObject& object) {
  DCHECK(ListModificationAllowedFor(object));
  data_->objects().insert(&object);
  data_->ordered_objects().clear();
}

void DepthOrderedLayoutObjectList::Remove(LayoutObject& object) {
  auto it = data_->objects_.find(&object);
  if (it == data_->objects_.end())
    return;
  DCHECK(ListModificationAllowedFor(object));
  data_->objects().erase(it);
  data_->ordered_objects().clear();
}

void DepthOrderedLayoutObjectList::Clear() {
  data_->objects_.clear();
  data_->ordered_objects_.clear();
}

unsigned LayoutObjectWithDepth::DetermineDepth(LayoutObject* object) {
  unsigned depth = 1;
  for (LayoutObject* parent = object->Parent(); parent;
       parent = parent->Parent())
    ++depth;
  return depth;
}

const HashSet<LayoutObject*>& DepthOrderedLayoutObjectList::Unordered() const {
  return data_->objects_;
}

const Vector<LayoutObjectWithDepth>& DepthOrderedLayoutObjectList::Ordered() {
  if (data_->objects_.IsEmpty() || !data_->ordered_objects_.IsEmpty())
    return data_->ordered_objects_;

  CopyToVector(data_->objects_, data_->ordered_objects_);
  std::sort(data_->ordered_objects_.begin(), data_->ordered_objects_.end());
  return data_->ordered_objects_;
}

}  // namespace blink
