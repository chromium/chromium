// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_EXTERNAL_MEMORY_ACCOUNTER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_EXTERNAL_MEMORY_ACCOUNTER_H_

#include <stdlib.h>

#include <utility>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "v8/include/v8-isolate.h"

namespace blink {

class V8ExternalMemoryAccounterBase {
 public:
  V8ExternalMemoryAccounterBase() = default;

  V8ExternalMemoryAccounterBase(const V8ExternalMemoryAccounterBase&) = delete;
  V8ExternalMemoryAccounterBase(V8ExternalMemoryAccounterBase&& other) {
#if DCHECK_IS_ON()
    amount_of_external_memory_ =
        std::exchange(other.amount_of_external_memory_, 0U);
    isolate_ = std::exchange(other.isolate_, nullptr);
#endif
  }

  V8ExternalMemoryAccounterBase& operator=(
      const V8ExternalMemoryAccounterBase&) = delete;
  V8ExternalMemoryAccounterBase& operator=(
      V8ExternalMemoryAccounterBase&& other) {
#if DCHECK_IS_ON()
    if (this == &other) {
      return *this;
    }
    amount_of_external_memory_ =
        std::exchange(other.amount_of_external_memory_, 0U);
    isolate_ = std::exchange(other.isolate_, nullptr);
#endif
    return *this;
  }

  ~V8ExternalMemoryAccounterBase() {
#if DCHECK_IS_ON()
    DCHECK_EQ(amount_of_external_memory_, 0U);
#endif
  }

  void Increase(v8::Isolate* isolate, size_t size) {
#if DCHECK_IS_ON()
    DCHECK(isolate == isolate_ || isolate_ == nullptr);
    isolate_ = isolate;
    amount_of_external_memory_ += size;
#endif
    isolate->AdjustAmountOfExternalAllocatedMemory(static_cast<int64_t>(size));
  }

  void Update(v8::Isolate* isolate, int64_t delta) {
#if DCHECK_IS_ON()
    DCHECK(isolate == isolate_ || isolate_ == nullptr);
    DCHECK_GE(static_cast<int64_t>(amount_of_external_memory_), -delta);
    isolate_ = isolate;
    amount_of_external_memory_ += delta;
#endif
    isolate->AdjustAmountOfExternalAllocatedMemory(delta);
  }

  void Decrease(v8::Isolate* isolate, size_t size) const {
    if (size == 0) {
      return;
    }
#if DCHECK_IS_ON()
    DCHECK_EQ(isolate, isolate_);
    DCHECK_GE(amount_of_external_memory_, size);
    amount_of_external_memory_ -= size;
#endif
    isolate->AdjustAmountOfExternalAllocatedMemory(-static_cast<int64_t>(size));
  }

 private:
#if DCHECK_IS_ON()
  mutable size_t amount_of_external_memory_ = 0;
  // `isolate_` may become dangling, this is safe since it's only used for
  // checking passed `isolate`.
  raw_ptr<v8::Isolate, DisableDanglingPtrDetection> isolate_;
#endif
};

class V8ExternalMemoryAccounter {
 public:
  void Increase(v8::Isolate* isolate, size_t size) {
    amount_of_external_memory_ += size;
    memory_accounter_base_.Increase(isolate, size);
  }

  void Set(v8::Isolate* isolate, size_t size) {
    int64_t delta = size - amount_of_external_memory_;
    if (delta != 0) {
      amount_of_external_memory_ = size;
      memory_accounter_base_.Update(isolate, delta);
    }
  }

  void Clear(v8::Isolate* isolate) {
    if (amount_of_external_memory_ != 0) {
      memory_accounter_base_.Decrease(isolate, amount_of_external_memory_);
      amount_of_external_memory_ = 0;
    }
  }

 private:
  NO_UNIQUE_ADDRESS V8ExternalMemoryAccounterBase memory_accounter_base_;
  size_t amount_of_external_memory_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_EXTERNAL_MEMORY_ACCOUNTER_H_
