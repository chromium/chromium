// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/floss_bluetooth_telephony_client.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_exported_object.h"
#include "dbus/mock_object_proxy.h"
#include "device/bluetooth/floss/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using testing::DoAll;
}  // namespace

namespace floss {

class FlossBluetoothTelephonyClientTest : public testing::Test {
 public:
  base::Version GetCurrVersion() {
    return floss::version::GetMaximalSupportedVersion();
  }

  void SetUpMocks() {
    bluetooth_telephony_path_ =
        FlossDBusClient::GenerateBluetoothTelephonyPath(adapter_index_);
    bluetooth_telephony_object_proxy_ =
        base::MakeRefCounted<::dbus::MockObjectProxy>(
            bus_.get(), kBluetoothTelephonyInterface,
            bluetooth_telephony_path_);

    EXPECT_CALL(*bus_.get(), GetObjectProxy(kBluetoothTelephonyInterface,
                                            bluetooth_telephony_path_))
        .WillRepeatedly(
            ::testing::Return(bluetooth_telephony_object_proxy_.get()));

    // Handle method calls on the object proxy
    ON_CALL(*bluetooth_telephony_object_proxy_.get(),
            DoCallMethodWithErrorResponse(
                HasMemberOf(bluetooth_telephony::kSetPhoneOpsEnabled),
                testing::_, testing::_))
        .WillByDefault(Invoke(
            this,
            &FlossBluetoothTelephonyClientTest::HandleSetPhoneOpsEnabled));
  }

  void SetUp() override {
    ::dbus::Bus::Options options;
    options.bus_type = ::dbus::Bus::BusType::SYSTEM;
    bus_ = base::MakeRefCounted<::dbus::MockBus>(options);
    client_ = FlossBluetoothTelephonyClient::Create();
    SetUpMocks();
  }

  void TearDown() override { client_.reset(); }

  void SetPhoneOpsEnabledCallback(DBusResult<Void> ret) { callback_count_++; }

  void HandleSetPhoneOpsEnabled(
      ::dbus::MethodCall* method_call,
      int timeout_ms,
      ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
    auto response = ::dbus::Response::CreateEmpty();
    ::dbus::MessageWriter msg(response.get());
    std::move(*cb).Run(response.get(), nullptr);
  }

  void TestSetPhoneOpsEnabled() {
    EXPECT_CALL(*bluetooth_telephony_object_proxy_.get(),
                DoCallMethodWithErrorResponse)
        .Times(testing::AnyNumber());
    EXPECT_CALL(*bluetooth_telephony_object_proxy_.get(),
                DoCallMethodWithErrorResponse(
                    HasMemberOf(bluetooth_telephony::kSetPhoneOpsEnabled),
                    testing::_, testing::_))
        .Times(1);
    EXPECT_EQ(callback_count_, 0);
    client_->Init(bus_.get(), kBluetoothTelephonyInterface, adapter_index_,
                  GetCurrVersion(), base::DoNothing());
    client_->SetPhoneOpsEnabled(
        base::BindOnce(
            &FlossBluetoothTelephonyClientTest::SetPhoneOpsEnabledCallback,
            weak_ptr_factory_.GetWeakPtr()),
        true);
    EXPECT_EQ(callback_count_, 1);
  }

  int callback_count_ = 0;
  int adapter_index_ = 5;
  dbus::ObjectPath callback_path_;
  dbus::ObjectPath bluetooth_telephony_path_;
  scoped_refptr<::dbus::MockBus> bus_;
  scoped_refptr<::dbus::MockObjectProxy> bluetooth_telephony_object_proxy_;
  std::unique_ptr<FlossBluetoothTelephonyClient> client_;

  base::WeakPtrFactory<FlossBluetoothTelephonyClientTest> weak_ptr_factory_{
      this};
};

TEST_F(FlossBluetoothTelephonyClientTest, SetPhoneOpsEnabled) {
  TestSetPhoneOpsEnabled();
}

}  // namespace floss
