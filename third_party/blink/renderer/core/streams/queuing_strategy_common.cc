// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/queuing_strategy_common.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

namespace {

class QueuingStrategySizeFunctionPerContextData final
    : public GarbageCollected<QueuingStrategySizeFunctionPerContextData>,
      public V8PerContextData::Data {
 public:
  v8::Local<v8::Function> GetOrCreate(ScriptState* script_state,
                                      QueuingStrategySizeFunctionType type,
                                      SizeFunctionFactory factory) {
    TraceWrapperV8Reference<v8::Function>& slot = SlotForType(type);
    v8::Isolate* const isolate = script_state->GetIsolate();
    if (slot.IsEmpty()) {
      const v8::Local<v8::Function> function = factory(script_state);
      slot.Reset(isolate, function);
      return function;
    }
    return slot.Get(isolate);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(byte_length_size_function_);
    visitor->Trace(count_size_function_);
    V8PerContextData::Data::Trace(visitor);
  }

 private:
  TraceWrapperV8Reference<v8::Function>& SlotForType(
      QueuingStrategySizeFunctionType type) {
    switch (type) {
      case QueuingStrategySizeFunctionType::kByteLength:
        return byte_length_size_function_;
      case QueuingStrategySizeFunctionType::kCount:
        return count_size_function_;
    }
    NOTREACHED();
  }

  TraceWrapperV8Reference<v8::Function> byte_length_size_function_;
  TraceWrapperV8Reference<v8::Function> count_size_function_;
};

}  // namespace

ScriptValue GetCachedSizeFunction(ScriptState* script_state,
                                  QueuingStrategySizeFunctionType type,
                                  SizeFunctionFactory factory) {
  v8::Isolate* const isolate = script_state->GetIsolate();
  V8PerContextData* const per_context_data = script_state->PerContextData();
  if (!per_context_data) {
    // When a context is detached, `ScriptState::DisposePerContextData()` clears
    // `PerContextData()` so per-context references do not keep detached realms
    // alive. Intentionally deviate slightly from the standard here by returning
    // a fresh function rather than caching on the detached context (matching
    // `V8DOMWrapper::CreateWrapper`).
    return ScriptValue(isolate, factory(script_state));
  }

  static constexpr char kPerContextDataKey[] = "QueuingStrategySizeFunction";
  auto* cache = static_cast<QueuingStrategySizeFunctionPerContextData*>(
      per_context_data->GetData(kPerContextDataKey));
  if (!cache) {
    cache = MakeGarbageCollected<QueuingStrategySizeFunctionPerContextData>();
    per_context_data->AddData(kPerContextDataKey, cache);
  }
  return ScriptValue(isolate, cache->GetOrCreate(script_state, type, factory));
}

}  // namespace blink
