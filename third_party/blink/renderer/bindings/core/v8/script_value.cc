/*
 * Copyright (C) 2008, 2009, 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"

#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value_factory.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {
ASSERT_SIZE(ScriptValue, WorldSafeV8Reference<v8::Value>);
v8::Local<v8::Value> ScriptValue::V8Value() const {
  return IsEmpty() ? v8::Local<v8::Value>()
                   : value_.Get(ScriptState::ForCurrentRealm(GetIsolate()));
}

v8::Local<v8::Value> ScriptValue::V8ValueFor(
    ScriptState* target_script_state) const {
  if (IsEmpty())
    return v8::Local<v8::Value>();

  return value_.GetAcrossWorld(target_script_state);
}

bool ScriptValue::ToString(String& result) const {
  if (IsEmpty())
    return false;

  auto* isolate = GetIsolate();
  DCHECK(isolate->InContext());
  v8::Local<v8::String> string =
      V8Value()->ToString(isolate->GetCurrentContext()).ToLocalChecked();
  result = ToCoreString(isolate, string);
  return true;
}

ScriptValue ScriptValue::CreateNull(v8::Isolate* isolate) {
  return ScriptValue(isolate, v8::Null(isolate));
}

}  // namespace blink
