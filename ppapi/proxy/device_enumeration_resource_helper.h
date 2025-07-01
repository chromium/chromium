// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_DEVICE_ENUMERATION_RESOURCE_HELPER_H_
#define PPAPI_PROXY_DEVICE_ENUMERATION_RESOURCE_HELPER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "ppapi/c/dev/ppb_device_ref_dev.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/thread_aware_callback.h"

namespace IPC {
class Message;
}

struct PP_ArrayOutput;

namespace ppapi {

struct DeviceRefData;
class TrackedCallback;

namespace proxy {

class PluginResource;
class ResourceMessageReplyParams;

class PPAPI_PROXY_EXPORT DeviceEnumerationResourceHelper final {
 public:
  // |owner| must outlive this object.
  explicit DeviceEnumerationResourceHelper(PluginResource* owner);

  DeviceEnumerationResourceHelper(const DeviceEnumerationResourceHelper&) =
      delete;
  DeviceEnumerationResourceHelper& operator=(
      const DeviceEnumerationResourceHelper&) = delete;

  ~DeviceEnumerationResourceHelper();

  int32_t EnumerateDevices(const PP_ArrayOutput& output,
                           scoped_refptr<TrackedCallback> callback);
  int32_t EnumerateDevicesSync(const PP_ArrayOutput& output);
  int32_t MonitorDeviceChange(PP_MonitorDeviceChangeCallback callback,
                              void* user_data);

  // Returns true if the message has been handled.
  bool HandleReply(const ResourceMessageReplyParams& params,
                   const IPC::Message& msg);

  void LastPluginRefWasDeleted();

 private:
  void OnPluginMsgEnumerateDevicesReply(
      const PP_ArrayOutput& output,
      scoped_refptr<TrackedCallback> callback,
      const ResourceMessageReplyParams& params,
      const std::vector<DeviceRefData>& devices);
  void OnPluginMsgNotifyDeviceChange(const ResourceMessageReplyParams& params,
                                     uint32_t callback_id,
                                     const std::vector<DeviceRefData>& devices);

  int32_t WriteToArrayOutput(const std::vector<DeviceRefData>& devices,
                             const PP_ArrayOutput& output);

  // Not owned by this object.
  PluginResource* owner_;

  bool pending_enumerate_devices_;

  uint32_t monitor_callback_id_;
  std::unique_ptr<ThreadAwareCallback<PP_MonitorDeviceChangeCallback>>
      monitor_callback_;
  void* monitor_user_data_;
  base::WeakPtrFactory<DeviceEnumerationResourceHelper> weak_ptr_factory_{this};
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_DEVICE_ENUMERATION_RESOURCE_HELPER_H_
