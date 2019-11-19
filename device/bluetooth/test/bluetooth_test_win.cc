// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/bluetooth_test_win.h"

#include <windows.devices.bluetooth.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/containers/circular_deque.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/test_pending_task.h"
#include "base/time/time.h"
#include "base/win/vector.h"
#include "base/win/windows_version.h"
#include "device/base/features.h"
#include "device/bluetooth/bluetooth_adapter_win.h"
#include "device/bluetooth/bluetooth_adapter_winrt.h"
#include "device/bluetooth/bluetooth_advertisement_winrt.h"
#include "device/bluetooth/bluetooth_device_winrt.h"
#include "device/bluetooth/bluetooth_low_energy_win.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic_win.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic_winrt.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor_win.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor_winrt.h"
#include "device/bluetooth/bluetooth_remote_gatt_service_win.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/bluetooth/test/fake_bluetooth_adapter_winrt.h"
#include "device/bluetooth/test/fake_bluetooth_le_advertisement_publisher_winrt.h"
#include "device/bluetooth/test/fake_bluetooth_le_advertisement_watcher_winrt.h"
#include "device/bluetooth/test/fake_bluetooth_le_advertisement_winrt.h"
#include "device/bluetooth/test/fake_bluetooth_le_device_winrt.h"
#include "device/bluetooth/test/fake_bluetooth_le_manufacturer_data_winrt.h"
#include "device/bluetooth/test/fake_device_information_winrt.h"
#include "device/bluetooth/test/fake_device_watcher_winrt.h"
#include "device/bluetooth/test/fake_gatt_characteristic_winrt.h"
#include "device/bluetooth/test/fake_gatt_descriptor_winrt.h"
#include "device/bluetooth/test/fake_radio_winrt.h"

// Note: As UWP does not provide int specializations for IObservableVector and
// VectorChangedEventHandler we need to supply our own. UUIDs were generated
// using `uuidgen`.
namespace ABI {
namespace Windows {
namespace Foundation {
namespace Collections {

template <>
struct __declspec(uuid("2736c37e-4218-496f-a46a-92d5d9e610a9"))
    IObservableVector<GUID> : IObservableVector_impl<GUID> {};

template <>
struct __declspec(uuid("94844fba-ddf9-475c-be6e-ebb87039cef6"))
    VectorChangedEventHandler<GUID> : VectorChangedEventHandler_impl<GUID> {};

}  // namespace Collections
}  // namespace Foundation
}  // namespace Windows
}  // namespace ABI

namespace device {

namespace {

using ABI::Windows::Devices::Bluetooth::Advertisement::
    IBluetoothLEAdvertisement;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    IBluetoothLEAdvertisementPublisherFactory;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    IBluetoothLEManufacturerDataFactory;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    IBluetoothLEAdvertisementWatcher;
using ABI::Windows::Devices::Bluetooth::IBluetoothAdapter;
using ABI::Windows::Devices::Bluetooth::IBluetoothAdapterStatics;
using ABI::Windows::Devices::Bluetooth::IBluetoothLEDevice;
using ABI::Windows::Devices::Bluetooth::IBluetoothLEDeviceStatics;
using ABI::Windows::Devices::Enumeration::IDeviceInformation;
using ABI::Windows::Devices::Enumeration::IDeviceInformationStatics;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;

class TestBluetoothAdvertisementWinrt : public BluetoothAdvertisementWinrt {
 public:
  TestBluetoothAdvertisementWinrt() = default;

 protected:
  ~TestBluetoothAdvertisementWinrt() override = default;

  HRESULT
  GetBluetoothLEAdvertisementPublisherActivationFactory(
      IBluetoothLEAdvertisementPublisherFactory** factory) const override {
    return Make<FakeBluetoothLEAdvertisementPublisherFactoryWinrt>().CopyTo(
        factory);
  }

  HRESULT ActivateBluetoothLEAdvertisementInstance(
      IBluetoothLEAdvertisement** instance) const override {
    return Make<FakeBluetoothLEAdvertisementWinrt>().CopyTo(instance);
  }

  HRESULT GetBluetoothLEManufacturerDataFactory(
      IBluetoothLEManufacturerDataFactory** factory) const override {
    return Make<FakeBluetoothLEManufacturerDataFactory>().CopyTo(factory);
  }
};

class TestBluetoothDeviceWinrt : public BluetoothDeviceWinrt {
 public:
  TestBluetoothDeviceWinrt(BluetoothAdapterWinrt* adapter,
                           uint64_t raw_address,
                           BluetoothTestWinrt* bluetooth_test_winrt)
      : BluetoothDeviceWinrt(adapter, raw_address),
        bluetooth_test_winrt_(bluetooth_test_winrt) {}

  HRESULT GetBluetoothLEDeviceStaticsActivationFactory(
      IBluetoothLEDeviceStatics** statics) const override {
    auto device_statics =
        Make<FakeBluetoothLEDeviceStaticsWinrt>(bluetooth_test_winrt_);
    return device_statics.CopyTo(statics);
  }

  FakeBluetoothLEDeviceWinrt* ble_device() {
    return static_cast<FakeBluetoothLEDeviceWinrt*>(ble_device_.Get());
  }

 private:
  BluetoothTestWinrt* bluetooth_test_winrt_ = nullptr;
};

class TestBluetoothAdapterWinrt : public BluetoothAdapterWinrt {
 public:
  TestBluetoothAdapterWinrt(ComPtr<IBluetoothAdapter> adapter,
                            ComPtr<IDeviceInformation> device_information,
                            InitCallback init_cb,
                            BluetoothTestWinrt* bluetooth_test_winrt)
      : adapter_(std::move(adapter)),
        device_information_(std::move(device_information)),
        watcher_(Make<FakeBluetoothLEAdvertisementWatcherWinrt>()),
        bluetooth_test_winrt_(bluetooth_test_winrt) {
    ComPtr<IBluetoothAdapterStatics> bluetooth_adapter_statics;
    Make<FakeBluetoothAdapterStaticsWinrt>(adapter_).CopyTo(
        (IBluetoothAdapterStatics**)&bluetooth_adapter_statics);
    ComPtr<IDeviceInformationStatics> device_information_statics;
    Make<FakeDeviceInformationStaticsWinrt>(device_information_)
        .CopyTo((IDeviceInformationStatics**)&device_information_statics);
    InitForTests(std::move(init_cb), std::move(bluetooth_adapter_statics),
                 std::move(device_information_statics), nullptr);
  }

