// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/renderer/pepper/ppb_var_deprecated_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "content/renderer/pepper/host_globals.h"
#include "content/renderer/pepper/message_channel.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/pepper_try_catch.h"
#include "content/renderer/pepper/plugin_module.h"
#include "content/renderer/pepper/plugin_object.h"
#include "content/renderer/pepper/v8object_var.h"
#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/shared_impl/ppb_var_shared.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/blink/public/web/web_plugin_script_forbidden_scope.h"

using ppapi::V8ObjectVar;
using ppapi::PpapiGlobals;
using ppapi::ScopedPPVar;
using ppapi::ScopedPPVarArray;
using ppapi::StringVar;
using ppapi::Var;

namespace content {

namespace {

const char kInvalidIdentifierException[] = "Error: Invalid identifier.";
const char kInvalidObjectException[] = "Error: Invalid object";
const char kUnableToCallMethodException[] = "Error: Unable to call method";

class ObjectAccessor {
 public:
  ObjectAccessor(PP_Var var)
      : object_var_(V8ObjectVar::FromPPVar(var).get()),
        instance_(object_var_ ? object_var_->instance() : nullptr) {
    if (instance_) {
      converter_ = std::make_unique<V8VarConverter>(
          instance_->pp_instance(), V8VarConverter::kAllowObjectVars);
    }
  }

  // Check if the object is valid. If it isn't, set an exception and return
  // false.
  bool IsValid(PP_Var* exception) {
    // If we already have an exception, then the call is invalid according to
    // the unittests.
    if (exception && exception->type != PP_VARTYPE_UNDEFINED)
      return false;
    if (instance_)
      return !instance_->is_deleted() ||
             !blink::WebPluginScriptForbiddenScope::IsForbidden();
    if (exception)
      *exception = ppapi::StringVar::StringToPPVar(kInvalidObjectException);
    return false;
  }
  // Lazily grab the object so that the handle is created in the current handle
  // scope.
  v8::Local<v8::Object> GetObject() { return object_var_->GetHandle(); }
  PepperPluginInstanceImpl* instance() { return instance_; }
  V8VarConverter* converter() { return converter_.get(); }

