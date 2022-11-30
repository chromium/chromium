// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPP_INPUT_EVENT_PROXY_H_
#define PPAPI_PROXY_PPP_INPUT_EVENT_PROXY_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/ppp_input_event.h"
#include "ppapi/proxy/interface_proxy.h"

namespace ppapi {

struct InputEventData;

namespace proxy {

class PPP_InputEvent_Proxy : public InterfaceProxy {
 public:
  explicit PPP_InputEvent_Proxy(Dispatcher* dispatcher);

  PPP_InputEvent_Proxy(const PPP_InputEvent_Proxy&) = delete;
  PPP_InputEvent_Proxy& operator=(const PPP_InputEvent_Proxy&) = delete;

  ~PPP_InputEvent_Proxy() override;

  static const PPP_InputEvent* GetProxyInterface();

  // InterfaceProxy implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;

 private:
  // Message handlers.
  void OnMsgHandleInputEvent(PP_Instance instance,
                             const ppapi::InputEventData& data);
  void OnMsgHandleFilteredInputEvent(PP_Instance instance,
                                     const ppapi::InputEventData& data,
                                     PP_Bool* result);

  // When this proxy is in the plugin side, this value caches the interface
  // pointer so we don't have to retrieve it from the dispatcher each time.
  // In the host, this value is always NULL.
  const PPP_InputEvent* ppp_input_event_impl_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPP_INPUT_EVENT_PROXY_H_
