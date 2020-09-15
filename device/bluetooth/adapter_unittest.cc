// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/adapter.h"

#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "device/bluetooth/bluetooth_advertisement.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_advertisement.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using testing::NiceMock;
using testing::Return;

namespace {

const char kServiceId[] = "00000000-0000-0000-0000-000000000001";
const char kDeviceServiceDataStr[] = "ServiceData";

std::vector<uint8_t> GetByteVector(const std::string& str) {
  return std::vector<uint8_t>(str.begin(), str.end());
}

class MockBluetoothAdapterWithAdvertisements
    : public device::MockBluetoothAdapter {
 public:
  void RegisterAdvertisement(
      std::unique_ptr<device::BluetoothAdvertisement::Data> advertisement_data,
      device::BluetoothAdapter::CreateAdvertisementCallback callback,
      device::BluetoothAdapter::AdvertisementErrorCallback error_callback)
      override {
    last_register_advertisement_args_ =
        std::make_pair(*advertisement_data->service_uuids(),
                       *advertisement_data->service_data());

    if (should_advertisement_registration_succeed_) {
      std::move(callback).Run(
          base::MakeRefCounted<device::MockBluetoothAdvertisement>());
    } else {
      std::move(error_callback)
          .Run(device::BluetoothAdvertisement::ErrorCode::
                   INVALID_ADVERTISEMENT_ERROR_CODE);
    }
  }

  bool should_advertisement_registration_succeed_ = true;
  base::Optional<std::pair<device::BluetoothAdvertisement::UUIDList,
                           device::BluetoothAdvertisement::ServiceData>>
      last_register_advertisement_args_;

 protected:
  ~MockBluetoothAdapterWithAdvertisements() override = default;
};

}  // namespace

namespace bluetooth {

class AdapterTest : public testing::Test {
 public:
  AdapterTest() = default;
  ~AdapterTest() override = default;
  AdapterTest(const AdapterTest&) = delete;
  AdapterTest& operator=(const AdapterTest&) = delete;

  void SetUp() override {
    mock_bluetooth_adapter_ = base::MakeRefCounted<
        NiceMock<MockBluetoothAdapterWithAdvertisements>>();
    ON_CALL(*mock_bluetooth_adapter_, IsPresent()).WillByDefault(Return(true));
    ON_CALL(*mock_bluetooth_adapter_, IsPowered()).WillByDefault(Return(true));

    adapter_ = std::make_unique<Adapter>(mock_bluetooth_adapter_);
  }

 protected:
  void VerifyRegisterAdvertisement(bool should_succeed) {
    mock_bluetooth_adapter_->should_advertisement_registration_succeed_ =
        should_succeed;

    auto service_data = GetByteVector(kDeviceServiceDataStr);
    mojo::Remote<mojom::Advertisement> advertisement;

    base::RunLoop run_loop;
    adapter_->RegisterAdvertisement(
        device::BluetoothUUID(kServiceId), service_data,
        base::BindLambdaForTesting([&](mojo::PendingRemote<mojom::Advertisement>
                                           pending_advertisement) {
          EXPECT_EQ(should_succeed, pending_advertisement.is_valid());
          run_loop.Quit();
        }));
    run_loop.Run();

    auto& uuid_list =
        mock_bluetooth_adapter_->last_register_advertisement_args_->first;
    EXPECT_EQ(1u, uuid_list.size());
    EXPECT_EQ(kServiceId, uuid_list[0]);
    EXPECT_EQ(
        service_data,
        mock_bluetooth_adapter_->last_register_advertisement_args_->second.at(
            kServiceId));
  }

  scoped_refptr<NiceMock<MockBluetoothAdapterWithAdvertisements>>
      mock_bluetooth_adapter_;
  std::unique_ptr<Adapter> adapter_;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(AdapterTest, TestRegisterAdvertisement_Success) {
  VerifyRegisterAdvertisement(/*should_succeed=*/true);
}

TEST_F(AdapterTest, TestRegisterAdvertisement_Error) {
  VerifyRegisterAdvertisement(/*should_succeed=*/false);
}

}  // namespace bluetooth
