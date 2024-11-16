// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/floss_gatt_manager_client.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "dbus/message.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace floss {

class FlossGattClientTest : public testing::Test,
                            public FlossGattClientObserver {
 public:
  void SetUp() override {
    gatt_manager_client_ = FlossGattManagerClient::Create();
    gatt_manager_client_->AddObserver(this);
  }

  void TearDown() override { gatt_manager_client_.reset(); }

  // FlossGattClientObserver overrides
  void GattClientConnectionState(GattStatus status,
                                 int32_t client_id,
                                 bool connected,
                                 std::string address) override {
    last_connection_state_address_ = address;
  }

  void TestRegisterClient() {
    gatt_manager_client_->GattClientRegistered(GattStatus::kError, 10);
    EXPECT_EQ(gatt_manager_client_->client_id_, 0);

    gatt_manager_client_->GattClientRegistered(GattStatus::kSuccess, 10);
    EXPECT_EQ(gatt_manager_client_->client_id_, 10);

    gatt_manager_client_->GattClientRegistered(GattStatus::kSuccess, 20);
    EXPECT_EQ(gatt_manager_client_->client_id_, 10);
  }

  void TestConnectionState() {
    GattStatus success = GattStatus::kSuccess;
    last_connection_state_address_ = "";

    gatt_manager_client_->GattClientConnectionState(success, 10, false,
                                                    "12345");
    EXPECT_EQ(last_connection_state_address_, "12345");

    gatt_manager_client_->GattClientConnectionState(success, 20, false,
                                                    "23456");
    EXPECT_EQ(last_connection_state_address_, "12345");
  }

  std::unique_ptr<FlossGattManagerClient> gatt_manager_client_;
  std::string last_connection_state_address_ = "";

  base::test::TaskEnvironment task_environment_;
  base::WeakPtrFactory<FlossGattClientTest> weak_ptr_factory_{this};
};

TEST_F(FlossGattClientTest, UnexpectedClientHandling) {
  TestRegisterClient();
  TestConnectionState();
}

}  // namespace floss
