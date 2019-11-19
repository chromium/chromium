// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/bluetooth/bluetooth_api.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "components/device_event_log/device_event_log.h"
#include "content/public/browser/browser_thread.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "extensions/browser/api/bluetooth/bluetooth_api_utils.h"
#include "extensions/browser/api/bluetooth/bluetooth_event_router.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/api/bluetooth.h"

#if defined(OS_CHROMEOS)
#include "device/bluetooth/chromeos/bluetooth_utils.h"
#endif

using content::BrowserContext;
using content::BrowserThread;

using device::BluetoothAdapter;
using device::BluetoothDevice;

namespace bluetooth_api = extensions::api::bluetooth;
namespace GetDevice = extensions::api::bluetooth::GetDevice;
namespace GetDevices = extensions::api::bluetooth::GetDevices;

namespace {

const char kInvalidDevice[] = "Invalid device";
const char kStartDiscoveryFailed[] = "Starting discovery failed";
const char kStopDiscoveryFailed[] = "Failed to stop discovery";

extensions::BluetoothEventRouter* GetEventRouter(BrowserContext* context) {
  // Note: |context| is valid on UI thread only.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return extensions::BluetoothAPI::Get(context)->event_router();
}

}  // namespace

namespace extensions {

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<BluetoothAPI>>::DestructorAtExit g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<BluetoothAPI>*
BluetoothAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

// static
BluetoothAPI* BluetoothAPI::Get(BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return GetFactoryInstance()->Get(context);
}

BluetoothAPI::BluetoothAPI(content::BrowserContext* context)
    : browser_context_(context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BLUETOOTH_LOG(EVENT) << "BluetoothAPI: " << browser_context_;
  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(
      this, bluetooth_api::OnAdapterStateChanged::kEventName);
  event_router->RegisterObserver(this,
                                 bluetooth_api::OnDeviceAdded::kEventName);
  event_router->RegisterObserver(this,
                                 bluetooth_api::OnDeviceChanged::kEventName);
  event_router->RegisterObserver(this,
                                 bluetooth_api::OnDeviceRemoved::kEventName);
}

BluetoothAPI::~BluetoothAPI() {
  BLUETOOTH_LOG(EVENT) << "~BluetoothAPI: " << browser_context_;
}

BluetoothEventRouter* BluetoothAPI::event_router() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!event_router_) {
    BLUETOOTH_LOG(EVENT) << "BluetoothAPI: Creating BluetoothEventRouter";
    event_router_.reset(new BluetoothEventRouter(browser_context_));
  }
  return event_router_.get();
}

void BluetoothAPI::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BLUETOOTH_LOG(EVENT) << "BluetoothAPI: Shutdown";
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

void BluetoothAPI::OnListenerAdded(const EventListenerInfo& details) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (event_router()->IsBluetoothSupported())
    event_router()->OnListenerAdded(details);
}

void BluetoothAPI::OnListenerRemoved(const EventListenerInfo& details) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (event_router()->IsBluetoothSupported())
    event_router()->OnListenerRemoved(details);
}

namespace api {

BluetoothGetAdapterStateFunction::~BluetoothGetAdapterStateFunction() = default;

void BluetoothGetAdapterStateFunction::DoWork(
    scoped_refptr<BluetoothAdapter> adapter) {
  bluetooth_api::AdapterState state;
  PopulateAdapterState(*adapter, &state);
  Respond(ArgumentList(bluetooth_api::GetAdapterState::Results::Create(state)));
}

BluetoothGetDevicesFunction::BluetoothGetDevicesFunction() = default;

BluetoothGetDevicesFunction::~BluetoothGetDevicesFunction() = default;

bool BluetoothGetDevicesFunction::CreateParams() {
  params_ = GetDevices::Params::Create(*args_);
  return params_ != nullptr;
}

void BluetoothGetDevicesFunction::DoWork(
    scoped_refptr<BluetoothAdapter> adapter) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<base::ListValue> device_list(new base::ListValue);

  BluetoothAdapter::DeviceList devices;
#if defined(OS_CHROMEOS)
  // Default filter values.
  bluetooth_api::FilterType filter_type =
      bluetooth_api::FilterType::FILTER_TYPE_ALL;
  int limit = 0; /*no limit*/
  if (params_->filter) {
    filter_type = params_->filter->filter_type;
    if (params_->filter->limit)
      limit = *params_->filter->limit;
  }

  devices = device::FilterBluetoothDeviceList(
      adapter->GetDevices(), ToBluetoothDeviceFilterType(filter_type), limit);
#else
  devices = adapter->GetDevices();
#endif

  for (BluetoothAdapter::DeviceList::const_iterator iter = devices.begin();
       iter != devices.end();
       ++iter) {
    const BluetoothDevice* device = *iter;
    DCHECK(device);

    bluetooth_api::Device extension_device;
    bluetooth_api::BluetoothDeviceToApiDevice(*device, &extension_device);

    device_list->Append(extension_device.ToValue());
  }

  Respond(OneArgument(std::move(device_list)));
}

BluetoothGetDeviceFunction::BluetoothGetDeviceFunction() = default;

BluetoothGetDeviceFunction::~BluetoothGetDeviceFunction() = default;

bool BluetoothGetDeviceFunction::CreateParams() {
  params_ = GetDevice::Params::Create(*args_);
  return params_ != nullptr;
}

void BluetoothGetDeviceFunction::DoWork(
    scoped_refptr<BluetoothAdapter> adapter) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  BluetoothDevice* device = adapter->GetDevice(params_->device_address);
  if (device) {
    bluetooth_api::Device extension_device;
    bluetooth_api::BluetoothDeviceToApiDevice(*device, &extension_device);
    Respond(OneArgument(extension_device.ToValue()));
  } else {
    Respond(Error(kInvalidDevice));
  }
}

void BluetoothStartDiscoveryFunction::OnSuccessCallback() {
  Respond(NoArguments());
}

void BluetoothStartDiscoveryFunction::OnErrorCallback() {
  Respond(Error(kStartDiscoveryFailed));
}

void BluetoothStartDiscoveryFunction::DoWork(
    scoped_refptr<BluetoothAdapter> adapter) {
  GetEventRouter(browser_context())
      ->StartDiscoverySession(
          adapter.get(), GetExtensionId(),
          base::Bind(&BluetoothStartDiscoveryFunction::OnSuccessCallback, this),
          base::Bind(&BluetoothStartDiscoveryFunction::OnErrorCallback, this));
}

void BluetoothStopDiscoveryFunction::OnSuccessCallback() {
  Respond(NoArguments());
}

void BluetoothStopDiscoveryFunction::OnErrorCallback() {
  Respond(Error(kStopDiscoveryFailed));
}

void BluetoothStopDiscoveryFunction::DoWork(
    scoped_refptr<BluetoothAdapter> adapter) {
  GetEventRouter(browser_context())
      ->StopDiscoverySession(
          adapter.get(), GetExtensionId(),
          base::Bind(&BluetoothStopDiscoveryFunction::OnSuccessCallback, this),
          base::Bind(&BluetoothStopDiscoveryFunction::OnErrorCallback, this));
}

}  // namespace api
}  // namespace extensions
