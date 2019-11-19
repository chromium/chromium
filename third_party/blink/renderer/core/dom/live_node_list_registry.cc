// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/live_node_list_registry.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/live_node_list_base.h"

namespace blink {

static_assert(kNumNodeListInvalidationTypes <= sizeof(unsigned) * 8,
              "NodeListInvalidationType must fit in LiveNodeListRegistry bits");

void LiveNodeListRegistry::Add(const LiveNodeListBase* list,
                               NodeListInvalidationType type) {
  Entry entry = {list, MaskForInvalidationType(type)};
  DCHECK(std::find(data_.begin(), data_.end(), entry) == data_.end());
  data_.push_back(entry);
  mask_ |= entry.second;
}

void LiveNodeListRegistry::Remove(const LiveNodeListBase* list,
                                  NodeListInvalidationType type) {
  Entry entry = {list, MaskForInvalidationType(type)};
  auto* it = std::find(data_.begin(), data_.end(), entry);
  DCHECK(it != data_.end());
  data_.erase(it);
  data_.ShrinkToReasonableCapacity();
  RecomputeMask();
}

void LiveNodeListRegistry::Trace(Visitor* visitor) {
  visitor->RegisterWeakCallbackMethod<
      LiveNodeListRegistry, &LiveNodeListRegistry::ProcessCustomWeakness>(this);
}

void LiveNodeListRegistry::RecomputeMask() {
  unsigned mask = 0;
  for (const auto& entry : data_)
    mask |= entry.second;
  mask_ = mask;
}

void LiveNodeListRegistry::ProcessCustomWeakness(const WeakCallbackInfo& info) {
  auto* it = std::remove_if(data_.begin(), data_.end(), [info](Entry entry) {
    return !info.IsHeapObjectAlive(entry.first);
  });
  if (it == data_.end())
    return;

  data_.Shrink(static_cast<wtf_size_t>(it - data_.begin()));
  data_.ShrinkToReasonableCapacity();
  RecomputeMask();
}

}  // namespace blink
