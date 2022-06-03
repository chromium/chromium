// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_var_deprecated_proxy.h"

#include <stdlib.h>  // For malloc

#include "base/bind.h"
#include "base/notreached.h"
#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/plugin_var_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppp_class_proxy.h"
#include "ppapi/proxy/proxy_object_var.h"
#include "ppapi/proxy/serialized_var.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/ppb_var_shared.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {
namespace proxy {

namespace {

// Used to do get the set-up information for calling a var object. If the
// exception is set, returns NULL. Otherwise, computes the dispatcher for the
// given var object. If the var is not a valid object, returns NULL and sets
// the exception.
PluginDispatcher* CheckExceptionAndGetDispatcher(const PP_Var& object,
                                                 PP_Var* exception) {
  // If an exception is already set, we don't need to do anything, just return
  // an error to the caller.
  if (exception && exception->type != PP_VARTYPE_UNDEFINED)
    return NULL;


  if (object.type == PP_VARTYPE_OBJECT) {
    // Get the dispatcher for the object.
    PluginDispatcher* dispatcher =
        PluginGlobals::Get()->plugin_var_tracker()->
            DispatcherForPluginObject(object);
    if (dispatcher)
      return dispatcher;
  }

  // The object is invalid. This means we can't figure out which dispatcher
  // to use, which is OK because the call will fail anyway. Set the exception.
  if (exception) {
    *exception = StringVar::StringToPPVar(
        std::string("Attempting to use an invalid object"));
  }
  return NULL;
}

// PPB_Var_Deprecated plugin ---------------------------------------------------

bool HasProperty(PP_Var var,
                 PP_Var name,
                 PP_Var* exception) {
  ProxyAutoLock lock;
  Dispatcher* dispatcher = CheckExceptionAndGetDispatcher(var, exception);
  if (!dispatcher)
    return false;

  ReceiveSerializedException se(dispatcher, exception);
  PP_Bool result = PP_FALSE;
  if (!se.IsThrown()) {
    dispatcher->Send(new PpapiHostMsg_PPBVar_HasProperty(
        API_ID_PPB_VAR_DEPRECATED,
        SerializedVarSendInput(dispatcher, var),
        SerializedVarSendInput(dispatcher, name), &se, &result));
  }
  return PP_ToBool(result);
}

bool HasMethod(PP_Var var,
               PP_Var name,
               PP_Var* exception) {
  ProxyAutoLock lock;
  Dispatcher* dispatcher = CheckExceptionAndGetDispatcher(var, exception);
  if (!dispatcher)
    return false;

  ReceiveSerializedException se(dispatcher, exception);
  PP_Bool result = PP_FALSE;
  if (!se.IsThrown()) {
    dispatcher->Send(new PpapiHostMsg_PPBVar_HasMethodDeprecated(
        API_ID_PPB_VAR_DEPRECATED,
        SerializedVarSendInput(dispatcher, var),
        SerializedVarSendInput(dispatcher, name), &se, &result));
  }
  return PP_ToBool(result);
}

PP_Var GetProperty(PP_Var var,
                   PP_Var name,
                   PP_Var* exception) {
  ProxyAutoLock lock;
  Dispatcher* dispatcher = CheckExceptionAndGetDispatcher(var, exception);
  if (!dispatcher)
    return PP_MakeUndefined();

  ReceiveSerializedException se(dispatcher, exception);
  ReceiveSerializedVarReturnValue result;
  if (!se.IsThrown()) {
    dispatcher->Send(new PpapiHostMsg_PPBVar_GetProperty(
        API_ID_PPB_VAR_DEPRECATED,
        SerializedVarSendInput(dispatcher, var),
        SerializedVarSendInput(dispatcher, name), &se, &result));
  }
  return result.Return(dispatcher);
}

void EnumerateProperties(PP_Var var,
                         uint32_t* property_count,
                         PP_Var** properties,
                         PP_Var* exception) {
  ProxyAutoLock lock;
  Dispatcher* dispatcher = CheckExceptionAndGetDispatcher(var, exception);
  if (!dispatcher) {
    *property_count = 0;
    *properties = NULL;
    return;
  }

  ReceiveSerializedVarVectorOutParam out_vector(dispatcher,
                                                property_count, properties);
  ReceiveSerializedException se(dispatcher, exception);
  if (!se.IsThrown()) {
    dispatcher->Send(new PpapiHostMsg_PPBVar_EnumerateProperties(
        API_ID_PPB_VAR_DEPRECATED,
        SerializedVarSendInput(dispatcher, var),
        out_vector.OutParam(), &se));
  }
}

void SetProperty(PP_Var var,
                 PP_Var name,
                 PP_Var value,
                 PP_Var* exception) {
  ProxyAutoLock lock;
  Dispatcher* dispatcher = CheckExceptionAndGetDispatcher(var, exception);
  if (!dispatcher)
    return;

  ReceiveSerializedException se(dispatcher, exception);
  if (!se.IsThrown()) {
    dispatcher->Send(new PpapiHostMsg_PPBVar_SetPropertyDeprecated(
        API_ID_PPB_VAR_DEPRECATED,
        SerializedVarSendInput(dispatcher, var),
        SerializedVarSendInput(dispatcher, name),
        SerializedVarSendInput(dispatcher, value), &se));
  }
}

void RemoveProperty(PP_Var var,
                    PP_Var name,
                    PP_Var* exception) {
  ProxyAutoLock lock;
  Dispatcher* dispatcher = CheckExceptionAndGetDispatcher(var, exception);
  if (!dispatcher)
    return;

  ReceiveSerializedException se(dispatcher, exception);
  PP_Bool result = PP_FALSE;
  if (!se.IsThrown()) {
    dispatcher->Send(new PpapiHostMsg_PPBVar_DeleteProperty(
        API_ID_PPB_VAR_DEPRECATED,
        SerializedVarSendInput(dispatcher, var),
        SerializedVarSendInput(dispatcher, name), &se, &result));
  }
}

PP_Var Call(PP_Var object,
            PP_Var method_name,
            uint32_t argc,
            PP_Var* argv,
            PP_Var* exception) {
  ProxyAutoLock lock;
  Dispatcher* dispatcher = CheckExceptionAndGetDispatcher(object, exception);
  if (!dispatcher)
    return PP_MakeUndefined();

  ReceiveSerializedVarReturnValue result;
  ReceiveSerializedException se(dispatcher, exception);
  if (!se.IsThrown()) {
    std::vector<SerializedVar> argv_vect;
    SerializedVarSendInput::ConvertVector(dispatcher, argv, argc, &argv_vect);

    dispatcher->Send(new PpapiHostMsg_PPBVar_CallDeprecated(
        API_ID_PPB_VAR_DEPRECATED,
        SerializedVarSendInput(dispatcher, object),
        SerializedVarSendInput(dispatcher, method_name), argv_vect,
        &se, &result));
  }
  return result.Return(dispatcher);
}

PP_Var Construct(PP_Var object,
                 uint32_t argc,
                 PP_Var* argv,
                 PP_Var* exception) {
  ProxyAutoLock lock;
  Dispatcher* dispatcher = CheckExceptionAndGetDispatcher(object, exception);
  if (!dispatcher)
    return PP_MakeUndefined();

  ReceiveSerializedVarReturnValue result;
  ReceiveSerializedException se(dispatcher, exception);
  if (!se.IsThrown()) {
    std::vector<SerializedVar> argv_vect;
    SerializedVarSendInput::ConvertVector(dispatcher, argv, argc, &argv_vect);

    dispatcher->Send(new PpapiHostMsg_PPBVar_Construct(
        API_ID_PPB_VAR_DEPRECATED,
        SerializedVarSendInput(dispatcher, object),
        argv_vect, &se, &result));
  }
  return result.Return(dispatcher);
}

bool IsInstanceOf(PP_Var var,
                  const PPP_Class_Deprecated* ppp_class,
                  void** ppp_class_data) {
  ProxyAutoLock lock;
  Dispatcher* dispatcher = CheckExceptionAndGetDispatcher(var, NULL);
  if (!dispatcher)
    return false;

  PP_Bool result = PP_FALSE;
  int64_t class_int =
      static_cast<int64_t>(reinterpret_cast<intptr_t>(ppp_class));
  int64_t class_data_int = 0;
  dispatcher->Send(new PpapiHostMsg_PPBVar_IsInstanceOfDeprecated(
      API_ID_PPB_VAR_DEPRECATED, SerializedVarSendInput(dispatcher, var),
      class_int, &class_data_int, &result));
  *ppp_class_data =
      reinterpret_cast<void*>(static_cast<intptr_t>(class_data_int));
  return PP_ToBool(result);
}

PP_Var CreateObject(PP_Instance instance,
                    const PPP_Class_Deprecated* ppp_class,
                    void* ppp_class_data) {
  ProxyAutoLock lock;
  Dispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return PP_MakeUndefined();

  PluginVarTracker* tracker = PluginGlobals::Get()->plugin_var_tracker();
  if (tracker->IsPluginImplementedObjectAlive(ppp_class_data))
    return PP_MakeUndefined();  // Object already exists with this user data.

  ReceiveSerializedVarReturnValue result;
  int64_t class_int =
      static_cast<int64_t>(reinterpret_cast<intptr_t>(ppp_class));
  int64_t data_int =
      static_cast<int64_t>(reinterpret_cast<intptr_t>(ppp_class_data));
  dispatcher->Send(new PpapiHostMsg_PPBVar_CreateObjectDeprecated(
      API_ID_PPB_VAR_DEPRECATED, instance, class_int, data_int,
      &result));
  PP_Var ret_var = result.Return(dispatcher);

  // Register this object as being implemented by the plugin.
  if (ret_var.type == PP_VARTYPE_OBJECT) {
    tracker->PluginImplementedObjectCreated(instance, ret_var,
                                            ppp_class, ppp_class_data);
  }
  return ret_var;
}

}  // namespace

PPB_Var_Deprecated_Proxy::PPB_Var_Deprecated_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher), ppb_var_impl_(nullptr) {
  if (!dispatcher->IsPlugin()) {
    ppb_var_impl_ = static_cast<const PPB_Var_Deprecated*>(
        dispatcher->local_get_interface()(PPB_VAR_DEPRECATED_INTERFACE));
  }
}

