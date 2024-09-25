// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/bluetooth/test/bluetooth_test_win.h"

#include <windows.devices.bluetooth.h>
#include <windows.devices.radios.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/test_pending_task.h"
#include "base/time/time.h"
#include "base/win/vector.h"
#include "base/win/windows_version.h"
#include "device/base/features.h"
#include "device/bluetooth/bluetooth_adapter_win.h"
#include "device/bluetooth/bluetooth_adapter_winrt.h"
#include "device/bluetooth/bluetooth_advertisement_winrt.h"
#include "device/bluetooth/bluetooth_device_winrt.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic_winrt.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor_winrt.h"
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
#include "device/bluetooth/test/fake_gatt_session_winrt.h"
#include "device/bluetooth/test/fake_radio_winrt.h"

// Note: As UWP does not provide int specializations for IObservableVector and
// VectorChangedEventHandler we need to supply our own. UUIDs were generated
// using `uuidgen`.
namespace ABI::Windows::Foundation::Collections {

template <>
struct __declspec(uuid("2736c37e-4218-496f-a46a-92d5d9e610a9"))
    IObservableVector<GUID> : IObservableVector_impl<GUID> {};

template <>
struct __declspec(uuid("94844fba-ddf9-475c-be6e-ebb87039cef6"))
    VectorChangedEventHandler<GUID> : VectorChangedEventHandler_impl<GUID> {};

}  // namespace ABI::Windows::Foundation::Collections

namespace device {

namespace {

using ABI::Windows::Devices::Bluetooth::IBluetoothAdapter;
using ABI::Windows::Devices::Bluetooth::IBluetoothAdapterStatics;
using ABI::Windows::Devices::Bluetooth::IBluetoothLEDevice;
using ABI::Windows::Devices::Bluetooth::IBluetoothLEDeviceStatics;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    IBluetoothLEAdvertisement;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    IBluetoothLEAdvertisementPublisherFactory;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    IBluetoothLEAdvertisementWatcher;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    IBluetoothLEManufacturerDataFactory;
using ABI::Windows::Devices::Enumeration::DevicePairingKinds;
using ABI::Windows::Devices::Enumeration::IDeviceInformation;
using ABI::Windows::Devices::Enumeration::IDeviceInformationStatics;
using ABI::Windows::Devices::Radios::IRadioStatics;
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

  HRESULT GetGattSessionStaticsActivationFactory(
      ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
          IGattSessionStatics** statics) const override {
    auto gatt_session_statics =
        Make<FakeGattSessionStaticsWinrt>(bluetooth_test_winrt_);
    return gatt_session_statics.CopyTo(statics);
  }

  FakeBluetoothLEDeviceWinrt* ble_device() {
    return static_cast<FakeBluetoothLEDeviceWinrt*>(ble_device_.Get());
  }

  FakeGattSessionWinrt* gatt_session() {
    return static_cast<FakeGattSessionWinrt*>(gatt_session_.Get());
  }

