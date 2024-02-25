// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/v8_context_native_handler.h"

#include "base/functional/bind.h"
#include "extensions/common/features/feature.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"

namespace extensions {

V8ContextNativeHandler::V8ContextNativeHandler(ScriptContext* context)
    : ObjectBackedNativeHandler(context), context_(context) {}

void V8ContextNativeHandler::AddRoutes() {
  RouteHandlerFunction(
      "GetAvailability",
      base::BindRepeating(&V8ContextNativeHandler::GetAvailability,
                          base::Unretained(this)));
  RouteHandlerFunction(
      "GetModuleSystem",
      base::BindRepeating(&V8ContextNativeHandler::GetModuleSystem,
                          base::Unretained(this)));
}

void V8ContextNativeHandler::GetAvailability(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(args.Length(), 1);
  v8::Isolate* isolate = args.GetIsolate();
  std::string api_name = *v8::String::Utf8Value(isolate, args[0]);
  Feature::Availability availability = context_->GetAvailability(api_name);

  v8::Local<v8::Context> context = context_->v8_context();
  v8::Local<v8::Object> ret = v8::Object::New(isolate);
  v8::Maybe<bool> maybe = ret->SetPrototype(context, v8::Null(isolate));
  CHECK(maybe.IsJust() && maybe.FromJust());
  ret->Set(context,
           v8::String::NewFromUtf8(isolate, "is_available",
                                   v8::NewStringType::kInternalized)
               .ToLocalChecked(),
           v8::Boolean::New(isolate, availability.is_available()))
      .ToChecked();
  ret->Set(context,
           v8::String::NewFromUtf8(isolate, "message",
                                   v8::NewStringType::kInternalized)
               .ToLocalChecked(),
           v8::String::NewFromUtf8(isolate, availability.message().c_str(),
                                   v8::NewStringType::kNormal)
               .ToLocalChecked())
      .ToChecked();
  ret->Set(context,
           v8::String::NewFromUtf8(isolate, "result",
                                   v8::NewStringType::kInternalized)
               .ToLocalChecked(),
           v8::Integer::New(isolate, availability.result()))
      .ToChecked();
  args.GetReturnValue().Set(ret);
}

void V8ContextNativeHandler::GetModuleSystem(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsObject());
  ScriptContext* context = ScriptContextSet::GetContextByObject(
      v8::Local<v8::Object>::Cast(args[0]));
  if (context && blink::WebFrame::ScriptCanAccess(args.GetIsolate(),
                                                  context->web_frame())) {
    args.GetReturnValue().Set(context->module_system()->NewInstance());
  }
}

}  // namespace extensions
