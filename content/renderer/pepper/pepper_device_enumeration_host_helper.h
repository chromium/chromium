// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_DEVICE_ENUMERATION_HOST_HELPER_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_DEVICE_ENUMERATION_HOST_HELPER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "ppapi/c/dev/ppb_device_ref_dev.h"
#include "ppapi/host/host_message_context.h"
#include "url/gurl.h"

namespace ppapi {
struct DeviceRefData;

namespace host {
class ResourceHost;
}

}  // namespace ppapi

namespace IPC {
class Message;
}

namespace content {

// Resource hosts that support device enumeration can use this class to filter
// and process PpapiHostMsg_DeviceEnumeration_* messages.
// TODO(yzshen): Refactor ppapi::host::ResourceMessageFilter to support message
// handling on the same thread, and then derive this class from the filter
// class.
class CONTENT_EXPORT PepperDeviceEnumerationHostHelper {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    using DevicesCallback = base::RepeatingCallback<void(
        const std::vector<ppapi::DeviceRefData>& /* devices */)>;
    using DevicesOnceCallback = base::OnceCallback<void(
        const std::vector<ppapi::DeviceRefData>& /* devices */)>;

    // Enumerates devices of the specified type.
    virtual void EnumerateDevices(PP_DeviceType_Dev type,
                                  DevicesOnceCallback callback) = 0;

    // Starts monitoring devices of the specified |type|. Returns a
    // subscription ID that must be used to stop monitoring for the device
    // |type|. Does not invoke |callback| synchronously. |callback| is invoked
    // when device changes of the specified |type| occur.
    virtual size_t StartMonitoringDevices(PP_DeviceType_Dev type,
                                          const DevicesCallback& callback) = 0;

    // Stops monitoring devices of the specified |type|. The
    // |subscription_id| is the return value of StartMonitoringDevices.
    virtual void StopMonitoringDevices(PP_DeviceType_Dev type,
                                       size_t subscription_id) = 0;
  };

  // |resource_host| and |delegate| must outlive this object.
  PepperDeviceEnumerationHostHelper(ppapi::host::ResourceHost* resource_host,
                                    base::WeakPtr<Delegate> delegate,
                                    PP_DeviceType_Dev device_type,
                                    const GURL& document_url);
  ~PepperDeviceEnumerationHostHelper();

  // Returns true if the message has been handled.
  bool HandleResourceMessage(const IPC::Message& msg,
                             ppapi::host::HostMessageContext* context,
                             int32_t* result);

 private:
  class ScopedEnumerationRequest;
  class ScopedMonitoringRequest;

  // Has a different signature than HandleResourceMessage() in order to utilize
  // message dispatching macros.
  int32_t InternalHandleResourceMessage(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context,
      bool* handled);

  int32_t OnEnumerateDevices(ppapi::host::HostMessageContext* context);
  int32_t OnMonitorDeviceChange(ppapi::host::HostMessageContext* context,
                                uint32_t callback_id);
  int32_t OnStopMonitoringDeviceChange(
      ppapi::host::HostMessageContext* context);

  void OnEnumerateDevicesComplete(
      const std::vector<ppapi::DeviceRefData>& devices);
  void OnNotifyDeviceChange(uint32_t callback_id,
                            const std::vector<ppapi::DeviceRefData>& devices);

  // Non-owning pointers.
  ppapi::host::ResourceHost* resource_host_;
  base::WeakPtr<Delegate> delegate_;

  PP_DeviceType_Dev device_type_;

  std::unique_ptr<ScopedEnumerationRequest> enumerate_;
  std::unique_ptr<ScopedMonitoringRequest> monitor_;

  ppapi::host::ReplyMessageContext enumerate_devices_context_;

  DISALLOW_COPY_AND_ASSIGN(PepperDeviceEnumerationHostHelper);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_DEVICE_ENUMERATION_HOST_HELPER_H_
