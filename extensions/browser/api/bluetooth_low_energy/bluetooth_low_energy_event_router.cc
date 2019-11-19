// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/bluetooth_low_energy/bluetooth_low_energy_event_router.h"

#include <algorithm>
#include <iterator>
#include <unordered_set>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_gatt_service.h"
#include "device/bluetooth/bluetooth_local_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_local_gatt_descriptor.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/api/bluetooth_low_energy/bluetooth_low_energy_connection.h"
#include "extensions/browser/api/bluetooth_low_energy/bluetooth_low_energy_notify_session.h"
#include "extensions/browser/api/bluetooth_low_energy/utils.h"
#include "extensions/browser/event_listener_map.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/api/bluetooth/bluetooth_manifest_data.h"
#include "extensions/common/extension.h"

using content::BrowserThread;

using device::BluetoothAdapter;
using device::BluetoothAdapterFactory;
using device::BluetoothDevice;
using device::BluetoothRemoteGattCharacteristic;
using device::BluetoothGattConnection;
using device::BluetoothRemoteGattDescriptor;
using device::BluetoothRemoteGattService;

namespace apibtle = extensions::api::bluetooth_low_energy;

namespace {

void PopulateService(const BluetoothRemoteGattService* service,
                     apibtle::Service* out) {
  DCHECK(out);

  out->uuid = service->GetUUID().canonical_value();
  out->is_primary = service->IsPrimary();
  out->instance_id.reset(new std::string(service->GetIdentifier()));

  if (!service->GetDevice())
    return;

  out->device_address.reset(
      new std::string(service->GetDevice()->GetAddress()));
}

void PopulateCharacteristicProperties(
    BluetoothRemoteGattCharacteristic::Properties properties,
    std::vector<apibtle::CharacteristicProperty>* api_properties) {
  DCHECK(api_properties && api_properties->empty());

  if (properties == BluetoothRemoteGattCharacteristic::PROPERTY_NONE)
    return;

  if (properties & BluetoothRemoteGattCharacteristic::PROPERTY_BROADCAST)
    api_properties->push_back(apibtle::CHARACTERISTIC_PROPERTY_BROADCAST);
  if (properties & BluetoothRemoteGattCharacteristic::PROPERTY_READ)
    api_properties->push_back(apibtle::CHARACTERISTIC_PROPERTY_READ);
  if (properties &
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITE_WITHOUT_RESPONSE) {
    api_properties->push_back(
        apibtle::CHARACTERISTIC_PROPERTY_WRITEWITHOUTRESPONSE);
  }
  if (properties & BluetoothRemoteGattCharacteristic::PROPERTY_WRITE)
    api_properties->push_back(apibtle::CHARACTERISTIC_PROPERTY_WRITE);
  if (properties & BluetoothRemoteGattCharacteristic::PROPERTY_NOTIFY)
    api_properties->push_back(apibtle::CHARACTERISTIC_PROPERTY_NOTIFY);
  if (properties & BluetoothRemoteGattCharacteristic::PROPERTY_INDICATE)
    api_properties->push_back(apibtle::CHARACTERISTIC_PROPERTY_INDICATE);
  if (properties &
      BluetoothRemoteGattCharacteristic::PROPERTY_AUTHENTICATED_SIGNED_WRITES) {
    api_properties->push_back(
        apibtle::CHARACTERISTIC_PROPERTY_AUTHENTICATEDSIGNEDWRITES);
  }
  if (properties &
      BluetoothRemoteGattCharacteristic::PROPERTY_EXTENDED_PROPERTIES) {
    api_properties->push_back(
        apibtle::CHARACTERISTIC_PROPERTY_EXTENDEDPROPERTIES);
  }
  if (properties & BluetoothRemoteGattCharacteristic::PROPERTY_RELIABLE_WRITE)
    api_properties->push_back(apibtle::CHARACTERISTIC_PROPERTY_RELIABLEWRITE);
  if (properties &
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITABLE_AUXILIARIES) {
    api_properties->push_back(
        apibtle::CHARACTERISTIC_PROPERTY_WRITABLEAUXILIARIES);
  }
}

void PopulateCharacteristic(
    const BluetoothRemoteGattCharacteristic* characteristic,
    apibtle::Characteristic* out) {
  DCHECK(out);

  out->uuid = characteristic->GetUUID().canonical_value();
  out->instance_id.reset(new std::string(characteristic->GetIdentifier()));

  out->service = std::make_unique<apibtle::Service>();
  PopulateService(characteristic->GetService(), out->service.get());
  PopulateCharacteristicProperties(characteristic->GetProperties(),
                                   &out->properties);

  const std::vector<uint8_t>& value = characteristic->GetValue();
  if (value.empty())
    return;

  out->value.reset(new std::vector<uint8_t>(value));
}

void PopulateDescriptor(const BluetoothRemoteGattDescriptor* descriptor,
                        apibtle::Descriptor* out) {
  DCHECK(out);

  out->uuid = descriptor->GetUUID().canonical_value();
  out->instance_id.reset(new std::string(descriptor->GetIdentifier()));

  out->characteristic = std::make_unique<apibtle::Characteristic>();
  PopulateCharacteristic(descriptor->GetCharacteristic(),
                         out->characteristic.get());

  const std::vector<uint8_t>& value = descriptor->GetValue();
  if (value.empty())
    return;

  out->value.reset(new std::vector<uint8_t>(value));
}

void PopulateDevice(const device::BluetoothDevice* device,
                    apibtle::Request* request) {
  if (!device)
    return;
  request->device.address = device->GetAddress();
  request->device.name.reset(
      new std::string(base::UTF16ToUTF8(device->GetNameForDisplay())));
  request->device.device_class.reset(new int(device->GetBluetoothClass()));
}

typedef extensions::ApiResourceManager<extensions::BluetoothLowEnergyConnection>
    ConnectionResourceManager;
ConnectionResourceManager* GetConnectionResourceManager(
    content::BrowserContext* context) {
  ConnectionResourceManager* manager = ConnectionResourceManager::Get(context);
  DCHECK(manager)
      << "There is no Bluetooth low energy connection manager. "
         "If this assertion is failing during a test, then it is likely that "
         "TestExtensionSystem is failing to provide an instance of "
         "ApiResourceManager<BluetoothLowEnergyConnection>.";
  return manager;
}

typedef extensions::ApiResourceManager<
    extensions::BluetoothLowEnergyNotifySession>
    NotifySessionResourceManager;
NotifySessionResourceManager* GetNotifySessionResourceManager(
    content::BrowserContext* context) {
  NotifySessionResourceManager* manager =
      NotifySessionResourceManager::Get(context);
  DCHECK(manager)
      << "There is no Bluetooth low energy value update session manager."
         "If this assertion is failing during a test, then it is likely that "
         "TestExtensionSystem is failing to provide an instance of "
         "ApiResourceManager<BluetoothLowEnergyNotifySession>.";
  return manager;
}

// Translates GattErrorCodes to RouterError Codes
extensions::BluetoothLowEnergyEventRouter::Status GattErrorToRouterError(
    BluetoothRemoteGattService::GattErrorCode error_code) {
  extensions::BluetoothLowEnergyEventRouter::Status error_status =
      extensions::BluetoothLowEnergyEventRouter::kStatusErrorFailed;
  if (error_code == BluetoothRemoteGattService::GATT_ERROR_IN_PROGRESS) {
    error_status =
        extensions::BluetoothLowEnergyEventRouter::kStatusErrorInProgress;
  } else if (error_code ==
             BluetoothRemoteGattService::GATT_ERROR_INVALID_LENGTH) {
    error_status =
        extensions::BluetoothLowEnergyEventRouter::kStatusErrorInvalidLength;
  } else if (error_code ==
             BluetoothRemoteGattService::GATT_ERROR_NOT_PERMITTED) {
    error_status =
        extensions::BluetoothLowEnergyEventRouter::kStatusErrorPermissionDenied;
  } else if (error_code ==
             BluetoothRemoteGattService::GATT_ERROR_NOT_AUTHORIZED) {
    error_status = extensions::BluetoothLowEnergyEventRouter::
        kStatusErrorInsufficientAuthorization;
  } else if (error_code == BluetoothRemoteGattService::GATT_ERROR_NOT_PAIRED) {
    error_status =
        extensions::BluetoothLowEnergyEventRouter::kStatusErrorHigherSecurity;
  } else if (error_code ==
             BluetoothRemoteGattService::GATT_ERROR_NOT_SUPPORTED) {
    error_status =
        extensions::BluetoothLowEnergyEventRouter::kStatusErrorGattNotSupported;
  }

  return error_status;
}

}  // namespace

