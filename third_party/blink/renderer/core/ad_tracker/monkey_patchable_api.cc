// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/ad_tracker/monkey_patchable_api.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"

namespace blink {

base::span<const char* const> GetMonkeyPatchableApiPropertyPath(
    MonkeyPatchableApi api) {
  switch (api) {
    case MonkeyPatchableApi::kHistoryPushState: {
      static const char* const kPath[] = {"history", "pushState"};
      return kPath;
    }
    case MonkeyPatchableApi::kHistoryReplaceState: {
      static const char* const kPath[] = {"history", "replaceState"};
      return kPath;
    }
    case MonkeyPatchableApi::kNodeAppendChild: {
      static const char* const kPath[] = {"Node", "prototype", "appendChild"};
      return kPath;
    }
    case MonkeyPatchableApi::kNone:
      NOTREACHED();
  }
  NOTREACHED();
}

MonkeyPatchableApiFunctionInfo GetMonkeyPatchableApiFunctionInfo(
    v8::Isolate* isolate,
    MonkeyPatchableApi api) {
  v8::EscapableHandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (context.IsEmpty()) {
    return {};
  }

  v8::Context::Scope context_scope(context);

  // Start with the global object.
  v8::Local<v8::Value> current_value = context->Global();
  const base::span<const char* const> property_path =
      GetMonkeyPatchableApiPropertyPath(api);

  // Prevent script execution (e.g., via author-defined getters or proxy traps)
  // during prototype chain traversal to avoid evasion, side effects, or DOM
  // mutation re-entrancy crashes.
  v8::Isolate::DisallowJavascriptExecutionScope disallow_js(
      isolate, v8::Isolate::DisallowJavascriptExecutionScope::THROW_ON_FAILURE);
  v8::TryCatch try_catch(isolate);

  // Traverse the property path (e.g., global object -> `history` ->
  // `pushState`).
  for (const char* property_name : property_path) {
    // Each intermediate value in the path must be an object.
    if (!current_value->IsObject()) {
      return {};
    }

    v8::Local<v8::Object> current_object = current_value.As<v8::Object>();
    v8::Local<v8::String> property_key = V8AtomicString(isolate, property_name);

    v8::MaybeLocal<v8::Value> maybe_next_value =
        current_object->Get(context, property_key);

    // If the property doesn't exist, the chain is broken.
    if (maybe_next_value.IsEmpty()) {
      return {};
    }
    current_value = maybe_next_value.ToLocalChecked();
  }

  // At the end of the path, we expect a function. If it's not a function,
  // it has been tampered with, and we can't perform our check.
  if (!current_value->IsFunction()) {
    return {};
  }

  v8::Local<v8::Function> api_function = current_value.As<v8::Function>();

  // Native functions will have an invalid script ID. User-defined functions
  // (monkey patches) will have a valid one.
  bool is_monkey_patched =
      api_function->ScriptId() != v8::Message::kNoScriptIdInfo;

  return {handle_scope.Escape(api_function), is_monkey_patched};
}

bool IsFunctionAMonkeyPatch(v8::Isolate* isolate,
                            const v8::Local<v8::Function>& function,
                            MonkeyPatchableApi api) {
  v8::HandleScope handle_scope(isolate);
  MonkeyPatchableApiFunctionInfo api_info =
      GetMonkeyPatchableApiFunctionInfo(isolate, api);
  if (!api_info.is_monkey_patched) {
    return false;
  }

  v8::Local<v8::Function> api_function;
  if (!api_info.function.ToLocal(&api_function)) {
    return false;
  }

  return function == api_function;
}

}  // namespace blink
