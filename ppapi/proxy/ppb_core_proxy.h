// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_CORE_PROXY_H_
#define PPAPI_PROXY_PPB_CORE_PROXY_H_

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/shared_impl/host_resource.h"

namespace ppapi {
namespace proxy {

class PPB_Core_Proxy : public InterfaceProxy {
 public:
  explicit PPB_Core_Proxy(Dispatcher* dispatcher);
  ~PPB_Core_Proxy() override;

  static const PPB_Core* GetPPB_Core_Interface();

  // InterfaceProxy implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;

  static const ApiID kApiID = API_ID_PPB_CORE;

 private:
  // Message handlers.
  void OnMsgAddRefResource(const ppapi::HostResource& resource);
  void OnMsgReleaseResource(const ppapi::HostResource& resource);

  // When this proxy is in the host side, this value caches the interface
  // pointer so we don't have to retrieve it from the dispatcher each time.
  // In the plugin, this value is always NULL.
  const PPB_Core* ppb_core_impl_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_CORE_PROXY_H_