namespace extensions {

BluetoothLowEnergyEventRouter::AttributeValueRequest::AttributeValueRequest(
    Delegate::ValueCallback value_callback,
    Delegate::ErrorCallback error_callback) {
  this->type = ATTRIBUTE_READ_REQUEST;
  this->value_callback = std::move(value_callback);
  this->error_callback = std::move(error_callback);
}

BluetoothLowEnergyEventRouter::AttributeValueRequest::AttributeValueRequest(
    base::OnceClosure success_callback,
    Delegate::ErrorCallback error_callback) {
  this->type = ATTRIBUTE_WRITE_REQUEST;
  this->success_callback = std::move(success_callback);
  this->error_callback = std::move(error_callback);
}

BluetoothLowEnergyEventRouter::AttributeValueRequest::~AttributeValueRequest() =
    default;

BluetoothLowEnergyEventRouter::BluetoothLowEnergyEventRouter(
    content::BrowserContext* context)
    : browser_context_(context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(browser_context_);
  VLOG(1) << "Initializing BluetoothLowEnergyEventRouter.";

  if (!IsBluetoothSupported()) {
    VLOG(1) << "Bluetooth not supported on the current platform.";
    return;
  }

  // Register for unload event so we clean up created services for apps that
  // get unloaded.
  extension_registry_observer_.Add(ExtensionRegistry::Get(context));
}

BluetoothLowEnergyEventRouter::~BluetoothLowEnergyEventRouter() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!adapter_.get())
    return;

  // Delete any services owned by any apps. This will also unregister them all.
  for (const auto& services : app_id_to_service_ids_) {
    for (const auto& service_id : services.second) {
      device::BluetoothLocalGattService* service =
          adapter_->GetGattService(service_id);
      if (service)
        service->Delete();
    }
  }

  adapter_->RemoveObserver(this);
  adapter_.reset();
}

bool BluetoothLowEnergyEventRouter::IsBluetoothSupported() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return BluetoothAdapterFactory::IsBluetoothSupported();
}

bool BluetoothLowEnergyEventRouter::InitializeAdapterAndInvokeCallback(
    base::OnceClosure callback) {
  if (!IsBluetoothSupported())
    return false;

  if (adapter_.get()) {
    std::move(callback).Run();
    return true;
  }

  BluetoothAdapterFactory::GetAdapter(
      base::BindOnce(&BluetoothLowEnergyEventRouter::OnGetAdapter,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  return true;
}

bool BluetoothLowEnergyEventRouter::HasAdapter() const {
  return (adapter_.get() != NULL);
}

void BluetoothLowEnergyEventRouter::Connect(
    bool persistent,
    const Extension* extension,
    const std::string& device_address,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!adapter_.get()) {
    VLOG(1) << "BluetoothAdapter not ready.";
    error_callback.Run(kStatusErrorFailed);
    return;
  }

  const std::string extension_id = extension->id();
  const std::string connect_id = extension_id + device_address;

  if (connecting_devices_.count(connect_id) != 0) {
    error_callback.Run(kStatusErrorInProgress);
    return;
  }

  BluetoothLowEnergyConnection* conn =
      FindConnection(extension_id, device_address);
  if (conn) {
    if (conn->GetConnection()->IsConnected()) {
      VLOG(1) << "Application already connected to device: " << device_address;
      error_callback.Run(kStatusErrorAlreadyConnected);
      return;
    }

    // There is a connection object but it's no longer active. Simply remove it.
    RemoveConnection(extension_id, device_address);
  }

  BluetoothDevice* device = adapter_->GetDevice(device_address);
  if (!device) {
    VLOG(1) << "Bluetooth device not found: " << device_address;
    error_callback.Run(kStatusErrorNotFound);
    return;
  }

  connecting_devices_.insert(connect_id);
  device->CreateGattConnection(
      base::Bind(&BluetoothLowEnergyEventRouter::OnCreateGattConnection,
                 weak_ptr_factory_.GetWeakPtr(), persistent, extension_id,
                 device_address, callback),
      base::Bind(&BluetoothLowEnergyEventRouter::OnConnectError,
                 weak_ptr_factory_.GetWeakPtr(), extension_id, device_address,
                 error_callback));
}

void BluetoothLowEnergyEventRouter::Disconnect(
    const Extension* extension,
    const std::string& device_address,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!adapter_.get()) {
    VLOG(1) << "BluetoothAdapter not ready.";
    error_callback.Run(kStatusErrorFailed);
    return;
  }

  const std::string extension_id = extension->id();

  BluetoothLowEnergyConnection* conn =
      FindConnection(extension_id, device_address);
  if (!conn || !conn->GetConnection()->IsConnected()) {
    VLOG(1) << "Application not connected to device: " << device_address;
    error_callback.Run(kStatusErrorNotConnected);
    return;
  }

  conn->GetConnection()->Disconnect();
  VLOG(2) << "GATT connection terminated.";

  if (!RemoveConnection(extension_id, device_address)) {
    VLOG(1) << "The connection was removed before disconnect completed, id: "
            << extension_id << ", device: " << device_address;
  }

  callback.Run();
}

bool BluetoothLowEnergyEventRouter::GetServices(
    const std::string& device_address,
    ServiceList* out_services) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(out_services);
  if (!adapter_.get()) {
    VLOG(1) << "BluetoothAdapter not ready.";
    return false;
  }

  BluetoothDevice* device = adapter_->GetDevice(device_address);
  if (!device) {
    VLOG(1) << "Bluetooth device not found: " << device_address;
    return false;
  }

  out_services->clear();

  for (const BluetoothRemoteGattService* service : device->GetGattServices()) {
    // Populate an API service and add it to the return value.
    apibtle::Service api_service;
    PopulateService(service, &api_service);
    out_services->push_back(std::move(api_service));
  }

  return true;
}

BluetoothLowEnergyEventRouter::Status BluetoothLowEnergyEventRouter::GetService(
    const std::string& instance_id,
    apibtle::Service* out_service) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(out_service);
  if (!adapter_.get()) {
    VLOG(1) << "BluetoothAdapter not ready.";
    return kStatusErrorFailed;
  }

  BluetoothRemoteGattService* gatt_service = FindServiceById(instance_id);
  if (!gatt_service) {
    VLOG(1) << "Service not found: " << instance_id;
    return kStatusErrorNotFound;
  }

  PopulateService(gatt_service, out_service);
  return kStatusSuccess;
}

BluetoothLowEnergyEventRouter::Status
BluetoothLowEnergyEventRouter::GetIncludedServices(
    const std::string& instance_id,
    ServiceList* out_services) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(out_services);
  if (!adapter_.get()) {
    VLOG(1) << "BluetoothAdapter not ready.";
    return kStatusErrorFailed;
  }

  BluetoothRemoteGattService* service = FindServiceById(instance_id);
  if (!service) {
    VLOG(1) << "Service not found: " << instance_id;
    return kStatusErrorNotFound;
  }

  out_services->clear();

  for (const BluetoothRemoteGattService* included :
       service->GetIncludedServices()) {
    // Populate an API service and add it to the return value.
    apibtle::Service api_service;
    PopulateService(included, &api_service);
    out_services->push_back(std::move(api_service));
  }

  return kStatusSuccess;
}

BluetoothLowEnergyEventRouter::Status
BluetoothLowEnergyEventRouter::GetCharacteristics(
    const Extension* extension,
    const std::string& instance_id,
    CharacteristicList* out_characteristics) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(extension);
  DCHECK(out_characteristics);
  if (!adapter_.get()) {
    VLOG(1) << "BlutoothAdapter not ready.";
    return kStatusErrorFailed;
  }

  BluetoothRemoteGattService* service = FindServiceById(instance_id);
  if (!service) {
    VLOG(1) << "Service not found: " << instance_id;
    return kStatusErrorNotFound;
  }

  BluetoothPermissionRequest request(service->GetUUID().value());
  if (!BluetoothManifestData::CheckRequest(extension, request)) {
    VLOG(1) << "App has no permission to access the characteristics of this "
            << " " << instance_id;
    return kStatusErrorPermissionDenied;
  }

  out_characteristics->clear();

  for (const BluetoothRemoteGattCharacteristic* characteristic :
       service->GetCharacteristics()) {
    // Populate an API characteristic and add it to the return value.
    apibtle::Characteristic api_characteristic;
    PopulateCharacteristic(characteristic, &api_characteristic);
    out_characteristics->push_back(std::move(api_characteristic));
  }

  return kStatusSuccess;
}

