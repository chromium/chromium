// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPP_INSTANCE_PROXY_H_
#define PPAPI_PROXY_PPP_INSTANCE_PROXY_H_

#include <memory>
#include <string>
#include <vector>

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/shared_impl/host_resource.h"
#include "ppapi/shared_impl/ppp_instance_combined.h"

namespace ppapi {

struct URLResponseInfoData;
struct ViewData;

namespace proxy {

class PPP_Instance_Proxy : public InterfaceProxy {
 public:
  explicit PPP_Instance_Proxy(Dispatcher* dispatcher);
  ~PPP_Instance_Proxy() override;

  static const PPP_Instance* GetInstanceInterface();

  PPP_Instance_Combined* ppp_instance_target() const {
    return combined_interface_.get();
  }

  // InterfaceProxy implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;

 private:
  // Message handlers.
  void OnPluginMsgDidCreate(PP_Instance instance,
                            const std::vector<std::string>& argn,
                            const std::vector<std::string>& argv,
                            PP_Bool* result);
  void OnPluginMsgDidDestroy(PP_Instance instance);
  void OnPluginMsgDidChangeView(PP_Instance instance,
                                const ViewData& new_data,
                                PP_Bool flash_fullscreen);
  void OnPluginMsgDidChangeFocus(PP_Instance instance, PP_Bool has_focus);
  void OnPluginMsgHandleDocumentLoad(PP_Instance instance,
                                     int pending_loader_host_id,
                                     const URLResponseInfoData& data);

  std::unique_ptr<PPP_Instance_Combined> combined_interface_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPP_INSTANCE_PROXY_H_
