// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppp_graphics_3d_proxy.h"

#include "build/build_config.h"
#include "ppapi/c/ppp_graphics_3d.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/proxy_lock.h"

namespace ppapi {
namespace proxy {

namespace {

#if !BUILDFLAG(IS_NACL)
void ContextLost(PP_Instance instance) {
  HostDispatcher::GetForInstance(instance)->Send(
      new PpapiMsg_PPPGraphics3D_ContextLost(API_ID_PPP_GRAPHICS_3D, instance));
}

static const PPP_Graphics3D graphics_3d_interface = {
  &ContextLost
};
#else
// The NaCl plugin doesn't need the host side interface - stub it out.
static const PPP_Graphics3D graphics_3d_interface = {};
#endif  // !BUILDFLAG(IS_NACL)

}  // namespace

PPP_Graphics3D_Proxy::PPP_Graphics3D_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      ppp_graphics_3d_impl_(NULL) {
  if (dispatcher->IsPlugin()) {
    ppp_graphics_3d_impl_ = static_cast<const PPP_Graphics3D*>(
        dispatcher->local_get_interface()(PPP_GRAPHICS_3D_INTERFACE));
  }
}

PPP_Graphics3D_Proxy::~PPP_Graphics3D_Proxy() {
}

// static
const PPP_Graphics3D* PPP_Graphics3D_Proxy::GetProxyInterface() {
  return &graphics_3d_interface;
}

bool PPP_Graphics3D_Proxy::OnMessageReceived(const IPC::Message& msg) {
  if (!dispatcher()->IsPlugin())
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPP_Graphics3D_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPGraphics3D_ContextLost,
                        OnMsgContextLost)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPP_Graphics3D_Proxy::OnMsgContextLost(PP_Instance instance) {
  if (ppp_graphics_3d_impl_)
    CallWhileUnlocked(ppp_graphics_3d_impl_->Graphics3DContextLost, instance);
}

}  // namespace proxy
}  // namespace ppapi
