// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/object_backed_native_handler.h"

#include <stddef.h>
#include <utility>

#include "base/logging.h"
#include "content/public/renderer/worker_thread.h"
#include "extensions/common/extension_api.h"
#include "extensions/renderer/console.h"
#include "extensions/renderer/module_system.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "extensions/renderer/v8_helpers.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "v8/include/v8.h"

namespace extensions {

namespace {
// Key for the base::Bound routed function.
const char kHandlerFunction[] = "handler_function";
const char kFeatureName[] = "feature_name";
}  // namespace

ObjectBackedNativeHandler::ObjectBackedNativeHandler(ScriptContext* context)
    : router_data_(context->isolate()),
      context_(context),
      object_template_(context->isolate(),
                       v8::ObjectTemplate::New(context->isolate())) {
}

ObjectBackedNativeHandler::~ObjectBackedNativeHandler() {
}

void ObjectBackedNativeHandler::Initialize() {
  DCHECK_EQ(kUninitialized, init_state_)
      << "Initialize() can only be called once!";
  init_state_ = kInitializingRoutes;
  AddRoutes();
  init_state_ = kInitialized;
}

bool ObjectBackedNativeHandler::IsInitialized() {
  return init_state_ == kInitialized;
}

v8::Local<v8::Object> ObjectBackedNativeHandler::NewInstance() {
  DCHECK_EQ(kInitialized, init_state_)
      << "Initialize() must be called before a new instance is created!";
  return v8::Local<v8::ObjectTemplate>::New(GetIsolate(), object_template_)
      ->NewInstance(GetIsolate()->GetCurrentContext())
      .ToLocalChecked();
}

// static
void ObjectBackedNativeHandler::Router(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Object> data = args.Data().As<v8::Object>();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::Local<v8::Value> handler_function_value;
  v8::Local<v8::Value> feature_name_value;
  // See comment in header file for why we do this.
  if (!GetPrivate(context, data, kHandlerFunction, &handler_function_value) ||
      handler_function_value->IsUndefined() ||
      !GetPrivate(context, data, kFeatureName, &feature_name_value) ||
      !feature_name_value->IsString()) {
    ScriptContext* script_context =
        ScriptContextSet::GetContextByV8Context(context);
    console::AddMessage(script_context,
                        blink::mojom::ConsoleMessageLevel::kError,
                        "Extension view no longer exists");
    return;
  }

  // We can't access the ScriptContextSet on a worker thread. Luckily, we also
  // don't inject many bindings into worker threads.
  // TODO(devlin): Figure out a way around this.
  if (content::WorkerThread::GetCurrentId() == 0) {
    ScriptContext* script_context =
        ScriptContextSet::GetContextByV8Context(context);
    v8::Local<v8::String> feature_name_string =
        feature_name_value->ToString(context).ToLocalChecked();
    std::string feature_name =
        *v8::String::Utf8Value(isolate, feature_name_string);
    // TODO(devlin): Eventually, we should fail if either script_context is null
    // or feature_name is empty.
    if (script_context && !feature_name.empty()) {
      Feature::Availability availability =
          script_context->GetAvailability(feature_name);
      if (!availability.is_available()) {
        DVLOG(1) << feature_name
                 << " is not available: " << availability.message();
        return;
      }
    }
  }
  // This CHECK is *important*. Otherwise, we'll go around happily executing
  // something random.  See crbug.com/548273.
  CHECK(handler_function_value->IsExternal());
  static_cast<HandlerFunction*>(
      handler_function_value.As<v8::External>()->Value())->Run(args);

  // Verify that the return value, if any, is accessible by the context.
  v8::ReturnValue<v8::Value> ret = args.GetReturnValue();
  v8::Local<v8::Value> ret_value = ret.Get();
  if (ret_value->IsObject() && !ret_value->IsNull() &&
      !ContextCanAccessObject(context, v8::Local<v8::Object>::Cast(ret_value),
                              true)) {
    NOTREACHED() << "Insecure return value";
    ret.SetUndefined();
  }
}

void ObjectBackedNativeHandler::RouteHandlerFunction(
    const std::string& name,
    HandlerFunction handler_function) {
  RouteHandlerFunction(name, "", std::move(handler_function));
}

void ObjectBackedNativeHandler::RouteHandlerFunction(
    const std::string& name,
    const std::string& feature_name,
    HandlerFunction handler_function) {
  DCHECK_EQ(init_state_, kInitializingRoutes)
      << "RouteHandlerFunction() can only be called from AddRoutes()!";

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context_->v8_context());