BluetoothLowEnergyEventRouter::Status
BluetoothLowEnergyEventRouter::GetCharacteristic(
    const Extension* extension,
    const std::string& instance_id,
    apibtle::Characteristic* out_characteristic) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(extension);
  DCHECK(out_characteristic);
  if (!adapter_.get()) {
    VLOG(1) << "BluetoothAdapter not ready.";
    return kStatusErrorFailed;
  }

  BluetoothRemoteGattCharacteristic* characteristic =
      FindCharacteristicById(instance_id);
  if (!characteristic) {
    VLOG(1) << "Characteristic not found: " << instance_id;
    return kStatusErrorNotFound;
  }

  BluetoothPermissionRequest request(
      characteristic->GetService()->GetUUID().value());
  if (!BluetoothManifestData::CheckRequest(extension, request)) {
    VLOG(1) << "App has no permission to access this characteristic: "
            << instance_id;
    return kStatusErrorPermissionDenied;
  }

  PopulateCharacteristic(characteristic, out_characteristic);
  return kStatusSuccess;
}

BluetoothLowEnergyEventRouter::Status
BluetoothLowEnergyEventRouter::GetDescriptors(
    const Extension* extension,
    const std::string& instance_id,
    DescriptorList* out_descriptors) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(extension);
  DCHECK(out_descriptors);
  if (!adapter_.get()) {
    VLOG(1) << "BlutoothAdapter not ready.";
    return kStatusErrorFailed;
  }

  BluetoothRemoteGattCharacteristic* characteristic =
      FindCharacteristicById(instance_id);
  if (!characteristic) {
    VLOG(1) << "Characteristic not found: " << instance_id;
    return kStatusErrorNotFound;
  }

  BluetoothPermissionRequest request(
      characteristic->GetService()->GetUUID().value());
  if (!BluetoothManifestData::CheckRequest(extension, request)) {
    VLOG(1) << "App has no permission to access the descriptors of this "
            << "characteristic: " << instance_id;
    return kStatusErrorPermissionDenied;
  }

  out_descriptors->clear();

  for (const BluetoothRemoteGattDescriptor* descriptor :
       characteristic->GetDescriptors()) {
    // Populate an API descriptor and add it to the return value.
    apibtle::Descriptor api_descriptor;
    PopulateDescriptor(descriptor, &api_descriptor);
    out_descriptors->push_back(std::move(api_descriptor));
  }

  return kStatusSuccess;
}

BluetoothLowEnergyEventRouter::Status
BluetoothLowEnergyEventRouter::GetDescriptor(
    const Extension* extension,
    const std::string& instance_id,
    api::bluetooth_low_energy::Descriptor* out_descriptor) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(extension);
  DCHECK(out_descriptor);
  if (!adapter_.get()) {
    VLOG(1) << "BluetoothAdapter not ready.";
    return kStatusErrorFailed;
  }

  BluetoothRemoteGattDescriptor* descriptor = FindDescriptorById(instance_id);
  if (!descriptor) {
    VLOG(1) << "Descriptor not found: " << instance_id;
    return kStatusErrorNotFound;
  }

  BluetoothPermissionRequest request(
      descriptor->GetCharacteristic()->GetService()->GetUUID().value());
  if (!BluetoothManifestData::CheckRequest(extension, request)) {
    VLOG(1) << "App has no permission to access this descriptor: "
            << instance_id;
    return kStatusErrorPermissionDenied;
  }

  PopulateDescriptor(descriptor, out_descriptor);
  return kStatusSuccess;
}

void BluetoothLowEnergyEventRouter::ReadCharacteristicValue(
    const Extension* extension,
    const std::string& instance_id,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(extension);
  if (!adapter_.get()) {
    VLOG(1) << "BluetoothAdapter not ready.";
    error_callback.Run(kStatusErrorFailed);
    return;
  }

  BluetoothRemoteGattCharacteristic* characteristic =
      FindCharacteristicById(instance_id);
  if (!characteristic) {
    VLOG(1) << "Characteristic not found: " << instance_id;
    error_callback.Run(kStatusErrorNotFound);
    return;
  }

  BluetoothPermissionRequest request(
      characteristic->GetService()->GetUUID().value());
  if (!BluetoothManifestData::CheckRequest(extension, request)) {
    VLOG(1) << "App has no permission to access this characteristic: "
            << instance_id;
    error_callback.Run(kStatusErrorPermissionDenied);
    return;
  }

  characteristic->ReadRemoteCharacteristic(
      base::Bind(
          &BluetoothLowEnergyEventRouter::OnReadRemoteCharacteristicSuccess,
          weak_ptr_factory_.GetWeakPtr(), instance_id, callback),
      base::Bind(&BluetoothLowEnergyEventRouter::OnError,
                 weak_ptr_factory_.GetWeakPtr(), error_callback));
}

void BluetoothLowEnergyEventRouter::WriteCharacteristicValue(
    const Extension* extension,
    const std::string& instance_id,
    const std::vector<uint8_t>& value,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(extension);
  if (!adapter_.get()) {
    VLOG(1) << "BluetoothAdapter not ready.";
    error_callback.Run(kStatusErrorFailed);
    return;
  }

  BluetoothRemoteGattCharacteristic* characteristic =
      FindCharacteristicById(instance_id);
  if (!characteristic) {
    VLOG(1) << "Characteristic not found: " << instance_id;
    error_callback.Run(kStatusErrorNotFound);
    return;
  }

  BluetoothPermissionRequest request(
      characteristic->GetService()->GetUUID().value());
  if (!BluetoothManifestData::CheckRequest(extension, request)) {
    VLOG(1) << "App has no permission to access this characteristic: "
            << instance_id;
    error_callback.Run(kStatusErrorPermissionDenied);
    return;
  }

  characteristic->WriteRemoteCharacteristic(
      value, callback,
      base::Bind(&BluetoothLowEnergyEventRouter::OnError,
                 weak_ptr_factory_.GetWeakPtr(), error_callback));
}

void BluetoothLowEnergyEventRouter::StartCharacteristicNotifications(
    bool persistent,
    const Extension* extension,
    const std::string& instance_id,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!adapter_.get()) {
    VLOG(1) << "BluetoothAdapter not ready.";
    error_callback.Run(kStatusErrorFailed);
    return;
  }

  const std::string extension_id = extension->id();
  const std::string session_id = extension_id + instance_id;

  if (pending_session_calls_.count(session_id) != 0) {
    error_callback.Run(kStatusErrorInProgress);
    return;
  }

  BluetoothLowEnergyNotifySession* session =
      FindNotifySession(extension_id, instance_id);
  if (session) {
    if (session->GetSession()->IsActive()) {
      VLOG(1) << "Application has already enabled notifications from "
              << "characteristic: " << instance_id;
      error_callback.Run(kStatusErrorAlreadyNotifying);
      return;
    }

    RemoveNotifySession(extension_id, instance_id);
  }

  BluetoothRemoteGattCharacteristic* characteristic =
      FindCharacteristicById(instance_id);
  if (!characteristic) {
    VLOG(1) << "Characteristic not found: " << instance_id;
    error_callback.Run(kStatusErrorNotFound);
    return;
  }

  BluetoothPermissionRequest request(
      characteristic->GetService()->GetUUID().value());
  if (!BluetoothManifestData::CheckRequest(extension, request)) {
    VLOG(1) << "App has no permission to access this characteristic: "
            << instance_id;
    error_callback.Run(kStatusErrorPermissionDenied);
    return;
  }

  pending_session_calls_.insert(session_id);
  characteristic->StartNotifySession(
      base::Bind(&BluetoothLowEnergyEventRouter::OnStartNotifySession,
                 weak_ptr_factory_.GetWeakPtr(), persistent, extension_id,
                 instance_id, callback),
      base::Bind(&BluetoothLowEnergyEventRouter::OnStartNotifySessionError,
                 weak_ptr_factory_.GetWeakPtr(), extension_id, instance_id,
                 error_callback));
}

