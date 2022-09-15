// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppp_class_proxy.h"

#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/dev/ppp_class_deprecated.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/proxy/dispatcher.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/serialized_var.h"
#include "ppapi/shared_impl/api_id.h"
#include "ppapi/shared_impl/proxy_lock.h"

namespace ppapi {
namespace proxy {

namespace {

// PPP_Class in the browser implementation -------------------------------------

// Represents a plugin-implemented class in the browser process. This just
// stores the data necessary to call back the plugin.
struct ObjectProxy {
  ObjectProxy(Dispatcher* d, int64_t p, int64_t ud)
      : dispatcher(d), ppp_class(p), user_data(ud) {}

  Dispatcher* dispatcher;
  int64_t ppp_class;
  int64_t user_data;
};

ObjectProxy* ToObjectProxy(void* data) {
  ObjectProxy* obj = reinterpret_cast<ObjectProxy*>(data);
  if (!obj || !obj->dispatcher)
    return NULL;
  if (!obj->dispatcher->permissions().HasPermission(PERMISSION_FLASH))
    return NULL;
  return obj;
}

bool HasProperty(void* object, PP_Var name, PP_Var* exception) {
  ObjectProxy* obj = ToObjectProxy(object);
  if (!obj)
    return false;

  bool result = false;
  ReceiveSerializedException se(obj->dispatcher, exception);
  obj->dispatcher->Send(new PpapiMsg_PPPClass_HasProperty(
      API_ID_PPP_CLASS, obj->ppp_class, obj->user_data,
      SerializedVarSendInput(obj->dispatcher, name), &se, &result));
  return result;
}

bool HasMethod(void* object, PP_Var name, PP_Var* exception) {
  ObjectProxy* obj = ToObjectProxy(object);
  if (!obj)
    return false;

  bool result = false;
  ReceiveSerializedException se(obj->dispatcher, exception);
  obj->dispatcher->Send(new PpapiMsg_PPPClass_HasMethod(
      API_ID_PPP_CLASS, obj->ppp_class, obj->user_data,
      SerializedVarSendInput(obj->dispatcher, name), &se, &result));
  return result;
}

PP_Var GetProperty(void* object,
                   PP_Var name,
                   PP_Var* exception) {
  ObjectProxy* obj = ToObjectProxy(object);
  if (!obj)
    return PP_MakeUndefined();

  ReceiveSerializedException se(obj->dispatcher, exception);
  ReceiveSerializedVarReturnValue result;
  obj->dispatcher->Send(new PpapiMsg_PPPClass_GetProperty(
      API_ID_PPP_CLASS, obj->ppp_class, obj->user_data,
      SerializedVarSendInput(obj->dispatcher, name), &se, &result));
  return result.Return(obj->dispatcher);
}

void GetAllPropertyNames(void* object,
                         uint32_t* property_count,
                         PP_Var** properties,
                         PP_Var* exception) {
  NOTIMPLEMENTED();
  // TODO(brettw) implement this.
}

void SetProperty(void* object,
                 PP_Var name,
                 PP_Var value,
                 PP_Var* exception) {
  ObjectProxy* obj = ToObjectProxy(object);
  if (!obj)
    return;

  ReceiveSerializedException se(obj->dispatcher, exception);
  obj->dispatcher->Send(new PpapiMsg_PPPClass_SetProperty(
      API_ID_PPP_CLASS, obj->ppp_class, obj->user_data,
      SerializedVarSendInput(obj->dispatcher, name),
      SerializedVarSendInput(obj->dispatcher, value), &se));
}

void RemoveProperty(void* object,
                    PP_Var name,
                    PP_Var* exception) {
  ObjectProxy* obj = ToObjectProxy(object);
  if (!obj)
    return;

  ReceiveSerializedException se(obj->dispatcher, exception);
  obj->dispatcher->Send(new PpapiMsg_PPPClass_RemoveProperty(
      API_ID_PPP_CLASS, obj->ppp_class, obj->user_data,
      SerializedVarSendInput(obj->dispatcher, name), &se));
}

PP_Var Call(void* object,
            PP_Var method_name,
            uint32_t argc,
            PP_Var* argv,
            PP_Var* exception) {
  ObjectProxy* obj = ToObjectProxy(object);
  if (!obj)
    return PP_MakeUndefined();

  ReceiveSerializedVarReturnValue result;
  ReceiveSerializedException se(obj->dispatcher, exception);
  std::vector<SerializedVar> argv_vect;
  SerializedVarSendInput::ConvertVector(obj->dispatcher, argv, argc,
                                        &argv_vect);

  obj->dispatcher->Send(new PpapiMsg_PPPClass_Call(
      API_ID_PPP_CLASS, obj->ppp_class, obj->user_data,
      SerializedVarSendInput(obj->dispatcher, method_name), argv_vect,
      &se, &result));
  return result.Return(obj->dispatcher);
}

PP_Var Construct(void* object,
                 uint32_t argc,
                 PP_Var* argv,
                 PP_Var* exception) {
  ObjectProxy* obj = ToObjectProxy(object);
  if (!obj)
    return PP_MakeUndefined();

  ReceiveSerializedVarReturnValue result;
  ReceiveSerializedException se(obj->dispatcher, exception);
  std::vector<SerializedVar> argv_vect;
  SerializedVarSendInput::ConvertVector(obj->dispatcher, argv, argc,
                                        &argv_vect);

  obj->dispatcher->Send(new PpapiMsg_PPPClass_Construct(
      API_ID_PPP_CLASS,
      obj->ppp_class, obj->user_data, argv_vect, &se, &result));
  return result.Return(obj->dispatcher);
}

void Deallocate(void* object) {
  ObjectProxy* obj = ToObjectProxy(object);
  if (!obj)
    return;

  obj->dispatcher->Send(new PpapiMsg_PPPClass_Deallocate(
      API_ID_PPP_CLASS, obj->ppp_class, obj->user_data));
  delete obj;
}

const PPP_Class_Deprecated class_interface = {
  &HasProperty,
  &HasMethod,
  &GetProperty,
  &GetAllPropertyNames,
  &SetProperty,
  &RemoveProperty,
  &Call,
  &Construct,
  &Deallocate
};

// Plugin helper functions -----------------------------------------------------

// Converts an int64_t object from IPC to a PPP_Class* for calling into the
// plugin's implementation.
const PPP_Class_Deprecated* ToPPPClass(int64_t value) {
  return reinterpret_cast<const PPP_Class_Deprecated*>(
      static_cast<intptr_t>(value));
}

// Converts an int64_t object from IPC to a void* for calling into the plugin's
// implementation as the user data.
void* ToUserData(int64_t value) {
  return reinterpret_cast<void*>(static_cast<intptr_t>(value));
}

}  // namespace

// PPP_Class_Proxy -------------------------------------------------------------

PPP_Class_Proxy::PPP_Class_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher) {
}

PPP_Class_Proxy::~PPP_Class_Proxy() {
}

// static
InterfaceProxy* PPP_Class_Proxy::Create(Dispatcher* dispatcher) {
  return new PPP_Class_Proxy(dispatcher);
}

// static
PP_Var PPP_Class_Proxy::CreateProxiedObject(const PPB_Var_Deprecated* var,
                                            Dispatcher* dispatcher,
                                            PP_Instance instance_id,
                                            int64_t ppp_class,
                                            int64_t class_data) {
  ObjectProxy* object_proxy = new ObjectProxy(dispatcher,
                                              ppp_class, class_data);
  return var->CreateObject(instance_id, &class_interface, object_proxy);
}

// static
PP_Bool PPP_Class_Proxy::IsInstanceOf(const PPB_Var_Deprecated* ppb_var_impl,
                                      const PP_Var& var,
                                      int64_t ppp_class,
                                      int64_t* ppp_class_data) {
  void* proxied_object = NULL;
  if (ppb_var_impl->IsInstanceOf(var,
                                 &class_interface,
                                 &proxied_object)) {
    if (static_cast<ObjectProxy*>(proxied_object)->ppp_class == ppp_class) {
      DCHECK(ppp_class_data);
      *ppp_class_data = static_cast<ObjectProxy*>(proxied_object)->user_data;
      return PP_TRUE;
    }
  }
  return PP_FALSE;
}

bool PPP_Class_Proxy::OnMessageReceived(const IPC::Message& msg) {
  if (!dispatcher()->IsPlugin())
    return false;  // These messages are only valid from host->plugin.

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPP_Class_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPClass_HasProperty,
                        OnMsgHasProperty)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPClass_HasMethod,
                        OnMsgHasMethod)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPClass_GetProperty,
                        OnMsgGetProperty)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPClass_EnumerateProperties,
                        OnMsgEnumerateProperties)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPClass_SetProperty,
                        OnMsgSetProperty)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPClass_Call,
                        OnMsgCall)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPClass_Construct,
                        OnMsgConstruct)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPClass_Deallocate,
                        OnMsgDeallocate)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPP_Class_Proxy::OnMsgHasProperty(int64_t ppp_class,
                                       int64_t object,
                                       SerializedVarReceiveInput property,
                                       SerializedVarOutParam exception,
                                       bool* result) {
  if (!ValidateUserData(ppp_class, object, &exception))
    return;
  *result = CallWhileUnlocked(ToPPPClass(ppp_class)->HasProperty,
                              ToUserData(object),
                              property.Get(dispatcher()),
                              exception.OutParam(dispatcher()));
}

