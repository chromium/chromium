// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_low_energy_advertisement_manager_mac.h"

#import <CoreBluetooth/CoreBluetooth.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/test/bind.h"
#include "base/test/test_simple_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/ocmock/OCMock/OCMock.h"

namespace {
const char kTestUUID[] = "00000000-1111-2222-3333-444444444444";
}  // namespace

namespace device {

class BluetoothLowEnergyAdvertisementManagerMacTest : public testing::Test {
 public:
  BluetoothLowEnergyAdvertisementManagerMacTest()
      : ui_task_runner_(new base::TestSimpleTaskRunner()),
        peripheral_manager_state_(CBManagerStatePoweredOn),
        unregister_success_(false) {}

  void SetUp() override {
    peripheral_manager_ = OCMClassMock([CBPeripheralManager class]);
    peripheral_manager_mock_ = peripheral_manager_;
    OCMStub([peripheral_manager_ state]).andDo(^(NSInvocation* invocation) {
      [invocation setReturnValue:&peripheral_manager_state_];
    });

    advertisement_manager_.Init(ui_task_runner_, peripheral_manager_);
  }

  void OnAdvertisementRegistered(
      scoped_refptr<BluetoothAdvertisement> advertisement) {
    ASSERT_FALSE(advertisement_.get());
    advertisement_ = advertisement;
  }

  void OnAdvertisementRegisterError(
      BluetoothAdvertisement::ErrorCode error_code) {
    ASSERT_FALSE(registration_error_.get());
    registration_error_ =
        std::make_unique<BluetoothAdvertisement::ErrorCode>(error_code);
  }

  void OnUnregisterSuccess() {
    ASSERT_FALSE(unregister_success_);
    unregister_success_ = true;
  }

  void OnUnregisterError(BluetoothAdvertisement::ErrorCode error_code) {
    ASSERT_FALSE(unregister_error_);
    unregister_error_ =
        std::make_unique<BluetoothAdvertisement::ErrorCode>(error_code);
  }

  std::unique_ptr<BluetoothAdvertisement::Data> CreateAdvertisementData() {
    std::unique_ptr<BluetoothAdvertisement::Data> advertisement_data =
        std::make_unique<BluetoothAdvertisement::Data>(
            BluetoothAdvertisement::ADVERTISEMENT_TYPE_BROADCAST);

    BluetoothAdvertisement::UUIDList uuid_list;
    uuid_list.push_back(kTestUUID);
    advertisement_data->set_service_uuids(std::move(uuid_list));

    return advertisement_data;
  }

  void RegisterAdvertisement(
      std::unique_ptr<BluetoothAdvertisement::Data> advertisement_data) {
    advertisement_manager_.RegisterAdvertisement(
        std::move(advertisement_data),
        base::BindOnce(&BluetoothLowEnergyAdvertisementManagerMacTest::
                           OnAdvertisementRegistered,
                       base::Unretained(this)),
        base::BindOnce(&BluetoothLowEnergyAdvertisementManagerMacTest::
                           OnAdvertisementRegisterError,
                       base::Unretained(this)));
  }

  void Unregister(scoped_refptr<BluetoothAdvertisement> advertisement) {
    advertisement->Unregister(
        base::BindOnce(
            &BluetoothLowEnergyAdvertisementManagerMacTest::OnUnregisterSuccess,
            base::Unretained(this)),
        base::BindOnce(
            &BluetoothLowEnergyAdvertisementManagerMacTest::OnUnregisterError,
            base::Unretained(this)));
  }

 protected:
  scoped_refptr<base::TestSimpleTaskRunner> ui_task_runner_;
  BluetoothLowEnergyAdvertisementManagerMac advertisement_manager_;
  CBPeripheralManager* __strong peripheral_manager_;
  id __strong peripheral_manager_mock_;
  CBManagerState peripheral_manager_state_;

  scoped_refptr<BluetoothAdvertisement> advertisement_;
  std::unique_ptr<BluetoothAdvertisement::ErrorCode> registration_error_;

