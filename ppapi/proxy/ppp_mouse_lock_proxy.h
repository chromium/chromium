// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPP_MOUSE_LOCK_PROXY_H_
#define PPAPI_PROXY_PPP_MOUSE_LOCK_PROXY_H_

#include "base/compiler_specific.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/ppp_mouse_lock.h"
#include "ppapi/proxy/interface_proxy.h"

namespace ppapi {
namespace proxy {

class PPP_MouseLock_Proxy : public InterfaceProxy {
 public:
  PPP_MouseLock_Proxy(Dispatcher* dispatcher);

  PPP_MouseLock_Proxy(const PPP_MouseLock_Proxy&) = delete;
  PPP_MouseLock_Proxy& operator=(const PPP_MouseLock_Proxy&) = delete;

  ~PPP_MouseLock_Proxy() override;

  static const PPP_MouseLock* GetProxyInterface();

  // InterfaceProxy implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;

 private:
  // Message handlers.
  void OnMsgMouseLockLost(PP_Instance instance);

  // When this proxy is in the plugin side, this value caches the interface
  // pointer so we don't have to retrieve it from the dispatcher each time.
  // In the host, this value is always NULL.
  const PPP_MouseLock* ppp_mouse_lock_impl_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPP_MOUSE_LOCK_PROXY_H_