void PPP_Class_Proxy::OnMsgHasMethod(int64_t ppp_class,
                                     int64_t object,
                                     SerializedVarReceiveInput property,
                                     SerializedVarOutParam exception,
                                     bool* result) {
  if (!ValidateUserData(ppp_class, object, &exception))
    return;
  *result = CallWhileUnlocked(ToPPPClass(ppp_class)->HasMethod,
                              ToUserData(object),
                              property.Get(dispatcher()),
                              exception.OutParam(dispatcher()));
}

void PPP_Class_Proxy::OnMsgGetProperty(int64_t ppp_class,
                                       int64_t object,
                                       SerializedVarReceiveInput property,
                                       SerializedVarOutParam exception,
                                       SerializedVarReturnValue result) {
  if (!ValidateUserData(ppp_class, object, &exception))
    return;
  result.Return(dispatcher(), CallWhileUnlocked(
      ToPPPClass(ppp_class)->GetProperty,
      ToUserData(object), property.Get(dispatcher()),
      exception.OutParam(dispatcher())));
}

void PPP_Class_Proxy::OnMsgEnumerateProperties(
    int64_t ppp_class,
    int64_t object,
    std::vector<SerializedVar>* props,
    SerializedVarOutParam exception) {
  if (!ValidateUserData(ppp_class, object, &exception))
    return;
  NOTIMPLEMENTED();
  // TODO(brettw) implement this.
}

