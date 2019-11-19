// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/bluetooth_advertisement_bluez.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_advertisement.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_le_advertisement_service_provider.h"
#include "device/bluetooth/dbus/fake_bluetooth_le_advertising_manager_client.h"
#include "device/bluetooth/test/test_bluetooth_advertisement_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

using device::BluetoothAdapter;
using device::BluetoothAdapterFactory;
using device::BluetoothAdvertisement;
using device::TestBluetoothAdvertisementObserver;

namespace bluez {

class BluetoothAdvertisementBlueZTest : public testing::Test {
 public:
  void SetUp() override {
    bluez::BluezDBusManager::GetSetterForTesting();

    callback_count_ = 0;
    error_callback_count_ = 0;

    last_callback_count_ = 0;
    last_error_callback_count_ = 0;

    last_error_code_ = BluetoothAdvertisement::INVALID_ADVERTISEMENT_ERROR_CODE;

    GetAdapter();
  }

  void TearDown() override {
    observer_.reset();
    // The adapter should outlive the advertisement.
    advertisement_ = nullptr;
    BluetoothAdapterFactory::Shutdown();
    adapter_ = nullptr;
    bluez::BluezDBusManager::Shutdown();
  }

  // Gets the existing Bluetooth adapter.
  void GetAdapter() {
    BluetoothAdapterFactory::GetAdapter(
        base::BindOnce(&BluetoothAdvertisementBlueZTest::GetAdapterCallback,
                       base::Unretained(this)));
    base::RunLoop().Run();
  }

  // Called whenever BluetoothAdapter is retrieved successfully.
  void GetAdapterCallback(scoped_refptr<BluetoothAdapter> adapter) {
    adapter_ = adapter;
    ASSERT_NE(adapter_.get(), nullptr);
    ASSERT_TRUE(adapter_->IsInitialized());
    if (base::RunLoop::IsRunningOnCurrentThread())
      base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }

  std::unique_ptr<BluetoothAdvertisement::Data> CreateAdvertisementData() {
    std::unique_ptr<BluetoothAdvertisement::Data> data =
        std::make_unique<BluetoothAdvertisement::Data>(
            BluetoothAdvertisement::ADVERTISEMENT_TYPE_BROADCAST);
    data->set_service_uuids(
        std::make_unique<BluetoothAdvertisement::UUIDList>());
    data->set_manufacturer_data(
        std::make_unique<BluetoothAdvertisement::ManufacturerData>());
    data->set_solicit_uuids(
        std::make_unique<BluetoothAdvertisement::UUIDList>());
    data->set_service_data(
        std::make_unique<BluetoothAdvertisement::ServiceData>());
    return data;
  }

  // Creates and registers an advertisement with the adapter.
  scoped_refptr<BluetoothAdvertisement> CreateAdvertisement() {
    // Clear the last advertisement we created.
    advertisement_ = nullptr;

    adapter_->RegisterAdvertisement(
        CreateAdvertisementData(),
        base::Bind(&BluetoothAdvertisementBlueZTest::RegisterCallback,
                   base::Unretained(this)),
        base::Bind(&BluetoothAdvertisementBlueZTest::AdvertisementErrorCallback,
                   base::Unretained(this)));

    base::RunLoop().RunUntilIdle();
    return advertisement_;
  }

  void UnregisterAdvertisement(
      scoped_refptr<BluetoothAdvertisement> advertisement) {
    advertisement->Unregister(
        base::Bind(&BluetoothAdvertisementBlueZTest::Callback,
                   base::Unretained(this)),
        base::Bind(&BluetoothAdvertisementBlueZTest::AdvertisementErrorCallback,
                   base::Unretained(this)));

    base::RunLoop().RunUntilIdle();
  }

  void TriggerReleased(scoped_refptr<BluetoothAdvertisement> advertisement) {
    BluetoothAdvertisementBlueZ* adv =
        static_cast<BluetoothAdvertisementBlueZ*>(advertisement.get());
    bluez::FakeBluetoothLEAdvertisementServiceProvider* provider =
        static_cast<bluez::FakeBluetoothLEAdvertisementServiceProvider*>(
            adv->provider());
    provider->Release();
  }

  // Called whenever RegisterAdvertisement is completed successfully.
  void RegisterCallback(scoped_refptr<BluetoothAdvertisement> advertisement) {
    ++callback_count_;
    advertisement_ = advertisement;

    ASSERT_NE(advertisement_.get(), nullptr);
  }

  void AdvertisementErrorCallback(
      BluetoothAdvertisement::ErrorCode error_code) {
    ++error_callback_count_;
    last_error_code_ = error_code;
  }

  // Generic callbacks.
  void Callback() { ++callback_count_; }

  void ErrorCallback() { ++error_callback_count_; }

  void ExpectSuccess() {
    EXPECT_EQ(last_error_callback_count_, error_callback_count_);
    EXPECT_EQ(last_callback_count_ + 1, callback_count_);
    last_callback_count_ = callback_count_;
    last_error_callback_count_ = error_callback_count_;
  }

  void ExpectError(BluetoothAdvertisement::ErrorCode error_code) {
    EXPECT_EQ(last_callback_count_, callback_count_);
    EXPECT_EQ(last_error_callback_count_ + 1, error_callback_count_);
    last_callback_count_ = callback_count_;
    last_error_callback_count_ = error_callback_count_;
    EXPECT_EQ(error_code, last_error_code_);
  }

