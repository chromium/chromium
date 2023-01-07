// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_try_catch.h"

#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/v8_var_converter.h"
#include "gin/converter.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/var_tracker.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-primitive.h"

namespace content {

namespace {

const char kConversionException[] =
    "Error: Failed conversion between PP_Var and V8 value";
const char kInvalidException[] = "Error: An invalid exception was thrown.";

}  // namespace

PepperTryCatch::PepperTryCatch(PepperPluginInstanceImpl* instance,
                               V8VarConverter* var_converter)
    : instance_(instance), var_converter_(var_converter) {}

PepperTryCatch::~PepperTryCatch() {}

v8::Local<v8::Value> PepperTryCatch::ToV8(PP_Var var) {
  if (HasException()) {
    SetException(kConversionException);
    return v8::Local<v8::Value>();
  }

  v8::Local<v8::Value> result;
  bool success = var_converter_->ToV8Value(var, GetContext(), &result);
  if (!success) {
    SetException(kConversionException);
    return v8::Local<v8::Value>();
  }
  return result;
}

ppapi::ScopedPPVar PepperTryCatch::FromV8(v8::Local<v8::Value> v8_value) {
  if (HasException() || v8_value.IsEmpty()) {
    SetException(kConversionException);
    return ppapi::ScopedPPVar();
  }
  ppapi::ScopedPPVar result;
  bool success =
      var_converter_->FromV8ValueSync(v8_value, GetContext(), &result);
  if (!success) {
    SetException(kConversionException);
    return ppapi::ScopedPPVar();
  }
  return result;
}

ppapi::ScopedPPVar PepperTryCatch::FromV8Maybe(
    v8::MaybeLocal<v8::Value> v8_value) {
  return FromV8(v8_value.FromMaybe(v8::Local<v8::Value>()));
}

PepperTryCatchV8::PepperTryCatchV8(PepperPluginInstanceImpl* instance,
                                   V8VarConverter* var_converter,
                                   v8::Isolate* isolate)
    : PepperTryCatch(instance, var_converter),
      exception_(PP_MakeUndefined()) {
  // Typically when using PepperTryCatchV8 we are passed an isolate. We verify
  // that this isolate is the same as the plugin isolate.
  DCHECK(isolate == instance_->GetIsolate());

  // We assume that a handle scope and context has been setup by the user of
  // this class. This is typically true because this class is used when calling
  // into the plugin from JavaScript. We want to use whatever v8 context the
  // caller is in.
}

PepperTryCatchV8::~PepperTryCatchV8() {
  ppapi::PpapiGlobals::Get()->GetVarTracker()->ReleaseVar(exception_);
}

bool PepperTryCatchV8::HasException() {
  return GetContext().IsEmpty() || exception_.type != PP_VARTYPE_UNDEFINED;
}

v8::Local<v8::Context> PepperTryCatchV8::GetContext() {
  // When calling from JS into the plugin always use the current context.
  return instance_->GetIsolate()->GetCurrentContext();
}

bool PepperTryCatchV8::ThrowException() {
  if (!HasException())
    return false;

  // If there is no context then we have an exception but we don't try to throw
  // it into v8.
  if (GetContext().IsEmpty())
    return true;

  std::string message(kInvalidException);
  ppapi::StringVar* message_var = ppapi::StringVar::FromPPVar(exception_);
  if (message_var)
    message = message_var->value();
  instance_->GetIsolate()->ThrowException(v8::Exception::Error(
      gin::StringToV8(instance_->GetIsolate(), message)));

  ppapi::PpapiGlobals::Get()->GetVarTracker()->ReleaseVar(exception_);
  exception_ = PP_MakeUndefined();
  return true;
}

void PepperTryCatchV8::ThrowException(const char* message) {
  SetException(message);
  ThrowException();
}

void PepperTryCatchV8::SetException(const char* message) {
  if (HasException())
    return;

  exception_ = ppapi::StringVar::StringToPPVar(message);
}

PepperTryCatchVar::PepperTryCatchVar(PepperPluginInstanceImpl* instance,
                                     V8VarConverter* var_converter,
                                     PP_Var* exception)
    : PepperTryCatch(instance, var_converter),
      handle_scope_(instance_->GetIsolate()),
      try_catch_(instance_->GetIsolate()),
      exception_(exception),
      exception_is_set_(false) {
  // Store a handle to the context here for 2 reasons:
  // 1) To hold a handle to it in case all other handles are destroyed.
  // 2) Because calling PepperPluginInstanceImpl::GetMainWorldContext() later
  //    can result in trying to access the plugin element. However the plugin
  //    element may have been destroyed during the PepperTryCatchVar (for
  //    example if a script is executed which destroys the plugin element). So
  //    we want to avoid accessing the plugin element again beyond this point.
  context_ = instance_->GetMainWorldContext();

  // We switch to the plugin context if it's not empty.
  if (!context_.IsEmpty())
    context_->Enter();
}

PepperTryCatchVar::~PepperTryCatchVar() {
  if (!context_.IsEmpty())
    context_->Exit();
}

bool PepperTryCatchVar::HasException() {
  if (exception_is_set_)
    return true;

  std::string exception_message;
  if (context_.IsEmpty()) {
    exception_message = "The v8 context has been destroyed.";
  } else if (try_catch_.HasCaught()) {
    v8::Local<v8::Message> message(try_catch_.Message());
    if (!message.IsEmpty()) {
      v8::String::Utf8Value utf8(handle_scope_.GetIsolate(),
                                 try_catch_.Message()->Get());
      exception_message = std::string(*utf8, utf8.length());
    } else {
      exception_message = "There was a v8 exception.";
    }
  }

  if (!exception_message.empty()) {
    exception_is_set_ = true;
    if (exception_)
      *exception_ = ppapi::StringVar::StringToPPVar(exception_message);
  }

  return exception_is_set_;
}

v8::Local<v8::Context> PepperTryCatchVar::GetContext() {
  return context_;
}

void PepperTryCatchVar::SetException(const char* message) {
  if (exception_is_set_)
    return;

  if (exception_)
    *exception_ = ppapi::StringVar::StringToPPVar(message, strlen(message));
  exception_is_set_ = true;
}

}  // namespace content
