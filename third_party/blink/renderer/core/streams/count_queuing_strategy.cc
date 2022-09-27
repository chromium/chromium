// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/count_queuing_strategy.h"

#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_queuing_strategy_init.h"
#include "third_party/blink/renderer/core/streams/queuing_strategy_common.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

namespace {

static const V8PrivateProperty::SymbolKey kCountQueuingStrategySizeFunction;

class CountQueuingStrategySizeFunction final : public ScriptFunction::Callable {
 public:
  static v8::Local<v8::Function> CreateFunction(ScriptState* script_state) {
    auto* self = MakeGarbageCollected<ScriptFunction>(
        script_state, MakeGarbageCollected<CountQueuingStrategySizeFunction>());

    // https://streams.spec.whatwg.org/#count-queuing-strategy-size-function
    // 2. Let F be ! CreateBuiltinFunction(steps, « », globalObject’s relevant
    //    Realm).
    // 4. Perform ! SetFunctionLength(F, 0).
    v8::Local<v8::Function> function = self->V8Function();

    // 3. Perform ! SetFunctionName(F, "size").
    function->SetName(V8String(script_state->GetIsolate(), "size"));

    return function;
  }

  CountQueuingStrategySizeFunction() = default;

  void CallRaw(ScriptState* script_state,
               const v8::FunctionCallbackInfo<v8::Value>& args) override {
    // https://streams.spec.whatwg.org/#count-queuing-strategy-size-function
    // 1. Let steps be the following steps:
    //   1. Return 1.
    args.GetReturnValue().Set(v8::Integer::New(script_state->GetIsolate(), 1));
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
    : high_water_mark_(init->highWaterMark()) {}

CountQueuingStrategy::~CountQueuingStrategy() = default;

ScriptValue CountQueuingStrategy::size(ScriptState* script_state) const {
  // https://streams.spec.whatwg.org/#count-queuing-strategy-size-function
  // 5. Set globalObject’s count queuing strategy size function to a Function
  //    that represents a reference to F, with callback context equal to
  //    globalObject’s relevant settings object.
  return GetCachedSizeFunction(
      script_state, kCountQueuingStrategySizeFunction,
      &CountQueuingStrategySizeFunction::CreateFunction);
}

}  // namespace blink