  v8::Local<v8::Object> data = v8::Object::New(isolate);
  SetPrivate(data, kHandlerFunction,
             v8::External::New(
                 isolate, new HandlerFunction(std::move(handler_function))));
  DCHECK(feature_name.empty() ||
         ExtensionAPI::GetSharedInstance()->GetFeatureDependency(feature_name))
      << feature_name;
  SetPrivate(data, kFeatureName,
             v8_helpers::ToV8StringUnsafe(isolate, feature_name));
  v8::Local<v8::FunctionTemplate> function_template =
      v8::FunctionTemplate::New(isolate, Router, data);
  function_template->RemovePrototype();
  v8::Local<v8::ObjectTemplate>::New(isolate, object_template_)
      ->Set(isolate, name.c_str(), function_template);
  router_data_.Append(data);
}

v8::Isolate* ObjectBackedNativeHandler::GetIsolate() const {
  return context_->isolate();
}

void ObjectBackedNativeHandler::Invalidate() {
  v8::Isolate* isolate = GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context_->v8_context());

  for (size_t i = 0; i < router_data_.Size(); i++) {
    v8::Local<v8::Object> data = router_data_.Get(i);
    v8::Local<v8::Value> handler_function_value;
    CHECK(GetPrivate(data, kHandlerFunction, &handler_function_value));
    delete static_cast<HandlerFunction*>(
        handler_function_value.As<v8::External>()->Value());
    DeletePrivate(data, kHandlerFunction);
  }

  router_data_.Clear();
  object_template_.Reset();

  NativeHandler::Invalidate();
}

// static
bool ObjectBackedNativeHandler::ContextCanAccessObject(
    const v8::Local<v8::Context>& context,
    const v8::Local<v8::Object>& object,
    bool allow_null_context) {
  if (object->IsNull())
    return true;
  if (context == object->CreationContext())
    return true;
  // TODO(lazyboy): ScriptContextSet isn't available on worker threads. We
  // should probably use WorkerScriptContextSet somehow.
  ScriptContext* other_script_context =
      content::WorkerThread::GetCurrentId() == 0
          ? ScriptContextSet::GetContextByObject(object)
          : nullptr;
  if (!other_script_context || !other_script_context->web_frame())
    return allow_null_context;

  return blink::WebFrame::ScriptCanAccess(other_script_context->web_frame());
}

void ObjectBackedNativeHandler::SetPrivate(v8::Local<v8::Object> obj,
                                           const char* key,
                                           v8::Local<v8::Value> value) {
  SetPrivate(context_->v8_context(), obj, key, value);
}

// static
void ObjectBackedNativeHandler::SetPrivate(v8::Local<v8::Context> context,
                                           v8::Local<v8::Object> obj,
                                           const char* key,
                                           v8::Local<v8::Value> value) {
  obj->SetPrivate(
         context,
         v8::Private::ForApi(context->GetIsolate(),
                             v8::String::NewFromUtf8(context->GetIsolate(), key,
                                                     v8::NewStringType::kNormal)
                                 .ToLocalChecked()),
         value)
      .FromJust();
}

bool ObjectBackedNativeHandler::GetPrivate(v8::Local<v8::Object> obj,
                                           const char* key,
                                           v8::Local<v8::Value>* result) {
  return GetPrivate(context_->v8_context(), obj, key, result);
}

// static
bool ObjectBackedNativeHandler::GetPrivate(v8::Local<v8::Context> context,
                                           v8::Local<v8::Object> obj,
                                           const char* key,
                                           v8::Local<v8::Value>* result) {
  return obj
      ->GetPrivate(context, v8::Private::ForApi(context->GetIsolate(),
                                                v8::String::NewFromUtf8(
                                                    context->GetIsolate(), key,
                                                    v8::NewStringType::kNormal)
                                                    .ToLocalChecked()))
      .ToLocal(result);
}

void ObjectBackedNativeHandler::DeletePrivate(v8::Local<v8::Object> obj,
                                              const char* key) {
  DeletePrivate(context_->v8_context(), obj, key);
}

// static
void ObjectBackedNativeHandler::DeletePrivate(v8::Local<v8::Context> context,
                                              v8::Local<v8::Object> obj,
                                              const char* key) {
  obj->DeletePrivate(
         context,
         v8::Private::ForApi(context->GetIsolate(),
                             v8::String::NewFromUtf8(context->GetIsolate(), key,
                                                     v8::NewStringType::kNormal)
                                 .ToLocalChecked()))
      .FromJust();
}

}  // namespace extensions