 private:
  raw_ptr<V8ObjectVar> object_var_;
  raw_ptr<PepperPluginInstanceImpl> instance_;
  std::unique_ptr<V8VarConverter> converter_;
};

bool IsValidIdentifer(PP_Var identifier, PP_Var* exception) {
  if (identifier.type == PP_VARTYPE_INT32 ||
      identifier.type == PP_VARTYPE_STRING) {
    return true;
  }
  if (exception)
    *exception = ppapi::StringVar::StringToPPVar(kInvalidIdentifierException);
  return false;
}

bool HasPropertyDeprecated(PP_Var var, PP_Var name, PP_Var* exception) {
  ObjectAccessor accessor(var);
  if (!accessor.IsValid(exception) || !IsValidIdentifer(name, exception))
    return false;

  PepperTryCatchVar try_catch(accessor.instance(), accessor.converter(),
                              exception);
  v8::Local<v8::Context> context = try_catch.GetContext();
  v8::MicrotasksScope microtasks_scope(
      context, v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Local<v8::Value> v8_name = try_catch.ToV8(name);
  if (try_catch.HasException())
    return false;

  bool result = false;
  if (!accessor.GetObject()->Has(context, v8_name).To(&result)) {
    try_catch.HasException();
    return false;
  }

  return result;
}

bool HasMethodDeprecated(PP_Var var, PP_Var name, PP_Var* exception) {
  ObjectAccessor accessor(var);
  if (!accessor.IsValid(exception) || !IsValidIdentifer(name, exception))
    return false;

  PepperTryCatchVar try_catch(accessor.instance(), accessor.converter(),
                              exception);
  v8::Local<v8::Context> context = try_catch.GetContext();
  v8::MicrotasksScope microtasks_scope(
      context, v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Local<v8::Value> v8_name = try_catch.ToV8(name);
  if (try_catch.HasException())
    return false;

  bool has_name = false;
  if (!accessor.GetObject()->Has(context, v8_name).To(&has_name)) {
    try_catch.HasException();
    return false;
  }

  if (!has_name)
    return false;

  v8::Local<v8::Value> function;
  return accessor.GetObject()->Get(context, v8_name).ToLocal(&function) &&
         function->IsFunction();
}

PP_Var GetProperty(PP_Var var, PP_Var name, PP_Var* exception) {
  ObjectAccessor accessor(var);
  if (!accessor.IsValid(exception) || !IsValidIdentifer(name, exception))
    return PP_MakeUndefined();

  PepperTryCatchVar try_catch(accessor.instance(), accessor.converter(),
                              exception);
  v8::Local<v8::Context> context = try_catch.GetContext();
  v8::MicrotasksScope microtasks_scope(
      context, v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Local<v8::Value> v8_name = try_catch.ToV8(name);
  if (try_catch.HasException())
    return PP_MakeUndefined();

  ScopedPPVar result_var =
      try_catch.FromV8Maybe(accessor.GetObject()->Get(context, v8_name));
  if (try_catch.HasException())
    return PP_MakeUndefined();

  return result_var.Release();
}

void EnumerateProperties(PP_Var var,
                         uint32_t* property_count,
                         PP_Var** properties,
                         PP_Var* exception) {
  ObjectAccessor accessor(var);
  if (!accessor.IsValid(exception))
    return;

  PepperTryCatchVar try_catch(accessor.instance(), accessor.converter(),
                              exception);
  v8::Local<v8::Context> context = try_catch.GetContext();
  v8::MicrotasksScope microtasks_scope(
      context, v8::MicrotasksScope::kDoNotRunMicrotasks);

  *properties = nullptr;
  *property_count = 0;

  v8::Local<v8::Array> identifiers;
  if (!accessor.GetObject()->GetPropertyNames(context).ToLocal(&identifiers))
    return;
  ScopedPPVarArray identifier_vars(identifiers->Length());
  for (uint32_t i = 0; i < identifiers->Length(); ++i) {
    ScopedPPVar identifier =
        try_catch.FromV8Maybe(identifiers->Get(context, i));
    if (try_catch.HasException())
      return;
    identifier_vars.Set(i, identifier);
  }

  size_t size = identifier_vars.size();
  *properties = identifier_vars.Release(
      ScopedPPVarArray::PassPPBMemoryAllocatedArray());
  *property_count = size;
}

void SetPropertyDeprecated(PP_Var var,
                           PP_Var name,
                           PP_Var value,
                           PP_Var* exception) {
  ObjectAccessor accessor(var);
  if (!accessor.IsValid(exception) || !IsValidIdentifer(name, exception))
    return;

  PepperTryCatchVar try_catch(accessor.instance(), accessor.converter(),
                              exception);
  v8::Local<v8::Context> context = try_catch.GetContext();
  v8::MicrotasksScope microtasks_scope(
      context, v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Local<v8::Value> v8_name = try_catch.ToV8(name);
  v8::Local<v8::Value> v8_value = try_catch.ToV8(value);

  if (try_catch.HasException())
    return;

  if (accessor.GetObject()
          ->Set(try_catch.GetContext(), v8_name, v8_value)
          .IsNothing()) {
    try_catch.HasException();  // Ensure an exception gets set.
  }
}

void DeletePropertyDeprecated(PP_Var var, PP_Var name, PP_Var* exception) {
  ObjectAccessor accessor(var);
  if (!accessor.IsValid(exception) || !IsValidIdentifer(name, exception))
    return;

  PepperTryCatchVar try_catch(accessor.instance(), accessor.converter(),
                              exception);
  v8::Local<v8::Context> context = try_catch.GetContext();
  v8::MicrotasksScope microtasks_scope(
      context, v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Local<v8::Value> v8_name = try_catch.ToV8(name);

  if (try_catch.HasException())
    return;

  if (accessor.GetObject()->Delete(context, v8_name).IsNothing()) {
    // Ensure exception object is created if V8 has thrown.
    try_catch.HasException();
    return;
  }
  return;
}

PP_Var CallDeprecatedInternal(PP_Var var,
                              PP_Var method_name,
                              uint32_t argc,
                              PP_Var* argv,
                              PP_Var* exception) {
  ObjectAccessor accessor(var);
  if (!accessor.IsValid(exception))
    return PP_MakeUndefined();

  // If the method name is undefined, set it to the empty string to trigger
  // calling |var| as a function.
  ScopedPPVar scoped_name(method_name);
  if (method_name.type == PP_VARTYPE_UNDEFINED) {
    scoped_name = ScopedPPVar(ScopedPPVar::PassRef(),
                                StringVar::StringToPPVar(""));
  }

  PepperTryCatchVar try_catch(accessor.instance(), accessor.converter(),
                              exception);
  v8::Local<v8::Context> context = try_catch.GetContext();
  v8::MicrotasksScope microtasks_scope(
      context, v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Local<v8::Value> v8_method_name = try_catch.ToV8(scoped_name.get());
  if (try_catch.HasException())
    return PP_MakeUndefined();

  if (!v8_method_name->IsString()) {
    try_catch.SetException(kUnableToCallMethodException);
    return PP_MakeUndefined();
  }

  v8::Local<v8::Object> function = accessor.GetObject();
  v8::Local<v8::Object> recv = context->Global();
  if (v8_method_name.As<v8::String>()->Length() != 0) {
    v8::Local<v8::Value> value;
    if (!function->Get(context, v8_method_name).ToLocal(&value) ||
        !value->ToObject(context).ToLocal(&function)) {
      try_catch.SetException(kUnableToCallMethodException);
      return PP_MakeUndefined();
    }
    recv = accessor.GetObject();
  }

  if (try_catch.HasException())
    return PP_MakeUndefined();

  if (!function->IsFunction()) {
    try_catch.SetException(kUnableToCallMethodException);
    return PP_MakeUndefined();
  }

  std::unique_ptr<v8::Local<v8::Value>[]> converted_args(
      new v8::Local<v8::Value>[argc]);
  for (uint32_t i = 0; i < argc; ++i) {
    converted_args[i] = try_catch.ToV8(argv[i]);
    if (try_catch.HasException())
      return PP_MakeUndefined();
  }

  blink::WebPluginContainer* container = accessor.instance()->container();
  blink::WebLocalFrame* frame = nullptr;
  if (container)
    frame = container->GetDocument().GetFrame();

  if (!frame) {
    try_catch.SetException("No frame to execute script in.");
    return PP_MakeUndefined();
  }

  ScopedPPVar result_var;
  v8::Local<v8::Value> result;
  if (frame
          ->CallFunctionEvenIfScriptDisabled(function.As<v8::Function>(), recv,
                                             argc, converted_args.get())
          .ToLocal(&result)) {
    result_var = try_catch.FromV8(result);
  }

  if (try_catch.HasException())
    return PP_MakeUndefined();

  return result_var.Release();
}

PP_Var CallDeprecated(PP_Var var,
                      PP_Var method_name,
                      uint32_t argc,
                      PP_Var* argv,
                      PP_Var* exception) {
  ObjectAccessor accessor(var);
  if (accessor.instance() &&
      accessor.instance()->HasTransientUserActivation()) {
    return CallDeprecatedInternal(var, method_name, argc, argv, exception);
  }
  return CallDeprecatedInternal(var, method_name, argc, argv, exception);
}

PP_Var Construct(PP_Var var, uint32_t argc, PP_Var* argv, PP_Var* exception) {
  // Deprecated.
  NOTREACHED_IN_MIGRATION();
  return PP_MakeUndefined();
}

bool IsInstanceOfDeprecated(PP_Var var,
                            const PPP_Class_Deprecated* ppp_class,
                            void** ppp_class_data) {
  scoped_refptr<V8ObjectVar> object(V8ObjectVar::FromPPVar(var));
  if (!object.get())
    return false;  // Not an object at all.

  v8::HandleScope handle_scope(object->instance()->GetIsolate());
  v8::Local<v8::Context> context = object->instance()->GetMainWorldContext();
  if (context.IsEmpty())
    return false;
  v8::Context::Scope context_scope(context);
  PluginObject* plugin_object = PluginObject::FromV8Object(
      object->instance()->GetIsolate(), object->GetHandle());
  if (plugin_object && plugin_object->ppp_class() == ppp_class) {
    if (ppp_class_data)
      *ppp_class_data = plugin_object->ppp_class_data();
    return true;
  }

  return false;
}

PP_Var CreateObjectDeprecated(PP_Instance pp_instance,
                              const PPP_Class_Deprecated* ppp_class,
                              void* ppp_class_data) {
  PepperPluginInstanceImpl* instance =
      HostGlobals::Get()->GetInstance(pp_instance);
  if (!instance) {
    DLOG(ERROR) << "Create object passed an invalid instance.";
    return PP_MakeNull();
  }
  return PluginObject::Create(instance, ppp_class, ppp_class_data);
}

PP_Var CreateObjectWithModuleDeprecated(PP_Module pp_module,
                                        const PPP_Class_Deprecated* ppp_class,
                                        void* ppp_class_data) {
  PluginModule* module = HostGlobals::Get()->GetModule(pp_module);
  if (!module)
    return PP_MakeNull();
  return PluginObject::Create(
      module->GetSomeInstance(), ppp_class, ppp_class_data);
}

}  // namespace

// static
const PPB_Var_Deprecated* PPB_Var_Deprecated_Impl::GetVarDeprecatedInterface() {
  static const PPB_Var_Deprecated var_deprecated_interface = {
      ppapi::PPB_Var_Shared::GetVarInterface1_0()->AddRef,
      ppapi::PPB_Var_Shared::GetVarInterface1_0()->Release,
      ppapi::PPB_Var_Shared::GetVarInterface1_0()->VarFromUtf8,
      ppapi::PPB_Var_Shared::GetVarInterface1_0()->VarToUtf8,
      &HasPropertyDeprecated,
      &HasMethodDeprecated,
      &GetProperty,
      &EnumerateProperties,
      &SetPropertyDeprecated,
      &DeletePropertyDeprecated,
      &CallDeprecated,
      &Construct,
      &IsInstanceOfDeprecated,
      &CreateObjectDeprecated,
      &CreateObjectWithModuleDeprecated, };

  return &var_deprecated_interface;
}

}  // namespace content