void BluetoothLowEnergyEventRouter::StopCharacteristicNotifications(
    const Extension* extension,
    const std::string& instance_id,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!adapter_.get()) {
    VLOG(1) << "BluetoothAdapter not ready.";
    error_callback.Run(kStatusErrorFailed);
    return;
  }

  const std::string extension_id = extension->id();

  BluetoothLowEnergyNotifySession* session =
      FindNotifySession(extension_id, instance_id);
  if (!session || !session->GetSession()->IsActive()) {
    VLOG(1) << "Application has not enabled notifications from "
            << "characteristic: " << instance_id;
    error_callback.Run(kStatusErrorNotNotifying);
    return;
  }

  session->GetSession()->Stop(base::Bind(
      &BluetoothLowEnergyEventRouter::OnStopNotifySession,
      weak_ptr_factory_.GetWeakPtr(), extension_id, instance_id, callback));
}

void BluetoothLowEnergyEventRouter::ReadDescriptorValue(
    const Extension* extension,
    const std::string& instance_id,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(extension);
  if (!adapter_.get()) {
    VLOG(1) << "BluetoothAdapter not ready.";
    error_callback.Run(kStatusErrorFailed);
    return;
  }

  BluetoothRemoteGattDescriptor* descriptor = FindDescriptorById(instance_id);
  if (!descriptor) {
    VLOG(1) << "Descriptor not found: " << instance_id;
    error_callback.Run(kStatusErrorNotFound);
    return;
  }

  BluetoothPermissionRequest request(
      descriptor->GetCharacteristic()->GetService()->GetUUID().value());
  if (!BluetoothManifestData::CheckRequest(extension, request)) {
    VLOG(1) << "App has no permission to access this descriptor: "
            << instance_id;
    error_callback.Run(kStatusErrorPermissionDenied);
    return;
  }

  descriptor->ReadRemoteDescriptor(
      base::Bind(&BluetoothLowEnergyEventRouter::OnReadRemoteDescriptorSuccess,
                 weak_ptr_factory_.GetWeakPtr(), callback),
      base::Bind(&BluetoothLowEnergyEventRouter::OnError,
                 weak_ptr_factory_.GetWeakPtr(), error_callback));
}

void BluetoothLowEnergyEventRouter::WriteDescriptorValue(
    const Extension* extension,
    const std::string& instance_id,
    const std::vector<uint8_t>& value,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(extension);
  if (!adapter_.get()) {
    VLOG(1) << "BluetoothAdapter not ready.";
    error_callback.Run(kStatusErrorFailed);
    return;
  }

  BluetoothRemoteGattDescriptor* descriptor = FindDescriptorById(instance_id);
  if (!descriptor) {
    VLOG(1) << "Descriptor not found: " << instance_id;
    error_callback.Run(kStatusErrorNotFound);
    return;
  }

  BluetoothPermissionRequest request(
      descriptor->GetCharacteristic()->GetService()->GetUUID().value());
  if (!BluetoothManifestData::CheckRequest(extension, request)) {
    VLOG(1) << "App has no permission to access this descriptor: "
            << instance_id;
    error_callback.Run(kStatusErrorPermissionDenied);
    return;
  }

  descriptor->WriteRemoteDescriptor(
      value, callback,
      base::Bind(&BluetoothLowEnergyEventRouter::OnError,
                 weak_ptr_factory_.GetWeakPtr(), error_callback));
}

void BluetoothLowEnergyEventRouter::SetAdapterForTesting(
    device::BluetoothAdapter* adapter) {
  adapter_ = adapter;
  InitializeIdentifierMappings();
}

void BluetoothLowEnergyEventRouter::GattServiceAdded(
    BluetoothAdapter* adapter,
    BluetoothDevice* device,
    BluetoothRemoteGattService* service) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(adapter, adapter_.get());
  VLOG(2) << "GATT service added: " << service->GetIdentifier();

  DCHECK(service_id_to_device_address_.find(service->GetIdentifier()) ==
         service_id_to_device_address_.end());

  service_id_to_device_address_[service->GetIdentifier()] =
      device->GetAddress();
}

void BluetoothLowEnergyEventRouter::GattServiceRemoved(
    BluetoothAdapter* adapter,
    BluetoothDevice* device,
    BluetoothRemoteGattService* service) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(adapter, adapter_.get());
  VLOG(2) << "GATT service removed: " << service->GetIdentifier();

  DCHECK(service_id_to_device_address_.find(service->GetIdentifier()) !=
         service_id_to_device_address_.end());

  DCHECK(device->GetAddress() ==
         service_id_to_device_address_[service->GetIdentifier()]);
  service_id_to_device_address_.erase(service->GetIdentifier());

  // Signal API event.
  apibtle::Service api_service;
  PopulateService(service, &api_service);

  std::unique_ptr<base::ListValue> args =
      apibtle::OnServiceRemoved::Create(api_service);
  std::unique_ptr<Event> event(
      new Event(events::BLUETOOTH_LOW_ENERGY_ON_SERVICE_REMOVED,
                apibtle::OnServiceRemoved::kEventName, std::move(args)));
  EventRouter::Get(browser_context_)->BroadcastEvent(std::move(event));
}

void BluetoothLowEnergyEventRouter::GattDiscoveryCompleteForService(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattService* service) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(adapter, adapter_.get());
  VLOG(2) << "GATT service discovery complete: " << service->GetIdentifier();

  DCHECK(service_id_to_device_address_.find(service->GetIdentifier()) !=
         service_id_to_device_address_.end());

  // Signal the service added event here.
  apibtle::Service api_service;
  PopulateService(service, &api_service);

  std::unique_ptr<base::ListValue> args =
      apibtle::OnServiceAdded::Create(api_service);
  std::unique_ptr<Event> event(
      new Event(events::BLUETOOTH_LOW_ENERGY_ON_SERVICE_ADDED,
                apibtle::OnServiceAdded::kEventName, std::move(args)));
  EventRouter::Get(browser_context_)->BroadcastEvent(std::move(event));
}

void BluetoothLowEnergyEventRouter::DeviceAddressChanged(
    BluetoothAdapter* adapter,
    BluetoothDevice* device,
    const std::string& old_address) {
  for (auto& iter : service_id_to_device_address_) {
    if (iter.second == old_address)
      service_id_to_device_address_[iter.first] = device->GetAddress();
  }
}

void BluetoothLowEnergyEventRouter::GattServiceChanged(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattService* service) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(adapter, adapter_.get());
  VLOG(2) << "GATT service changed: " << service->GetIdentifier();
  DCHECK(service_id_to_device_address_.find(service->GetIdentifier()) !=
         service_id_to_device_address_.end());

  // Signal API event.
  apibtle::Service api_service;
  PopulateService(service, &api_service);

  DispatchEventToExtensionsWithPermission(
      events::BLUETOOTH_LOW_ENERGY_ON_SERVICE_CHANGED,
      apibtle::OnServiceChanged::kEventName, service->GetUUID(),
      "" /* characteristic_id */,
      apibtle::OnServiceChanged::Create(api_service));
}

void BluetoothLowEnergyEventRouter::GattCharacteristicAdded(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattCharacteristic* characteristic) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(adapter, adapter_.get());
  VLOG(2) << "GATT characteristic added: " << characteristic->GetIdentifier();

  BluetoothRemoteGattService* service = characteristic->GetService();
  DCHECK(service);

  DCHECK(chrc_id_to_service_id_.find(characteristic->GetIdentifier()) ==
         chrc_id_to_service_id_.end());
  DCHECK(service_id_to_device_address_.find(service->GetIdentifier()) !=
         service_id_to_device_address_.end());

  chrc_id_to_service_id_[characteristic->GetIdentifier()] =
      service->GetIdentifier();
}

void BluetoothLowEnergyEventRouter::GattCharacteristicRemoved(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattCharacteristic* characteristic) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(adapter, adapter_.get());
  VLOG(2) << "GATT characteristic removed: " << characteristic->GetIdentifier();

  BluetoothRemoteGattService* service = characteristic->GetService();
  DCHECK(service);

  DCHECK(chrc_id_to_service_id_.find(characteristic->GetIdentifier()) !=
         chrc_id_to_service_id_.end());
  DCHECK(service->GetIdentifier() ==
         chrc_id_to_service_id_[characteristic->GetIdentifier()]);

  chrc_id_to_service_id_.erase(characteristic->GetIdentifier());
}

