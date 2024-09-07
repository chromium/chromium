// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SERIALIZATION_V8_EXTERNAL_MEMORY_ACCOUNTER_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SERIALIZATION_V8_EXTERNAL_MEMORY_ACCOUNTER_H_

#include <stdlib.h>

#include "base/check_op.h"
#include "v8/include/v8-isolate.h"

namespace blink {

class V8ExternalMemoryAccounter {
 public:
  ~V8ExternalMemoryAccounter() { Unregister(); }

  void Register(size_t size) {
    CHECK_EQ(amount_of_external_memory_, 0u);
    v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
        static_cast<int64_t>(size));
    amount_of_external_memory_ = size;
  }

  void Unregister() {
    if (amount_of_external_memory_ > 0) {
      v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
          -static_cast<int64_t>(amount_of_external_memory_));
      amount_of_external_memory_ = 0;
    }
  }

 private:
  size_t amount_of_external_memory_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SERIALIZATION_V8_EXTERNAL_MEMORY_ACCOUNTER_H_
