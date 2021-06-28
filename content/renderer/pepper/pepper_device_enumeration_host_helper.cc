// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_device_enumeration_host_helper.h"

#include "base/bind.h"
#include "base/check.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ipc/ipc_message.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/ppb_device_ref_shared.h"

using ppapi::host::HostMessageContext;

namespace content {

// Makes sure that StopEnumerateDevices() is called for each EnumerateDevices().
class PepperDeviceEnumerationHostHelper::ScopedEnumerationRequest
    : public base::SupportsWeakPtr<ScopedEnumerationRequest> {
 public:
  // |owner| must outlive this object.
  ScopedEnumerationRequest(PepperDeviceEnumerationHostHelper* owner,
                           Delegate::DevicesOnceCallback callback)
      : callback_(std::move(callback)), requested_(false), sync_call_(false) {
    if (!owner->delegate_) {
      // If no delegate, return an empty list of devices.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &ScopedEnumerationRequest::EnumerateDevicesCallbackBody,
              AsWeakPtr(), std::vector<ppapi::DeviceRefData>()));
      return;
    }

    requested_ = true;

    // Note that the callback passed into
    // PepperDeviceEnumerationHostHelper::Delegate::EnumerateDevices() may be
    // called synchronously. In that case, |callback| may destroy this
    // object. So we don't pass in |callback| directly. Instead, we use
    // EnumerateDevicesCallbackBody() to ensure that we always call |callback|
    // asynchronously.
    sync_call_ = true;
    owner->delegate_->EnumerateDevices(
        owner->device_type_,
        base::BindOnce(&ScopedEnumerationRequest::EnumerateDevicesCallbackBody,
                       AsWeakPtr()));
    sync_call_ = false;
  }

  bool requested() const { return requested_; }

 private:
  void EnumerateDevicesCallbackBody(
      const std::vector<ppapi::DeviceRefData>& devices) {
    if (sync_call_) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &ScopedEnumerationRequest::EnumerateDevicesCallbackBody,
              AsWeakPtr(), devices));
    } else {
      std::move(callback_).Run(devices);
      // This object may have been destroyed at this point.
    }
  }

  PepperDeviceEnumerationHostHelper::Delegate::DevicesOnceCallback callback_;
  bool requested_;
  bool sync_call_;

  DISALLOW_COPY_AND_ASSIGN(ScopedEnumerationRequest);
};

// Makes sure that StopMonitoringDevices() is called for each
// StartMonitoringDevices().
class PepperDeviceEnumerationHostHelper::ScopedMonitoringRequest
    : public base::SupportsWeakPtr<ScopedMonitoringRequest> {
 public:
  // |owner| must outlive this object.
  ScopedMonitoringRequest(PepperDeviceEnumerationHostHelper* owner,
                          Delegate::DevicesCallback callback)
      : owner_(owner),
        callback_(std::move(callback)),
        requested_(false),
        subscription_id_(0u) {
    DCHECK(owner_);
    if (!owner->delegate_) {
      return;
    }

    requested_ = true;

    // |callback| is never called synchronously by StartMonitoringDevices(),
    // so it is OK to pass it directly, even if |callback| destroys |this|.
    subscription_id_ = owner_->delegate_->StartMonitoringDevices(
        owner_->device_type_, callback_);
  }

  ~ScopedMonitoringRequest() {
    if (requested_ && owner_->delegate_) {
      owner_->delegate_->StopMonitoringDevices(owner_->device_type_,
                                               subscription_id_);
    }
  }

  bool requested() const { return requested_; }

 private:
  PepperDeviceEnumerationHostHelper* const owner_;
  PepperDeviceEnumerationHostHelper::Delegate::DevicesCallback callback_;
  bool requested_;
  size_t subscription_id_;

  DISALLOW_COPY_AND_ASSIGN(ScopedMonitoringRequest);
};

PepperDeviceEnumerationHostHelper::PepperDeviceEnumerationHostHelper(
    ppapi::host::ResourceHost* resource_host,
    base::WeakPtr<Delegate> delegate,
    PP_DeviceType_Dev device_type,
    const GURL& document_url)
    : resource_host_(resource_host),
      delegate_(delegate),
      device_type_(device_type) {}

PepperDeviceEnumerationHostHelper::~PepperDeviceEnumerationHostHelper() {}

bool PepperDeviceEnumerationHostHelper::HandleResourceMessage(
    const IPC::Message& msg,
    HostMessageContext* context,
    int32_t* result) {
  bool return_value = false;
  *result = InternalHandleResourceMessage(msg, context, &return_value);
  return return_value;
}

int32_t PepperDeviceEnumerationHostHelper::InternalHandleResourceMessage(
    const IPC::Message& msg,
    HostMessageContext* context,
    bool* handled) {
  *handled = true;
  PPAPI_BEGIN_MESSAGE_MAP(PepperDeviceEnumerationHostHelper, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(
        PpapiHostMsg_DeviceEnumeration_EnumerateDevices, OnEnumerateDevices)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_DeviceEnumeration_MonitorDeviceChange,
        OnMonitorDeviceChange)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(
        PpapiHostMsg_DeviceEnumeration_StopMonitoringDeviceChange,
        OnStopMonitoringDeviceChange)
  PPAPI_END_MESSAGE_MAP()

  *handled = false;
  return PP_ERROR_FAILED;
}

int32_t PepperDeviceEnumerationHostHelper::OnEnumerateDevices(
    HostMessageContext* context) {
  if (enumerate_devices_context_.is_valid())
    return PP_ERROR_INPROGRESS;

  enumerate_ = std::make_unique<ScopedEnumerationRequest>(
      this, base::BindOnce(
                &PepperDeviceEnumerationHostHelper::OnEnumerateDevicesComplete,
                base::Unretained(this)));
  if (!enumerate_->requested())
    return PP_ERROR_FAILED;

  enumerate_devices_context_ = context->MakeReplyMessageContext();
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperDeviceEnumerationHostHelper::OnMonitorDeviceChange(
    HostMessageContext* /* context */,
    uint32_t callback_id) {
  monitor_ = std::make_unique<ScopedMonitoringRequest>(
      this, base::BindRepeating(
                &PepperDeviceEnumerationHostHelper::OnNotifyDeviceChange,
                base::Unretained(this), callback_id));

  return monitor_->requested() ? PP_OK : PP_ERROR_FAILED;
}

int32_t PepperDeviceEnumerationHostHelper::OnStopMonitoringDeviceChange(
    HostMessageContext* /* context */) {
  monitor_.reset(nullptr);
  return PP_OK;
}

void PepperDeviceEnumerationHostHelper::OnEnumerateDevicesComplete(
    const std::vector<ppapi::DeviceRefData>& devices) {
  DCHECK(enumerate_devices_context_.is_valid());

  enumerate_.reset(nullptr);

  enumerate_devices_context_.params.set_result(PP_OK);
  resource_host_->host()->SendReply(
      enumerate_devices_context_,
      PpapiPluginMsg_DeviceEnumeration_EnumerateDevicesReply(devices));
  enumerate_devices_context_ = ppapi::host::ReplyMessageContext();
}

void PepperDeviceEnumerationHostHelper::OnNotifyDeviceChange(
    uint32_t callback_id,
    const std::vector<ppapi::DeviceRefData>& devices) {
  resource_host_->host()->SendUnsolicitedReply(
      resource_host_->pp_resource(),
      PpapiPluginMsg_DeviceEnumeration_NotifyDeviceChange(callback_id,
                                                          devices));
}

}  // namespace content
