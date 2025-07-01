// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppp_mouse_lock_proxy.h"

#include "build/build_config.h"
#include "ppapi/c/ppp_mouse_lock.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/proxy_lock.h"

namespace ppapi {
namespace proxy {

namespace {

#if !BUILDFLAG(IS_NACL)
void MouseLockLost(PP_Instance instance) {
  HostDispatcher* dispatcher = HostDispatcher::GetForInstance(instance);
  if (!dispatcher) {
    // The dispatcher should always be valid.
    NOTREACHED();
  }

  dispatcher->Send(new PpapiMsg_PPPMouseLock_MouseLockLost(
      API_ID_PPP_MOUSE_LOCK, instance));
}

static const PPP_MouseLock mouse_lock_interface = {
  &MouseLockLost
};
#else
// The NaCl plugin doesn't need the host side interface - stub it out.
static const PPP_MouseLock mouse_lock_interface = {};
#endif  // !BUILDFLAG(IS_NACL)

}  // namespace

PPP_MouseLock_Proxy::PPP_MouseLock_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      ppp_mouse_lock_impl_(NULL) {
  if (dispatcher->IsPlugin()) {
    ppp_mouse_lock_impl_ = static_cast<const PPP_MouseLock*>(
        dispatcher->local_get_interface()(PPP_MOUSELOCK_INTERFACE));
  }
}

PPP_MouseLock_Proxy::~PPP_MouseLock_Proxy() {
}

// static
const PPP_MouseLock* PPP_MouseLock_Proxy::GetProxyInterface() {
  return &mouse_lock_interface;
}

bool PPP_MouseLock_Proxy::OnMessageReceived(const IPC::Message& msg) {
  if (!dispatcher()->IsPlugin())
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPP_MouseLock_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPMouseLock_MouseLockLost,
                        OnMsgMouseLockLost)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPP_MouseLock_Proxy::OnMsgMouseLockLost(PP_Instance instance) {
  if (ppp_mouse_lock_impl_)
    CallWhileUnlocked(ppp_mouse_lock_impl_->MouseLockLost, instance);
}

}  // namespace proxy
}  // namespace ppapi