PPB_Var_Deprecated_Proxy::~PPB_Var_Deprecated_Proxy() {
}

// static
const PPB_Var_Deprecated* PPB_Var_Deprecated_Proxy::GetProxyInterface() {
  static const PPB_Var_Deprecated var_deprecated_interface = {
    ppapi::PPB_Var_Shared::GetVarInterface1_0()->AddRef,
    ppapi::PPB_Var_Shared::GetVarInterface1_0()->Release,
    ppapi::PPB_Var_Shared::GetVarInterface1_0()->VarFromUtf8,
    ppapi::PPB_Var_Shared::GetVarInterface1_0()->VarToUtf8,
    &HasProperty,
    &HasMethod,
    &GetProperty,
    &EnumerateProperties,
    &SetProperty,
    &RemoveProperty,
    &Call,
    &Construct,
    &IsInstanceOf,
    &CreateObject
  };
  return &var_deprecated_interface;
}

bool PPB_Var_Deprecated_Proxy::OnMessageReceived(const IPC::Message& msg) {
  if (!dispatcher()->permissions().HasPermission(PERMISSION_FLASH))
    return false;

  // Prevent the dispatcher from going away during a call to Call or other
  // function that could mutate the DOM. This must happen OUTSIDE of
  // the message handlers since the SerializedVars use the dispatcher upon
  // return of the function (converting the SerializedVarReturnValue/OutParam
  // to a SerializedVar in the destructor).
  ScopedModuleReference death_grip(dispatcher());

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Var_Deprecated_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVar_AddRefObject, OnMsgAddRefObject)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVar_ReleaseObject, OnMsgReleaseObject)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVar_HasProperty,
                        OnMsgHasProperty)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVar_HasMethodDeprecated,
                        OnMsgHasMethodDeprecated)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVar_GetProperty,
                        OnMsgGetProperty)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVar_DeleteProperty,
                        OnMsgDeleteProperty)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVar_EnumerateProperties,
                        OnMsgEnumerateProperties)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVar_SetPropertyDeprecated,
                        OnMsgSetPropertyDeprecated)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVar_CallDeprecated,
                        OnMsgCallDeprecated)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVar_Construct,
                        OnMsgConstruct)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVar_IsInstanceOfDeprecated,
                        OnMsgIsInstanceOfDeprecated)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVar_CreateObjectDeprecated,
                        OnMsgCreateObjectDeprecated)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  // TODO(brettw) handle bad messages!
  return handled;
}