 protected:
  int callback_count_;
  int error_callback_count_;

  int last_callback_count_;
  int last_error_callback_count_;

  BluetoothAdvertisement::ErrorCode last_error_code_;

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  std::unique_ptr<TestBluetoothAdvertisementObserver> observer_;
  scoped_refptr<BluetoothAdapter> adapter_;
  scoped_refptr<BluetoothAdvertisement> advertisement_;
};

TEST_F(BluetoothAdvertisementBlueZTest, RegisterSucceeded) {
  scoped_refptr<BluetoothAdvertisement> advertisement = CreateAdvertisement();
  ExpectSuccess();
  EXPECT_TRUE(advertisement);

  UnregisterAdvertisement(advertisement);
  ExpectSuccess();
}

TEST_F(BluetoothAdvertisementBlueZTest, DoubleRegisterSucceeded) {
  scoped_refptr<BluetoothAdvertisement> advertisement = CreateAdvertisement();
  ExpectSuccess();
  EXPECT_TRUE(advertisement);

  // Creating a second advertisement should still be fine.
  scoped_refptr<BluetoothAdvertisement> advertisement2 = CreateAdvertisement();
  ExpectSuccess();
  EXPECT_TRUE(advertisement2);
}

TEST_F(BluetoothAdvertisementBlueZTest, RegisterTooManyFailed) {
  // Register the maximum available number of advertisements.
  constexpr size_t kMaxBluezAdvertisements =
      bluez::FakeBluetoothLEAdvertisingManagerClient::kMaxBluezAdvertisements;

  std::vector<scoped_refptr<BluetoothAdvertisement>> advertisements;
  scoped_refptr<BluetoothAdvertisement> current_advertisement;
  for (size_t i = 0; i < kMaxBluezAdvertisements; i++) {
    current_advertisement = CreateAdvertisement();
    ExpectSuccess();
    EXPECT_TRUE(current_advertisement);
    advertisements.emplace_back(std::move(current_advertisement));
  }

  // The next advertisement should fail to register.
  current_advertisement = CreateAdvertisement();
  ExpectError(BluetoothAdvertisement::ERROR_ADVERTISEMENT_ALREADY_EXISTS);
  EXPECT_FALSE(current_advertisement);
}

TEST_F(BluetoothAdvertisementBlueZTest, DoubleUnregisterFailed) {
  scoped_refptr<BluetoothAdvertisement> advertisement = CreateAdvertisement();
  ExpectSuccess();
  EXPECT_TRUE(advertisement);

  UnregisterAdvertisement(advertisement);
  ExpectSuccess();

  // Unregistering an already unregistered advertisement should give us an
  // error.
  UnregisterAdvertisement(advertisement);
  ExpectError(BluetoothAdvertisement::ERROR_ADVERTISEMENT_DOES_NOT_EXIST);
}

TEST_F(BluetoothAdvertisementBlueZTest, UnregisterAfterReleasedFailed) {
  scoped_refptr<BluetoothAdvertisement> advertisement = CreateAdvertisement();
  ExpectSuccess();
  EXPECT_TRUE(advertisement);

  observer_.reset(new TestBluetoothAdvertisementObserver(advertisement));
  TriggerReleased(advertisement);
  EXPECT_TRUE(observer_->released());

  // Unregistering an advertisement that has been released should give us an
  // error.
  UnregisterAdvertisement(advertisement);
  ExpectError(BluetoothAdvertisement::ERROR_ADVERTISEMENT_DOES_NOT_EXIST);
}

TEST_F(BluetoothAdvertisementBlueZTest, UnregisterAfterAdapterShutdown) {
  scoped_refptr<BluetoothAdvertisement> advertisement = CreateAdvertisement();
  ExpectSuccess();
  EXPECT_TRUE(advertisement);

  // Shutdown the default adapter.
  BluetoothAdapterFactory::Shutdown();

  UnregisterAdvertisement(advertisement);
  ExpectError(BluetoothAdvertisement::ERROR_ADVERTISEMENT_DOES_NOT_EXIST);
}

TEST_F(BluetoothAdvertisementBlueZTest, ResetAdvertising) {
  bluez::FakeBluetoothLEAdvertisingManagerClient* adv_client =
      static_cast<bluez::FakeBluetoothLEAdvertisingManagerClient*>(
          bluez::BluezDBusManager::Get()
              ->GetBluetoothLEAdvertisingManagerClient());

  // Creates and registers multiple advertisements.
  scoped_refptr<BluetoothAdvertisement> advertisement1 = CreateAdvertisement();
  ExpectSuccess();
  EXPECT_TRUE(advertisement1);
  scoped_refptr<BluetoothAdvertisement> advertisement2 = CreateAdvertisement();
  ExpectSuccess();
  EXPECT_TRUE(advertisement2);
  // There should be 2 currently registered advertisements.
  EXPECT_EQ(2, adv_client->currently_registered());

  adapter_->ResetAdvertising(
      base::Bind(&BluetoothAdvertisementBlueZTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothAdvertisementBlueZTest::AdvertisementErrorCallback,
                 base::Unretained(this)));
  ExpectSuccess();

  // Checks that the advertisements have been cleared after ResetAdvertising.
  EXPECT_EQ(0, adv_client->currently_registered());
}

}  // namespace bluez