  bool unregister_success_;
  std::unique_ptr<BluetoothAdvertisement::ErrorCode> unregister_error_;
};

TEST_F(BluetoothLowEnergyAdvertisementManagerMacTest,
       Register_AdapterPoweredOff) {
  // Simulate adapter being powered off.
  peripheral_manager_state_ = CBManagerStatePoweredOff;

  RegisterAdvertisement(CreateAdvertisementData());
  ui_task_runner_->RunPendingTasks();

  EXPECT_FALSE(advertisement_);
  ASSERT_TRUE(registration_error_);
  EXPECT_EQ(BluetoothAdvertisement::ERROR_ADAPTER_POWERED_OFF,
            *registration_error_);
}

TEST_F(BluetoothLowEnergyAdvertisementManagerMacTest,
       Register_AdapterInitializing_ThenUnsupported) {
  // Simulate adapter state being unknown and is initializing.
  peripheral_manager_state_ = CBManagerStateUnknown;

  // The advertisement will not start or fail until the adapter state is
  // initialized.
  RegisterAdvertisement(CreateAdvertisementData());
  ui_task_runner_->RunPendingTasks();
  EXPECT_FALSE(advertisement_);
  EXPECT_FALSE(registration_error_);

  // Change the adapter state to CBManagerStateUnsupported, which causes the
  // registration to fail.
  peripheral_manager_state_ = CBManagerStateUnsupported;
  advertisement_manager_.OnPeripheralManagerStateChanged();
  ui_task_runner_->RunPendingTasks();
  EXPECT_FALSE(advertisement_);
  ASSERT_TRUE(registration_error_);
  EXPECT_EQ(BluetoothAdvertisement::ERROR_UNSUPPORTED_PLATFORM,
            *registration_error_);
}

TEST_F(BluetoothLowEnergyAdvertisementManagerMacTest,
       Register_AdapterInitializing_ThenPoweredOff) {
  // Simulate adapter state being unknown and is initializing.
  peripheral_manager_state_ = CBManagerStateUnknown;

  // The advertisement will not start or fail until the adapter state is
  // initialized.
  RegisterAdvertisement(CreateAdvertisementData());
  ui_task_runner_->RunPendingTasks();
  EXPECT_FALSE(advertisement_);
  EXPECT_FALSE(registration_error_);

  // Change the adapter state to CBManagerStatePoweredOff, which causes the
  // registration to fail.
  peripheral_manager_state_ = CBManagerStatePoweredOff;
  advertisement_manager_.OnPeripheralManagerStateChanged();
  ui_task_runner_->RunPendingTasks();
  EXPECT_FALSE(advertisement_);
  ASSERT_TRUE(registration_error_);
  EXPECT_EQ(BluetoothAdvertisement::ERROR_ADAPTER_POWERED_OFF,
            *registration_error_);
}

TEST_F(BluetoothLowEnergyAdvertisementManagerMacTest,
       Register_ExistingAdvertisement) {
  // Start an advertisement.
  RegisterAdvertisement(CreateAdvertisementData());
  advertisement_manager_.DidStartAdvertising(nil);
  ui_task_runner_->RunPendingTasks();
  ASSERT_TRUE(advertisement_);

  // Only one advertisement is supported.
  RegisterAdvertisement(CreateAdvertisementData());
  ui_task_runner_->RunPendingTasks();
  ASSERT_TRUE(registration_error_);
  EXPECT_EQ(BluetoothAdvertisement::ERROR_ADVERTISEMENT_ALREADY_EXISTS,
            *registration_error_);
}

TEST_F(BluetoothLowEnergyAdvertisementManagerMacTest, Register_InvalidData) {
  std::unique_ptr<BluetoothAdvertisement::Data> advertisement_data =
      CreateAdvertisementData();

  // Advertising service data is not supported.
  BluetoothAdvertisement::ServiceData service_data;
  service_data[kTestUUID] = std::vector<uint8_t>(10);
  advertisement_data->set_service_data(std::move(service_data));

  RegisterAdvertisement(std::move(advertisement_data));
  ui_task_runner_->RunPendingTasks();

  EXPECT_FALSE(advertisement_);
  ASSERT_TRUE(registration_error_);
  EXPECT_EQ(BluetoothAdvertisement::ERROR_UNSUPPORTED_PLATFORM,
            *registration_error_);
}

TEST_F(BluetoothLowEnergyAdvertisementManagerMacTest, Register_Success) {
  OCMExpect([peripheral_manager_ startAdvertising:[OCMArg any]]);

  RegisterAdvertisement(CreateAdvertisementData());
  advertisement_manager_.DidStartAdvertising(nil);
  ui_task_runner_->RunPendingTasks();

  EXPECT_TRUE(advertisement_);
  EXPECT_FALSE(registration_error_);

  [peripheral_manager_mock_ verifyAtLocation:nil];
}

TEST_F(BluetoothLowEnergyAdvertisementManagerMacTest, Unregister_Success) {
  RegisterAdvertisement(CreateAdvertisementData());
  advertisement_manager_.DidStartAdvertising(nil);
  ui_task_runner_->RunPendingTasks();
  ASSERT_TRUE(advertisement_);

  OCMExpect([peripheral_manager_ stopAdvertising]);
  Unregister(advertisement_);
  ui_task_runner_->RunPendingTasks();

  EXPECT_TRUE(unregister_success_);
  EXPECT_FALSE(unregister_error_);
  [peripheral_manager_mock_ verifyAtLocation:nil];
}

TEST_F(BluetoothLowEnergyAdvertisementManagerMacTest,
       Unregister_InvalidAdvertisement) {
  scoped_refptr<BluetoothAdvertisementMac> invalid_advertisement =
      new BluetoothAdvertisementMac(std::nullopt, base::DoNothing(),
                                    base::DoNothing(), &advertisement_manager_);

  // Register advertisement.
  RegisterAdvertisement(CreateAdvertisementData());
  advertisement_manager_.DidStartAdvertising(nil);
  ui_task_runner_->RunPendingTasks();
  ASSERT_TRUE(advertisement_);

  // Try to unregister the invalid advertisement.
  Unregister(invalid_advertisement);
  ui_task_runner_->RunPendingTasks();
  EXPECT_FALSE(unregister_success_);
  ASSERT_TRUE(unregister_error_);
}

TEST_F(BluetoothLowEnergyAdvertisementManagerMacTest, Register_Twice) {
  // Register one advertisement.
  RegisterAdvertisement(CreateAdvertisementData());
  advertisement_manager_.DidStartAdvertising(nil);
  ui_task_runner_->RunPendingTasks();
  EXPECT_TRUE(advertisement_);

  // Unregister the advertisement.
  Unregister(advertisement_);
  advertisement_ = nullptr;

  // Register another advertisement.
  RegisterAdvertisement(CreateAdvertisementData());
  advertisement_manager_.DidStartAdvertising(nil);
  ui_task_runner_->RunPendingTasks();
  EXPECT_TRUE(advertisement_);
}

TEST_F(BluetoothLowEnergyAdvertisementManagerMacTest,
       AdapterReset_RestartAdvertising) {
  // Register advertisement.
  RegisterAdvertisement(CreateAdvertisementData());
  advertisement_manager_.DidStartAdvertising(nil);
  ui_task_runner_->RunPendingTasks();
  EXPECT_TRUE(advertisement_);

  // Reset the adapter (i.e. on system crash).
  peripheral_manager_state_ = CBManagerStateResetting;
  advertisement_manager_.OnPeripheralManagerStateChanged();
  advertisement_ = nullptr;
  peripheral_manager_state_ = CBManagerStatePoweredOn;

  // Registering another advertisement should succeed.
  OCMExpect([peripheral_manager_ startAdvertising:[OCMArg any]]);
  RegisterAdvertisement(CreateAdvertisementData());
  advertisement_manager_.DidStartAdvertising(nil);
  ui_task_runner_->RunPendingTasks();
  EXPECT_TRUE(advertisement_);
  [peripheral_manager_mock_ verifyAtLocation:nil];
}

TEST_F(BluetoothLowEnergyAdvertisementManagerMacTest,
       Advertising_ThenAdapterPoweredOff_ThenReregister) {
  // Register advertisement.
  RegisterAdvertisement(CreateAdvertisementData());
  advertisement_manager_.DidStartAdvertising(nil);
  ui_task_runner_->RunPendingTasks();
  EXPECT_TRUE(advertisement_);

  // Power off the adapter. Advertisement should be stopped.
  OCMExpect([peripheral_manager_ stopAdvertising]);
  peripheral_manager_state_ = CBManagerStatePoweredOff;
  advertisement_manager_.OnPeripheralManagerStateChanged();
  [peripheral_manager_mock_ verifyAtLocation:nil];

  // Register a new advertisement after powering back on the adapter.
  // This should fail as the caller needs to manually unregister the
  // advertisement.
  peripheral_manager_state_ = CBManagerStatePoweredOn;
  advertisement_ = nullptr;
  RegisterAdvertisement(CreateAdvertisementData());
  ui_task_runner_->RunPendingTasks();
  EXPECT_FALSE(advertisement_);
  ASSERT_TRUE(registration_error_);
  EXPECT_EQ(BluetoothAdvertisement::ERROR_ADVERTISEMENT_ALREADY_EXISTS,
            *registration_error_);
}

}  // namespace device
