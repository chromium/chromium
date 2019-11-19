// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_device_winrt.h"

#include <windows.devices.enumeration.h>
#include <windows.foundation.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/win/core_winrt_util.h"
#include "base/win/post_async_results.h"
#include "base/win/scoped_hstring.h"
#include "components/device_event_log/device_event_log.h"
#include "device/bluetooth/bluetooth_adapter_winrt.h"
#include "device/bluetooth/bluetooth_gatt_discoverer_winrt.h"
#include "device/bluetooth/bluetooth_pairing_winrt.h"
#include "device/bluetooth/bluetooth_remote_gatt_service_winrt.h"
#include "device/bluetooth/event_utils_winrt.h"

namespace device {

namespace {

using ABI::Windows::Devices::Bluetooth::BluetoothConnectionStatus;
using ABI::Windows::Devices::Bluetooth::BluetoothConnectionStatus_Connected;
using ABI::Windows::Devices::Bluetooth::BluetoothLEDevice;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus_Success;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattDeviceServicesResult;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    IGattDeviceServicesResult;
using ABI::Windows::Devices::Bluetooth::IBluetoothLEDevice2;
using ABI::Windows::Devices::Bluetooth::IBluetoothLEDevice3;
using ABI::Windows::Devices::Bluetooth::IBluetoothLEDevice;
using ABI::Windows::Devices::Bluetooth::IBluetoothLEDeviceStatics;
using ABI::Windows::Devices::Enumeration::DevicePairingResultStatus;
using ABI::Windows::Devices::Enumeration::IDeviceInformation2;
using ABI::Windows::Devices::Enumeration::IDeviceInformation;
using ABI::Windows::Devices::Enumeration::IDeviceInformationCustomPairing;
using ABI::Windows::Devices::Enumeration::IDeviceInformationPairing2;
using ABI::Windows::Devices::Enumeration::IDeviceInformationPairing;
using ABI::Windows::Devices::Enumeration::IDevicePairingRequestedEventArgs;
using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Foundation::IClosable;
using Microsoft::WRL::ComPtr;

void PostTask(BluetoothPairingWinrt::ErrorCallback error_callback,
              BluetoothDevice::ConnectErrorCode error_code) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(error_callback), error_code));
}

ComPtr<IDeviceInformationPairing> GetDeviceInformationPairing(
    ComPtr<IBluetoothLEDevice> ble_device) {
  if (!ble_device) {
    BLUETOOTH_LOG(DEBUG) << "No BLE device instance present.";
    return nullptr;
  }

  ComPtr<IBluetoothLEDevice2> ble_device_2;
  HRESULT hr = ble_device.As(&ble_device_2);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(DEBUG) << "Obtaining IBluetoothLEDevice2 failed: "
                         << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  ComPtr<IDeviceInformation> device_information;
  hr = ble_device_2->get_DeviceInformation(&device_information);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(DEBUG) << "Getting Device Information failed: "
                         << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  ComPtr<IDeviceInformation2> device_information_2;
  hr = device_information.As(&device_information_2);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(DEBUG) << "Obtaining IDeviceInformation2 failed: "
                         << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  ComPtr<IDeviceInformationPairing> pairing;
  hr = device_information_2->get_Pairing(&pairing);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(DEBUG) << "DeviceInformation::get_Pairing() failed: "
                         << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  return pairing;
}

void CloseDevice(ComPtr<IBluetoothLEDevice> ble_device) {
  if (!ble_device)
    return;

  ComPtr<IClosable> closable;
  HRESULT hr = ble_device.As(&closable);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(DEBUG) << "As IClosable failed: "
                         << logging::SystemErrorCodeToString(hr);
    return;
  }

  hr = closable->Close();
  if (FAILED(hr)) {
    BLUETOOTH_LOG(DEBUG) << "IClosable::close() failed: "
                         << logging::SystemErrorCodeToString(hr);
  }
}

void RemoveConnectionStatusHandler(IBluetoothLEDevice* ble_device,
                                   EventRegistrationToken token) {
  HRESULT hr = ble_device->remove_ConnectionStatusChanged(token);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(DEBUG) << "Removing ConnectionStatus Handler failed: "
                         << logging::SystemErrorCodeToString(hr);
  }
}

