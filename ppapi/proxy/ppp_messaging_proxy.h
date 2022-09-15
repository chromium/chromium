// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPP_MESSAGING_PROXY_H_
#define PPAPI_PROXY_PPP_MESSAGING_PROXY_H_

#include "base/compiler_specific.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/ppp_messaging.h"
#include "ppapi/proxy/interface_proxy.h"

namespace ppapi {
namespace proxy {

class SerializedVarReceiveInput;

class PPP_Messaging_Proxy : public InterfaceProxy {
 public:
  PPP_Messaging_Proxy(Dispatcher* dispatcher);

  PPP_Messaging_Proxy(const PPP_Messaging_Proxy&) = delete;
  PPP_Messaging_Proxy& operator=(const PPP_Messaging_Proxy&) = delete;

  ~PPP_Messaging_Proxy() override;

  // InterfaceProxy implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;

 private:
  // Message handlers.
  void OnMsgHandleMessage(PP_Instance instance,
                          SerializedVarReceiveInput data);
  void OnMsgHandleBlockingMessage(PP_Instance instance,
                                  SerializedVarReceiveInput data,
                                  IPC::Message* reply);

  // When this proxy is in the plugin side, this value caches the interface
  // pointer so we don't have to retrieve it from the dispatcher each time.
  // In the host, this value is always NULL.
  const PPP_Messaging* ppp_messaging_impl_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPP_MESSAGING_PROXY_H_