void BluetoothLowEnergyEventRouter::GattDescriptorAdded(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattDescriptor* descriptor) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(adapter, adapter_.get());
  VLOG(2) << "GATT descriptor added: " << descriptor->GetIdentifier();

  BluetoothRemoteGattCharacteristic* characteristic =
      descriptor->GetCharacteristic();
  DCHECK(characteristic);

  DCHECK(desc_id_to_chrc_id_.find(descriptor->GetIdentifier()) ==
         desc_id_to_chrc_id_.end());
  DCHECK(chrc_id_to_service_id_.find(characteristic->GetIdentifier()) !=
         chrc_id_to_service_id_.end());

  desc_id_to_chrc_id_[descriptor->GetIdentifier()] =
      characteristic->GetIdentifier();
}

void BluetoothLowEnergyEventRouter::GattDescriptorRemoved(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattDescriptor* descriptor) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(adapter, adapter_.get());
  VLOG(2) << "GATT descriptor removed: " << descriptor->GetIdentifier();

  BluetoothRemoteGattCharacteristic* characteristic =
      descriptor->GetCharacteristic();
  DCHECK(characteristic);

  DCHECK(desc_id_to_chrc_id_.find(descriptor->GetIdentifier()) !=
         desc_id_to_chrc_id_.end());
  DCHECK(characteristic->GetIdentifier() ==
         desc_id_to_chrc_id_[descriptor->GetIdentifier()]);

  desc_id_to_chrc_id_.erase(descriptor->GetIdentifier());
}

void BluetoothLowEnergyEventRouter::GattCharacteristicValueChanged(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(adapter, adapter_.get());
  VLOG(2) << "GATT characteristic value changed: "
          << characteristic->GetIdentifier();

  BluetoothRemoteGattService* service = characteristic->GetService();
  DCHECK(service);

  DCHECK(service_id_to_device_address_.find(service->GetIdentifier()) !=
         service_id_to_device_address_.end());
  DCHECK(chrc_id_to_service_id_.find(characteristic->GetIdentifier()) !=
         chrc_id_to_service_id_.end());
  DCHECK(chrc_id_to_service_id_[characteristic->GetIdentifier()] ==
         service->GetIdentifier());

  // Send the event; manually construct the arguments, instead of using
  // apibtle::OnCharacteristicValueChanged::Create, as it doesn't convert
  // lists of enums correctly.
  apibtle::Characteristic api_characteristic;
  PopulateCharacteristic(characteristic, &api_characteristic);
  std::unique_ptr<base::ListValue> args(new base::ListValue());
  args->Append(apibtle::CharacteristicToValue(&api_characteristic));

  DispatchEventToExtensionsWithPermission(
      events::BLUETOOTH_LOW_ENERGY_ON_CHARACTERISTIC_VALUE_CHANGED,
      apibtle::OnCharacteristicValueChanged::kEventName, service->GetUUID(),
      characteristic->GetIdentifier(), std::move(args));
}

void BluetoothLowEnergyEventRouter::GattDescriptorValueChanged(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattDescriptor* descriptor,
    const std::vector<uint8_t>& value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(adapter, adapter_.get());
  VLOG(2) << "GATT descriptor value changed: " << descriptor->GetIdentifier();

  BluetoothRemoteGattCharacteristic* characteristic =
      descriptor->GetCharacteristic();
  DCHECK(characteristic);

  DCHECK(desc_id_to_chrc_id_.find(descriptor->GetIdentifier()) !=
         desc_id_to_chrc_id_.end());
  DCHECK(characteristic->GetIdentifier() ==
         desc_id_to_chrc_id_[descriptor->GetIdentifier()]);

  // Send the event; manually construct the arguments, instead of using
  // apibtle::OnDescriptorValueChanged::Create, as it doesn't convert
  // lists of enums correctly.
  apibtle::Descriptor api_descriptor;
  PopulateDescriptor(descriptor, &api_descriptor);
  std::unique_ptr<base::ListValue> args(new base::ListValue());
  args->Append(apibtle::DescriptorToValue(&api_descriptor));

  DispatchEventToExtensionsWithPermission(
      events::BLUETOOTH_LOW_ENERGY_ON_DESCRIPTOR_VALUE_CHANGED,
      apibtle::OnDescriptorValueChanged::kEventName,
      characteristic->GetService()->GetUUID(), "" /* characteristic_id */,
      std::move(args));
}

void BluetoothLowEnergyEventRouter::OnCharacteristicReadRequest(
    const device::BluetoothDevice* device,
    const device::BluetoothLocalGattCharacteristic* characteristic,
    int offset,
    Delegate::ValueCallback value_callback,
    Delegate::ErrorCallback error_callback) {
  const std::string& service_id = characteristic->GetService()->GetIdentifier();
  if (service_id_to_extension_id_.find(service_id) ==
      service_id_to_extension_id_.end()) {
    LOG(DFATAL) << "Service with ID " << service_id
                << " does not belong to any extension.";
    return;
  }

  const std::string& extension_id = service_id_to_extension_id_.at(service_id);
  apibtle::Request request;
  request.request_id = StoreSentRequest(
      extension_id, std::make_unique<AttributeValueRequest>(
                        std::move(value_callback), std::move(error_callback)));
  PopulateDevice(device, &request);
  DispatchEventToExtension(
      extension_id, events::BLUETOOTH_LOW_ENERGY_ON_CHARACTERISTIC_READ_REQUEST,
      apibtle::OnCharacteristicReadRequest::kEventName,
      apibtle::OnCharacteristicReadRequest::Create(
          request, characteristic->GetIdentifier()));
}

void BluetoothLowEnergyEventRouter::OnCharacteristicWriteRequest(
    const device::BluetoothDevice* device,
    const device::BluetoothLocalGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value,
    int offset,
    base::OnceClosure callback,
    Delegate::ErrorCallback error_callback) {
  const std::string& service_id = characteristic->GetService()->GetIdentifier();
  if (service_id_to_extension_id_.find(service_id) ==
      service_id_to_extension_id_.end()) {
    LOG(DFATAL) << "Service with ID " << service_id
                << " does not belong to any extension.";
    return;
  }

  const std::string& extension_id = service_id_to_extension_id_.at(service_id);

  apibtle::Request request;
  request.request_id = StoreSentRequest(
      extension_id, std::make_unique<AttributeValueRequest>(
                        std::move(callback), std::move(error_callback)));
  request.value = std::make_unique<std::vector<uint8_t>>(value);
  PopulateDevice(device, &request);
  DispatchEventToExtension(
      extension_id,
      events::BLUETOOTH_LOW_ENERGY_ON_CHARACTERISTIC_WRITE_REQUEST,
      apibtle::OnCharacteristicWriteRequest::kEventName,
      apibtle::OnCharacteristicWriteRequest::Create(
          request, characteristic->GetIdentifier()));
}

void BluetoothLowEnergyEventRouter::OnCharacteristicPrepareWriteRequest(
    const device::BluetoothDevice* device,
    const device::BluetoothLocalGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value,
    int offset,
    bool has_subsequent_request,
    base::OnceClosure callback,
    Delegate::ErrorCallback error_callback) {
  // TODO(crbug/856869): Support reliable write.
  OnCharacteristicWriteRequest(device, characteristic, value, offset,
                               std::move(callback), std::move(error_callback));
}

void BluetoothLowEnergyEventRouter::OnDescriptorReadRequest(
    const device::BluetoothDevice* device,
    const device::BluetoothLocalGattDescriptor* descriptor,
    int offset,
    Delegate::ValueCallback value_callback,
    Delegate::ErrorCallback error_callback) {
  const std::string& service_id =
      descriptor->GetCharacteristic()->GetService()->GetIdentifier();
  if (service_id_to_extension_id_.find(service_id) ==
      service_id_to_extension_id_.end()) {
    LOG(DFATAL) << "Service with ID " << service_id
                << " does not belong to any extension.";
    return;
  }

  const std::string& extension_id = service_id_to_extension_id_.at(service_id);

  apibtle::Request request;
  request.request_id = StoreSentRequest(
      extension_id, std::make_unique<AttributeValueRequest>(
                        std::move(value_callback), std::move(error_callback)));
  PopulateDevice(device, &request);
  DispatchEventToExtension(
      extension_id,
      events::BLUETOOTH_LOW_ENERGY_ON_CHARACTERISTIC_WRITE_REQUEST,
      apibtle::OnDescriptorReadRequest::kEventName,
      apibtle::OnDescriptorReadRequest::Create(request,
                                               descriptor->GetIdentifier()));
}

