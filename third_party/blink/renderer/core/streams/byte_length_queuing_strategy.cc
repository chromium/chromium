// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/byte_length_queuing_strategy.h"

#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_queuing_strategy_init.h"
#include "third_party/blink/renderer/core/streams/queuing_strategy_common.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

namespace {

static const V8PrivateProperty::SymbolKey
    kByteLengthQueuingStrategySizeFunction;

class ByteLengthQueuingStrategySizeFunction final
    : public ScriptFunction::Callable {
 public:
  static v8::Local<v8::Function> CreateFunction(ScriptState* script_state) {
    auto* self = MakeGarbageCollected<ScriptFunction>(
        script_state,
        MakeGarbageCollected<ByteLengthQueuingStrategySizeFunction>());

    // https://streams.spec.whatwg.org/#byte-length-queuing-strategy-size-function

    // 2. Let F be ! CreateBuiltinFunction(steps, « », globalObject’s relevant
    //    Realm).
    // 4. Perform ! SetFunctionLength(F, 1).
    v8::Local<v8::Function> function = self->V8Function();

    // 3. Perform ! SetFunctionName(F, "size").
    function->SetName(V8String(script_state->GetIsolate(), "size"));

    return function;
  }

  ByteLengthQueuingStrategySizeFunction() = default;

  void CallRaw(ScriptState* script_state,
               const v8::FunctionCallbackInfo<v8::Value>& args) override {
    auto* isolate = args.GetIsolate();
    DCHECK_EQ(isolate, script_state->GetIsolate());
    auto context = script_state->GetContext();
    v8::Local<v8::Value> chunk;
    if (args.Length() < 1) {
      chunk = v8::Undefined(isolate);
    } else {
      chunk = args[0];
    }

    // https://streams.spec.whatwg.org/#byte-length-queuing-strategy-size-function

    // 1. Let steps be the following steps, given chunk:
    //   1. Return ? GetV(chunk, "byteLength").

    // https://tc39.es/ecma262/#sec-getv
    // 1. Assert: IsPropertyKey(P) is true.
    // 2. Let O be ? ToObject(V).
    v8::Local<v8::Object> chunk_as_object;
    if (!chunk->ToObject(context).ToLocal(&chunk_as_object)) {
      // Should have thrown an exception, which will be caught further up the
      // stack.
      return;
    }
    // 3. Return ? O.[[Get]](P, V).
    v8::Local<v8::Value> byte_length;
    if (!chunk_as_object->Get(context, V8AtomicString(isolate, "byteLength"))
             .ToLocal(&byte_length)) {
      // Should have thrown an exception.
      return;
    }
    args.GetReturnValue().Set(byte_length);
  }

  int Length() const override { return 1; }
};

}  // namespace

ByteLengthQueuingStrategy* ByteLengthQueuingStrategy::Create(
    ScriptState* script_state,
    const QueuingStrategyInit* init) {
  return MakeGarbageCollected<ByteLengthQueuingStrategy>(script_state, init);
}

ByteLengthQueuingStrategy::ByteLengthQueuingStrategy(
    ScriptState* script_state,
    const QueuingStrategyInit* init)
    : high_water_mark_(init->highWaterMark()) {}

ByteLengthQueuingStrategy::~ByteLengthQueuingStrategy() = default;

ScriptValue ByteLengthQueuingStrategy::size(ScriptState* script_state) const {
  // https://streams.spec.whatwg.org/#byte-length-queuing-strategy-size-function
  // 5. Set globalObject’s byte length queuing strategy size function to a
  //    Function that represents a reference to F, with callback context equal
  //    to globalObject’s relevant settings object.
  return GetCachedSizeFunction(
      script_state, kByteLengthQueuingStrategySizeFunction,
      &ByteLengthQueuingStrategySizeFunction::CreateFunction);
}

}  // namespace blink
