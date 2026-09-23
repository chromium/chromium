// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_result_list.h"

#include "base/check_op.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"

namespace blink {

LayoutResultList::LayoutResultList(const LayoutResultList& other)
    : head_(other.head_),
      tail_(other.tail_
                ? MakeGarbageCollected<
                      GCedHeapVector<Member<const LayoutResult>>>(*other.tail_)
                : nullptr) {}

LayoutResultList& LayoutResultList::operator=(const LayoutResultList& other) {
  head_ = other.head_;
  tail_ =
      other.tail_
          ? MakeGarbageCollected<GCedHeapVector<Member<const LayoutResult>>>(
                *other.tail_)
          : nullptr;
  return *this;
}

void LayoutResultList::push_back(const LayoutResult* result) {
  if (!head_) {
    DCHECK(!tail_);
    head_ = result;
    return;
  }
  if (!tail_) {
    tail_ = MakeGarbageCollected<GCedHeapVector<Member<const LayoutResult>>>();
  }
  tail_->push_back(result);
}

void LayoutResultList::Shrink(wtf_size_t new_size) {
  DCHECK_LE(new_size, size());
  if (new_size == 0u) {
    head_ = nullptr;
    tail_ = nullptr;
  } else if (new_size == 1u) {
    tail_ = nullptr;
  } else if (tail_) {
    tail_->Shrink(new_size - 1);
  }
}

void LayoutResultList::Trace(Visitor* visitor) const {
  visitor->Trace(head_);
  visitor->Trace(tail_);
}

}  // namespace blink
