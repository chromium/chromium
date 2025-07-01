// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/cpp/private/var_private.h"

#include <stddef.h>

#include "ppapi/c/dev/ppb_memory_dev.h"
#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/private/instance_private.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/dev/scriptable_object_deprecated.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Var_Deprecated>() {
  return PPB_VAR_DEPRECATED_INTERFACE;
}

}  // namespace

VarPrivate::VarPrivate(const InstanceHandle& instance,
                       deprecated::ScriptableObject* object) {
  if (has_interface<PPB_Var_Deprecated>()) {
    var_ = get_interface<PPB_Var_Deprecated>()->CreateObject(
        instance.pp_instance(), object->GetClass(), object);
  } else {
    var_.type = PP_VARTYPE_NULL;
    var_.padding = 0;
  }
  is_managed_ = true;
}

deprecated::ScriptableObject* VarPrivate::AsScriptableObject() const {
  if (!is_object()) {
    PP_NOTREACHED();
  } else if (has_interface<PPB_Var_Deprecated>()) {
    void* object = NULL;
    if (get_interface<PPB_Var_Deprecated>()->IsInstanceOf(
            var_, deprecated::ScriptableObject::GetClass(), &object)) {
      return reinterpret_cast<deprecated::ScriptableObject*>(object);
    }
  }
  return NULL;
}

bool VarPrivate::HasProperty(const Var& name, Var* exception) const {
  if (!has_interface<PPB_Var_Deprecated>())
    return false;
  return get_interface<PPB_Var_Deprecated>()->HasProperty(
      var_, name.pp_var(), OutException(exception).get());
}

bool VarPrivate::HasMethod(const Var& name, Var* exception) const {
  if (!has_interface<PPB_Var_Deprecated>())
    return false;
  return get_interface<PPB_Var_Deprecated>()->HasMethod(
      var_, name.pp_var(), OutException(exception).get());
}

VarPrivate VarPrivate::GetProperty(const Var& name, Var* exception) const {
  if (!has_interface<PPB_Var_Deprecated>())
    return Var();
  return Var(PassRef(), get_interface<PPB_Var_Deprecated>()->GetProperty(
      var_, name.pp_var(), OutException(exception).get()));
}

void VarPrivate::GetAllPropertyNames(std::vector<Var>* properties,
                                     Var* exception) const {
  if (!has_interface<PPB_Var_Deprecated>())
    return;
  PP_Var* props = NULL;
  uint32_t prop_count = 0;
  get_interface<PPB_Var_Deprecated>()->GetAllPropertyNames(
      var_, &prop_count, &props, OutException(exception).get());
  if (!prop_count)
    return;
  properties->resize(prop_count);
  for (uint32_t i = 0; i < prop_count; ++i) {
    Var temp(PassRef(), props[i]);
    (*properties)[i] = temp;
  }
  const PPB_Memory_Dev* memory_if = static_cast<const PPB_Memory_Dev*>(
      pp::Module::Get()->GetBrowserInterface(PPB_MEMORY_DEV_INTERFACE));
  memory_if->MemFree(props);
}

void VarPrivate::SetProperty(const Var& name, const Var& value,
                             Var* exception) {
  if (!has_interface<PPB_Var_Deprecated>())
    return;
  get_interface<PPB_Var_Deprecated>()->SetProperty(
      var_, name.pp_var(), value.pp_var(), OutException(exception).get());
}

void VarPrivate::RemoveProperty(const Var& name, Var* exception) {
  if (!has_interface<PPB_Var_Deprecated>())
    return;
  get_interface<PPB_Var_Deprecated>()->RemoveProperty(
      var_, name.pp_var(), OutException(exception).get());
}

VarPrivate VarPrivate::Call(const Var& method_name, uint32_t argc, Var* argv,
                            Var* exception) {
  if (!has_interface<PPB_Var_Deprecated>())
    return Var();
  if (argc > 0) {
    std::vector<PP_Var> args;
    args.reserve(argc);
    for (size_t i = 0; i < argc; i++)
      args.push_back(argv[i].pp_var());
    return Var(PassRef(), get_interface<PPB_Var_Deprecated>()->Call(
        var_, method_name.pp_var(), argc, &args[0],
        OutException(exception).get()));
  } else {
    // Don't try to get the address of a vector if it's empty.
    return Var(PassRef(), get_interface<PPB_Var_Deprecated>()->Call(
        var_, method_name.pp_var(), 0, NULL,
        OutException(exception).get()));
  }
}

VarPrivate VarPrivate::Construct(uint32_t argc, Var* argv,
                                 Var* exception) const {
  if (!has_interface<PPB_Var_Deprecated>())
    return Var();
  if (argc > 0) {
    std::vector<PP_Var> args;
    args.reserve(argc);
    for (size_t i = 0; i < argc; i++)
      args.push_back(argv[i].pp_var());
    return Var(PassRef(), get_interface<PPB_Var_Deprecated>()->Construct(
        var_, argc, &args[0], OutException(exception).get()));
  } else {
    // Don't try to get the address of a vector if it's empty.
    return Var(PassRef(), get_interface<PPB_Var_Deprecated>()->Construct(
        var_, 0, NULL, OutException(exception).get()));
  }
}

VarPrivate VarPrivate::Call(const Var& method_name, Var* exception) {
  if (!has_interface<PPB_Var_Deprecated>())
    return Var();
  return Var(PassRef(), get_interface<PPB_Var_Deprecated>()->Call(
      var_, method_name.pp_var(), 0, NULL, OutException(exception).get()));
}

VarPrivate VarPrivate::Call(const Var& method_name, const Var& arg1,
                            Var* exception) {
  if (!has_interface<PPB_Var_Deprecated>())
    return Var();
  PP_Var args[1] = {arg1.pp_var()};
  return Var(PassRef(), get_interface<PPB_Var_Deprecated>()->Call(
      var_, method_name.pp_var(), 1, args, OutException(exception).get()));
}

VarPrivate VarPrivate::Call(const Var& method_name, const Var& arg1,
                            const Var& arg2, Var* exception) {
  if (!has_interface<PPB_Var_Deprecated>())
    return Var();
  PP_Var args[2] = {arg1.pp_var(), arg2.pp_var()};
  return Var(PassRef(), get_interface<PPB_Var_Deprecated>()->Call(
      var_, method_name.pp_var(), 2, args, OutException(exception).get()));
}

VarPrivate VarPrivate::Call(const Var& method_name, const Var& arg1,
                            const Var& arg2, const Var& arg3, Var* exception) {
  if (!has_interface<PPB_Var_Deprecated>())
    return Var();
  PP_Var args[3] = {arg1.pp_var(), arg2.pp_var(), arg3.pp_var()};
  return Var(PassRef(), get_interface<PPB_Var_Deprecated>()->Call(
      var_, method_name.pp_var(), 3, args, OutException(exception).get()));
}

VarPrivate VarPrivate::Call(const Var& method_name, const Var& arg1,
                            const Var& arg2, const Var& arg3, const Var& arg4,
                            Var* exception) {
  if (!has_interface<PPB_Var_Deprecated>())
    return Var();
  PP_Var args[4] = {arg1.pp_var(), arg2.pp_var(), arg3.pp_var(), arg4.pp_var()};
  return Var(PassRef(), get_interface<PPB_Var_Deprecated>()->Call(
      var_, method_name.pp_var(), 4, args, OutException(exception).get()));
}

}  // namespace pp