  FakeBluetoothLEAdvertisementWatcherWinrt* watcher() { return watcher_.Get(); }

 protected:
  ~TestBluetoothAdapterWinrt() override = default;

  HRESULT GetTestBluetoothAdapterStaticsActivationFactory(
      IBluetoothAdapterStatics** statics) const {
    auto adapter_statics = Make<FakeBluetoothAdapterStaticsWinrt>(adapter_);
    return adapter_statics.CopyTo(statics);
  }

  HRESULT
  GetTestDeviceInformationStaticsActivationFactory(
      IDeviceInformationStatics** statics) const {
    auto device_information_statics =
        Make<FakeDeviceInformationStaticsWinrt>(device_information_);
    return device_information_statics.CopyTo(statics);
  }

  HRESULT ActivateBluetoothAdvertisementLEWatcherInstance(
      IBluetoothLEAdvertisementWatcher** instance) const override {
    return watcher_.CopyTo(instance);
  }

  scoped_refptr<BluetoothAdvertisementWinrt> CreateAdvertisement()
      const override {
    return base::MakeRefCounted<TestBluetoothAdvertisementWinrt>();
  }

  std::unique_ptr<BluetoothDeviceWinrt> CreateDevice(
      uint64_t raw_address) override {
    return std::make_unique<TestBluetoothDeviceWinrt>(this, raw_address,
                                                      bluetooth_test_winrt_);
  }

