// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_gatt_session_winrt.h"

#include <wrl/client.h>

#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/win/async_operation.h"
#include "device/bluetooth/test/bluetooth_test_win.h"

namespace device {

namespace {

using ABI::Windows::Devices::Bluetooth::BluetoothError;
using ABI::Windows::Devices::Bluetooth::BluetoothError_OtherError;
using ABI::Windows::Devices::Bluetooth::BluetoothError_Success;
using ABI::Windows::Devices::Bluetooth::IBluetoothDeviceId;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::GattSession;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattSessionStatus;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattSessionStatus_Active;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattSessionStatus_Closed;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattSessionStatusChangedEventArgs;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    IGattSessionStatusChangedEventArgs;
using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Foundation::ITypedEventHandler;
using Microsoft::WRL::Make;

class FakeGattSessionStatusChangedEventArgs
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              IGattSessionStatusChangedEventArgs> {
 public:
  FakeGattSessionStatusChangedEventArgs(GattSessionStatus status,
                                        BluetoothError error)
      : status_(status), error_(error) {}
  FakeGattSessionStatusChangedEventArgs(
      const FakeGattSessionStatusChangedEventArgs&) = delete;
  FakeGattSessionStatusChangedEventArgs& operator=(
      const FakeGattSessionStatusChangedEventArgs&) = delete;
  ~FakeGattSessionStatusChangedEventArgs() override {}

  IFACEMETHODIMP get_Error(BluetoothError* value) override {
    *value = error_;
    return S_OK;
  }

  IFACEMETHODIMP get_Status(GattSessionStatus* value) override {
    *value = status_;
    return S_OK;
  }

 private:
  GattSessionStatus status_;
  BluetoothError error_;
};

}  // namespace

FakeGattSessionWinrt::FakeGattSessionWinrt(
    BluetoothTestWinrt* bluetooth_test_winrt)
    : bluetooth_test_winrt_(bluetooth_test_winrt) {}

FakeGattSessionWinrt::~FakeGattSessionWinrt() = default;

void FakeGattSessionWinrt::SimulateGattConnection() {
  // Ensure the GattSession.MaintainConnection property has been set in order
  // to force the OS to establish a GATT connection.
  DCHECK(maintain_connection_);
  status_ = GattSessionStatus_Active;
  auto args = Make<FakeGattSessionStatusChangedEventArgs>(
      GattSessionStatus_Active, BluetoothError_Success);
  status_changed_handler_->Invoke(this, args.Get());
}

void FakeGattSessionWinrt::SimulateGattDisconnection() {
  if (status_ == GattSessionStatus_Closed) {
    SimulateGattConnectionError();
    return;
  }

  status_ = GattSessionStatus_Closed;
  auto args = Make<FakeGattSessionStatusChangedEventArgs>(
      GattSessionStatus_Closed, BluetoothError_Success);
  status_changed_handler_->Invoke(this, args.Get());
}

void FakeGattSessionWinrt::SimulateGattConnectionError() {
  status_ = GattSessionStatus_Closed;
  auto args = Make<FakeGattSessionStatusChangedEventArgs>(
      GattSessionStatus_Closed, BluetoothError_OtherError);
  status_changed_handler_->Invoke(this, args.Get());
}

HRESULT FakeGattSessionWinrt::get_DeviceId(IBluetoothDeviceId** value) {
  return E_NOTIMPL;
}

HRESULT FakeGattSessionWinrt::get_CanMaintainConnection(::boolean* value) {
  *value = true;
  return S_OK;
}

HRESULT FakeGattSessionWinrt::put_MaintainConnection(::boolean value) {
  if (!maintain_connection_ && value) {
    bluetooth_test_winrt_->OnFakeBluetoothDeviceConnectGattAttempt();
  }

  maintain_connection_ = value;
  return S_OK;
}

HRESULT FakeGattSessionWinrt::get_MaintainConnection(::boolean* value) {
  *value = maintain_connection_;
  return S_OK;
}

HRESULT FakeGattSessionWinrt::get_MaxPduSize(UINT16* value) {
  return E_NOTIMPL;
}

HRESULT FakeGattSessionWinrt::get_SessionStatus(GattSessionStatus* value) {
  *value = status_;
  return S_OK;
}

HRESULT FakeGattSessionWinrt::add_MaxPduSizeChanged(
    ITypedEventHandler<GattSession*, IInspectable*>* handler,
    EventRegistrationToken* token) {
  return E_NOTIMPL;
}

HRESULT
FakeGattSessionWinrt::remove_MaxPduSizeChanged(EventRegistrationToken token) {
  return E_NOTIMPL;
}

HRESULT FakeGattSessionWinrt::add_SessionStatusChanged(
    ITypedEventHandler<GattSession*, GattSessionStatusChangedEventArgs*>*
        handler,
    EventRegistrationToken* token) {
  status_changed_handler_ = handler;
  return S_OK;
}

HRESULT
FakeGattSessionWinrt::remove_SessionStatusChanged(
    EventRegistrationToken token) {
  status_changed_handler_ = nullptr;
  return S_OK;
}

HRESULT FakeGattSessionWinrt::Close() {
  return S_OK;
}

FakeGattSessionStaticsWinrt::FakeGattSessionStaticsWinrt(
    BluetoothTestWinrt* bluetooth_test_winrt)
    : bluetooth_test_winrt_(bluetooth_test_winrt) {
  // BluetoothDeviceWinrt does not obtain a GattSession if
  // |kNewBLEGattSessionHandling| is disabled.
  DCHECK(bluetooth_test_winrt_->UsesNewGattSessionHandling());
}

FakeGattSessionStaticsWinrt::~FakeGattSessionStaticsWinrt() = default;

HRESULT FakeGattSessionStaticsWinrt::FromDeviceIdAsync(
    IBluetoothDeviceId* device_id,
    IAsyncOperation<GattSession*>** operation) {
  DCHECK(device_id);
  auto async_op = Make<base::win::AsyncOperation<GattSession*>>();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(async_op->callback(),
                     Make<FakeGattSessionWinrt>(bluetooth_test_winrt_)));
  *operation = async_op.Detach();
  return S_OK;
}

}  // namespace device
