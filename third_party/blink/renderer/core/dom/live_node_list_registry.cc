// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/live_node_list_registry.h"

#include "base/containers/contains.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/live_node_list_base.h"

namespace blink {

static_assert(kNumNodeListInvalidationTypes <= sizeof(unsigned) * 8,
              "NodeListInvalidationType must fit in LiveNodeListRegistry bits");

void LiveNodeListRegistry::Add(const LiveNodeListBase* list,
                               NodeListInvalidationType type) {
  Entry entry = {list, MaskForInvalidationType(type)};
  DCHECK(!base::Contains(data_, entry));
  data_.push_back(entry);
  mask_ |= entry.second;
}

void LiveNodeListRegistry::Remove(const LiveNodeListBase* list,
                                  NodeListInvalidationType type) {
  Entry entry = {list, MaskForInvalidationType(type)};
  auto it = base::ranges::find(data_, entry);
  CHECK(it != data_.end(), base::NotFatalUntil::M130);
  data_.erase(it);
  data_.ShrinkToReasonableCapacity();
  RecomputeMask();
}

void LiveNodeListRegistry::Trace(Visitor* visitor) const {
  visitor->RegisterWeakCallbackMethod<
      LiveNodeListRegistry, &LiveNodeListRegistry::ProcessCustomWeakness>(this);
}

void LiveNodeListRegistry::RecomputeMask() {
  unsigned mask = 0;
  for (const auto& entry : data_)
    mask |= entry.second;
  mask_ = mask;
}

void LiveNodeListRegistry::ProcessCustomWeakness(const LivenessBroker& info) {
  auto it = std::remove_if(data_.begin(), data_.end(), [info](Entry entry) {
    return !info.IsHeapObjectAlive(entry.first);
  });
  if (it == data_.end())
    return;

  data_.Shrink(static_cast<wtf_size_t>(it - data_.begin()));
  data_.ShrinkToReasonableCapacity();
  RecomputeMask();
}

}  // namespace blink