void BluetoothLowEnergyEventRouter::OnDescriptorWriteRequest(
    const device::BluetoothDevice* device,
    const device::BluetoothLocalGattDescriptor* descriptor,
    const std::vector<uint8_t>& value,
    int offset,
    base::OnceClosure callback,
    Delegate::ErrorCallback error_callback) {
  const std::string& service_id =
      descriptor->GetCharacteristic()->GetService()->GetIdentifier();
  if (service_id_to_extension_id_.find(service_id) ==
      service_id_to_extension_id_.end()) {
    LOG(DFATAL) << "Service with ID " << service_id
                << " does not belong to any extension.";
    return;
  }

  const std::string& extension_id = service_id_to_extension_id_.at(service_id);

  apibtle::Request request;
  request.request_id = StoreSentRequest(
      extension_id, std::make_unique<AttributeValueRequest>(
                        std::move(callback), std::move(error_callback)));
  request.value = std::make_unique<std::vector<uint8_t>>(value);
  PopulateDevice(device, &request);
  DispatchEventToExtension(
      extension_id,
      events::BLUETOOTH_LOW_ENERGY_ON_CHARACTERISTIC_WRITE_REQUEST,
      apibtle::OnDescriptorWriteRequest::kEventName,
      apibtle::OnDescriptorWriteRequest::Create(request,
                                                descriptor->GetIdentifier()));
}

void BluetoothLowEnergyEventRouter::OnNotificationsStart(
    const device::BluetoothDevice* device,
    device::BluetoothGattCharacteristic::NotificationType notification_type,
    const device::BluetoothLocalGattCharacteristic* characteristic) {}

void BluetoothLowEnergyEventRouter::OnNotificationsStop(
    const device::BluetoothDevice* device,
    const device::BluetoothLocalGattCharacteristic* characteristic) {}

void BluetoothLowEnergyEventRouter::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  const std::string& app_id = extension->id();
  const auto& services = app_id_to_service_ids_.find(app_id);
  if (services == app_id_to_service_ids_.end())
    return;

  // Find all services owned by this app and delete them.
  for (const auto& service_id : services->second) {
    device::BluetoothLocalGattService* service =
        adapter_->GetGattService(service_id);
    if (service)
      service->Delete();
  }
  app_id_to_service_ids_.erase(services);
}

void BluetoothLowEnergyEventRouter::AddLocalCharacteristic(
    const std::string& id,
    const std::string& service_id) {
  if (chrc_id_to_service_id_.find(id) != chrc_id_to_service_id_.end())
    VLOG(2) << "Local characteristic with id " << id
            << " already exists. Replacing.";
  chrc_id_to_service_id_[id] = service_id;
}

device::BluetoothLocalGattCharacteristic*
BluetoothLowEnergyEventRouter::GetLocalCharacteristic(
    const std::string& id) const {
  if (chrc_id_to_service_id_.find(id) == chrc_id_to_service_id_.end()) {
    VLOG(1) << "Characteristic with id " << id << " not found.";
    return nullptr;
  }
  device::BluetoothLocalGattService* service =
      adapter_->GetGattService(chrc_id_to_service_id_.at(id));
  if (!service) {
    VLOG(1) << "Parent service of characteristic with id " << id
            << " not found.";
    return nullptr;
  }

  return service->GetCharacteristic(id);
}

void BluetoothLowEnergyEventRouter::AddServiceToApp(
    const std::string& app_id,
    const std::string& service_id) {
  const auto& services = app_id_to_service_ids_.find(app_id);
  if (services == app_id_to_service_ids_.end()) {
    std::vector<std::string> service_ids;
    service_ids.push_back(service_id);
    app_id_to_service_ids_[app_id] = service_ids;
  } else {
    services->second.push_back(service_id);
  }
}

void BluetoothLowEnergyEventRouter::RemoveServiceFromApp(
    const std::string& app_id,
    const std::string& service_id) {
  const auto& services = app_id_to_service_ids_.find(app_id);
  if (services == app_id_to_service_ids_.end()) {
    LOG(WARNING) << "No service mapping exists for app: " << app_id;
    return;
  }

  const auto& service =
      find(services->second.begin(), services->second.end(), service_id);
  if (service == services->second.end()) {
    LOG(WARNING) << "Service:" << service_id
                 << " doesn't exist in app: " << app_id;
    return;
  }

  services->second.erase(service);
}

void BluetoothLowEnergyEventRouter::RegisterGattService(
    const Extension* extension,
    const std::string& service_id,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(extension);
  if (!adapter_.get()) {
    VLOG(1) << "BluetoothAdapter not ready.";
    error_callback.Run(kStatusErrorFailed);
    return;
  }

  device::BluetoothLocalGattService* service =
      adapter_->GetGattService(service_id);
  if (!service) {
    error_callback.Run(kStatusErrorInvalidServiceId);
    return;
  }

  service->Register(
      base::Bind(&BluetoothLowEnergyEventRouter::OnRegisterGattServiceSuccess,
                 weak_ptr_factory_.GetWeakPtr(), service_id, extension->id(),
                 callback),
      base::Bind(&BluetoothLowEnergyEventRouter::OnError,
                 weak_ptr_factory_.GetWeakPtr(), error_callback));
}

void BluetoothLowEnergyEventRouter::UnregisterGattService(
    const Extension* extension,
    const std::string& service_id,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(extension);
  if (!adapter_.get()) {
    VLOG(1) << "BluetoothAdapter not ready.";
    error_callback.Run(kStatusErrorFailed);
    return;
  }

  device::BluetoothLocalGattService* service =
      adapter_->GetGattService(service_id);
  if (!service) {
    error_callback.Run(kStatusErrorInvalidServiceId);
    return;
  }

  service->Unregister(
      base::Bind(&BluetoothLowEnergyEventRouter::OnUnregisterGattServiceSuccess,
                 weak_ptr_factory_.GetWeakPtr(), service_id, extension->id(),
                 callback),
      base::Bind(&BluetoothLowEnergyEventRouter::OnError,
                 weak_ptr_factory_.GetWeakPtr(), error_callback));
}

void BluetoothLowEnergyEventRouter::HandleRequestResponse(
    const Extension* extension,
    size_t request_id,
    bool is_error,
    const std::vector<uint8_t>& value) {
  const auto& iter = requests_.find(extension->id());
  if (iter == requests_.end())
    return;

  RequestIdToRequestMap* request_id_map = iter->second.get();
  const auto& request_iter = request_id_map->find(request_id);
  if (request_iter == request_id_map->end())
    return;

  std::unique_ptr<AttributeValueRequest> request =
      std::move(request_iter->second);
  // Request is being handled, delete it.
  request_id_map->erase(request_iter);

  if (is_error) {
    std::move(request->error_callback).Run();
    return;
  }

  if (request->type == AttributeValueRequest::ATTRIBUTE_READ_REQUEST) {
    std::move(request->value_callback).Run(value);
  } else {
    std::move(request->success_callback).Run();
  }
}

void BluetoothLowEnergyEventRouter::OnGetAdapter(
    base::OnceClosure callback,
    scoped_refptr<device::BluetoothAdapter> adapter) {
  adapter_ = adapter;

  // Initialize instance ID mappings for all discovered GATT objects and add
  // observers.
  InitializeIdentifierMappings();
  adapter_->AddObserver(this);

  std::move(callback).Run();
}