void PPB_Var_Deprecated_Proxy::OnMsgAddRefObject(int64_t object_id) {
  PP_Var var = { PP_VARTYPE_OBJECT };
  var.value.as_id = object_id;
  ppb_var_impl_->AddRef(var);
}

void PPB_Var_Deprecated_Proxy::OnMsgReleaseObject(int64_t object_id) {
  // Ok, so this is super subtle.
  // When the browser side sends a sync IPC message that returns a var, and the
  // plugin wants to give ownership of that var to the browser, dropping all
  // references, it may call ReleaseObject right after returning the result.
  // However, the IPC system doesn't enforce strict ordering of messages in that
  // case, where a message that is set to unblock (e.g. a sync message, or in
  // our case all messages coming from the plugin) that is sent *after* the
  // result may be dispatched on the browser side *before* the sync send
  // returned (see ipc_sync_channel.cc). In this case, that means it could
  // release the object before it is AddRef'ed on the browser side.
  // To work around this, we post a task here, that will not execute before
  // control goes back to the main message loop, that will ensure the sync send
  // has returned and the browser side can take its reference before we Release.
  // Note: if the instance is gone by the time the task is executed, then it
  // will Release the objects itself and this Release will be a NOOP (aside of a
  // spurious warning).
  // TODO(piman): See if we can fix the IPC code to enforce strict ordering, and
  // then remove this.
  PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostNonNestableTask(
      FROM_HERE,
      RunWhileLocked(base::BindOnce(&PPB_Var_Deprecated_Proxy::DoReleaseObject,
                                    task_factory_.GetWeakPtr(), object_id)));
}

