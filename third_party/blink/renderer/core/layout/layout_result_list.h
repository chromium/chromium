// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_RESULT_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_RESULT_LIST_H_

#include "base/check.h"
#include "base/memory/stack_allocated.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

class LayoutResult;

// This is a specialized container type for LayoutResults.
//
// Most of the time we have a single LayoutResult.
// Storing within a HeapVector<Member<const LayoutResult>, 1> uses 20-bytes,
// where-as this class only uses 8-bytes (with pointer compression enabled).
class CORE_EXPORT LayoutResultList {
  DISALLOW_NEW();

 public:
  template <typename ListType, typename ValueType>
  class IteratorTemplate {
    STACK_ALLOCATED();

   public:
    constexpr IteratorTemplate() = default;
    IteratorTemplate(ListType* list, wtf_size_t index)
        : list_(list), index_(index) {}

    ValueType& operator*() const { return (*list_)[index_]; }
    ValueType* operator->() const { return &(*list_)[index_]; }

    IteratorTemplate& operator++() {
      ++index_;
      return *this;
    }

    bool operator==(const IteratorTemplate& other) const {
      return list_ == other.list_ && index_ == other.index_;
    }

   private:
    ListType* list_ = nullptr;
    wtf_size_t index_ = 0u;
  };

  using iterator =
      IteratorTemplate<LayoutResultList, Member<const LayoutResult>>;
  using const_iterator = IteratorTemplate<const LayoutResultList,
                                          const Member<const LayoutResult>>;

  LayoutResultList() = default;
  LayoutResultList(const LayoutResultList& other);
  LayoutResultList& operator=(const LayoutResultList& other);

  bool empty() const { return !head_; }
  wtf_size_t size() const {
    return (head_ ? 1u : 0u) + (tail_ ? tail_->size() : 0u);
  }

  const Member<const LayoutResult>& operator[](wtf_size_t i) const {
    if (i == 0u) {
      CHECK(head_);
      return head_;
    }
    return (*tail_)[i - 1u];
  }
  Member<const LayoutResult>& operator[](wtf_size_t i) {
    if (i == 0u) {
      CHECK(head_);
      return head_;
    }
    return (*tail_)[i - 1u];
  }

  const Member<const LayoutResult>& front() const {
    CHECK(head_);
    return head_;
  }
  const Member<const LayoutResult>& back() const {
    CHECK(head_);
    return tail_ ? tail_->back() : head_;
  }

  void push_back(const LayoutResult* result);
  void Shrink(wtf_size_t new_size);

  iterator begin() { return iterator(this, 0); }
  iterator end() { return iterator(this, size()); }
  const_iterator begin() const { return const_iterator(this, 0); }
  const_iterator end() const { return const_iterator(this, size()); }

  void Trace(Visitor* visitor) const;

 private:
  Member<const LayoutResult> head_;
  Member<GCedHeapVector<Member<const LayoutResult>>> tail_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_RESULT_LIST_H_
