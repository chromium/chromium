// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_TESTING_PROXY_H_
#define PPAPI_PROXY_PPB_TESTING_PROXY_H_

#include <stdint.h>

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/private/ppb_testing_private.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/shared_impl/host_resource.h"

struct PP_Point;

namespace ppapi {

struct InputEventData;

namespace proxy {

class PPB_Testing_Proxy : public InterfaceProxy {
 public:
  explicit PPB_Testing_Proxy(Dispatcher* dispatcher);

  PPB_Testing_Proxy(const PPB_Testing_Proxy&) = delete;
  PPB_Testing_Proxy& operator=(const PPB_Testing_Proxy&) = delete;

  ~PPB_Testing_Proxy() override;

  static const PPB_Testing_Private* GetProxyInterface();

  // InterfaceProxy implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;

 private:
  // Message handlers.
  void OnMsgReadImageData(const ppapi::HostResource& device_context_2d,
                          const ppapi::HostResource& image,
                          const PP_Point& top_left,
                          PP_Bool* result);
  void OnMsgRunMessageLoop(PP_Instance instance);
  void OnMsgQuitMessageLoop(PP_Instance instance);
  void OnMsgGetLiveObjectsForInstance(PP_Instance instance, uint32_t* result);
  void OnMsgPostPowerSaverStatus(PP_Instance instance);
  void OnMsgSubscribeToPowerSaverNotifications(PP_Instance instance);
  void OnMsgSimulateInputEvent(PP_Instance instance,
                               const ppapi::InputEventData& input_event);
  void OnMsgSetMinimumArrayBufferSizeForShmem(uint32_t threshold);

  // When this proxy is in the host side, this value caches the interface
  // pointer so we don't have to retrieve it from the dispatcher each time.
  // In the plugin, this value is always NULL.
  const PPB_Testing_Private* ppb_testing_impl_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_TESTING_PROXY_H_
