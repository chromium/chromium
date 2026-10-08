// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_AUTO_RESET_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_AUTO_RESET_H_

#include <utility>

#include "base/check.h"
#include "base/memory/stack_allocated.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"

namespace blink {

// A GC-safe variant of base::AutoReset that's intended for use with garbage
// collected types.
//
// blink::HeapAutoReset<> is useful for setting a pointer to a GCed objecte to a
// new value only within a particular scope. A blink::HeapAutoReset<> object
// resets a pointer to its original value upon destruction, making it an
// alternative to writing "var = old_val;" at all of a block's exit points.
//
// This should be obvious, but note that a blink::HeapAutoReset<> instance
// should have a shorter lifetime than its scoped_variable, to prevent invalid
// memory writes when the blink::HeapAutoReset<> object is destroyed.
template <typename T>
  requires IsGarbageCollectedTypeV<T>
class [[maybe_unused, nodiscard]] HeapAutoReset {
  STACK_ALLOCATED();

 public:
  HeapAutoReset(T** scoped_variable, T* new_value)
      : scoped_variable_(scoped_variable),
        original_value_(std::exchange(*scoped_variable_, new_value)) {
    DCHECK(scoped_variable);
  }
  ~HeapAutoReset() { *scoped_variable_ = original_value_; }

  // Prevent copying.
  HeapAutoReset(const HeapAutoReset&) = delete;
  HeapAutoReset& operator=(const HeapAutoReset&) = delete;

 private:
  T** const scoped_variable_;
  T* const original_value_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_AUTO_RESET_H_