void PPB_Var_Deprecated_Proxy::OnMsgHasProperty(
    SerializedVarReceiveInput var,
    SerializedVarReceiveInput name,
    SerializedVarOutParam exception,
    PP_Bool* result) {
  SetAllowPluginReentrancy();
  *result = PP_FromBool(ppb_var_impl_->HasProperty(
      var.Get(dispatcher()),
      name.Get(dispatcher()),
      exception.OutParam(dispatcher())));
}

void PPB_Var_Deprecated_Proxy::OnMsgHasMethodDeprecated(
    SerializedVarReceiveInput var,
    SerializedVarReceiveInput name,
    SerializedVarOutParam exception,
    PP_Bool* result) {
  SetAllowPluginReentrancy();
  *result = PP_FromBool(ppb_var_impl_->HasMethod(
      var.Get(dispatcher()),
      name.Get(dispatcher()),
      exception.OutParam(dispatcher())));
}

void PPB_Var_Deprecated_Proxy::OnMsgGetProperty(
    SerializedVarReceiveInput var,
    SerializedVarReceiveInput name,
    SerializedVarOutParam exception,
    SerializedVarReturnValue result) {
  SetAllowPluginReentrancy();
  result.Return(dispatcher(), ppb_var_impl_->GetProperty(
      var.Get(dispatcher()), name.Get(dispatcher()),
      exception.OutParam(dispatcher())));
}

void PPB_Var_Deprecated_Proxy::OnMsgEnumerateProperties(
    SerializedVarReceiveInput var,
    SerializedVarVectorOutParam props,
    SerializedVarOutParam exception) {
  SetAllowPluginReentrancy();
  ppb_var_impl_->GetAllPropertyNames(var.Get(dispatcher()),
      props.CountOutParam(), props.ArrayOutParam(dispatcher()),
      exception.OutParam(dispatcher()));
}