void PPP_Class_Proxy::OnMsgSetProperty(int64_t ppp_class,
                                       int64_t object,
                                       SerializedVarReceiveInput property,
                                       SerializedVarReceiveInput value,
                                       SerializedVarOutParam exception) {
  if (!ValidateUserData(ppp_class, object, &exception))
    return;
  CallWhileUnlocked(ToPPPClass(ppp_class)->SetProperty,
      ToUserData(object), property.Get(dispatcher()), value.Get(dispatcher()),
      exception.OutParam(dispatcher()));
}

void PPP_Class_Proxy::OnMsgRemoveProperty(int64_t ppp_class,
                                          int64_t object,
                                          SerializedVarReceiveInput property,
                                          SerializedVarOutParam exception) {
  if (!ValidateUserData(ppp_class, object, &exception))
    return;
  CallWhileUnlocked(ToPPPClass(ppp_class)->RemoveProperty,
      ToUserData(object), property.Get(dispatcher()),
      exception.OutParam(dispatcher()));
}

void PPP_Class_Proxy::OnMsgCall(int64_t ppp_class,
                                int64_t object,
                                SerializedVarReceiveInput method_name,
                                SerializedVarVectorReceiveInput arg_vector,
                                SerializedVarOutParam exception,
                                SerializedVarReturnValue result) {
  if (!ValidateUserData(ppp_class, object, &exception))
    return;
  uint32_t arg_count = 0;
  PP_Var* args = arg_vector.Get(dispatcher(), &arg_count);
  result.Return(dispatcher(), CallWhileUnlocked(ToPPPClass(ppp_class)->Call,
      ToUserData(object), method_name.Get(dispatcher()),
      arg_count, args, exception.OutParam(dispatcher())));
}

void PPP_Class_Proxy::OnMsgConstruct(int64_t ppp_class,
                                     int64_t object,
                                     SerializedVarVectorReceiveInput arg_vector,
                                     SerializedVarOutParam exception,
                                     SerializedVarReturnValue result) {
  if (!ValidateUserData(ppp_class, object, &exception))
    return;
  uint32_t arg_count = 0;
  PP_Var* args = arg_vector.Get(dispatcher(), &arg_count);
  result.Return(dispatcher(), CallWhileUnlocked(
      ToPPPClass(ppp_class)->Construct,
      ToUserData(object), arg_count, args, exception.OutParam(dispatcher())));
}

void PPP_Class_Proxy::OnMsgDeallocate(int64_t ppp_class, int64_t object) {
  if (!ValidateUserData(ppp_class, object, NULL))
    return;
  PluginGlobals::Get()->plugin_var_tracker()->PluginImplementedObjectDestroyed(
      ToUserData(object));
  CallWhileUnlocked(ToPPPClass(ppp_class)->Deallocate, ToUserData(object));
}

bool PPP_Class_Proxy::ValidateUserData(int64_t ppp_class,
                                       int64_t class_data,
                                       SerializedVarOutParam* exception) {
  if (!PluginGlobals::Get()->plugin_var_tracker()->ValidatePluginObjectCall(
          ToPPPClass(ppp_class), ToUserData(class_data))) {
    // Set the exception. This is so the caller will know about the error and
    // also that we won't assert that somebody forgot to call OutParam on the
    // output parameter. Although this exception of "1" won't be very useful
    // this shouldn't happen in normal usage, only when the renderer is being
    // malicious.
    if (exception)
      *exception->OutParam(dispatcher()) = PP_MakeInt32(1);
    return false;
  }
  return true;
}

}  // namespace proxy
}  // namespace ppapi
