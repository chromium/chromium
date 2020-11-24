// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_MEASURE_MEMORY_MEASURE_MEMORY_DELEGATE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_MEASURE_MEMORY_MEASURE_MEMORY_DELEGATE_H_

#include "third_party/blink/renderer/platform/bindings/scoped_persistent.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "v8/include/v8.h"

namespace blink {

class MemoryBreakdownEntry;

// Specifies V8 contexts to be measured and invokes the given callback once V8
// completes the memory measurement.
class MeasureMemoryDelegate : public v8::MeasureMemoryDelegate {
 public:
  using ResultCallback =
      base::OnceCallback<void(HeapVector<Member<MemoryBreakdownEntry>>)>;

  MeasureMemoryDelegate(v8::Isolate* isolate,
                        v8::Local<v8::Context> context,
                        ResultCallback callback);

  // v8::MeasureMemoryDelegate overrides.
  bool ShouldMeasure(v8::Local<v8::Context> context) override;
  void MeasurementComplete(
      const std::vector<std::pair<v8::Local<v8::Context>, size_t>>&
          context_sizes,
      size_t unattributed_size) override;
 private:
  v8::Isolate* isolate_;
  ScopedPersistent<v8::Context> context_;
  ResultCallback callback_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_MEASURE_MEMORY_MEASURE_MEMORY_DELEGATE_H_