 private:
  ComPtr<IBluetoothAdapter> adapter_;
  ComPtr<IDeviceInformation> device_information_;
  ComPtr<FakeBluetoothLEAdvertisementWatcherWinrt> watcher_;
  BluetoothTestWinrt* bluetooth_test_winrt_ = nullptr;
};

BLUETOOTH_ADDRESS
CanonicalStringToBLUETOOTH_ADDRESS(std::string device_address) {
  BLUETOOTH_ADDRESS win_addr;
  unsigned int data[6];
  int result =
      sscanf_s(device_address.c_str(), "%02X:%02X:%02X:%02X:%02X:%02X",
               &data[5], &data[4], &data[3], &data[2], &data[1], &data[0]);
  CHECK_EQ(6, result);
  for (int i = 0; i < 6; i++) {
    win_addr.rgBytes[i] = data[i];
  }
  return win_addr;
}

// The canonical UUID string format is device::BluetoothUUID.value().
BTH_LE_UUID CanonicalStringToBTH_LE_UUID(base::StringPiece uuid) {
  BTH_LE_UUID win_uuid = {0};
  if (uuid.size() == 4) {
    win_uuid.IsShortUuid = TRUE;
    unsigned int data[1];
    int result = sscanf_s(uuid.data(), "%04x", &data[0]);
    CHECK_EQ(1, result);
    win_uuid.Value.ShortUuid = data[0];
  } else if (uuid.size() == 36) {
    win_uuid.IsShortUuid = FALSE;
    win_uuid.Value.LongUuid = BluetoothUUID::GetCanonicalValueAsGUID(uuid);
  } else {
    CHECK(false);
  }

  return win_uuid;
}

}  // namespace

BluetoothTestWin::BluetoothTestWin()
    : ui_task_runner_(new base::TestSimpleTaskRunner()),
      bluetooth_task_runner_(new base::TestSimpleTaskRunner()),
      fake_bt_le_wrapper_(nullptr) {}
BluetoothTestWin::~BluetoothTestWin() {}

bool BluetoothTestWin::PlatformSupportsLowEnergy() {
  if (fake_bt_le_wrapper_)
    return fake_bt_le_wrapper_->IsBluetoothLowEnergySupported();
  return true;
}

void BluetoothTestWin::InitWithDefaultAdapter() {
  auto adapter =
      base::WrapRefCounted(new BluetoothAdapterWin(base::DoNothing()));
  adapter->Init();
  adapter_ = std::move(adapter);
}

void BluetoothTestWin::InitWithoutDefaultAdapter() {
  auto adapter =
      base::WrapRefCounted(new BluetoothAdapterWin(base::DoNothing()));
  adapter->InitForTest(nullptr, nullptr, ui_task_runner_,
                       bluetooth_task_runner_);
  adapter_ = std::move(adapter);
}

void BluetoothTestWin::InitWithFakeAdapter() {
  auto fake_bt_classic_wrapper =
      std::make_unique<win::BluetoothClassicWrapperFake>();
  fake_bt_classic_wrapper->SimulateARadio(
      base::UTF8ToUTF16(kTestAdapterName),
      CanonicalStringToBLUETOOTH_ADDRESS(kTestAdapterAddress));

  auto fake_bt_le_wrapper =
      std::make_unique<win::BluetoothLowEnergyWrapperFake>();
  fake_bt_le_wrapper_ = fake_bt_le_wrapper.get();
  fake_bt_le_wrapper_->AddObserver(this);

  auto adapter =
      base::WrapRefCounted(new BluetoothAdapterWin(base::DoNothing()));
  adapter->InitForTest(std::move(fake_bt_classic_wrapper),
                       std::move(fake_bt_le_wrapper), nullptr,
                       bluetooth_task_runner_);
  adapter_ = std::move(adapter);
  FinishPendingTasks();
}

bool BluetoothTestWin::DenyPermission() {
  return false;
}

void BluetoothTestWin::StartLowEnergyDiscoverySession() {
  __super ::StartLowEnergyDiscoverySession();
  FinishPendingTasks();
}

BluetoothDevice* BluetoothTestWin::SimulateLowEnergyDevice(int device_ordinal) {
  LowEnergyDeviceData data = GetLowEnergyDeviceData(device_ordinal);
  win::BLEDevice* simulated_device = fake_bt_le_wrapper_->SimulateBLEDevice(
      data.name.value_or(std::string()),
      CanonicalStringToBLUETOOTH_ADDRESS(data.address));
  if (simulated_device != nullptr) {
    for (const auto& uuid : data.advertised_uuids) {
      fake_bt_le_wrapper_->SimulateGattService(
          simulated_device, nullptr,
          CanonicalStringToBTH_LE_UUID(uuid.canonical_value()));
    }
  }
  FinishPendingTasks();

  return adapter_->GetDevice(data.address);
}

void BluetoothTestWin::SimulateGattConnection(BluetoothDevice* device) {
  FinishPendingTasks();
  // We don't actually attempt to discover on Windows, so fake it for testing.
  gatt_discovery_attempts_++;
}

void BluetoothTestWin::SimulateGattServicesDiscovered(
    BluetoothDevice* device,
    const std::vector<std::string>& uuids) {
  std::string address =
      device ? device->GetAddress() : remembered_device_address_;

  win::BLEDevice* simulated_device =
      fake_bt_le_wrapper_->GetSimulatedBLEDevice(address);
  CHECK(simulated_device);

  for (auto uuid : uuids) {
    fake_bt_le_wrapper_->SimulateGattService(
        simulated_device, nullptr, CanonicalStringToBTH_LE_UUID(uuid));
  }

  FinishPendingTasks();

  // We still need to discover characteristics.  Wait for the appropriate method
  // to be posted and then finish the pending tasks.
  base::RunLoop().RunUntilIdle();
  FinishPendingTasks();
}

void BluetoothTestWin::SimulateGattServiceRemoved(
    BluetoothRemoteGattService* service) {
  std::string device_address = service->GetDevice()->GetAddress();
  win::BLEDevice* target_device =
      fake_bt_le_wrapper_->GetSimulatedBLEDevice(device_address);
  CHECK(target_device);

  BluetoothRemoteGattServiceWin* win_service =
      static_cast<BluetoothRemoteGattServiceWin*>(service);
  std::string service_att_handle =
      std::to_string(win_service->GetAttributeHandle());
  fake_bt_le_wrapper_->SimulateGattServiceRemoved(target_device, nullptr,
                                                  service_att_handle);

  ForceRefreshDevice();
}

void BluetoothTestWin::SimulateGattCharacteristic(
    BluetoothRemoteGattService* service,
    const std::string& uuid,
    int properties) {
  std::string device_address = service->GetDevice()->GetAddress();
  win::BLEDevice* target_device =
      fake_bt_le_wrapper_->GetSimulatedBLEDevice(device_address);
  CHECK(target_device);
  win::GattService* target_service =
      GetSimulatedService(target_device, service);
  CHECK(target_service);

  BTH_LE_GATT_CHARACTERISTIC win_characteristic_info;
  win_characteristic_info.CharacteristicUuid =
      CanonicalStringToBTH_LE_UUID(uuid);
  win_characteristic_info.IsBroadcastable = FALSE;
  win_characteristic_info.IsReadable = FALSE;
  win_characteristic_info.IsWritableWithoutResponse = FALSE;
  win_characteristic_info.IsWritable = FALSE;
  win_characteristic_info.IsNotifiable = FALSE;
  win_characteristic_info.IsIndicatable = FALSE;
  win_characteristic_info.IsSignedWritable = FALSE;
  win_characteristic_info.HasExtendedProperties = FALSE;
  if (properties & BluetoothRemoteGattCharacteristic::PROPERTY_BROADCAST)
    win_characteristic_info.IsBroadcastable = TRUE;
  if (properties & BluetoothRemoteGattCharacteristic::PROPERTY_READ)
    win_characteristic_info.IsReadable = TRUE;
  if (properties &
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITE_WITHOUT_RESPONSE)
    win_characteristic_info.IsWritableWithoutResponse = TRUE;
  if (properties & BluetoothRemoteGattCharacteristic::PROPERTY_WRITE)
    win_characteristic_info.IsWritable = TRUE;
  if (properties & BluetoothRemoteGattCharacteristic::PROPERTY_NOTIFY)
    win_characteristic_info.IsNotifiable = TRUE;
  if (properties & BluetoothRemoteGattCharacteristic::PROPERTY_INDICATE)
    win_characteristic_info.IsIndicatable = TRUE;
  if (properties &
      BluetoothRemoteGattCharacteristic::PROPERTY_AUTHENTICATED_SIGNED_WRITES) {
    win_characteristic_info.IsSignedWritable = TRUE;
  }
  if (properties &
      BluetoothRemoteGattCharacteristic::PROPERTY_EXTENDED_PROPERTIES)
    win_characteristic_info.HasExtendedProperties = TRUE;

  fake_bt_le_wrapper_->SimulateGattCharacterisc(device_address, target_service,
                                                win_characteristic_info);

  ForceRefreshDevice();
}

void BluetoothTestWin::SimulateGattCharacteristicRemoved(
    BluetoothRemoteGattService* service,
    BluetoothRemoteGattCharacteristic* characteristic) {
  CHECK(service);
  CHECK(characteristic);

  std::string device_address = service->GetDevice()->GetAddress();
  win::GattService* target_service = GetSimulatedService(
      fake_bt_le_wrapper_->GetSimulatedBLEDevice(device_address), service);
  CHECK(target_service);

  std::string characteristic_att_handle = std::to_string(
      static_cast<BluetoothRemoteGattCharacteristicWin*>(characteristic)
          ->GetAttributeHandle());
  fake_bt_le_wrapper_->SimulateGattCharacteriscRemove(
      target_service, characteristic_att_handle);

  ForceRefreshDevice();
}

void BluetoothTestWin::RememberCharacteristicForSubsequentAction(
    BluetoothRemoteGattCharacteristic* characteristic) {
  CHECK(characteristic);
  BluetoothRemoteGattCharacteristicWin* win_characteristic =
      static_cast<BluetoothRemoteGattCharacteristicWin*>(characteristic);

  std::string device_address =
      win_characteristic->GetService()->GetDevice()->GetAddress();
  win::BLEDevice* target_device =
      fake_bt_le_wrapper_->GetSimulatedBLEDevice(device_address);
  CHECK(target_device);
  win::GattService* target_service =
      GetSimulatedService(target_device, win_characteristic->GetService());
  CHECK(target_service);
  fake_bt_le_wrapper_->RememberCharacteristicForSubsequentAction(
      target_service, std::to_string(win_characteristic->GetAttributeHandle()));
}

void BluetoothTestWin::SimulateGattCharacteristicRead(
    BluetoothRemoteGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value) {
  win::GattCharacteristic* target_simulated_characteristic = nullptr;
  if (characteristic) {
    target_simulated_characteristic =
        GetSimulatedCharacteristic(characteristic);
  }

  fake_bt_le_wrapper_->SimulateGattCharacteristicValue(
      target_simulated_characteristic, value);

  RunPendingTasksUntilCallback();
}

void BluetoothTestWin::SimulateGattCharacteristicReadError(
    BluetoothRemoteGattCharacteristic* characteristic,
    BluetoothRemoteGattService::GattErrorCode error_code) {
  win::GattCharacteristic* target_characteristic =
      GetSimulatedCharacteristic(characteristic);
  CHECK(target_characteristic);
  HRESULT hr = HRESULT_FROM_WIN32(ERROR_SEM_TIMEOUT);
  if (error_code == BluetoothRemoteGattService::GATT_ERROR_INVALID_LENGTH)
    hr = E_BLUETOOTH_ATT_INVALID_ATTRIBUTE_VALUE_LENGTH;
  fake_bt_le_wrapper_->SimulateGattCharacteristicReadError(
      target_characteristic, hr);

  FinishPendingTasks();
}

void BluetoothTestWin::SimulateGattCharacteristicWrite(
    BluetoothRemoteGattCharacteristic* characteristic) {
  RunPendingTasksUntilCallback();
}

void BluetoothTestWin::SimulateGattCharacteristicWriteError(
    BluetoothRemoteGattCharacteristic* characteristic,
    BluetoothRemoteGattService::GattErrorCode error_code) {
  win::GattCharacteristic* target_characteristic =
      GetSimulatedCharacteristic(characteristic);
  CHECK(target_characteristic);
  HRESULT hr = HRESULT_FROM_WIN32(ERROR_SEM_TIMEOUT);
  if (error_code == BluetoothRemoteGattService::GATT_ERROR_INVALID_LENGTH)
    hr = E_BLUETOOTH_ATT_INVALID_ATTRIBUTE_VALUE_LENGTH;
  fake_bt_le_wrapper_->SimulateGattCharacteristicWriteError(
      target_characteristic, hr);

  FinishPendingTasks();
}

void BluetoothTestWin::RememberDeviceForSubsequentAction(
    BluetoothDevice* device) {
  remembered_device_address_ = device->GetAddress();
}

void BluetoothTestWin::DeleteDevice(BluetoothDevice* device) {
  CHECK(device);
  fake_bt_le_wrapper_->RemoveSimulatedBLEDevice(device->GetAddress());
  FinishPendingTasks();
}

void BluetoothTestWin::SimulateGattDescriptor(
    BluetoothRemoteGattCharacteristic* characteristic,
    const std::string& uuid) {
  win::GattCharacteristic* target_characteristic =
      GetSimulatedCharacteristic(characteristic);
  CHECK(target_characteristic);
  fake_bt_le_wrapper_->SimulateGattDescriptor(
      characteristic->GetService()->GetDevice()->GetAddress(),
      target_characteristic, CanonicalStringToBTH_LE_UUID(uuid));
  ForceRefreshDevice();
}

void BluetoothTestWin::SimulateGattNotifySessionStarted(
    BluetoothRemoteGattCharacteristic* characteristic) {
  FinishPendingTasks();
}

void BluetoothTestWin::SimulateGattNotifySessionStartError(
    BluetoothRemoteGattCharacteristic* characteristic,
    BluetoothRemoteGattService::GattErrorCode error_code) {
  win::GattCharacteristic* simulated_characteristic =
      GetSimulatedCharacteristic(characteristic);
  DCHECK(simulated_characteristic);
  DCHECK(error_code == BluetoothRemoteGattService::GATT_ERROR_UNKNOWN);
  fake_bt_le_wrapper_->SimulateGattCharacteristicSetNotifyError(
      simulated_characteristic, E_BLUETOOTH_ATT_UNKNOWN_ERROR);
}

void BluetoothTestWin::SimulateGattCharacteristicChanged(
    BluetoothRemoteGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value) {
  win::GattCharacteristic* target_simulated_characteristic = nullptr;
  if (characteristic) {
    target_simulated_characteristic =
        GetSimulatedCharacteristic(characteristic);
  }

  fake_bt_le_wrapper_->SimulateGattCharacteristicValue(
      target_simulated_characteristic, value);
  fake_bt_le_wrapper_->SimulateCharacteristicValueChangeNotification(
      target_simulated_characteristic);

  FinishPendingTasks();
}

void BluetoothTestWin::OnReadGattCharacteristicValue() {
  gatt_read_characteristic_attempts_++;
}

void BluetoothTestWin::OnWriteGattCharacteristicValue(
    const PBTH_LE_GATT_CHARACTERISTIC_VALUE value) {
  gatt_write_characteristic_attempts_++;
  last_write_value_.clear();
  for (ULONG i = 0; i < value->DataSize; i++)
    last_write_value_.push_back(value->Data[i]);
}

void BluetoothTestWin::OnStartCharacteristicNotification() {
  gatt_notify_characteristic_attempts_++;
}

void BluetoothTestWin::OnWriteGattDescriptorValue(
    const std::vector<uint8_t>& value) {
  gatt_write_descriptor_attempts_++;
  last_write_value_.assign(value.begin(), value.end());
}

win::GattService* BluetoothTestWin::GetSimulatedService(
    win::BLEDevice* device,
    BluetoothRemoteGattService* service) {
  CHECK(device);
  CHECK(service);

  std::vector<std::string> chain_of_att_handles;
  BluetoothRemoteGattServiceWin* win_service =
      static_cast<BluetoothRemoteGattServiceWin*>(service);
  chain_of_att_handles.insert(
      chain_of_att_handles.begin(),
      std::to_string(win_service->GetAttributeHandle()));
  win::GattService* simulated_service =
      fake_bt_le_wrapper_->GetSimulatedGattService(device,
                                                   chain_of_att_handles);
  CHECK(simulated_service);
  return simulated_service;
}

win::GattCharacteristic* BluetoothTestWin::GetSimulatedCharacteristic(
    BluetoothRemoteGattCharacteristic* characteristic) {
  CHECK(characteristic);
  BluetoothRemoteGattCharacteristicWin* win_characteristic =
      static_cast<BluetoothRemoteGattCharacteristicWin*>(characteristic);

  std::string device_address =
      win_characteristic->GetService()->GetDevice()->GetAddress();
  win::BLEDevice* target_device =
      fake_bt_le_wrapper_->GetSimulatedBLEDevice(device_address);
  if (target_device == nullptr)
    return nullptr;
  win::GattService* target_service =
      GetSimulatedService(target_device, win_characteristic->GetService());
  if (target_service == nullptr)
    return nullptr;
  return fake_bt_le_wrapper_->GetSimulatedGattCharacteristic(
      target_service, std::to_string(win_characteristic->GetAttributeHandle()));
}

void BluetoothTestWin::RunPendingTasksUntilCallback() {
  base::circular_deque<base::TestPendingTask> tasks =
      bluetooth_task_runner_->TakePendingTasks();
  int original_callback_count = callback_count_;
  int original_error_callback_count = error_callback_count_;
  do {
    base::TestPendingTask task = std::move(tasks.front());
    tasks.pop_front();
    std::move(task.task).Run();
    base::RunLoop().RunUntilIdle();
  } while (tasks.size() && callback_count_ == original_callback_count &&
           error_callback_count_ == original_error_callback_count);

  // Put the rest of pending tasks back to Bluetooth task runner.
  for (auto& task : tasks) {
    if (task.delay.is_zero()) {
      bluetooth_task_runner_->PostTask(task.location, std::move(task.task));
    } else {
      bluetooth_task_runner_->PostDelayedTask(task.location,
                                              std::move(task.task), task.delay);
    }
  }
}

void BluetoothTestWin::ForceRefreshDevice() {
  auto* adapter_win = static_cast<BluetoothAdapterWin*>(adapter_.get());
  adapter_win->force_update_device_for_test_ = true;
  FinishPendingTasks();
  adapter_win->force_update_device_for_test_ = false;

  // The characteristics still need to be discovered.
  base::RunLoop().RunUntilIdle();
  FinishPendingTasks();
}

void BluetoothTestWin::FinishPendingTasks() {
  bluetooth_task_runner_->RunPendingTasks();
  base::RunLoop().RunUntilIdle();
}

BluetoothTestWinrt::BluetoothTestWinrt() {
  if (GetParam()) {
    scoped_feature_list_.InitAndEnableFeature(kNewBLEWinImplementation);
    if (base::win::GetVersion() >= base::win::Version::WIN10) {
      scoped_winrt_initializer_.emplace();
    }
  } else {
    scoped_feature_list_.InitAndDisableFeature(kNewBLEWinImplementation);
  }
}

BluetoothTestWinrt::~BluetoothTestWinrt() {
  // The callbacks run by |notify_sessions_| may end up calling back into
  // |this|, so run them early to prevent a use-after-free.
  notify_sessions_.clear();
}

bool BluetoothTestWinrt::PlatformSupportsLowEnergy() {
  return GetParam() ? base::win::GetVersion() >= base::win::Version::WIN10
                    : BluetoothTestWin::PlatformSupportsLowEnergy();
}

void BluetoothTestWinrt::InitWithDefaultAdapter() {
  if (!GetParam()) {
    BluetoothTestWin::InitWithDefaultAdapter();
    return;
  }

  base::RunLoop run_loop;
  auto adapter = base::WrapRefCounted(new BluetoothAdapterWinrt());
  adapter->Init(run_loop.QuitClosure());
  adapter_ = std::move(adapter);
  run_loop.Run();
}

void BluetoothTestWinrt::InitWithoutDefaultAdapter() {
  if (!GetParam()) {
    BluetoothTestWin::InitWithoutDefaultAdapter();
    return;
  }

  base::RunLoop run_loop;
  adapter_ = base::MakeRefCounted<TestBluetoothAdapterWinrt>(
      nullptr, nullptr, run_loop.QuitClosure(), this);
  run_loop.Run();
}

void BluetoothTestWinrt::InitWithFakeAdapter() {
  if (!GetParam()) {
    BluetoothTestWin::InitWithFakeAdapter();
    return;
  }

  base::RunLoop run_loop;
  adapter_ = base::MakeRefCounted<TestBluetoothAdapterWinrt>(
      Make<FakeBluetoothAdapterWinrt>(kTestAdapterAddress,
                                      Make<FakeRadioWinrt>()),
      Make<FakeDeviceInformationWinrt>(kTestAdapterName),
      run_loop.QuitClosure(), this);
  run_loop.Run();
}

void BluetoothTestWinrt::InitFakeAdapterWithoutRadio() {
  base::RunLoop run_loop;
  adapter_ = base::MakeRefCounted<TestBluetoothAdapterWinrt>(
      Make<FakeBluetoothAdapterWinrt>(kTestAdapterAddress, nullptr /* radio */),
      Make<FakeDeviceInformationWinrt>(kTestAdapterName),
      run_loop.QuitClosure(), this);
  run_loop.Run();
}

void BluetoothTestWinrt::SimulateAdapterPowerFailure() {
  static_cast<FakeRadioWinrt*>(
      static_cast<TestBluetoothAdapterWinrt*>(adapter_.get())
          ->GetRadioForTesting())
      ->SimulateAdapterPowerFailure();
}

void BluetoothTestWinrt::SimulateAdapterPoweredOn() {
  auto* radio = static_cast<FakeRadioWinrt*>(
      static_cast<TestBluetoothAdapterWinrt*>(adapter_.get())
          ->GetRadioForTesting());

  if (radio) {
    radio->SimulateAdapterPoweredOn();
    return;
  }

  // This can happen when we simulate a fake adapter without a radio.
  static_cast<FakeDeviceWatcherWinrt*>(
      static_cast<TestBluetoothAdapterWinrt*>(adapter_.get())
          ->GetPoweredRadioWatcherForTesting())
      ->SimulateAdapterPoweredOn();
}

void BluetoothTestWinrt::SimulateAdapterPoweredOff() {
  auto* radio = static_cast<FakeRadioWinrt*>(
      static_cast<TestBluetoothAdapterWinrt*>(adapter_.get())
          ->GetRadioForTesting());

  if (radio) {
    radio->SimulateAdapterPoweredOff();
    return;
  }

  // This can happen when we simulate a fake adapter without a radio.
  static_cast<FakeDeviceWatcherWinrt*>(
      static_cast<TestBluetoothAdapterWinrt*>(adapter_.get())
          ->GetPoweredRadioWatcherForTesting())
      ->SimulateAdapterPoweredOff();
}

BluetoothDevice* BluetoothTestWinrt::SimulateLowEnergyDevice(
    int device_ordinal) {
  if (!GetParam() || !PlatformSupportsLowEnergy())
    return BluetoothTestWin::SimulateLowEnergyDevice(device_ordinal);

  LowEnergyDeviceData data = GetLowEnergyDeviceData(device_ordinal);
  static_cast<TestBluetoothAdapterWinrt*>(adapter_.get())
      ->watcher()
      ->SimulateLowEnergyDevice(data);

  base::RunLoop().RunUntilIdle();
  return adapter_->GetDevice(data.address);
}

void BluetoothTestWinrt::SimulateDevicePaired(BluetoothDevice* device,
                                              bool is_paired) {
  auto* const ble_device =
      static_cast<TestBluetoothDeviceWinrt*>(device)->ble_device();
  DCHECK(ble_device);
  ble_device->SimulateDevicePaired(is_paired);
}

void BluetoothTestWinrt::SimulatePairingPinCode(BluetoothDevice* device,
                                                std::string pin_code) {
  auto* const ble_device =
      static_cast<TestBluetoothDeviceWinrt*>(device)->ble_device();
  DCHECK(ble_device);
  ble_device->SimulatePairingPinCode(std::move(pin_code));
}

void BluetoothTestWinrt::SimulateAdvertisementStarted(
    BluetoothAdvertisement* advertisement) {
  static_cast<FakeBluetoothLEAdvertisementPublisherWinrt*>(
      static_cast<BluetoothAdvertisementWinrt*>(advertisement)
          ->GetPublisherForTesting())
      ->SimulateAdvertisementStarted();
}

void BluetoothTestWinrt::SimulateAdvertisementStopped(
    BluetoothAdvertisement* advertisement) {
  static_cast<FakeBluetoothLEAdvertisementPublisherWinrt*>(
      static_cast<BluetoothAdvertisementWinrt*>(advertisement)
          ->GetPublisherForTesting())
      ->SimulateAdvertisementStopped();
}

void BluetoothTestWinrt::SimulateAdvertisementError(
    BluetoothAdvertisement* advertisement,
    BluetoothAdvertisement::ErrorCode error_code) {
  static_cast<FakeBluetoothLEAdvertisementPublisherWinrt*>(
      static_cast<BluetoothAdvertisementWinrt*>(advertisement)
          ->GetPublisherForTesting())
      ->SimulateAdvertisementError(error_code);
}

void BluetoothTestWinrt::SimulateGattConnection(BluetoothDevice* device) {
  if (!GetParam() || !PlatformSupportsLowEnergy())
    return BluetoothTestWin::SimulateGattConnection(device);

  // Spin the message loop to make sure a device instance was obtained.
  base::RunLoop().RunUntilIdle();
  auto* const ble_device =
      static_cast<TestBluetoothDeviceWinrt*>(device)->ble_device();
  DCHECK(ble_device);
  ble_device->SimulateGattConnection();
}

void BluetoothTestWinrt::SimulateGattNameChange(BluetoothDevice* device,
                                                const std::string& new_name) {
  if (!GetParam() || !PlatformSupportsLowEnergy())
    return BluetoothTestWin::SimulateGattNameChange(device, new_name);

  auto* const ble_device =
      static_cast<TestBluetoothDeviceWinrt*>(device)->ble_device();
  DCHECK(ble_device);
  ble_device->SimulateGattNameChange(new_name);
}

void BluetoothTestWinrt::SimulateGattConnectionError(
    BluetoothDevice* device,
    BluetoothDevice::ConnectErrorCode error_code) {
  auto* const ble_device =
      static_cast<TestBluetoothDeviceWinrt*>(device)->ble_device();
  DCHECK(ble_device);
  ble_device->SimulateGattConnectionError(error_code);
}

void BluetoothTestWinrt::SimulateGattDisconnection(BluetoothDevice* device) {
  auto* const ble_device =
      static_cast<TestBluetoothDeviceWinrt*>(device)->ble_device();
  DCHECK(ble_device);
  ble_device->SimulateGattDisconnection();
}

void BluetoothTestWinrt::SimulateDeviceBreaksConnection(
    BluetoothDevice* device) {
  auto* const ble_device =
      static_cast<TestBluetoothDeviceWinrt*>(device)->ble_device();
  DCHECK(ble_device);
  ble_device->SimulateDeviceBreaksConnection();
}

void BluetoothTestWinrt::SimulateGattServicesDiscovered(
    BluetoothDevice* device,
    const std::vector<std::string>& uuids) {
  if (!GetParam() || !PlatformSupportsLowEnergy())
    return BluetoothTestWin::SimulateGattServicesDiscovered(device, uuids);

  auto* const ble_device =
      static_cast<TestBluetoothDeviceWinrt*>(device)->ble_device();
  DCHECK(ble_device);
  ble_device->SimulateGattServicesDiscovered(uuids);
}

void BluetoothTestWinrt::SimulateGattServicesChanged(BluetoothDevice* device) {
  if (!GetParam() || !PlatformSupportsLowEnergy())
    return BluetoothTestWin::SimulateGattServicesChanged(device);

  auto* const ble_device =
      static_cast<TestBluetoothDeviceWinrt*>(device)->ble_device();
  DCHECK(ble_device);
  ble_device->SimulateGattServicesChanged();
}

void BluetoothTestWinrt::SimulateGattServiceRemoved(
    BluetoothRemoteGattService* service) {
  if (!GetParam() || !PlatformSupportsLowEnergy())
    return BluetoothTestWin::SimulateGattServiceRemoved(service);

  auto* const ble_device =
      static_cast<TestBluetoothDeviceWinrt*>(service->GetDevice())
          ->ble_device();
  DCHECK(ble_device);
  ble_device->SimulateGattServiceRemoved(service);
}

void BluetoothTestWinrt::SimulateGattServicesDiscoveryError(
    BluetoothDevice* device) {
  if (!GetParam() || !PlatformSupportsLowEnergy())
    return BluetoothTestWin::SimulateGattServicesDiscoveryError(device);

  auto* const ble_device =
      static_cast<TestBluetoothDeviceWinrt*>(device)->ble_device();
  DCHECK(ble_device);
  ble_device->SimulateGattServicesDiscoveryError();
}

void BluetoothTestWinrt::SimulateGattCharacteristic(
    BluetoothRemoteGattService* service,
    const std::string& uuid,
    int properties) {
  if (!GetParam() || !PlatformSupportsLowEnergy())
    return BluetoothTestWin::SimulateGattCharacteristic(service, uuid,
                                                        properties);

  auto* const ble_device =
      static_cast<TestBluetoothDeviceWinrt*>(service->GetDevice())
          ->ble_device();
  DCHECK(ble_device);
  ble_device->SimulateGattCharacteristic(service, uuid, properties);
}

void BluetoothTestWinrt::SimulateGattNotifySessionStarted(
    BluetoothRemoteGattCharacteristic* characteristic) {
  if (!GetParam() || !PlatformSupportsLowEnergy())
    return BluetoothTestWin::SimulateGattNotifySessionStarted(characteristic);

  static_cast<FakeGattCharacteristicWinrt*>(
      static_cast<BluetoothRemoteGattCharacteristicWinrt*>(characteristic)
          ->GetCharacteristicForTesting())
      ->SimulateGattNotifySessionStarted();
}

void BluetoothTestWinrt::SimulateGattNotifySessionStartError(
    BluetoothRemoteGattCharacteristic* characteristic,
    BluetoothRemoteGattService::GattErrorCode error_code) {
  if (!GetParam() || !PlatformSupportsLowEnergy()) {
    return BluetoothTestWin::SimulateGattNotifySessionStartError(characteristic,
                                                                 error_code);
  }

  static_cast<FakeGattCharacteristicWinrt*>(
      static_cast<BluetoothRemoteGattCharacteristicWinrt*>(characteristic)
          ->GetCharacteristicForTesting())
      ->SimulateGattNotifySessionStartError(error_code);
}

void BluetoothTestWinrt::SimulateGattNotifySessionStopped(
    BluetoothRemoteGattCharacteristic* characteristic) {
  static_cast<FakeGattCharacteristicWinrt*>(
      static_cast<BluetoothRemoteGattCharacteristicWinrt*>(characteristic)
          ->GetCharacteristicForTesting())
      ->SimulateGattNotifySessionStopped();
}

void BluetoothTestWinrt::SimulateGattNotifySessionStopError(
    BluetoothRemoteGattCharacteristic* characteristic,
    BluetoothRemoteGattService::GattErrorCode error_code) {
  static_cast<FakeGattCharacteristicWinrt*>(
      static_cast<BluetoothRemoteGattCharacteristicWinrt*>(characteristic)
          ->GetCharacteristicForTesting())
      ->SimulateGattNotifySessionStopError(error_code);
}

void BluetoothTestWinrt::SimulateGattCharacteristicChanged(
    BluetoothRemoteGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value) {
  if (!GetParam() || !PlatformSupportsLowEnergy()) {
    return BluetoothTestWin::SimulateGattCharacteristicChanged(characteristic,
                                                               value);
  }

  static_cast<FakeGattCharacteristicWinrt*>(
      static_cast<BluetoothRemoteGattCharacteristicWinrt*>(characteristic)
          ->GetCharacteristicForTesting())
      ->SimulateGattCharacteristicChanged(value);
}

void BluetoothTestWinrt::SimulateGattCharacteristicRead(
    BluetoothRemoteGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value) {
  if (!GetParam() || !PlatformSupportsLowEnergy()) {
    return BluetoothTestWin::SimulateGattCharacteristicRead(characteristic,
                                                            value);
  }

  static_cast<FakeGattCharacteristicWinrt*>(
      static_cast<BluetoothRemoteGattCharacteristicWinrt*>(characteristic)
          ->GetCharacteristicForTesting())
      ->SimulateGattCharacteristicRead(value);
}

void BluetoothTestWinrt::SimulateGattCharacteristicReadError(
    BluetoothRemoteGattCharacteristic* characteristic,
    BluetoothRemoteGattService::GattErrorCode error_code) {
  if (!GetParam() || !PlatformSupportsLowEnergy()) {
    return BluetoothTestWin::SimulateGattCharacteristicReadError(characteristic,
                                                                 error_code);
  }

  static_cast<FakeGattCharacteristicWinrt*>(
      static_cast<BluetoothRemoteGattCharacteristicWinrt*>(characteristic)
          ->GetCharacteristicForTesting())
      ->SimulateGattCharacteristicReadError(error_code);
}

void BluetoothTestWinrt::SimulateGattCharacteristicWrite(
    BluetoothRemoteGattCharacteristic* characteristic) {
  if (!GetParam() || !PlatformSupportsLowEnergy())
    return BluetoothTestWin::SimulateGattCharacteristicWrite(characteristic);

  static_cast<FakeGattCharacteristicWinrt*>(
      static_cast<BluetoothRemoteGattCharacteristicWinrt*>(characteristic)
          ->GetCharacteristicForTesting())
      ->SimulateGattCharacteristicWrite();
}

void BluetoothTestWinrt::SimulateGattCharacteristicWriteError(
    BluetoothRemoteGattCharacteristic* characteristic,
    BluetoothRemoteGattService::GattErrorCode error_code) {
  if (!GetParam() || !PlatformSupportsLowEnergy()) {
    return BluetoothTestWin::SimulateGattCharacteristicWriteError(
        characteristic, error_code);
  }

  static_cast<FakeGattCharacteristicWinrt*>(
      static_cast<BluetoothRemoteGattCharacteristicWinrt*>(characteristic)
          ->GetCharacteristicForTesting())
      ->SimulateGattCharacteristicWriteError(error_code);
}

void BluetoothTestWinrt::SimulateGattDescriptor(
    BluetoothRemoteGattCharacteristic* characteristic,
    const std::string& uuid) {
  if (!GetParam() || !PlatformSupportsLowEnergy())
    return BluetoothTestWin::SimulateGattDescriptor(characteristic, uuid);

  auto* const ble_device = static_cast<TestBluetoothDeviceWinrt*>(
                               characteristic->GetService()->GetDevice())
                               ->ble_device();
  DCHECK(ble_device);
  ble_device->SimulateGattDescriptor(characteristic, uuid);
}

void BluetoothTestWinrt::SimulateGattDescriptorRead(
    BluetoothRemoteGattDescriptor* descriptor,
    const std::vector<uint8_t>& value) {
  static_cast<FakeGattDescriptorWinrt*>(
      static_cast<BluetoothRemoteGattDescriptorWinrt*>(descriptor)
          ->GetDescriptorForTesting())
      ->SimulateGattDescriptorRead(value);
}

void BluetoothTestWinrt::SimulateGattDescriptorReadError(
    BluetoothRemoteGattDescriptor* descriptor,
    BluetoothRemoteGattService::GattErrorCode error_code) {
  static_cast<FakeGattDescriptorWinrt*>(
      static_cast<BluetoothRemoteGattDescriptorWinrt*>(descriptor)
          ->GetDescriptorForTesting())
      ->SimulateGattDescriptorReadError(error_code);
}

void BluetoothTestWinrt::SimulateGattDescriptorWrite(
    BluetoothRemoteGattDescriptor* descriptor) {
  static_cast<FakeGattDescriptorWinrt*>(
      static_cast<BluetoothRemoteGattDescriptorWinrt*>(descriptor)
          ->GetDescriptorForTesting())
      ->SimulateGattDescriptorWrite();
}

void BluetoothTestWinrt::SimulateGattDescriptorWriteError(
    BluetoothRemoteGattDescriptor* descriptor,
    BluetoothRemoteGattService::GattErrorCode error_code) {
  static_cast<FakeGattDescriptorWinrt*>(
      static_cast<BluetoothRemoteGattDescriptorWinrt*>(descriptor)
          ->GetDescriptorForTesting())
      ->SimulateGattDescriptorWriteError(error_code);
}

void BluetoothTestWinrt::DeleteDevice(BluetoothDevice* device) {
  (!GetParam() || !PlatformSupportsLowEnergy())
      ? BluetoothTestWin::DeleteDevice(device)
      : BluetoothTestBase::DeleteDevice(device);
}

void BluetoothTestWinrt::OnFakeBluetoothDeviceConnectGattCalled() {
  ++gatt_connection_attempts_;
  ++gatt_discovery_attempts_;
}

void BluetoothTestWinrt::OnFakeBluetoothGattDisconnect() {
  ++gatt_disconnection_attempts_;
}

void BluetoothTestWinrt::OnFakeBluetoothCharacteristicReadValue() {
  ++gatt_read_characteristic_attempts_;
}

void BluetoothTestWinrt::OnFakeBluetoothCharacteristicWriteValue(
    std::vector<uint8_t> value) {
  last_write_value_ = std::move(value);
  ++gatt_write_characteristic_attempts_;
}

void BluetoothTestWinrt::OnFakeBluetoothGattSetCharacteristicNotification(
    NotifyValueState state) {
  last_write_value_ = {static_cast<uint8_t>(state), 0};
  ++gatt_notify_characteristic_attempts_;
  ++gatt_write_descriptor_attempts_;
}

void BluetoothTestWinrt::OnFakeBluetoothDescriptorReadValue() {
  ++gatt_read_descriptor_attempts_;
}

void BluetoothTestWinrt::OnFakeBluetoothDescriptorWriteValue(
    std::vector<uint8_t> value) {
  last_write_value_ = std::move(value);
  ++gatt_write_descriptor_attempts_;
}

}  // namespace device