void RemoveGattServicesChangedHandler(IBluetoothLEDevice* ble_device,
                                      EventRegistrationToken token) {
  HRESULT hr = ble_device->remove_GattServicesChanged(token);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(DEBUG) << "Removing Gatt Services Changed Handler failed: "
                         << logging::SystemErrorCodeToString(hr);
  }
}

void RemoveNameChangedHandler(IBluetoothLEDevice* ble_device,
                              EventRegistrationToken token) {
  HRESULT hr = ble_device->remove_NameChanged(token);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(DEBUG) << "Removing NameChanged Handler failed: "
                         << logging::SystemErrorCodeToString(hr);
  }
}

}  // namespace

BluetoothDeviceWinrt::BluetoothDeviceWinrt(BluetoothAdapterWinrt* adapter,
                                           uint64_t raw_address)
    : BluetoothDevice(adapter),
      raw_address_(raw_address),
      address_(CanonicalizeAddress(raw_address)) {}

BluetoothDeviceWinrt::~BluetoothDeviceWinrt() {
  CloseDevice(ble_device_);
  ClearEventRegistrations();
}

uint32_t BluetoothDeviceWinrt::GetBluetoothClass() const {
  NOTIMPLEMENTED();
  return 0;
}

std::string BluetoothDeviceWinrt::GetAddress() const {
  return address_;
}

BluetoothDevice::VendorIDSource BluetoothDeviceWinrt::GetVendorIDSource()
    const {
  NOTIMPLEMENTED();
  return VendorIDSource();
}

uint16_t BluetoothDeviceWinrt::GetVendorID() const {
  NOTIMPLEMENTED();
  return 0;
}

uint16_t BluetoothDeviceWinrt::GetProductID() const {
  NOTIMPLEMENTED();
  return 0;
}

uint16_t BluetoothDeviceWinrt::GetDeviceID() const {
  NOTIMPLEMENTED();
  return 0;
}

uint16_t BluetoothDeviceWinrt::GetAppearance() const {
  NOTIMPLEMENTED();
  return 0;
}

base::Optional<std::string> BluetoothDeviceWinrt::GetName() const {
  if (!ble_device_)
    return local_name_;

  HSTRING name;
  HRESULT hr = ble_device_->get_Name(&name);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(DEBUG) << "Getting Name failed: "
                         << logging::SystemErrorCodeToString(hr);
    return local_name_;
  }

  // Prefer returning |local_name_| over an empty string.
  if (!name)
    return local_name_;

  return base::win::ScopedHString(name).GetAsUTF8();
}

bool BluetoothDeviceWinrt::IsPaired() const {
  ComPtr<IDeviceInformationPairing> pairing =
      GetDeviceInformationPairing(ble_device_);
  if (!pairing) {
    BLUETOOTH_LOG(DEBUG) << "Failed to get DeviceInformationPairing.";
    return false;
  }

  boolean is_paired;
  HRESULT hr = pairing->get_IsPaired(&is_paired);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(DEBUG) << "DeviceInformationPairing::get_IsPaired() failed: "
                         << logging::SystemErrorCodeToString(hr);
    return false;
  }

  BLUETOOTH_LOG(DEBUG) << "BluetoothDeviceWinrt::IsPaired(): "
                       << (is_paired ? "True" : "False");
  return is_paired;
}

bool BluetoothDeviceWinrt::IsConnected() const {
  return IsGattConnected();
}

bool BluetoothDeviceWinrt::IsGattConnected() const {
  if (!ble_device_)
    return false;

  BluetoothConnectionStatus status;
  HRESULT hr = ble_device_->get_ConnectionStatus(&status);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(DEBUG) << "Getting ConnectionStatus failed: "
                         << logging::SystemErrorCodeToString(hr);
    return false;
  }

  return status == BluetoothConnectionStatus_Connected;
}

