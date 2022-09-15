// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPP_GRAPHICS_3D_PROXY_H_
#define PPAPI_PROXY_PPP_GRAPHICS_3D_PROXY_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/ppp_graphics_3d.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/shared_impl/host_resource.h"

namespace ppapi {
namespace proxy {

class PPP_Graphics3D_Proxy : public InterfaceProxy {
 public:
  explicit PPP_Graphics3D_Proxy(Dispatcher* dispatcher);

  PPP_Graphics3D_Proxy(const PPP_Graphics3D_Proxy&) = delete;
  PPP_Graphics3D_Proxy& operator=(const PPP_Graphics3D_Proxy&) = delete;

  ~PPP_Graphics3D_Proxy() override;

  static const PPP_Graphics3D* GetProxyInterface();

  // InterfaceProxy implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;

 private:
  // Message handlers.
  void OnMsgContextLost(PP_Instance instance);

  // When this proxy is in the plugin side, this value caches the interface
  // pointer so we don't have to retrieve it from the dispatcher each time.
  // In the host, this value is always NULL.
  const PPP_Graphics3D* ppp_graphics_3d_impl_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPP_GRAPHICS_3D_PROXY_H_
