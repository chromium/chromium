// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppp_instance_private_proxy.h"

#include <algorithm>

#include "ppapi/c/pp_var.h"
#include "ppapi/c/private/ppp_instance_private.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/proxy_lock.h"

namespace ppapi {
namespace proxy {

namespace {

PP_Var GetInstanceObject(PP_Instance instance) {
  Dispatcher* dispatcher = HostDispatcher::GetForInstance(instance);
  if (!dispatcher->permissions().HasPermission(PERMISSION_PRIVATE))
    return PP_MakeUndefined();

  ReceiveSerializedVarReturnValue result;
  dispatcher->Send(new PpapiMsg_PPPInstancePrivate_GetInstanceObject(
      API_ID_PPP_INSTANCE_PRIVATE, instance, &result));
  return result.Return(dispatcher);
}

static const PPP_Instance_Private instance_private_interface = {
  &GetInstanceObject
};

}  // namespace

PPP_Instance_Private_Proxy::PPP_Instance_Private_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      ppp_instance_private_impl_(NULL) {
  if (dispatcher->IsPlugin()) {
    ppp_instance_private_impl_ = static_cast<const PPP_Instance_Private*>(
        dispatcher->local_get_interface()(PPP_INSTANCE_PRIVATE_INTERFACE));
  }
}

PPP_Instance_Private_Proxy::~PPP_Instance_Private_Proxy() {
}

// static
const PPP_Instance_Private* PPP_Instance_Private_Proxy::GetProxyInterface() {
  return &instance_private_interface;
}

bool PPP_Instance_Private_Proxy::OnMessageReceived(const IPC::Message& msg) {
  if (!dispatcher()->IsPlugin())
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPP_Instance_Private_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPInstancePrivate_GetInstanceObject,
                        OnMsgGetInstanceObject)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPP_Instance_Private_Proxy::OnMsgGetInstanceObject(
    PP_Instance instance,
    SerializedVarReturnValue result) {
  result.Return(dispatcher(),
                CallWhileUnlocked(ppp_instance_private_impl_->GetInstanceObject,
                                  instance));
}

}  // namespace proxy
}  // namespace ppapi
