// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPP_INSTANCE_PRIVATE_PROXY_H_
#define PPAPI_PROXY_PPP_INSTANCE_PRIVATE_PROXY_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/private/ppp_instance_private.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/shared_impl/host_resource.h"

namespace ppapi {
namespace proxy {

class SerializedVarReturnValue;

class PPP_Instance_Private_Proxy : public InterfaceProxy {
 public:
  explicit PPP_Instance_Private_Proxy(Dispatcher* dispatcher);

  PPP_Instance_Private_Proxy(const PPP_Instance_Private_Proxy&) = delete;
  PPP_Instance_Private_Proxy& operator=(const PPP_Instance_Private_Proxy&) =
      delete;

  ~PPP_Instance_Private_Proxy() override;

  static const PPP_Instance_Private* GetProxyInterface();

 private:
  // InterfaceProxy implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;

  // Message handlers.
  void OnMsgGetInstanceObject(PP_Instance instance,
                              SerializedVarReturnValue result);

  // When this proxy is in the plugin side, this value caches the interface
  // pointer so we don't have to retrieve it from the dispatcher each time.
  // In the host, this value is always NULL.
  const PPP_Instance_Private* ppp_instance_private_impl_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPP_INSTANCE_PRIVATE_PROXY_H_
