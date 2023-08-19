// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPP_PRINTING_PROXY_H_
#define PPAPI_PROXY_PPP_PRINTING_PROXY_H_

#include <stdint.h>

#include <vector>

#include "ppapi/c/dev/ppp_printing_dev.h"
#include "ppapi/proxy/interface_proxy.h"

struct PP_PrintPageNumberRange_Dev;

namespace ppapi {

class HostResource;

namespace proxy {

class PPP_Printing_Proxy : public InterfaceProxy {
 public:
  explicit PPP_Printing_Proxy(Dispatcher* dispatcher);

  PPP_Printing_Proxy(const PPP_Printing_Proxy&) = delete;
  PPP_Printing_Proxy& operator=(const PPP_Printing_Proxy&) = delete;

  ~PPP_Printing_Proxy() override;

  static const PPP_Printing_Dev* GetProxyInterface();

  // InterfaceProxy implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;

 private:
  // Message handlers.
  void OnPluginMsgQuerySupportedFormats(PP_Instance instance, uint32_t* result);
  void OnPluginMsgBegin(PP_Instance instance,
                        const PP_PrintSettings_Dev& settings,
                        int32_t* result);
  void OnPluginMsgPrintPages(
      PP_Instance instance,
      const std::vector<PP_PrintPageNumberRange_Dev>& pages,
      HostResource* result);
  void OnPluginMsgEnd(PP_Instance instance);
  void OnPluginMsgIsScalingDisabled(PP_Instance instance, bool* result);

  // When this proxy is in the plugin side, this value caches the interface
  // pointer so we don't have to retrieve it from the dispatcher each time.
  // In the host, this value is always NULL.
  const PPP_Printing_Dev* ppp_printing_impl_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPP_PRINTING_PROXY_H_
