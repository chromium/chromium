// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/api_binding_bridge.h"

#include "base/values.h"
#include "extensions/common/extension_id.h"
#include "extensions/renderer/bindings/api_binding_hooks.h"
#include "extensions/renderer/bindings/api_binding_util.h"
#include "extensions/renderer/bindings/js_runner.h"
#include "gin/converter.h"
#include "gin/object_template_builder.h"

namespace extensions {

namespace {

const char kApiObjectKey[] = "extensions::bridge::api_object";
const char kHookInterfaceKey[] = "extensions::bridge::hook_object";

v8::Local<v8::Private> GetPrivatePropertyName(v8::Isolate* isolate,
                                              const char* key) {
  return v8::Private::ForApi(isolate, gin::StringToSymbol(isolate, key));
}

}  // namespace

gin::WrapperInfo APIBindingBridge::kWrapperInfo = {gin::kEmbedderNativeGin};

APIBindingBridge::APIBindingBridge(APIBindingHooks* hooks,
                                   v8::Local<v8::Context> context,
                                   v8::Local<v8::Value> api_object,
                                   const ExtensionId& extension_id,
                                   const std::string& context_type)
    : extension_id_(extension_id), context_type_(context_type) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::Local<v8::Object> wrapper = GetWrapper(isolate).ToLocalChecked();
  v8::Maybe<bool> result = wrapper->SetPrivate(
      context, GetPrivatePropertyName(isolate, kApiObjectKey), api_object);
  if (!result.IsJust() || !result.FromJust()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  v8::Local<v8::Object> js_hook_interface = hooks->GetJSHookInterface(context);
  result = wrapper->SetPrivate(context,
                               GetPrivatePropertyName(isolate,
                                                      kHookInterfaceKey),
                               js_hook_interface);
  DCHECK(result.IsJust() && result.FromJust());
}

APIBindingBridge::~APIBindingBridge() = default;

gin::ObjectTemplateBuilder APIBindingBridge::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return Wrappable<APIBindingBridge>::GetObjectTemplateBuilder(isolate)
      .SetMethod("registerCustomHook", &APIBindingBridge::RegisterCustomHook);
}

void APIBindingBridge::RegisterCustomHook(v8::Isolate* isolate,
                                          v8::Local<v8::Function> function) {
  // The object and arguments here are meant to match those passed to the hook
  // functions in binding.js.
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (!binding::IsContextValidOrThrowError(context))
    return;  // Context has been invalidated.

  v8::Local<v8::Object> hook_object = v8::Object::New(isolate);
  v8::Local<v8::Object> wrapper;
  if (!GetWrapper(isolate).ToLocal(&wrapper))
    return;

  v8::Local<v8::Value> hook_interface =
      wrapper->GetPrivate(
          context, GetPrivatePropertyName(isolate, kHookInterfaceKey))
              .ToLocalChecked();
  v8::Maybe<bool> result = hook_object->CreateDataProperty(
      context, gin::StringToSymbol(isolate, "apiFunctions"), hook_interface);
  if (!result.IsJust() || !result.FromJust())
    return;

  v8::Local<v8::Value> api_object =
      wrapper
           ->GetPrivate(context, GetPrivatePropertyName(isolate, kApiObjectKey))
           .ToLocalChecked();
  result = hook_object->CreateDataProperty(
      context, gin::StringToSymbol(isolate, "compiledApi"), api_object);
  if (!result.IsJust() || !result.FromJust())
    return;

  result = hook_object->SetPrototype(context, v8::Null(isolate));
  if (!result.IsJust() || !result.FromJust())
    return;

  v8::Local<v8::String> extension_id =
      gin::StringToSymbol(isolate, extension_id_);
  v8::Local<v8::String> context_type =
      gin::StringToSymbol(isolate, context_type_);
  v8::Local<v8::Value> args[] = {hook_object, extension_id, context_type};

  // TODO(devlin): The context should still be valid at this point - nothing
  // above should be able to invalidate it. But let's make extra sure.
  // This CHECK is helping to track down https://crbug.com/819968, and should be
  // removed when that's fixed.
  CHECK(binding::IsContextValid(context));
  JSRunner::Get(context)->RunJSFunction(function, context, std::size(args),
                                        args);
}

}  // namespace extensions
