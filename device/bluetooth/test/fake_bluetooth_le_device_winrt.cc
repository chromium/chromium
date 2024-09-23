// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_bluetooth_le_device_winrt.h"

#include <wrl/client.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/win/async_operation.h"
#include "base/win/scoped_hstring.h"
#include "device/bluetooth/bluetooth_remote_gatt_service_winrt.h"
#include "device/bluetooth/test/bluetooth_test_win.h"
#include "device/bluetooth/test/fake_device_information_pairing_winrt.h"
#include "device/bluetooth/test/fake_gatt_characteristic_winrt.h"
#include "device/bluetooth/test/fake_gatt_device_service_winrt.h"
#include "device/bluetooth/test/fake_gatt_device_services_result_winrt.h"

namespace device {

namespace {

using ABI::Windows::Devices::Bluetooth::BluetoothAddressType;
using ABI::Windows::Devices::Bluetooth::BluetoothCacheMode;
using ABI::Windows::Devices::Bluetooth::BluetoothConnectionStatus;
using ABI::Windows::Devices::Bluetooth::BluetoothConnectionStatus_Connected;
using ABI::Windows::Devices::Bluetooth::BluetoothConnectionStatus_Disconnected;
using ABI::Windows::Devices::Bluetooth::BluetoothLEDevice;
using ABI::Windows::Devices::Bluetooth::IBluetoothDeviceId;
using ABI::Windows::Devices::Bluetooth::IBluetoothLEAppearance;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus_AccessDenied;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus_ProtocolError;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus_Success;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus_Unreachable;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattDeviceService;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattDeviceServicesResult;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    IGattDeviceService;
using ABI::Windows::Devices::Enumeration::DeviceAccessStatus;
using ABI::Windows::Devices::Enumeration::IDeviceAccessInformation;
using ABI::Windows::Devices::Enumeration::IDeviceInformation;
using ABI::Windows::Devices::Enumeration::DevicePairingKinds::
    DevicePairingKinds_ConfirmOnly;
using ABI::Windows::Devices::Enumeration::DevicePairingKinds::
    DevicePairingKinds_ConfirmPinMatch;
using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Foundation::ITypedEventHandler;
using ABI::Windows::Foundation::Collections::IVectorView;
using Microsoft::WRL::Make;

class FakeBluetoothDeviceId
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          IBluetoothDeviceId> {
 public:
  FakeBluetoothDeviceId() = default;
  FakeBluetoothDeviceId(const FakeBluetoothDeviceId&) = delete;
  FakeBluetoothDeviceId& operator=(const FakeBluetoothDeviceId&) = delete;
  ~FakeBluetoothDeviceId() override {}

  // IBluetoothDeviceId:
  IFACEMETHODIMP get_Id(HSTRING* value) override {
    *value = base::win::ScopedHString::Create(L"FakeBluetoothLEDeviceWinrt")
                 .release();
    return S_OK;
  }
  IFACEMETHODIMP get_IsClassicDevice(::boolean* value) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_IsLowEnergyDevice(::boolean* value) override {
    return E_NOTIMPL;
  }
};

}  // namespace

FakeBluetoothLEDeviceWinrt::FakeBluetoothLEDeviceWinrt(
    BluetoothTestWinrt* bluetooth_test_winrt)
    : bluetooth_test_winrt_(bluetooth_test_winrt) {}

FakeBluetoothLEDeviceWinrt::~FakeBluetoothLEDeviceWinrt() = default;

HRESULT FakeBluetoothLEDeviceWinrt::get_DeviceId(HSTRING* value) {
  *value =
      base::win::ScopedHString::Create(L"FakeBluetoothLEDeviceWinrt").release();
  return S_OK;
}

HRESULT FakeBluetoothLEDeviceWinrt::get_Name(HSTRING* value) {
  if (!name_)
    return E_FAIL;

  *value = base::win::ScopedHString::Create(*name_).release();
  return S_OK;
}

HRESULT FakeBluetoothLEDeviceWinrt::get_GattServices(
    IVectorView<GattDeviceService*>** value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceWinrt::get_ConnectionStatus(
    BluetoothConnectionStatus* value) {
  *value = status_;
  return S_OK;
}

HRESULT FakeBluetoothLEDeviceWinrt::get_BluetoothAddress(uint64_t* value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceWinrt::GetGattService(
    GUID service_uuid,
    IGattDeviceService** service) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceWinrt::add_NameChanged(
    ITypedEventHandler<BluetoothLEDevice*, IInspectable*>* handler,
    EventRegistrationToken* token) {
  name_changed_handler_ = handler;
  return S_OK;
}

HRESULT FakeBluetoothLEDeviceWinrt::remove_NameChanged(
    EventRegistrationToken token) {
  name_changed_handler_ = nullptr;
  return S_OK;
}

HRESULT FakeBluetoothLEDeviceWinrt::add_GattServicesChanged(
    ITypedEventHandler<BluetoothLEDevice*, IInspectable*>* handler,
    EventRegistrationToken* token) {
  gatt_services_changed_handler_ = handler;
  return S_OK;
}

HRESULT FakeBluetoothLEDeviceWinrt::remove_GattServicesChanged(
    EventRegistrationToken token) {
  gatt_services_changed_handler_ = nullptr;
  return S_OK;
}

HRESULT FakeBluetoothLEDeviceWinrt::add_ConnectionStatusChanged(
    ITypedEventHandler<BluetoothLEDevice*, IInspectable*>* handler,
    EventRegistrationToken* token) {
  connection_status_changed_handler_ = handler;
  return S_OK;
}

HRESULT FakeBluetoothLEDeviceWinrt::remove_ConnectionStatusChanged(
    EventRegistrationToken token) {
  connection_status_changed_handler_ = nullptr;
  return S_OK;
}

HRESULT FakeBluetoothLEDeviceWinrt::get_DeviceInformation(
    IDeviceInformation** value) {
  return device_information_.CopyTo(value);
}

HRESULT FakeBluetoothLEDeviceWinrt::get_Appearance(
    IBluetoothLEAppearance** value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceWinrt::get_BluetoothAddressType(
    BluetoothAddressType* value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceWinrt::get_DeviceAccessInformation(
    IDeviceAccessInformation** value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceWinrt::RequestAccessAsync(
    IAsyncOperation<DeviceAccessStatus>** operation) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceWinrt::GetGattServicesAsync(
    IAsyncOperation<GattDeviceServicesResult*>** operation) {
  auto async_op = Make<base::win::AsyncOperation<GattDeviceServicesResult*>>();
  gatt_services_callback_ = async_op->callback();
  *operation = async_op.Detach();
  service_uuid_.reset();
  if (!bluetooth_test_winrt_->UsesNewGattSessionHandling()) {
    bluetooth_test_winrt_->OnFakeBluetoothDeviceConnectGattAttempt();
  }
  bluetooth_test_winrt_->OnFakeBluetoothDeviceGattServiceDiscoveryAttempt();
  return S_OK;
}

HRESULT FakeBluetoothLEDeviceWinrt::GetGattServicesWithCacheModeAsync(
    BluetoothCacheMode cache_mode,
    IAsyncOperation<GattDeviceServicesResult*>** operation) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceWinrt::GetGattServicesForUuidAsync(
    GUID service_uuid,
    IAsyncOperation<GattDeviceServicesResult*>** operation) {
  auto async_op = Make<base::win::AsyncOperation<GattDeviceServicesResult*>>();
  gatt_services_callback_ = async_op->callback();
  service_uuid_ = service_uuid;
  *operation = async_op.Detach();
  if (!bluetooth_test_winrt_->UsesNewGattSessionHandling()) {
    bluetooth_test_winrt_->OnFakeBluetoothDeviceConnectGattAttempt();
  }
  bluetooth_test_winrt_->OnFakeBluetoothDeviceGattServiceDiscoveryAttempt();
  return S_OK;
}

HRESULT FakeBluetoothLEDeviceWinrt::GetGattServicesForUuidWithCacheModeAsync(
    GUID service_uuid,
    BluetoothCacheMode cache_mode,
    IAsyncOperation<GattDeviceServicesResult*>** operation) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceWinrt::get_BluetoothDeviceId(
    ABI::Windows::Devices::Bluetooth::IBluetoothDeviceId** value) {
  return Make<FakeBluetoothDeviceId>().CopyTo(value);
}

HRESULT FakeBluetoothLEDeviceWinrt::Close() {
  --reference_count_;
  fake_services_.clear();
  bluetooth_test_winrt_->OnFakeBluetoothGattDisconnect();
  return S_OK;
}

void FakeBluetoothLEDeviceWinrt::AddReference() {
  ++reference_count_;
}

void FakeBluetoothLEDeviceWinrt::RemoveReference() {
  --reference_count_;
}

void FakeBluetoothLEDeviceWinrt::SimulateDevicePaired(bool is_paired) {
  device_information_ = Make<FakeDeviceInformationWinrt>(
      Make<FakeDeviceInformationPairingWinrt>(is_paired));
}

void FakeBluetoothLEDeviceWinrt::SimulatePairingPinCode(std::string pin_code) {
  device_information_ = Make<FakeDeviceInformationWinrt>(
      Make<FakeDeviceInformationPairingWinrt>(std::move(pin_code)));
}

void FakeBluetoothLEDeviceWinrt::SimulateConfirmOnly() {
  device_information_ = Make<FakeDeviceInformationWinrt>(
      Make<FakeDeviceInformationPairingWinrt>(DevicePairingKinds_ConfirmOnly));
}

void FakeBluetoothLEDeviceWinrt::SimulateDisplayPin(
    std::string_view display_pin) {
  device_information_ =
      Make<FakeDeviceInformationWinrt>(Make<FakeDeviceInformationPairingWinrt>(
          DevicePairingKinds_ConfirmPinMatch, display_pin));
}

std::optional<BluetoothUUID> FakeBluetoothLEDeviceWinrt::GetTargetGattService()
    const {
  if (!service_uuid_)
    return std::nullopt;
  return BluetoothUUID(*service_uuid_);
}

void FakeBluetoothLEDeviceWinrt::SimulateGattConnection() {
  status_ = BluetoothConnectionStatus_Connected;
  connection_status_changed_handler_->Invoke(this, nullptr);
}

void FakeBluetoothLEDeviceWinrt::SimulateStatusChangeToDisconnect() {
  status_ = BluetoothConnectionStatus_Disconnected;
  connection_status_changed_handler_->Invoke(this, nullptr);
}

void FakeBluetoothLEDeviceWinrt ::SimulateGattConnectionError(
    BluetoothDevice::ConnectErrorCode error_code) {
  if (!gatt_services_callback_)
    return;

  std::move(gatt_services_callback_)
      .Run(Make<FakeGattDeviceServicesResultWinrt>(
          GattCommunicationStatus_ProtocolError));
}

void FakeBluetoothLEDeviceWinrt::SimulateGattDisconnection() {
  if (status_ == BluetoothConnectionStatus_Disconnected) {
    if (!gatt_services_callback_) {
      DCHECK(bluetooth_test_winrt_->UsesNewGattSessionHandling());
      return;
    }

    std::move(gatt_services_callback_)
        .Run(Make<FakeGattDeviceServicesResultWinrt>(
            GattCommunicationStatus_Unreachable));
    return;
  }

  // Simulate production UWP behavior that only really disconnects once all
  // references to a device are dropped.
  if (reference_count_ == 0u)
    SimulateStatusChangeToDisconnect();
}

void FakeBluetoothLEDeviceWinrt::SimulateDeviceBreaksConnection() {
  if (status_ == BluetoothConnectionStatus_Disconnected) {
    DCHECK(gatt_services_callback_);
    std::move(gatt_services_callback_)
        .Run(Make<FakeGattDeviceServicesResultWinrt>(
            GattCommunicationStatus_Unreachable));
    return;
  }

  // Simulate a Gatt Disconnecion regardless of the reference count.
  SimulateStatusChangeToDisconnect();
}

void FakeBluetoothLEDeviceWinrt::SimulateGattNameChange(
    const std::string& new_name) {
  name_ = new_name;
  name_changed_handler_->Invoke(this, nullptr);
}

void FakeBluetoothLEDeviceWinrt::SimulateGattServicesDiscovered(
    const std::vector<std::string>& uuids,
    const std::vector<std::string>& blocked_uuids) {
  for (const auto& uuid : uuids) {
    // Attribute handles need to be unique for a given BLE device. Increasing by
    // a large number ensures enough address space for the contained
    // characteristics and descriptors.
    fake_services_.push_back(Make<FakeGattDeviceServiceWinrt>(
        bluetooth_test_winrt_, this, uuid, service_attribute_handle_ += 0x0400,
        /*allowed=*/true));
  }
  for (const auto& uuid : blocked_uuids) {
    // Attribute handles need to be unique for a given BLE device. Increasing by
    // a large number ensures enough address space for the contained
    // characteristics and descriptors.
    fake_services_.push_back(Make<FakeGattDeviceServiceWinrt>(
        bluetooth_test_winrt_, this, uuid, service_attribute_handle_ += 0x0400,
        /*allowed=*/false));
  }

  DCHECK(gatt_services_callback_);
  std::move(gatt_services_callback_)
      .Run(Make<FakeGattDeviceServicesResultWinrt>(fake_services_));
}

void FakeBluetoothLEDeviceWinrt::SimulateGattServiceRemoved(
    BluetoothRemoteGattService* service) {
  auto* device_service = static_cast<BluetoothRemoteGattServiceWinrt*>(service)
                             ->GetDeviceServiceForTesting();
  auto iter = base::ranges::find(
      fake_services_, device_service,
      &Microsoft::WRL::ComPtr<FakeGattDeviceServiceWinrt>::Get);
  CHECK(iter != fake_services_.end(), base::NotFatalUntil::M130);
  fake_services_.erase(iter);
  SimulateGattServicesChanged();
  DCHECK(gatt_services_callback_);
  std::move(gatt_services_callback_)
      .Run(Make<FakeGattDeviceServicesResultWinrt>(fake_services_));
}

void FakeBluetoothLEDeviceWinrt::SimulateGattCharacteristic(
    BluetoothRemoteGattService* service,
    const std::string& uuid,
    int properties) {
  // Simulate the fake characteristic on the GATT service and trigger a GATT
  // re-scan via GattServicesChanged().
  auto* const fake_service = static_cast<FakeGattDeviceServiceWinrt*>(
      static_cast<BluetoothRemoteGattServiceWinrt*>(service)
          ->GetDeviceServiceForTesting());
  DCHECK(fake_service);
  fake_service->SimulateGattCharacteristic(uuid, properties);

  SimulateGattServicesChanged();
  DCHECK(gatt_services_callback_);
  std::move(gatt_services_callback_)
      .Run(Make<FakeGattDeviceServicesResultWinrt>(fake_services_));
}

void FakeBluetoothLEDeviceWinrt::SimulateGattDescriptor(
    BluetoothRemoteGattCharacteristic* characteristic,
    const std::string& uuid) {
  // Simulate the fake descriptor on the GATT service and trigger a GATT
  // re-scan via GattServicesChanged().
  auto* const fake_characteristic = static_cast<FakeGattCharacteristicWinrt*>(
      static_cast<BluetoothRemoteGattCharacteristicWinrt*>(characteristic)
          ->GetCharacteristicForTesting());
  DCHECK(fake_characteristic);
  fake_characteristic->SimulateGattDescriptor(uuid);

  SimulateGattServicesChanged();
  DCHECK(gatt_services_callback_);
  std::move(gatt_services_callback_)
      .Run(Make<FakeGattDeviceServicesResultWinrt>(fake_services_));
}

void FakeBluetoothLEDeviceWinrt::SimulateGattServicesChanged() {
  DCHECK(gatt_services_changed_handler_);
  gatt_services_changed_handler_->Invoke(this, nullptr);
  base::RunLoop().RunUntilIdle();
}

void FakeBluetoothLEDeviceWinrt::SimulateGattServicesDiscoveryError() {
  DCHECK(gatt_services_callback_);
  std::move(gatt_services_callback_)
      .Run(Make<FakeGattDeviceServicesResultWinrt>(
          GattCommunicationStatus_ProtocolError));
}

FakeBluetoothLEDeviceStaticsWinrt::FakeBluetoothLEDeviceStaticsWinrt(
    BluetoothTestWinrt* bluetooth_test_winrt)
    : bluetooth_test_winrt_(bluetooth_test_winrt) {}

FakeBluetoothLEDeviceStaticsWinrt::~FakeBluetoothLEDeviceStaticsWinrt() =
    default;

HRESULT FakeBluetoothLEDeviceStaticsWinrt::FromIdAsync(
    HSTRING device_id,
    IAsyncOperation<BluetoothLEDevice*>** operation) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceStaticsWinrt::FromBluetoothAddressAsync(
    uint64_t bluetooth_address,
    IAsyncOperation<BluetoothLEDevice*>** operation) {
  auto async_op = Make<base::win::AsyncOperation<BluetoothLEDevice*>>();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(async_op->callback(),
                     Make<FakeBluetoothLEDeviceWinrt>(bluetooth_test_winrt_)));
  *operation = async_op.Detach();
  return S_OK;
}

HRESULT FakeBluetoothLEDeviceStaticsWinrt::GetDeviceSelector(
    HSTRING* device_selector) {
  return E_NOTIMPL;
}

}  // namespace device