void BluetoothLowEnergyEventRouter::InitializeIdentifierMappings() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(service_id_to_device_address_.empty());
  DCHECK(chrc_id_to_service_id_.empty());

  // Devices
  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  for (auto iter = devices.begin(); iter != devices.end(); ++iter) {
    BluetoothDevice* device = *iter;

    // Services
    std::vector<BluetoothRemoteGattService*> services =
        device->GetGattServices();
    for (auto siter = services.begin(); siter != services.end(); ++siter) {
      BluetoothRemoteGattService* service = *siter;

      const std::string& service_id = service->GetIdentifier();
      service_id_to_device_address_[service_id] = device->GetAddress();

      // Characteristics
      const std::vector<BluetoothRemoteGattCharacteristic*>& characteristics =
          service->GetCharacteristics();
      for (auto citer = characteristics.cbegin();
           citer != characteristics.cend(); ++citer) {
        BluetoothRemoteGattCharacteristic* characteristic = *citer;

        const std::string& chrc_id = characteristic->GetIdentifier();
        chrc_id_to_service_id_[chrc_id] = service_id;

        // Descriptors
        const std::vector<BluetoothRemoteGattDescriptor*>& descriptors =
            characteristic->GetDescriptors();
        for (auto diter = descriptors.cbegin(); diter != descriptors.cend();
             ++diter) {
          BluetoothRemoteGattDescriptor* descriptor = *diter;

          const std::string& desc_id = descriptor->GetIdentifier();
          desc_id_to_chrc_id_[desc_id] = chrc_id;
        }
      }
    }
  }
}

void BluetoothLowEnergyEventRouter::DispatchEventToExtensionsWithPermission(
    events::HistogramValue histogram_value,
    const std::string& event_name,
    const device::BluetoothUUID& uuid,
    const std::string& characteristic_id,
    std::unique_ptr<base::ListValue> args) {
  // Obtain the listeners of |event_name|. The list can contain multiple
  // entries for the same extension, so we keep track of the extensions that we
  // already sent the event to, since we want the send an event to an extension
  // only once.
  BluetoothPermissionRequest request(uuid.value());
  std::set<std::string> handled_extensions;
  const EventListenerMap::ListenerList& listeners =
      EventRouter::Get(browser_context_)
          ->listeners()
          .GetEventListenersByName(event_name);

  for (const std::unique_ptr<EventListener>& listener : listeners) {
    const std::string& extension_id = listener->extension_id();
    if (handled_extensions.find(extension_id) != handled_extensions.end())
      continue;

    handled_extensions.insert(extension_id);

    const Extension* extension =
        ExtensionRegistry::Get(browser_context_)
            ->GetExtensionById(extension_id, ExtensionRegistry::EVERYTHING);

    // For all API methods, the "low_energy" permission check is handled by
    // BluetoothLowEnergyExtensionFunction but for events we have to do the
    // check here.
    if (!BluetoothManifestData::CheckRequest(extension, request) ||
        !BluetoothManifestData::CheckLowEnergyPermitted(extension))
      continue;

    // If |event_name| is "onCharacteristicValueChanged", then send the
    // event only if the extension has requested notifications from the
    // related characteristic.
    if (event_name == apibtle::OnCharacteristicValueChanged::kEventName &&
        !characteristic_id.empty() &&
        !FindNotifySession(extension_id, characteristic_id))
      continue;

    // Send the event.
    auto event = std::make_unique<Event>(histogram_value, event_name,
                                         args->CreateDeepCopy());
    EventRouter::Get(browser_context_)
        ->DispatchEventToExtension(extension_id, std::move(event));
  }
}

void BluetoothLowEnergyEventRouter::DispatchEventToExtension(
    const std::string& extension_id,
    events::HistogramValue histogram_value,
    const std::string& event_name,
    std::unique_ptr<base::ListValue> args) {
  // For all API methods, the "low_energy" permission check is handled by
  // BluetoothLowEnergyExtensionFunction but for events we have to do the
  // check here.
  const Extension* extension =
      ExtensionRegistry::Get(browser_context_)
          ->GetExtensionById(extension_id, ExtensionRegistry::EVERYTHING);
  if (!extension || !BluetoothManifestData::CheckLowEnergyPermitted(extension))
    return;

  // Send the event.
  auto event = std::make_unique<Event>(histogram_value, event_name,
                                       args->CreateDeepCopy());
  EventRouter::Get(browser_context_)
      ->DispatchEventToExtension(extension_id, std::move(event));
}

BluetoothRemoteGattService* BluetoothLowEnergyEventRouter::FindServiceById(
    const std::string& instance_id) const {
  auto iter = service_id_to_device_address_.find(instance_id);
  if (iter == service_id_to_device_address_.end()) {
    VLOG(1) << "GATT service identifier unknown: " << instance_id;
    return NULL;
  }

  const std::string& address = iter->second;

  BluetoothDevice* device = adapter_->GetDevice(address);
  if (!device) {
    VLOG(1) << "Bluetooth device not found: " << address;
    return NULL;
  }

  BluetoothRemoteGattService* service = device->GetGattService(instance_id);
  if (!service) {
    VLOG(1) << "GATT service with ID \"" << instance_id
            << "\" not found on device \"" << address << "\"";
    return NULL;
  }

  return service;
}

BluetoothRemoteGattCharacteristic*
BluetoothLowEnergyEventRouter::FindCharacteristicById(
    const std::string& instance_id) const {
  auto iter = chrc_id_to_service_id_.find(instance_id);
  if (iter == chrc_id_to_service_id_.end()) {
    VLOG(1) << "GATT characteristic identifier unknown: " << instance_id;
    return NULL;
  }

  const std::string& service_id = iter->second;

  BluetoothRemoteGattService* service = FindServiceById(service_id);
  if (!service) {
    VLOG(1) << "Failed to obtain service for characteristic: " << instance_id;
    return NULL;
  }

  BluetoothRemoteGattCharacteristic* characteristic =
      service->GetCharacteristic(instance_id);
  if (!characteristic) {
    VLOG(1) << "GATT characteristic with ID \"" << instance_id
            << "\" not found on service \"" << service_id << "\"";
    return NULL;
  }

  return characteristic;
}

BluetoothRemoteGattDescriptor*
BluetoothLowEnergyEventRouter::FindDescriptorById(
    const std::string& instance_id) const {
  auto iter = desc_id_to_chrc_id_.find(instance_id);
  if (iter == desc_id_to_chrc_id_.end()) {
    VLOG(1) << "GATT descriptor identifier unknown: " << instance_id;
    return NULL;
  }

  const std::string& chrc_id = iter->second;
  BluetoothRemoteGattCharacteristic* chrc = FindCharacteristicById(chrc_id);
  if (!chrc) {
    VLOG(1) << "Failed to obtain characteristic for descriptor: "
            << instance_id;
    return NULL;
  }

  BluetoothRemoteGattDescriptor* descriptor = chrc->GetDescriptor(instance_id);
  if (!descriptor) {
    VLOG(1) << "GATT descriptor with ID \"" << instance_id
            << "\" not found on characteristic \"" << chrc_id << "\"";
    return NULL;
  }

  return descriptor;
}

void BluetoothLowEnergyEventRouter::OnReadRemoteCharacteristicSuccess(
    const std::string& characteristic_instance_id,
    const base::Closure& callback,
    const std::vector<uint8_t>& value) {
  VLOG(2) << "Remote characteristic value read successful.";

  BluetoothRemoteGattCharacteristic* characteristic =
      FindCharacteristicById(characteristic_instance_id);

  GattCharacteristicValueChanged(adapter_.get(), characteristic, value);
  callback.Run();
}

void BluetoothLowEnergyEventRouter::OnReadRemoteDescriptorSuccess(
    const base::Closure& callback,
    const std::vector<uint8_t>& value) {
  VLOG(2) << "Remote descriptor value read successful.";
  callback.Run();
}

void BluetoothLowEnergyEventRouter::OnRegisterGattServiceSuccess(
    const std::string& service_id,
    const std::string& extension_id,
    const base::Closure& callback) {
  VLOG(2) << "Register GATT service successful.";
  service_id_to_extension_id_[service_id] = extension_id;
  callback.Run();
}

void BluetoothLowEnergyEventRouter::OnUnregisterGattServiceSuccess(
    const std::string& service_id,
    const std::string& extension_id,
    const base::Closure& callback) {
  VLOG(2) << "Unregister GATT service successful.";
  const auto& iter = service_id_to_extension_id_.find(service_id);
  if (iter != service_id_to_extension_id_.end())
    service_id_to_extension_id_.erase(iter);
  callback.Run();
}

