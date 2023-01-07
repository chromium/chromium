// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/queuing_strategy_common.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_queuing_strategy_init.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

ScriptValue GetCachedSizeFunction(ScriptState* script_state,
                                  const V8PrivateProperty::SymbolKey& key,
                                  SizeFunctionFactory factory) {
  auto* isolate = script_state->GetIsolate();
  auto function_cache = V8PrivateProperty::GetSymbol(isolate, key);
  v8::Local<v8::Object> global_proxy = script_state->GetContext()->Global();
  v8::Local<v8::Value> function;
  if (!function_cache.GetOrUndefined(global_proxy).ToLocal(&function) ||
      function->IsUndefined()) {
    function = factory(script_state);
    bool is_set = function_cache.Set(global_proxy, function);
    DCHECK(is_set || isolate->IsExecutionTerminating());
  }
  return ScriptValue(isolate, function);
}

}  // namespace blink
