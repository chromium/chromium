// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/digital_credentials/cross_device_transaction_impl.h"

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/scoped_logging_settings.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "content/public/browser/digital_credentials_cross_device.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/fido/cable/v2_test_util.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using testing::Invoke;
using testing::NiceMock;
using testing::Return;

namespace content::digital_credentials::cross_device {
namespace {

MATCHER_P(ContainsError, expected_error, "") {
  if (arg.has_value()) {
    *result_listener << "unexpected successful value";
    return false;
  }
  const auto* error =
      absl::get_if<std::remove_const_t<decltype(expected_error)>>(&arg.error());
  if (!error) {
    *result_listener << "error of unexpected type";
    return false;
  }
  if (*error != expected_error) {
    *result_listener << "error had correct type but value "
                     << static_cast<int>(*error) << " when "
                     << static_cast<int>(expected_error) << " was wanted";
    return false;
  }
  return true;
}

class DigitalIdentityCrossDeviceTransactionTest : public ::testing::Test {
 public:
  void SetUp() override {
    scoped_vmodule_.InitWithSwitches("device_event_log_impl=2");

    network_context_ = device::cablev2::NewMockTunnelServer(std::nullopt);
    mock_adapter_ =
        base::MakeRefCounted<NiceMock<device::MockBluetoothAdapter>>();
    device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter_);
    bluetooth_values_for_testing_ =
        device::BluetoothAdapterFactory::Get()->InitGlobalOverrideValues();
    bluetooth_values_for_testing_->SetLESupported(true);
  }

 protected:
  static url::Origin origin() {
    const GURL url("https://example.com");
    return url::Origin::Create(url);
  }

  static base::Value request() {
    base::Value::Dict request_value;
    request_value.Set("foo", "bar");
    return base::Value(std::move(request));
  }

  static std::array<uint8_t, device::cablev2::kQRKeySize> qr_generator_key() {
    std::array<uint8_t, device::cablev2::kQRKeySize> key = {0};
    return key;
  }

  device::NetworkContextFactory network_context_factory() {
    return base::BindLambdaForTesting([&]() { return network_context_.get(); });
  }

  logging::ScopedVmoduleSwitches scoped_vmodule_;
  std::unique_ptr<device::BluetoothAdapterFactory::GlobalOverrideValues>
      bluetooth_values_for_testing_;
  std::unique_ptr<network::mojom::NetworkContext> network_context_;
  scoped_refptr<NiceMock<device::MockBluetoothAdapter>> mock_adapter_;
  base::test::TestFuture<base::expected<Response, Error>> callback_;
  base::test::TestFuture<Event> event_callback_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(DigitalIdentityCrossDeviceTransactionTest, NoBle) {
  bluetooth_values_for_testing_->SetLESupported(false);

  std::unique_ptr<Transaction> transaction = Transaction::New(
      origin(), request(), qr_generator_key(), network_context_factory(),
      base::DoNothing(), callback_.GetCallback());
  EXPECT_THAT(callback_.Take(), ContainsError(SystemError::kNoBleSupport));
}

TEST_F(DigitalIdentityCrossDeviceTransactionTest, NoAdapter) {
  EXPECT_CALL(*mock_adapter_, IsPresent).WillRepeatedly(Return(false));

  std::unique_ptr<Transaction> transaction = Transaction::New(
      origin(), request(), qr_generator_key(), network_context_factory(),
      base::DoNothing(), callback_.GetCallback());
  EXPECT_THAT(callback_.Take(), ContainsError(SystemError::kNoBleSupport));
}

TEST_F(DigitalIdentityCrossDeviceTransactionTest, PermissionDenied) {
  EXPECT_CALL(*mock_adapter_, IsPresent).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_adapter_, GetOsPermissionStatus)
      .WillRepeatedly(
          Return(device::BluetoothAdapter::PermissionStatus::kDenied));

  std::unique_ptr<Transaction> transaction = Transaction::New(
      origin(), request(), qr_generator_key(), network_context_factory(),
      base::DoNothing(), callback_.GetCallback());
  EXPECT_THAT(callback_.Take(), ContainsError(SystemError::kPermissionDenied));
}

TEST_F(DigitalIdentityCrossDeviceTransactionTest, NoPowerThenPowered) {
  EXPECT_CALL(*mock_adapter_, IsPresent).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_adapter_, GetOsPermissionStatus)
      .WillRepeatedly(
          Return(device::BluetoothAdapter::PermissionStatus::kAllowed));
  EXPECT_CALL(*mock_adapter_, IsPowered).WillRepeatedly(Return(false));

