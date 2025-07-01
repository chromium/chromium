// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/device_enumeration_resource_helper.h"

#include <stddef.h>

#include <memory>

#include "base/check.h"
#include "base/functional/bind.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ppapi/c/pp_array_output.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/dispatch_reply_message.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"
#include "ppapi/shared_impl/array_writer.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/ppb_device_ref_shared.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/shared_impl/tracked_callback.h"

namespace ppapi {
namespace proxy {

DeviceEnumerationResourceHelper::DeviceEnumerationResourceHelper(
    PluginResource* owner)
    : owner_(owner),
      pending_enumerate_devices_(false),
      monitor_callback_id_(0),
      monitor_user_data_(NULL) {
}

DeviceEnumerationResourceHelper::~DeviceEnumerationResourceHelper() {
}

int32_t DeviceEnumerationResourceHelper::EnumerateDevices(
    const PP_ArrayOutput& output,
    scoped_refptr<TrackedCallback> callback) {
  if (pending_enumerate_devices_)
    return PP_ERROR_INPROGRESS;

  pending_enumerate_devices_ = true;
  PpapiHostMsg_DeviceEnumeration_EnumerateDevices msg;
  owner_->Call<PpapiPluginMsg_DeviceEnumeration_EnumerateDevicesReply>(
      PluginResource::RENDERER, msg,
      base::BindOnce(
          &DeviceEnumerationResourceHelper::OnPluginMsgEnumerateDevicesReply,
          weak_ptr_factory_.GetWeakPtr(), output, callback));
  return PP_OK_COMPLETIONPENDING;
}

int32_t DeviceEnumerationResourceHelper::EnumerateDevicesSync(
    const PP_ArrayOutput& output) {
  std::vector<DeviceRefData> devices;
  int32_t result =
      owner_->SyncCall<PpapiPluginMsg_DeviceEnumeration_EnumerateDevicesReply>(
          PluginResource::RENDERER,
          PpapiHostMsg_DeviceEnumeration_EnumerateDevices(),
          &devices);

  if (result == PP_OK)
    result = WriteToArrayOutput(devices, output);

  return result;
}

int32_t DeviceEnumerationResourceHelper::MonitorDeviceChange(
    PP_MonitorDeviceChangeCallback callback,
    void* user_data) {
  monitor_callback_id_++;
  monitor_user_data_ = user_data;
  if (callback) {
    monitor_callback_.reset(
        ThreadAwareCallback<PP_MonitorDeviceChangeCallback>::Create(callback));
    if (!monitor_callback_.get())
      return PP_ERROR_NO_MESSAGE_LOOP;

    owner_->Post(PluginResource::RENDERER,
                 PpapiHostMsg_DeviceEnumeration_MonitorDeviceChange(
                     monitor_callback_id_));
  } else {
    monitor_callback_.reset(NULL);

    owner_->Post(PluginResource::RENDERER,
                 PpapiHostMsg_DeviceEnumeration_StopMonitoringDeviceChange());
  }
  return PP_OK;
}

bool DeviceEnumerationResourceHelper::HandleReply(
    const ResourceMessageReplyParams& params,
    const IPC::Message& msg) {
  PPAPI_BEGIN_MESSAGE_MAP(DeviceEnumerationResourceHelper, msg)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL(
        PpapiPluginMsg_DeviceEnumeration_NotifyDeviceChange,
        OnPluginMsgNotifyDeviceChange)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL_UNHANDLED(return false)
  PPAPI_END_MESSAGE_MAP()

  return true;
}

void DeviceEnumerationResourceHelper::LastPluginRefWasDeleted() {
  // Make sure that no further notifications are sent to the plugin.
  monitor_callback_id_++;
  monitor_callback_.reset(NULL);
  monitor_user_data_ = NULL;

  // There is no need to do anything with pending callback of
  // EnumerateDevices(), because OnPluginMsgEnumerateDevicesReply*() will handle
  // that properly.
}

void DeviceEnumerationResourceHelper::OnPluginMsgEnumerateDevicesReply(
    const PP_ArrayOutput& output,
    scoped_refptr<TrackedCallback> callback,
    const ResourceMessageReplyParams& params,
    const std::vector<DeviceRefData>& devices) {
  pending_enumerate_devices_ = false;

  // We shouldn't access |output| if the callback has been called, which is
  // possible if the last plugin reference to the corresponding resource has
  // gone away, and the callback has been aborted.
  if (!TrackedCallback::IsPending(callback))
    return;

  int32_t result = params.result();
  if (result == PP_OK)
    result = WriteToArrayOutput(devices, output);

  callback->Run(result);
}

void DeviceEnumerationResourceHelper::OnPluginMsgNotifyDeviceChange(
    const ResourceMessageReplyParams& /* params */,
    uint32_t callback_id,
    const std::vector<DeviceRefData>& devices) {
  if (monitor_callback_id_ != callback_id) {
    // A new callback or NULL has been set.
    return;
  }

  CHECK(monitor_callback_.get());

  std::unique_ptr<PP_Resource[]> elements;
  uint32_t size = static_cast<uint32_t>(devices.size());
  if (size > 0) {
    elements.reset(new PP_Resource[size]);
    for (size_t index = 0; index < size; ++index) {
      PPB_DeviceRef_Shared* device_object = new PPB_DeviceRef_Shared(
          OBJECT_IS_PROXY, owner_->pp_instance(), devices[index]);
      elements[index] = device_object->GetReference();
    }
  }

  monitor_callback_->RunOnTargetThread(monitor_user_data_, size,
                                       elements.get());
  for (size_t index = 0; index < size; ++index)
    PpapiGlobals::Get()->GetResourceTracker()->ReleaseResource(elements[index]);
}

int32_t DeviceEnumerationResourceHelper::WriteToArrayOutput(
    const std::vector<DeviceRefData>& devices,
    const PP_ArrayOutput& output) {
  ArrayWriter writer(output);
  if (!writer.is_valid())
    return PP_ERROR_BADARGUMENT;

  std::vector<scoped_refptr<Resource> > device_resources;
  for (size_t i = 0; i < devices.size(); ++i) {
    device_resources.push_back(new PPB_DeviceRef_Shared(
        OBJECT_IS_PROXY, owner_->pp_instance(), devices[i]));
  }
  if (!writer.StoreResourceVector(device_resources))
    return PP_ERROR_FAILED;

  return PP_OK;
}

}  // namespace proxy
}  // namespace ppapi
