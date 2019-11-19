// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/byte_length_queuing_strategy.h"

#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/streams/queuing_strategy_common.h"
#include "third_party/blink/renderer/core/streams/queuing_strategy_init.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

namespace {

class ByteLengthQueuingStrategySizeFunction final : public ScriptFunction {
 public:
  static v8::Local<v8::Function> CreateFunction(ScriptState* script_state) {
    ByteLengthQueuingStrategySizeFunction* self =
        MakeGarbageCollected<ByteLengthQueuingStrategySizeFunction>(
            script_state);
    return self->BindToV8Function();
  }

  explicit ByteLengthQueuingStrategySizeFunction(ScriptState* script_state)
      : ScriptFunction(script_state) {}

 private:
  void CallRaw(const v8::FunctionCallbackInfo<v8::Value>& args) override {
    auto* isolate = args.GetIsolate();
    DCHECK_EQ(isolate, GetScriptState()->GetIsolate());
    auto context = GetScriptState()->GetContext();
    v8::Local<v8::Value> chunk;
    if (args.Length() < 1) {
      chunk = v8::Undefined(isolate);
    } else {
      chunk = args[0];
    }

    // https://streams.spec.whatwg.org/#blqs-size
    // 1. Return ? GetV(chunk, "byteLength").

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
    : high_water_mark_(script_state->GetIsolate(),
                       HighWaterMarkOrUndefined(script_state, init)) {}

ByteLengthQueuingStrategy::~ByteLengthQueuingStrategy() = default;

ScriptValue ByteLengthQueuingStrategy::highWaterMark(
    ScriptState* script_state) const {
  return ScriptValue(script_state->GetIsolate(),
                     high_water_mark_.NewLocal(script_state->GetIsolate()));
}

ScriptValue ByteLengthQueuingStrategy::size(ScriptState* script_state) const {
  // We don't cache the result because normally this method will only be called
  // once anyway.
  return ScriptValue(
      script_state->GetIsolate(),
      ByteLengthQueuingStrategySizeFunction::CreateFunction(script_state));
}

void ByteLengthQueuingStrategy::Trace(Visitor* visitor) {
  visitor->Trace(high_water_mark_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
