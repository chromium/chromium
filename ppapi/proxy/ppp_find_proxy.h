// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPP_FIND_PROXY_H_
#define PPAPI_PROXY_PPP_FIND_PROXY_H_

#include <string>

#include "base/macros.h"
#include "ppapi/c/private/ppp_find_private.h"
#include "ppapi/proxy/interface_proxy.h"

namespace ppapi {

namespace proxy {

class PPP_Find_Proxy : public InterfaceProxy {
 public:
  explicit PPP_Find_Proxy(Dispatcher* dispatcher);

  PPP_Find_Proxy(const PPP_Find_Proxy&) = delete;
  PPP_Find_Proxy& operator=(const PPP_Find_Proxy&) = delete;

  ~PPP_Find_Proxy() override;

  static const PPP_Find_Private* GetProxyInterface();

  // InterfaceProxy implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;

 private:
  // Message handlers.
  void OnPluginMsgStartFind(PP_Instance instance,
                            const std::string& text);
  void OnPluginMsgSelectFindResult(PP_Instance instance,
                                   PP_Bool forward);
  void OnPluginMsgStopFind(PP_Instance instance);

  // When this proxy is in the plugin side, this value caches the interface
  // pointer so we don't have to retrieve it from the dispatcher each time.
  // In the host, this value is always NULL.
  const PPP_Find_Private* ppp_find_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPP_FIND_PROXY_H_