void BluetoothLowEnergyEventRouter::OnCreateGattConnection(
    bool persistent,
    const std::string& extension_id,
    const std::string& device_address,
    const base::Closure& callback,
    std::unique_ptr<BluetoothGattConnection> connection) {
  VLOG(2) << "GATT connection created.";
  DCHECK(connection.get());
  DCHECK(!FindConnection(extension_id, device_address));
  DCHECK_EQ(device_address, connection->GetDeviceAddress());

  const std::string connect_id = extension_id + device_address;
  DCHECK_NE(0U, connecting_devices_.count(connect_id));

  BluetoothLowEnergyConnection* conn = new BluetoothLowEnergyConnection(
      persistent, extension_id, std::move(connection));
  ConnectionResourceManager* manager =
      GetConnectionResourceManager(browser_context_);
  manager->Add(conn);

  connecting_devices_.erase(connect_id);
  callback.Run();
}

void BluetoothLowEnergyEventRouter::OnError(
    const ErrorCallback& error_callback,
    BluetoothRemoteGattService::GattErrorCode error_code) {
  VLOG(2) << "Remote characteristic/descriptor value read/write failed.";

  error_callback.Run(GattErrorToRouterError(error_code));
}

void BluetoothLowEnergyEventRouter::OnConnectError(
    const std::string& extension_id,
    const std::string& device_address,
    const ErrorCallback& error_callback,
    BluetoothDevice::ConnectErrorCode error_code) {
  VLOG(2) << "Failed to create GATT connection: " << error_code;

  const std::string connect_id = extension_id + device_address;
  DCHECK_NE(0U, connecting_devices_.count(connect_id));

  connecting_devices_.erase(connect_id);
  Status error_status = kStatusErrorFailed;
  switch (error_code) {
    case BluetoothDevice::ERROR_AUTH_CANCELED:
      error_status = kStatusErrorCanceled;
      break;
    case BluetoothDevice::ERROR_AUTH_FAILED:
      error_status = kStatusErrorAuthenticationFailed;
      break;
    case BluetoothDevice::ERROR_AUTH_REJECTED:
      error_status = kStatusErrorAuthenticationFailed;
      break;
    case BluetoothDevice::ERROR_AUTH_TIMEOUT:
      error_status = kStatusErrorTimeout;
      break;
    case BluetoothDevice::ERROR_FAILED:
      error_status = kStatusErrorFailed;
      break;
    case BluetoothDevice::ERROR_INPROGRESS:
      error_status = kStatusErrorInProgress;
      break;
    case BluetoothDevice::ERROR_UNKNOWN:
      error_status = kStatusErrorFailed;
      break;
    case BluetoothDevice::ERROR_UNSUPPORTED_DEVICE:
      error_status = kStatusErrorUnsupportedDevice;
      break;
    case BluetoothDevice::NUM_CONNECT_ERROR_CODES:
      NOTREACHED();
      error_status = kStatusErrorInvalidArguments;
      break;
  }

  error_callback.Run(error_status);
}

void BluetoothLowEnergyEventRouter::OnStartNotifySession(
    bool persistent,
    const std::string& extension_id,
    const std::string& characteristic_id,
    const base::Closure& callback,
    std::unique_ptr<device::BluetoothGattNotifySession> session) {
  VLOG(2) << "Value update session created for characteristic: "
          << characteristic_id;
  DCHECK(session.get());
  DCHECK(!FindNotifySession(extension_id, characteristic_id));
  DCHECK_EQ(characteristic_id, session->GetCharacteristicIdentifier());

  const std::string session_id = extension_id + characteristic_id;
  DCHECK_NE(0U, pending_session_calls_.count(session_id));

  BluetoothLowEnergyNotifySession* resource =
      new BluetoothLowEnergyNotifySession(persistent, extension_id,
                                          std::move(session));

  NotifySessionResourceManager* manager =
      GetNotifySessionResourceManager(browser_context_);
  manager->Add(resource);

  pending_session_calls_.erase(session_id);
  callback.Run();
}

void BluetoothLowEnergyEventRouter::OnStartNotifySessionError(
    const std::string& extension_id,
    const std::string& characteristic_id,
    const ErrorCallback& error_callback,
    device::BluetoothRemoteGattService::GattErrorCode error_code) {
  VLOG(2) << "Failed to create value update session for characteristic: "
          << characteristic_id;

  const std::string session_id = extension_id + characteristic_id;
  DCHECK_NE(0U, pending_session_calls_.count(session_id));

  pending_session_calls_.erase(session_id);
  error_callback.Run(GattErrorToRouterError(error_code));
}

void BluetoothLowEnergyEventRouter::OnStopNotifySession(
    const std::string& extension_id,
    const std::string& characteristic_id,
    const base::Closure& callback) {
  VLOG(2) << "Value update session terminated.";

  if (!RemoveNotifySession(extension_id, characteristic_id)) {
    VLOG(1) << "The value update session was removed before Stop completed, "
            << "id: " << extension_id
            << ", characteristic: " << characteristic_id;
  }

  callback.Run();
}

BluetoothLowEnergyConnection* BluetoothLowEnergyEventRouter::FindConnection(
    const std::string& extension_id,
    const std::string& device_address) {
  ConnectionResourceManager* manager =
      GetConnectionResourceManager(browser_context_);

  std::unordered_set<int>* connection_ids =
      manager->GetResourceIds(extension_id);
  if (!connection_ids)
    return NULL;

  for (auto iter = connection_ids->cbegin(); iter != connection_ids->cend();
       ++iter) {
    extensions::BluetoothLowEnergyConnection* conn =
        manager->Get(extension_id, *iter);
    if (!conn)
      continue;

    if (conn->GetConnection()->GetDeviceAddress() == device_address)
      return conn;
  }

  return NULL;
}

bool BluetoothLowEnergyEventRouter::RemoveConnection(
    const std::string& extension_id,
    const std::string& device_address) {
  ConnectionResourceManager* manager =
      GetConnectionResourceManager(browser_context_);

  std::unordered_set<int>* connection_ids =
      manager->GetResourceIds(extension_id);
  if (!connection_ids)
    return false;

  for (auto iter = connection_ids->cbegin(); iter != connection_ids->cend();
       ++iter) {
    extensions::BluetoothLowEnergyConnection* conn =
        manager->Get(extension_id, *iter);
    if (!conn || conn->GetConnection()->GetDeviceAddress() != device_address)
      continue;

    manager->Remove(extension_id, *iter);
    return true;
  }

  return false;
}

BluetoothLowEnergyNotifySession*
BluetoothLowEnergyEventRouter::FindNotifySession(
    const std::string& extension_id,
    const std::string& characteristic_id) {
  NotifySessionResourceManager* manager =
      GetNotifySessionResourceManager(browser_context_);

  std::unordered_set<int>* ids = manager->GetResourceIds(extension_id);
  if (!ids)
    return NULL;

  for (auto iter = ids->cbegin(); iter != ids->cend(); ++iter) {
    BluetoothLowEnergyNotifySession* session =
        manager->Get(extension_id, *iter);
    if (!session)
      continue;

    if (session->GetSession()->GetCharacteristicIdentifier() ==
        characteristic_id)
      return session;
  }

  return NULL;
}

bool BluetoothLowEnergyEventRouter::RemoveNotifySession(
    const std::string& extension_id,
    const std::string& characteristic_id) {
  NotifySessionResourceManager* manager =
      GetNotifySessionResourceManager(browser_context_);

  std::unordered_set<int>* ids = manager->GetResourceIds(extension_id);
  if (!ids)
    return false;

  for (auto iter = ids->cbegin(); iter != ids->cend(); ++iter) {
    BluetoothLowEnergyNotifySession* session =
        manager->Get(extension_id, *iter);
    if (!session ||
        session->GetSession()->GetCharacteristicIdentifier() !=
            characteristic_id)
      continue;

    manager->Remove(extension_id, *iter);
    return true;
  }

  return false;
}

size_t BluetoothLowEnergyEventRouter::StoreSentRequest(
    const std::string& extension_id,
    std::unique_ptr<AttributeValueRequest> request) {
  // Either find or create a request_id -> request map for this extension.
  RequestIdToRequestMap* request_id_map = nullptr;
  const auto& iter = requests_.find(extension_id);
  if (iter == requests_.end()) {
    auto new_request_id_map = std::make_unique<RequestIdToRequestMap>();
    request_id_map = new_request_id_map.get();
    requests_[extension_id] = std::move(new_request_id_map);
  } else {
    request_id_map = iter->second.get();
  }

  (*request_id_map)[++last_callback_request_id_] = std::move(request);
  return last_callback_request_id_;
}

}  // namespace extensions
