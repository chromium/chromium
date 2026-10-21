// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/ad_tracker/monkey_patchable_api.h"

#include "base/containers/span.h"
#include "base/notreached.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/frame/history.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"

namespace blink {

namespace {

// Returns the `History` object of `context`'s window, i.e., what the native
// `window.history` getter returns, or an empty handle if there's no window.
v8::MaybeLocal<v8::Object> GetHistoryObject(v8::Isolate* isolate,
                                            v8::Local<v8::Context> context) {
  LocalDOMWindow* window = ToLocalDOMWindow(context);
  if (!window) {
    return {};
  }
  return window->history()
      ->ToV8(ScriptState::From(isolate, context))
      .As<v8::Object>();
}

// Looks up `property_names` in order, starting from `object`, and returns the
// value of the last one, or an empty handle if `object` is empty or any lookup
// fails. Lookups never invoke getters, Proxy traps or interceptors (see
// `v8::Object::TryGetWithNoSideEffects()`), as they can run functions installed
// by page script, including native ones (e.g., DOM APIs) that can mutate the
// DOM.
v8::MaybeLocal<v8::Value> GetPropertyPath(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    v8::MaybeLocal<v8::Object> object,
    base::span<const char* const> property_names) {
  v8::Local<v8::Value> value;
  if (!object.ToLocal(&value)) {
    return {};
  }
  for (const char* property_name : property_names) {
    // Each intermediate value in the path must be an object.
    if (!value->IsObject()) {
      return {};
    }
    if (!value.As<v8::Object>()
             ->TryGetWithNoSideEffects(context,
                                       V8AtomicString(isolate, property_name))
             .ToLocal(&value)) {
      return {};
    }
  }
  return value;
}

// Returns the current value of `api` in `context`, or an empty handle if it
// can't be read (see `GetPropertyPath()`). Since `GetPropertyPath()` never
// invokes getters, objects that script reaches via accessor properties (e.g.,
// `window.history`) are resolved natively.
v8::MaybeLocal<v8::Value> GetMonkeyPatchableApi(v8::Isolate* isolate,
                                                v8::Local<v8::Context> context,
                                                MonkeyPatchableApi api) {
  switch (api) {
    case MonkeyPatchableApi::kHistoryPushState:
      return GetPropertyPath(isolate, context,
                             GetHistoryObject(isolate, context), {"pushState"});
    case MonkeyPatchableApi::kHistoryReplaceState:
      return GetPropertyPath(isolate, context,
                             GetHistoryObject(isolate, context),
                             {"replaceState"});
    case MonkeyPatchableApi::kNodeAppendChild:
      return GetPropertyPath(isolate, context, context->Global(),
                             {"Node", "prototype", "appendChild"});
    case MonkeyPatchableApi::kNone:
      NOTREACHED();
  }
  NOTREACHED();
}

}  // namespace

MonkeyPatchableApiFunctionInfo GetMonkeyPatchableApiFunctionInfo(
    v8::Isolate* isolate,
    MonkeyPatchableApi api) {
  v8::EscapableHandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (context.IsEmpty()) {
    return {};
  }

  v8::Context::Scope context_scope(context);

  v8::Local<v8::Value> current_value;
  if (!GetMonkeyPatchableApi(isolate, context, api).ToLocal(&current_value)) {
    return {};
  }

  // We expect the API to be a function. If it's not, it has been tampered with,
  // and we can't perform our check.
  if (!current_value->IsFunction()) {
    return {};
  }

  v8::Local<v8::Function> api_function = current_value.As<v8::Function>();

  // Native functions will have an invalid script ID. User-defined functions
  // (monkey patches) will have a valid one. A Proxy object is also considered
  // a monkey patch since native Blink API properties are never backed by
  // Proxies. If one is found, it means script has overwritten the property.
  bool is_monkey_patched =
      current_value->IsProxy() ||
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

  return IsFunctionAMonkeyPatch(isolate, function, api_function);
}

bool IsFunctionAMonkeyPatch(v8::Isolate* isolate,
                            const v8::Local<v8::Function>& function,
                            const v8::Local<v8::Function>& api_function) {
  if (function == api_function) {
    return true;
  }

  // If the API is monkey-patched via a Proxy, the function on the stack
  // will be the proxy's `apply` trap, not the Proxy object itself.
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (context.IsEmpty()) {
    return false;
  }

  v8::Local<v8::Value> current = api_function;
  while (current->IsProxy()) {
    v8::Local<v8::Proxy> proxy = current.As<v8::Proxy>();
    if (proxy->IsRevoked()) {
      break;
    }

    v8::Local<v8::Value> handler = proxy->GetHandler();
    if (handler->IsObject()) {
      // The handler is controlled by page script, so read its `apply` trap
      // without side effects (see `GetPropertyPath()`).
      v8::Local<v8::Object> handler_obj = handler.As<v8::Object>();
      v8::MaybeLocal<v8::Value> maybe_apply_trap =
          handler_obj->TryGetWithNoSideEffects(
              context, V8AtomicString(isolate, "apply"));

      v8::Local<v8::Value> apply_trap;
      if (maybe_apply_trap.ToLocal(&apply_trap)) {
        if (apply_trap == function) {
          return true;
        }
      }
    }
    current = proxy->GetTarget();
    if (current == function) {
      return true;
    }
  }

  return false;
}

}  // namespace blink
