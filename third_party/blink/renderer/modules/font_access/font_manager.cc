// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/font_access/font_manager.h"

#include <algorithm>

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/font_access/font_iterator.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

namespace {

void ReturnDataFunction(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8SetReturnValue(info, info.Data());
}

}  // namespace

ScriptValue FontManager::query(ScriptState* script_state,
                               ExceptionState& exception_state) {
  if (exception_state.HadException())
    return ScriptValue();

  auto* iterator =
      MakeGarbageCollected<FontIterator>(ExecutionContext::From(script_state));
  auto* isolate = script_state->GetIsolate();
  auto context = script_state->GetContext();

  v8::Local<v8::Object> result = v8::Object::New(isolate);
  if (!result
           ->Set(context, v8::Symbol::GetAsyncIterator(isolate),
                 v8::Function::New(context, &ReturnDataFunction,
                                   ToV8(iterator, script_state))
                     .ToLocalChecked())
           .ToChecked()) {
    return ScriptValue();
  }
  return ScriptValue(script_state->GetIsolate(), result);
}

ScriptPromise FontManager::showFontChooser(ScriptState* script_state,
                                           const QueryOptions* options) {
  return ScriptPromise::RejectWithDOMException(
      script_state,
      MakeGarbageCollected<DOMException>(DOMExceptionCode::kNotSupportedError,
                                         "Not implemented yet"));
}

void FontManager::Trace(blink::Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