 private:
  raw_ptr<BluetoothTestWinrt> bluetooth_test_winrt_ = nullptr;
};

class TestBluetoothAdapterWinrt : public BluetoothAdapterWinrt {
 public:
  TestBluetoothAdapterWinrt(ComPtr<IBluetoothAdapter> adapter,
                            ComPtr<IDeviceInformation> device_information,
                            ComPtr<IRadioStatics> radio_statics,
                            base::OnceClosure init_cb,
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
                 std::move(device_information_statics),
                 std::move(radio_statics));
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
  raw_ptr<BluetoothTestWinrt> bluetooth_test_winrt_ = nullptr;
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

}  // namespace

BluetoothTestWin::BluetoothTestWin()
    : ui_task_runner_(new base::TestSimpleTaskRunner()),
      bluetooth_task_runner_(new base::TestSimpleTaskRunner()) {}

BluetoothTestWin::~BluetoothTestWin() = default;

bool BluetoothTestWin::PlatformSupportsLowEnergy() {
  return false;
}

void BluetoothTestWin::InitWithDefaultAdapter() {
  auto adapter = base::WrapRefCounted(new BluetoothAdapterWin());
  base::RunLoop run_loop;
  adapter->Initialize(run_loop.QuitClosure());
  run_loop.Run();
  adapter_ = std::move(adapter);
}

void BluetoothTestWin::InitWithoutDefaultAdapter() {
  auto adapter = base::WrapRefCounted(new BluetoothAdapterWin());
  adapter->InitForTest(base::DoNothing(), nullptr, ui_task_runner_,
                       bluetooth_task_runner_);
  adapter_ = std::move(adapter);
}

void BluetoothTestWin::InitWithFakeAdapter() {
  auto fake_bt_classic_wrapper =
      std::make_unique<win::BluetoothClassicWrapperFake>();
  fake_bt_classic_wrapper->SimulateARadio(
      base::UTF8ToUTF16(kTestAdapterName),
      CanonicalStringToBLUETOOTH_ADDRESS(kTestAdapterAddress));

  auto adapter = base::WrapRefCounted(new BluetoothAdapterWin());
  base::RunLoop run_loop;
  adapter->InitForTest(run_loop.QuitClosure(),
                       std::move(fake_bt_classic_wrapper), nullptr,
                       bluetooth_task_runner_);
  adapter_ = std::move(adapter);
  FinishPendingTasks();
  run_loop.Run();
}

bool BluetoothTestWin::DenyPermission() {
  return false;
}

void BluetoothTestWin::StartLowEnergyDiscoverySession() {
  __super::StartLowEnergyDiscoverySession();
  FinishPendingTasks();
}

BluetoothDevice* BluetoothTestWin::SimulateLowEnergyDevice(int device_ordinal) {
  NOTREACHED();
}

std::optional<BluetoothUUID> BluetoothTestWin::GetTargetGattService(
    BluetoothDevice* device) {
  auto* const ble_device =
      static_cast<TestBluetoothDeviceWinrt*>(device)->ble_device();
  DCHECK(ble_device);
  return ble_device->GetTargetGattService();
}

void BluetoothTestWin::SimulateGattConnection(BluetoothDevice* device) {
  NOTREACHED();
}

void BluetoothTestWin::SimulateStatusChangeToDisconnect(
    BluetoothDevice* device) {
  NOTREACHED();
}

void BluetoothTestWin::SimulateGattServicesDiscovered(
    BluetoothDevice* device,
    const std::vector<std::string>& uuids,
    const std::vector<std::string>& blocked_uuids) {
  NOTREACHED();
}

void BluetoothTestWin::SimulateGattServiceRemoved(
    BluetoothRemoteGattService* service) {
  NOTREACHED();
}

void BluetoothTestWin::SimulateGattCharacteristic(
    BluetoothRemoteGattService* service,
    const std::string& uuid,
    int properties) {
  NOTREACHED();
}

void BluetoothTestWin::SimulateGattCharacteristicRemoved(
    BluetoothRemoteGattService* service,
    BluetoothRemoteGattCharacteristic* characteristic) {
  NOTREACHED();
}

void BluetoothTestWin::SimulateGattCharacteristicRead(
    BluetoothRemoteGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value) {
  NOTREACHED();
}

void BluetoothTestWin::SimulateGattCharacteristicReadError(
    BluetoothRemoteGattCharacteristic* characteristic,
    BluetoothGattService::GattErrorCode error_code) {
  NOTREACHED();
}

void BluetoothTestWin::SimulateGattCharacteristicWrite(
    BluetoothRemoteGattCharacteristic* characteristic) {
  NOTREACHED();
}

void BluetoothTestWin::SimulateGattCharacteristicWriteError(
    BluetoothRemoteGattCharacteristic* characteristic,
    BluetoothGattService::GattErrorCode error_code) {
  NOTREACHED();
}

void BluetoothTestWin::DeleteDevice(BluetoothDevice* device) {
  NOTREACHED();
}

void BluetoothTestWin::SimulateGattDescriptor(
    BluetoothRemoteGattCharacteristic* characteristic,
    const std::string& uuid) {
  NOTREACHED();
}

void BluetoothTestWin::SimulateGattNotifySessionStarted(
    BluetoothRemoteGattCharacteristic* characteristic) {
  NOTREACHED();
}

void BluetoothTestWin::SimulateGattNotifySessionStartError(
    BluetoothRemoteGattCharacteristic* characteristic,
    BluetoothGattService::GattErrorCode error_code) {
  NOTREACHED();
}

void BluetoothTestWin::SimulateGattCharacteristicChanged(
    BluetoothRemoteGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value) {
  NOTREACHED();
}

void BluetoothTestWin::FinishPendingTasks() {
  bluetooth_task_runner_->RunPendingTasks();
  base::RunLoop().RunUntilIdle();
}

BluetoothTestWinrt::BluetoothTestWinrt() {
  std::vector<base::test::FeatureRef> enabled;
  std::vector<base::test::FeatureRef> disabled;
  if (GetParam().new_gatt_session_handling_enabled) {
    enabled.push_back(kNewBLEGattSessionHandling);
  } else {
    disabled.push_back(kNewBLEGattSessionHandling);
  }
  // TODO(crbug.com/40847175): Remove once `kWebBluetoothConfirmPairingSupport`
  // is enabled by default.
  enabled.push_back(features::kWebBluetoothConfirmPairingSupport);
  scoped_feature_list_.InitWithFeatures(enabled, disabled);
}

BluetoothTestWinrt::~BluetoothTestWinrt() {
  // The callbacks run by |notify_sessions_| may end up calling back into
  // |this|, so run them early to prevent a use-after-free.
  notify_sessions_.clear();
}

bool BluetoothTestWinrt::UsesNewGattSessionHandling() const {
  return GetParam().new_gatt_session_handling_enabled &&
         base::win::GetVersion() >= base::win::Version::WIN10_RS3;
}

bool BluetoothTestWinrt::PlatformSupportsLowEnergy() {
  return true;
}

void BluetoothTestWinrt::InitWithDefaultAdapter() {
  base::RunLoop run_loop;
  auto adapter = base::WrapRefCounted(new BluetoothAdapterWinrt());
  adapter->Initialize(run_loop.QuitClosure());
  adapter_ = std::move(adapter);
  run_loop.Run();
}

void BluetoothTestWinrt::InitWithoutDefaultAdapter() {
  base::RunLoop run_loop;
  adapter_ = base::MakeRefCounted<TestBluetoothAdapterWinrt>(
      /*adapter=*/nullptr, /*device_information=*/nullptr,
      Make<FakeRadioStaticsWinrt>(), run_loop.QuitClosure(), this);
  run_loop.Run();
}

void BluetoothTestWinrt::InitWithFakeAdapter() {
  base::RunLoop run_loop;
  adapter_ = base::MakeRefCounted<TestBluetoothAdapterWinrt>(
      Make<FakeBluetoothAdapterWinrt>(kTestAdapterAddress,
                                      Make<FakeRadioWinrt>()),
      Make<FakeDeviceInformationWinrt>(kTestAdapterName),
      Make<FakeRadioStaticsWinrt>(), run_loop.QuitClosure(), this);
  run_loop.Run();
}

void BluetoothTestWinrt::InitFakeAdapterWithoutRadio() {
  base::RunLoop run_loop;
  adapter_ = base::MakeRefCounted<TestBluetoothAdapterWinrt>(
      Make<FakeBluetoothAdapterWinrt>(kTestAdapterAddress, /*radio=*/nullptr),
      Make<FakeDeviceInformationWinrt>(kTestAdapterName),
      Make<FakeRadioStaticsWinrt>(), run_loop.QuitClosure(), this);
  run_loop.Run();
}

void BluetoothTestWinrt::InitFakeAdapterWithRadioAccessDenied() {
  // Simulate "allow apps to control radio access" toggled off in
  // Windows 10 Privacy settings.
  base::RunLoop run_loop;
  auto radio_statics = Make<FakeRadioStaticsWinrt>();
  radio_statics->SimulateRequestAccessAsyncError(
      ABI::Windows::Devices::Radios::RadioAccessStatus_DeniedByUser);
  adapter_ = base::MakeRefCounted<TestBluetoothAdapterWinrt>(
      Make<FakeBluetoothAdapterWinrt>(kTestAdapterAddress,
                                      Make<FakeRadioWinrt>()),
      Make<FakeDeviceInformationWinrt>(kTestAdapterName),
      std::move(radio_statics), run_loop.QuitClosure(), this);
  run_loop.Run();
}

void BluetoothTestWinrt::SimulateSpuriousRadioStateChangedEvent() {
  static_cast<FakeRadioWinrt*>(
      static_cast<TestBluetoothAdapterWinrt*>(adapter_.get())
          ->GetRadioForTesting())
      ->SimulateSpuriousStateChangedEvent();
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
  LowEnergyDeviceData data = GetLowEnergyDeviceData(device_ordinal);
  static_cast<TestBluetoothAdapterWinrt*>(adapter_.get())
      ->watcher()
      ->SimulateLowEnergyDevice(data);

  base::RunLoop().RunUntilIdle();
  return adapter_->GetDevice(data.address);
}

void BluetoothTestWinrt::SimulateLowEnergyDiscoveryFailure() {
  static_cast<TestBluetoothAdapterWinrt*>(adapter_.get())
      ->watcher()
      ->SimulateDiscoveryError();

  // Spin until the WatcherStopped event fires.
  base::RunLoop().RunUntilIdle();
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

void BluetoothTestWinrt::SimulateConfirmOnly(BluetoothDevice* device) {
  auto* const ble_device =
      static_cast<TestBluetoothDeviceWinrt*>(device)->ble_device();
  DCHECK(ble_device);
  ble_device->SimulateConfirmOnly();
}

void BluetoothTestWinrt::SimulateDisplayPin(BluetoothDevice* device,
                                            std::string_view display_pin) {
  auto* const ble_device =
      static_cast<TestBluetoothDeviceWinrt*>(device)->ble_device();
  DCHECK(ble_device);
  ble_device->SimulateDisplayPin(display_pin);
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
  // Spin the message loop to make sure a device instance was obtained.
  base::RunLoop().RunUntilIdle();
  FakeBluetoothLEDeviceWinrt* const ble_device =
      static_cast<TestBluetoothDeviceWinrt*>(device)->ble_device();
  ble_device->SimulateGattConnection();

  if (UsesNewGattSessionHandling()) {
    static_cast<TestBluetoothDeviceWinrt*>(device)
        ->gatt_session()
        ->SimulateGattConnection();

    // Spin the message loop again to make sure the device received a
    // GattSessionStatus change event.
    base::RunLoop().RunUntilIdle();
  }
}

void BluetoothTestWinrt::SimulateGattNameChange(BluetoothDevice* device,
                                                const std::string& new_name) {
  auto* const ble_device =
      static_cast<TestBluetoothDeviceWinrt*>(device)->ble_device();
  DCHECK(ble_device);
  ble_device->SimulateGattNameChange(new_name);
}

void BluetoothTestWinrt::SimulateStatusChangeToDisconnect(
    BluetoothDevice* device) {
  // Spin the message loop to make sure a device instance was obtained.
  base::RunLoop().RunUntilIdle();
  auto* const ble_device =
      static_cast<TestBluetoothDeviceWinrt*>(device)->ble_device();
  DCHECK(ble_device);
  ble_device->SimulateStatusChangeToDisconnect();
}

void BluetoothTestWinrt::SimulateGattConnectionError(
    BluetoothDevice* device,
    BluetoothDevice::ConnectErrorCode error_code) {
  auto* const ble_device =
      static_cast<TestBluetoothDeviceWinrt*>(device)->ble_device();
  DCHECK(ble_device);
  ble_device->SimulateGattConnectionError(error_code);
  if (UsesNewGattSessionHandling()) {
    static_cast<TestBluetoothDeviceWinrt*>(device)
        ->gatt_session()
        ->SimulateGattConnectionError();
  }
}

void BluetoothTestWinrt::SimulateGattDisconnection(BluetoothDevice* device) {
  auto* const ble_device =
      static_cast<TestBluetoothDeviceWinrt*>(device)->ble_device();
  DCHECK(ble_device);
  ble_device->SimulateGattDisconnection();
  if (UsesNewGattSessionHandling()) {
    static_cast<TestBluetoothDeviceWinrt*>(device)
        ->gatt_session()
        ->SimulateGattDisconnection();
  }
}

void BluetoothTestWinrt::SimulateDeviceBreaksConnection(
    BluetoothDevice* device) {
  auto* const ble_device =
      static_cast<TestBluetoothDeviceWinrt*>(device)->ble_device();
  DCHECK(ble_device);
  ble_device->SimulateDeviceBreaksConnection();
  if (UsesNewGattSessionHandling()) {
    static_cast<TestBluetoothDeviceWinrt*>(device)
        ->gatt_session()
        ->SimulateGattDisconnection();
  }
}

void BluetoothTestWinrt::SimulateGattServicesDiscovered(
    BluetoothDevice* device,
    const std::vector<std::string>& uuids,
    const std::vector<std::string>& blocked_uuids) {
  auto* const ble_device =
      static_cast<TestBluetoothDeviceWinrt*>(device)->ble_device();
  DCHECK(ble_device);
  ble_device->SimulateGattServicesDiscovered(uuids, blocked_uuids);
}

void BluetoothTestWinrt::SimulateGattServicesChanged(BluetoothDevice* device) {
  auto* const ble_device =
      static_cast<TestBluetoothDeviceWinrt*>(device)->ble_device();
  DCHECK(ble_device);
  ble_device->SimulateGattServicesChanged();
}

void BluetoothTestWinrt::SimulateGattServiceRemoved(
    BluetoothRemoteGattService* service) {
  auto* const ble_device =
      static_cast<TestBluetoothDeviceWinrt*>(service->GetDevice())
          ->ble_device();
  DCHECK(ble_device);
  ble_device->SimulateGattServiceRemoved(service);
}

void BluetoothTestWinrt::SimulateGattServicesDiscoveryError(
    BluetoothDevice* device) {
  auto* const ble_device =
      static_cast<TestBluetoothDeviceWinrt*>(device)->ble_device();
  DCHECK(ble_device);
  ble_device->SimulateGattServicesDiscoveryError();
}

void BluetoothTestWinrt::SimulateGattCharacteristic(
    BluetoothRemoteGattService* service,
    const std::string& uuid,
    int properties) {
  auto* const ble_device =
      static_cast<TestBluetoothDeviceWinrt*>(service->GetDevice())
          ->ble_device();
  DCHECK(ble_device);
  ble_device->SimulateGattCharacteristic(service, uuid, properties);
}

void BluetoothTestWinrt::SimulateGattNotifySessionStarted(
    BluetoothRemoteGattCharacteristic* characteristic) {
  static_cast<FakeGattCharacteristicWinrt*>(
      static_cast<BluetoothRemoteGattCharacteristicWinrt*>(characteristic)
          ->GetCharacteristicForTesting())
      ->SimulateGattNotifySessionStarted();
}

void BluetoothTestWinrt::SimulateGattNotifySessionStartError(
    BluetoothRemoteGattCharacteristic* characteristic,
    BluetoothGattService::GattErrorCode error_code) {
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
    BluetoothGattService::GattErrorCode error_code) {
  static_cast<FakeGattCharacteristicWinrt*>(
      static_cast<BluetoothRemoteGattCharacteristicWinrt*>(characteristic)
          ->GetCharacteristicForTesting())
      ->SimulateGattNotifySessionStopError(error_code);
}

void BluetoothTestWinrt::SimulateGattCharacteristicChanged(
    BluetoothRemoteGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value) {
  static_cast<FakeGattCharacteristicWinrt*>(
      static_cast<BluetoothRemoteGattCharacteristicWinrt*>(characteristic)
          ->GetCharacteristicForTesting())
      ->SimulateGattCharacteristicChanged(value);
}

void BluetoothTestWinrt::SimulateGattCharacteristicRead(
    BluetoothRemoteGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value) {
  static_cast<FakeGattCharacteristicWinrt*>(
      static_cast<BluetoothRemoteGattCharacteristicWinrt*>(characteristic)
          ->GetCharacteristicForTesting())
      ->SimulateGattCharacteristicRead(value);
}

void BluetoothTestWinrt::SimulateGattCharacteristicReadError(
    BluetoothRemoteGattCharacteristic* characteristic,
    BluetoothGattService::GattErrorCode error_code) {
  static_cast<FakeGattCharacteristicWinrt*>(
      static_cast<BluetoothRemoteGattCharacteristicWinrt*>(characteristic)
          ->GetCharacteristicForTesting())
      ->SimulateGattCharacteristicReadError(error_code);
}

void BluetoothTestWinrt::SimulateGattCharacteristicWrite(
    BluetoothRemoteGattCharacteristic* characteristic) {
  static_cast<FakeGattCharacteristicWinrt*>(
      static_cast<BluetoothRemoteGattCharacteristicWinrt*>(characteristic)
          ->GetCharacteristicForTesting())
      ->SimulateGattCharacteristicWrite();
}

void BluetoothTestWinrt::SimulateGattCharacteristicWriteError(
    BluetoothRemoteGattCharacteristic* characteristic,
    BluetoothGattService::GattErrorCode error_code) {
  static_cast<FakeGattCharacteristicWinrt*>(
      static_cast<BluetoothRemoteGattCharacteristicWinrt*>(characteristic)
          ->GetCharacteristicForTesting())
      ->SimulateGattCharacteristicWriteError(error_code);
}

void BluetoothTestWinrt::SimulateGattDescriptor(
    BluetoothRemoteGattCharacteristic* characteristic,
    const std::string& uuid) {
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
    BluetoothGattService::GattErrorCode error_code) {
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
    BluetoothGattService::GattErrorCode error_code) {
  static_cast<FakeGattDescriptorWinrt*>(
      static_cast<BluetoothRemoteGattDescriptorWinrt*>(descriptor)
          ->GetDescriptorForTesting())
      ->SimulateGattDescriptorWriteError(error_code);
}

void BluetoothTestWinrt::DeleteDevice(BluetoothDevice* device) {
  BluetoothTestBase::DeleteDevice(device);
}

void BluetoothTestWinrt::OnFakeBluetoothDeviceConnectGattAttempt() {
  ++gatt_connection_attempts_;
}

void BluetoothTestWinrt::OnFakeBluetoothDeviceGattServiceDiscoveryAttempt() {
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
