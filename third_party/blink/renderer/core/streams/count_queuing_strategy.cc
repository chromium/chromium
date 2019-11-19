// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/count_queuing_strategy.h"

#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/streams/queuing_strategy_common.h"
#include "third_party/blink/renderer/core/streams/queuing_strategy_init.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

namespace {

class CountQueuingStrategySizeFunction final : public ScriptFunction {
 public:
  static v8::Local<v8::Function> CreateFunction(ScriptState* script_state) {
    CountQueuingStrategySizeFunction* self =
        MakeGarbageCollected<CountQueuingStrategySizeFunction>(script_state);
    return self->BindToV8Function();
  }

  explicit CountQueuingStrategySizeFunction(ScriptState* script_state)
      : ScriptFunction(script_state) {}

 private:
  void CallRaw(const v8::FunctionCallbackInfo<v8::Value>& args) override {
    // https://streams.spec.whatwg.org/#cqs-size
    // 1. Return 1.
    args.GetReturnValue().Set(
        v8::Integer::New(GetScriptState()->GetIsolate(), 1));
  }
};

}  // namespace

CountQueuingStrategy* CountQueuingStrategy::Create(
    ScriptState* script_state,
    const QueuingStrategyInit* init) {
  return MakeGarbageCollected<CountQueuingStrategy>(script_state, init);
}

CountQueuingStrategy::CountQueuingStrategy(ScriptState* script_state,
                                           const QueuingStrategyInit* init)
    : high_water_mark_(script_state->GetIsolate(),
                       HighWaterMarkOrUndefined(script_state, init)) {}

CountQueuingStrategy::~CountQueuingStrategy() = default;

ScriptValue CountQueuingStrategy::highWaterMark(
    ScriptState* script_state) const {
  return ScriptValue(script_state->GetIsolate(),
                     high_water_mark_.NewLocal(script_state->GetIsolate()));
}

ScriptValue CountQueuingStrategy::size(ScriptState* script_state) const {
  // We don't cache the result because normally this method will only be called
  // once anyway.
  return ScriptValue(
      script_state->GetIsolate(),
      CountQueuingStrategySizeFunction::CreateFunction(script_state));
}

void CountQueuingStrategy::Trace(Visitor* visitor) {
  visitor->Trace(high_water_mark_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
