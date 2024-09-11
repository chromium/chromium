// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_EXTERNAL_MEMORY_ACCOUNTER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_EXTERNAL_MEMORY_ACCOUNTER_H_

#include <stdlib.h>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "v8/include/v8-isolate.h"

namespace blink {

class V8ExternalMemoryAccounterBase {
 public:
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

  void Decrease(v8::Isolate* isolate, size_t size) {
#if DCHECK_IS_ON()
    DCHECK_EQ(isolate, isolate_);
    DCHECK_GE(amount_of_external_memory_, size);
    amount_of_external_memory_ -= size;
#endif
    isolate->AdjustAmountOfExternalAllocatedMemory(-static_cast<int64_t>(size));
  }

 private:
#if DCHECK_IS_ON()
  size_t amount_of_external_memory_ = 0;
  raw_ptr<v8::Isolate> isolate_;
#endif
};

class V8ExternalMemoryAccounter {
 public:
  void Increase(v8::Isolate* isolate, size_t size) {
    amount_of_external_memory_ += size;
    memory_accounter_base_.Increase(isolate, size);
  }

  void Clear(v8::Isolate* isolate) {
    if (amount_of_external_memory_ != 0) {
      memory_accounter_base_.Decrease(isolate, amount_of_external_memory_);
    }
    amount_of_external_memory_ = 0;
  }

 private:
  NO_UNIQUE_ADDRESS V8ExternalMemoryAccounterBase memory_accounter_base_;
  size_t amount_of_external_memory_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_EXTERNAL_MEMORY_ACCOUNTER_H_