void PPB_Var_Deprecated_Proxy::OnMsgSetPropertyDeprecated(
    SerializedVarReceiveInput var,
    SerializedVarReceiveInput name,
    SerializedVarReceiveInput value,
    SerializedVarOutParam exception) {
  SetAllowPluginReentrancy();
  ppb_var_impl_->SetProperty(var.Get(dispatcher()),
                                name.Get(dispatcher()),
                                value.Get(dispatcher()),
                                exception.OutParam(dispatcher()));
}

void PPB_Var_Deprecated_Proxy::OnMsgDeleteProperty(
    SerializedVarReceiveInput var,
    SerializedVarReceiveInput name,
    SerializedVarOutParam exception,
    PP_Bool* result) {
  SetAllowPluginReentrancy();
  ppb_var_impl_->RemoveProperty(var.Get(dispatcher()),
                                   name.Get(dispatcher()),
                                   exception.OutParam(dispatcher()));
  // This deprecated function doesn't actually return a value, but we re-use
  // the message from the non-deprecated interface with the return value.
  *result = PP_TRUE;
}

void PPB_Var_Deprecated_Proxy::OnMsgCallDeprecated(
    SerializedVarReceiveInput object,
    SerializedVarReceiveInput method_name,
    SerializedVarVectorReceiveInput arg_vector,
    SerializedVarOutParam exception,
    SerializedVarReturnValue result) {
  SetAllowPluginReentrancy();
  uint32_t arg_count = 0;
  PP_Var* args = arg_vector.Get(dispatcher(), &arg_count);
  result.Return(dispatcher(), ppb_var_impl_->Call(
      object.Get(dispatcher()),
      method_name.Get(dispatcher()),
      arg_count, args,
      exception.OutParam(dispatcher())));
}

void PPB_Var_Deprecated_Proxy::OnMsgConstruct(
    SerializedVarReceiveInput var,
    SerializedVarVectorReceiveInput arg_vector,
    SerializedVarOutParam exception,
    SerializedVarReturnValue result) {
  SetAllowPluginReentrancy();
  uint32_t arg_count = 0;
  PP_Var* args = arg_vector.Get(dispatcher(), &arg_count);
  result.Return(dispatcher(), ppb_var_impl_->Construct(
      var.Get(dispatcher()), arg_count, args,
      exception.OutParam(dispatcher())));
}

void PPB_Var_Deprecated_Proxy::OnMsgIsInstanceOfDeprecated(
    SerializedVarReceiveInput var,
    int64_t ppp_class,
    int64_t* ppp_class_data,
    PP_Bool* result) {
  SetAllowPluginReentrancy();
  *result = PPP_Class_Proxy::IsInstanceOf(ppb_var_impl_,
                                          var.Get(dispatcher()),
                                          ppp_class,
                                          ppp_class_data);
}

void PPB_Var_Deprecated_Proxy::OnMsgCreateObjectDeprecated(
    PP_Instance instance,
    int64_t ppp_class,
    int64_t class_data,
    SerializedVarReturnValue result) {
  SetAllowPluginReentrancy();
  result.Return(dispatcher(), PPP_Class_Proxy::CreateProxiedObject(
      ppb_var_impl_, dispatcher(), instance, ppp_class, class_data));
}

void PPB_Var_Deprecated_Proxy::SetAllowPluginReentrancy() {
  if (dispatcher()->IsPlugin())
    NOTREACHED();
  else
    static_cast<HostDispatcher*>(dispatcher())->set_allow_plugin_reentrancy();
}

void PPB_Var_Deprecated_Proxy::DoReleaseObject(int64_t object_id) {
  PP_Var var = { PP_VARTYPE_OBJECT };
  var.value.as_id = object_id;
  ppb_var_impl_->Release(var);
}

}  // namespace proxy
}  // namespace ppapi