  std::unique_ptr<Transaction> transaction = Transaction::New(
      origin(), request(), qr_generator_key(), network_context_factory(),
      event_callback_.GetRepeatingCallback(), callback_.GetCallback());

  EXPECT_EQ(event_callback_.Take(), Event(SystemEvent::kBluetoothNotPowered));

  reinterpret_cast<TransactionImpl*>(transaction.get())
      ->AdapterPoweredChanged(nullptr, /*powered=*/true);

  EXPECT_EQ(event_callback_.Take(), Event(SystemEvent::kReady));
}

TEST_F(DigitalIdentityCrossDeviceTransactionTest, NeedPermissionThenDenied) {
  EXPECT_CALL(*mock_adapter_, IsPresent).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_adapter_, GetOsPermissionStatus)
      .WillOnce(
          Return(device::BluetoothAdapter::PermissionStatus::kUndetermined));
  EXPECT_CALL(*mock_adapter_, IsPowered).WillRepeatedly(Return(true));

  device::BluetoothAdapter::RequestSystemPermissionCallback permission_callback;
  EXPECT_CALL(*mock_adapter_, RequestSystemPermission)
      .WillOnce(Invoke(
          [&permission_callback](
              device::BluetoothAdapter::RequestSystemPermissionCallback
                  callback) { permission_callback = std::move(callback); }));

  std::unique_ptr<Transaction> transaction = Transaction::New(
      origin(), request(), qr_generator_key(), network_context_factory(),
      event_callback_.GetRepeatingCallback(), callback_.GetCallback());

  EXPECT_EQ(event_callback_.Take(), Event(SystemEvent::kNeedPermission));

  std::move(permission_callback)
      .Run(device::BluetoothAdapter::PermissionStatus::kDenied);

  EXPECT_THAT(callback_.Take(), ContainsError(SystemError::kPermissionDenied));
}

TEST_F(DigitalIdentityCrossDeviceTransactionTest, NeedPermissionThenGranted) {
  EXPECT_CALL(*mock_adapter_, IsPresent).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_adapter_, GetOsPermissionStatus)
      .WillOnce(
          Return(device::BluetoothAdapter::PermissionStatus::kUndetermined));
  EXPECT_CALL(*mock_adapter_, IsPowered).WillRepeatedly(Return(true));

  device::BluetoothAdapter::RequestSystemPermissionCallback permission_callback;
  EXPECT_CALL(*mock_adapter_, RequestSystemPermission)
      .WillOnce(Invoke(
          [&permission_callback](
              device::BluetoothAdapter::RequestSystemPermissionCallback
                  callback) { permission_callback = std::move(callback); }));

  std::unique_ptr<Transaction> transaction = Transaction::New(
      origin(), request(), qr_generator_key(), network_context_factory(),
      event_callback_.GetRepeatingCallback(), callback_.GetCallback());

  EXPECT_EQ(event_callback_.Take(), Event(SystemEvent::kNeedPermission));

  std::move(permission_callback)
      .Run(device::BluetoothAdapter::PermissionStatus::kAllowed);

  EXPECT_EQ(event_callback_.Take(), Event(SystemEvent::kReady));
}

TEST_F(DigitalIdentityCrossDeviceTransactionTest,
       BleTurnedOffDuringTransaction) {
  EXPECT_CALL(*mock_adapter_, IsPresent).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_adapter_, GetOsPermissionStatus)
      .WillOnce(Return(device::BluetoothAdapter::PermissionStatus::kAllowed));
  EXPECT_CALL(*mock_adapter_, IsPowered).WillRepeatedly(Return(true));

  std::unique_ptr<Transaction> transaction = Transaction::New(
      origin(), request(), qr_generator_key(), network_context_factory(),
      event_callback_.GetRepeatingCallback(), callback_.GetCallback());

  EXPECT_EQ(event_callback_.Take(), Event(SystemEvent::kReady));

  reinterpret_cast<TransactionImpl*>(transaction.get())
      ->AdapterPoweredChanged(nullptr, /*powered=*/false);

  EXPECT_THAT(callback_.Take(), ContainsError(SystemError::kLostPower));
}

}  // namespace
}  // namespace content::digital_credentials::cross_device