bool BluetoothDeviceWinrt::IsConnectable() const {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothDeviceWinrt::IsConnecting() const {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothDeviceWinrt::ExpectingPinCode() const {
  return pairing_ && pairing_->ExpectingPinCode();
}

bool BluetoothDeviceWinrt::ExpectingPasskey() const {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothDeviceWinrt::ExpectingConfirmation() const {
  NOTIMPLEMENTED();
  return false;
}

void BluetoothDeviceWinrt::GetConnectionInfo(
    const ConnectionInfoCallback& callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWinrt::SetConnectionLatency(
    ConnectionLatency connection_latency,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWinrt::Connect(PairingDelegate* pairing_delegate,
                                   const base::Closure& callback,
                                   const ConnectErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWinrt::Pair(PairingDelegate* pairing_delegate,
                                const base::Closure& callback,
                                const ConnectErrorCallback& error_callback) {
  BLUETOOTH_LOG(DEBUG) << "BluetoothDeviceWinrt::Pair()";
  if (pairing_) {
    BLUETOOTH_LOG(DEBUG) << "Another Pair Operation is already in progress.";
    PostTask(error_callback, ERROR_INPROGRESS);
    return;
  }

  ComPtr<IDeviceInformationPairing> pairing =
      GetDeviceInformationPairing(ble_device_);
  if (!pairing) {
    BLUETOOTH_LOG(DEBUG) << "Failed to get DeviceInformationPairing.";
    PostTask(error_callback, ERROR_UNKNOWN);
    return;
  }

  ComPtr<IDeviceInformationPairing2> pairing_2;
  HRESULT hr = pairing.As(&pairing_2);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(DEBUG) << "Obtaining IDeviceInformationPairing2 failed: "
                         << logging::SystemErrorCodeToString(hr);
    PostTask(error_callback, ERROR_UNKNOWN);
    return;
  }

  ComPtr<IDeviceInformationCustomPairing> custom;
  hr = pairing_2->get_Custom(&custom);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(DEBUG) << "DeviceInformationPairing::get_Custom() failed: "
                         << logging::SystemErrorCodeToString(hr);
    PostTask(error_callback, ERROR_UNKNOWN);
    return;
  }

  // Wrap success and error callback, so that they clean up the pairing object
  // once they are run.
  auto wrapped_callback = base::BindOnce(
      [](base::WeakPtr<BluetoothDeviceWinrt> device,
         base::OnceClosure callback) {
        if (device)
          device->pairing_.reset();
        std::move(callback).Run();
      },
      weak_ptr_factory_.GetWeakPtr(), callback);

  auto wrapped_error_callback = base::BindOnce(
      [](base::WeakPtr<BluetoothDeviceWinrt> device,
         ConnectErrorCallback error_callback, ConnectErrorCode error_code) {
        if (device)
          device->pairing_.reset();
        std::move(error_callback).Run(error_code);
      },
      weak_ptr_factory_.GetWeakPtr(), error_callback);

  pairing_ = std::make_unique<BluetoothPairingWinrt>(
      this, pairing_delegate, std::move(custom), std::move(wrapped_callback),
      std::move(wrapped_error_callback));
  pairing_->StartPairing();
}

void BluetoothDeviceWinrt::SetPinCode(const std::string& pincode) {
  if (pairing_)
    pairing_->SetPinCode(pincode);
}

void BluetoothDeviceWinrt::SetPasskey(uint32_t passkey) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWinrt::ConfirmPairing() {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWinrt::RejectPairing() {
  if (pairing_)
    pairing_->RejectPairing();
}

void BluetoothDeviceWinrt::CancelPairing() {
  if (pairing_)
    pairing_->CancelPairing();
}

void BluetoothDeviceWinrt::Disconnect(const base::Closure& callback,
                                      const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWinrt::Forget(const base::Closure& callback,
                                  const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWinrt::ConnectToService(
    const BluetoothUUID& uuid,
    const ConnectToServiceCallback& callback,
    const ConnectToServiceErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWinrt::ConnectToServiceInsecurely(
    const device::BluetoothUUID& uuid,
    const ConnectToServiceCallback& callback,
    const ConnectToServiceErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

// static
std::string BluetoothDeviceWinrt::CanonicalizeAddress(uint64_t address) {
  std::string bluetooth_address = BluetoothDevice::CanonicalizeAddress(
      base::StringPrintf("%012llX", address));
  DCHECK(!bluetooth_address.empty());
  return bluetooth_address;
}

void BluetoothDeviceWinrt::UpdateLocalName(
    base::Optional<std::string> local_name) {
  if (!local_name)
    return;

  local_name_ = std::move(local_name);
}

void BluetoothDeviceWinrt::CreateGattConnectionImpl() {
  ComPtr<IBluetoothLEDeviceStatics> device_statics;
  HRESULT hr = GetBluetoothLEDeviceStaticsActivationFactory(&device_statics);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(DEBUG)
        << "GetBluetoothLEDeviceStaticsActivationFactory failed: "
        << logging::SystemErrorCodeToString(hr);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&BluetoothDeviceWinrt::DidFailToConnectGatt,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  ConnectErrorCode::ERROR_FAILED));
    return;
  }

  // Note: Even though we might have obtained a BluetoothLEDevice instance in
  // the past, we need to request a new instance as the old device might have
  // been closed. See also
  // https://docs.microsoft.com/en-us/windows/uwp/devices-sensors/gatt-client#connecting-to-the-device
  ComPtr<IAsyncOperation<BluetoothLEDevice*>> from_bluetooth_address_op;
  hr = device_statics->FromBluetoothAddressAsync(raw_address_,
                                                 &from_bluetooth_address_op);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(DEBUG)
        << "BluetoothLEDevice::FromBluetoothAddressAsync failed: "
        << logging::SystemErrorCodeToString(hr);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&BluetoothDeviceWinrt::DidFailToConnectGatt,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  ConnectErrorCode::ERROR_FAILED));
    return;
  }

  hr = base::win::PostAsyncResults(
      std::move(from_bluetooth_address_op),
      base::BindOnce(&BluetoothDeviceWinrt::OnFromBluetoothAddress,
                     weak_ptr_factory_.GetWeakPtr()));

  if (FAILED(hr)) {
    BLUETOOTH_LOG(DEBUG) << "PostAsyncResults failed: "
                         << logging::SystemErrorCodeToString(hr);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&BluetoothDeviceWinrt::DidFailToConnectGatt,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  ConnectErrorCode::ERROR_FAILED));
  }
}

void BluetoothDeviceWinrt::DisconnectGatt() {
  // Closing the device and disposing of all references will trigger a Gatt
  // Disconnection after a short timeout. Since the Gatt Services store a
  // reference to |ble_device_| as well, we need to clear them to drop all
  // remaining references, so that the OS disconnects.
  // Reference:
  // - https://docs.microsoft.com/en-us/windows/uwp/devices-sensors/gatt-client
  CloseDevice(ble_device_);
  ClearGattServices();

  // Stop any pending Gatt Discovery sessions and report an error. This will
  // destroy |gatt_discoverer_| and release remaining references the discoverer
  // might have hold.
  if (gatt_discoverer_)
    OnGattDiscoveryComplete(false);
}

HRESULT BluetoothDeviceWinrt::GetBluetoothLEDeviceStaticsActivationFactory(
    IBluetoothLEDeviceStatics** statics) const {
  return base::win::GetActivationFactory<
      IBluetoothLEDeviceStatics,
      RuntimeClass_Windows_Devices_Bluetooth_BluetoothLEDevice>(statics);
}

void BluetoothDeviceWinrt::OnFromBluetoothAddress(
    ComPtr<IBluetoothLEDevice> ble_device) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!ble_device) {
    BLUETOOTH_LOG(DEBUG) << "Getting Device From Bluetooth Address failed.";
    DidFailToConnectGatt(ConnectErrorCode::ERROR_FAILED);
    return;
  }

  // As we are about to replace |ble_device_| with |ble_device| existing event
  // handlers need to be unregistered. New ones will be added below.
  ClearEventRegistrations();

  ble_device_ = std::move(ble_device);

  connection_changed_token_ = AddTypedEventHandler(
      ble_device_.Get(), &IBluetoothLEDevice::add_ConnectionStatusChanged,
      base::BindRepeating(&BluetoothDeviceWinrt::OnConnectionStatusChanged,
                          weak_ptr_factory_.GetWeakPtr()));

  // For paired devices the OS immediately establishes a GATT connection after
  // the first advertisement. In this case our handler is registered too late to
  // catch the initial connection changed event, and we need to perform an
  // explicit check ourselves.
  if (IsGattConnected())
    DidConnectGatt();

  gatt_services_changed_token_ = AddTypedEventHandler(
      ble_device_.Get(), &IBluetoothLEDevice::add_GattServicesChanged,
      base::BindRepeating(&BluetoothDeviceWinrt::OnGattServicesChanged,
                          weak_ptr_factory_.GetWeakPtr()));

  gatt_discoverer_ =
      std::make_unique<BluetoothGattDiscovererWinrt>(ble_device_);
  // Initiating the GATT discovery will result in a GATT connection attempt as
  // well and triggers OnConnectionStatusChanged on success.
  gatt_discoverer_->StartGattDiscovery(
      base::BindOnce(&BluetoothDeviceWinrt::OnGattDiscoveryComplete,
                     weak_ptr_factory_.GetWeakPtr()));

  name_changed_token_ = AddTypedEventHandler(
      ble_device_.Get(), &IBluetoothLEDevice::add_NameChanged,
      base::BindRepeating(&BluetoothDeviceWinrt::OnNameChanged,
                          weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothDeviceWinrt::OnConnectionStatusChanged(
    IBluetoothLEDevice* ble_device,
    IInspectable* object) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (IsGattConnected()) {
    DidConnectGatt();
  } else {
    gatt_discoverer_.reset();
    ClearGattServices();
    DidDisconnectGatt();
  }
}

void BluetoothDeviceWinrt::OnGattServicesChanged(IBluetoothLEDevice* ble_device,
                                                 IInspectable* object) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Note: We don't clear out |gatt_services_| here, as we don't want to break
  // existing references to Gatt Services that did not change.
  device_uuids_.ClearServiceUUIDs();
  SetGattServicesDiscoveryComplete(false);
  adapter_->NotifyDeviceChanged(this);
  if (IsGattConnected()) {
    // In order to stop a potential ongoing GATT discovery, the GattDiscoverer
    // is reset and a new discovery is initiated.
    gatt_discoverer_ =
        std::make_unique<BluetoothGattDiscovererWinrt>(ble_device);
    gatt_discoverer_->StartGattDiscovery(
        base::BindOnce(&BluetoothDeviceWinrt::OnGattDiscoveryComplete,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void BluetoothDeviceWinrt::OnNameChanged(IBluetoothLEDevice* ble_device,
                                         IInspectable* object) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  adapter_->NotifyDeviceChanged(this);
}

void BluetoothDeviceWinrt::OnGattDiscoveryComplete(bool success) {
  if (!success) {
    if (!IsGattConnected())
      DidFailToConnectGatt(ConnectErrorCode::ERROR_FAILED);
    gatt_discoverer_.reset();
    return;
  }

  // Instead of clearing out |gatt_services_| and creating each service from
  // scratch, we create a new map and move already existing services into it in
  // order to preserve pointer stability.
  GattServiceMap gatt_services;
  for (const auto& gatt_service : gatt_discoverer_->GetGattServices()) {
    auto gatt_service_winrt =
        BluetoothRemoteGattServiceWinrt::Create(this, gatt_service);
    if (!gatt_service_winrt)
      continue;

    std::string identifier = gatt_service_winrt->GetIdentifier();
    auto iter = gatt_services_.find(identifier);
    if (iter != gatt_services_.end()) {
      iter = gatt_services.emplace(std::move(*iter)).first;
    } else {
      iter = gatt_services
                 .emplace(std::move(identifier), std::move(gatt_service_winrt))
                 .first;
    }

    static_cast<BluetoothRemoteGattServiceWinrt*>(iter->second.get())
        ->UpdateCharacteristics(gatt_discoverer_.get());
  }

  std::swap(gatt_services, gatt_services_);
  device_uuids_.ReplaceServiceUUIDs(gatt_services_);
  SetGattServicesDiscoveryComplete(true);
  adapter_->NotifyGattServicesDiscovered(this);
  adapter_->NotifyDeviceChanged(this);
  gatt_discoverer_.reset();
}

void BluetoothDeviceWinrt::ClearGattServices() {
  // Clearing |gatt_services_| can trigger callbacks. Move the existing
  // objects into a local variable to avoid re-entrancy into clear().
  GattServiceMap temp_gatt_services;
  temp_gatt_services.swap(gatt_services_);
  temp_gatt_services.clear();

  device_uuids_.ClearServiceUUIDs();
  SetGattServicesDiscoveryComplete(false);
}

void BluetoothDeviceWinrt::ClearEventRegistrations() {
  if (connection_changed_token_) {
    RemoveConnectionStatusHandler(ble_device_.Get(),
                                  *connection_changed_token_);
  }

  if (gatt_services_changed_token_) {
    RemoveGattServicesChangedHandler(ble_device_.Get(),
                                     *gatt_services_changed_token_);
  }

  if (name_changed_token_)
    RemoveNameChangedHandler(ble_device_.Get(), *name_changed_token_);
}

}  // namespace device
