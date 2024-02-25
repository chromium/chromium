// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/floss_dbus_manager.h"

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "device/bluetooth/floss/floss_dbus_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace floss {
namespace {
// This has to be different from FlossDBusManager::Get()->active_adapter_;
const int kMockAdapterIndex = 1;
}  // namespace

using testing::DoAll;

class FlossDBusManagerTest : public testing::Test {
 public:
  FlossDBusManagerTest() = default;

  void SetUp() override { FlossDBusManager::InitializeFake(); }

  void TearDown() override { FlossDBusManager::Shutdown(); }

  void SwitchAdapterOnTest() {
    base::test::TestFuture<void> ready;
    FlossDBusManager::Get()->SwitchAdapter(kMockAdapterIndex,
                                           ready.GetCallback());
    // Callback is not called since not all clients interface are present
    EXPECT_FALSE(ready.IsReady());
    PresentAllClients();
    // Callback is called since all clients interface are present
    EXPECT_TRUE(ready.Wait());
  }

  void SwitchAdapterOnTimeoutTest() {
    base::test::TestFuture<void> ready;
    FlossDBusManager::Get()->SwitchAdapter(kMockAdapterIndex,
                                           ready.GetCallback());
    // Callback is not called since not all clients interface are present
    EXPECT_FALSE(ready.IsReady());
    TriggerClientInitializerTimeout();
    // Callback is called because of timeout
    EXPECT_TRUE(ready.Wait());
  }

  void SwitchAdapterOnPartiallyPresentTimeoutTest() {
    base::test::TestFuture<void> ready;
    FlossDBusManager::Get()->SwitchAdapter(kMockAdapterIndex,
                                           ready.GetCallback());
    // Callback is not called since not all clients interface are present
    EXPECT_FALSE(ready.IsReady());
    PresentAllClientsExceptForGatt();
    // Callback is not called since not all clients interface are present
    EXPECT_FALSE(ready.IsReady());
    TriggerClientInitializerTimeout();
    // Callback is called because of timeout
    EXPECT_TRUE(ready.Wait());

    // If the client API is present later, we shouldn't crash.
    PresentGattClient();
    base::RunLoop().RunUntilIdle();
  }

  void SwitchAdapterOffTest() {
    base::test::TestFuture<void> ready;
    FlossDBusManager::Get()->SwitchAdapter(FlossDBusManager::kInvalidAdapter,
                                           ready.GetCallback());

    // Callback should be called immediately.
    EXPECT_TRUE(ready.Wait());
    UnpresentAllClients();
  }

 protected:
  void PresentAllClients() {
    FlossDBusManager::Get()->ObjectAdded(
        FlossDBusClient::GenerateAdapterPath(kMockAdapterIndex),
        kAdapterInterface);
    FlossDBusManager::Get()->ObjectAdded(
        FlossDBusClient::GenerateLoggingPath(kMockAdapterIndex),
        kAdapterLoggingInterface);
#if BUILDFLAG(IS_CHROMEOS)
    FlossDBusManager::Get()->ObjectAdded(
        FlossDBusClient::GenerateAdminPath(kMockAdapterIndex), kAdminInterface);
#endif
    FlossDBusManager::Get()->ObjectAdded(
        FlossDBusClient::GenerateBatteryManagerPath(kMockAdapterIndex),
        kBatteryManagerInterface);
    FlossDBusManager::Get()->ObjectAdded(
        FlossDBusClient::GenerateBluetoothTelephonyPath(kMockAdapterIndex),
        kBluetoothTelephonyInterface);
    FlossDBusManager::Get()->ObjectAdded(
        FlossDBusClient::GenerateGattPath(kMockAdapterIndex), kGattInterface);
    FlossDBusManager::Get()->ObjectAdded(
        FlossDBusClient::GenerateAdapterPath(kMockAdapterIndex),
        kSocketManagerInterface);
  }

  void PresentAllClientsExceptForGatt() {
    FlossDBusManager::Get()->ObjectAdded(
        FlossDBusClient::GenerateAdapterPath(kMockAdapterIndex),
        kAdapterInterface);
    FlossDBusManager::Get()->ObjectAdded(
        FlossDBusClient::GenerateLoggingPath(kMockAdapterIndex),
        kAdapterLoggingInterface);
#if BUILDFLAG(IS_CHROMEOS)
    FlossDBusManager::Get()->ObjectAdded(
        FlossDBusClient::GenerateAdminPath(kMockAdapterIndex), kAdminInterface);
#endif
    FlossDBusManager::Get()->ObjectAdded(
        FlossDBusClient::GenerateBatteryManagerPath(kMockAdapterIndex),
        kBatteryManagerInterface);
    FlossDBusManager::Get()->ObjectAdded(
        FlossDBusClient::GenerateBluetoothTelephonyPath(kMockAdapterIndex),
        kBluetoothTelephonyInterface);
    FlossDBusManager::Get()->ObjectAdded(
        FlossDBusClient::GenerateAdapterPath(kMockAdapterIndex),
        kSocketManagerInterface);
  }

  void PresentGattClient() {
    FlossDBusManager::Get()->ObjectAdded(
        FlossDBusClient::GenerateGattPath(kMockAdapterIndex), kGattInterface);
  }

  void UnpresentAllClients() {
    FlossDBusManager::Get()->ObjectRemoved(
        FlossDBusClient::GenerateAdapterPath(kMockAdapterIndex),
        kAdapterInterface);
    FlossDBusManager::Get()->ObjectRemoved(
        FlossDBusClient::GenerateLoggingPath(kMockAdapterIndex),
        kAdapterLoggingInterface);
#if BUILDFLAG(IS_CHROMEOS)
    FlossDBusManager::Get()->ObjectRemoved(
        FlossDBusClient::GenerateAdminPath(kMockAdapterIndex), kAdminInterface);
#endif
    FlossDBusManager::Get()->ObjectRemoved(
        FlossDBusClient::GenerateBatteryManagerPath(kMockAdapterIndex),
        kBatteryManagerInterface);
    FlossDBusManager::Get()->ObjectRemoved(
        FlossDBusClient::GenerateBluetoothTelephonyPath(kMockAdapterIndex),
        kBluetoothTelephonyInterface);
    FlossDBusManager::Get()->ObjectRemoved(
        FlossDBusClient::GenerateGattPath(kMockAdapterIndex), kGattInterface);
    FlossDBusManager::Get()->ObjectRemoved(
        FlossDBusClient::GenerateAdapterPath(kMockAdapterIndex),
        kSocketManagerInterface);
  }

  void TriggerClientInitializerTimeout() {
    task_environment_.FastForwardBy(
        base::Milliseconds(FlossDBusManager::kClientReadyTimeoutMs));
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// Make sure callback is called after all clients API present
TEST_F(FlossDBusManagerTest, AllClientsReadyBeforeTimeout) {
  SwitchAdapterOnTest();
}

// Make sure callback is not called until timeout
TEST_F(FlossDBusManagerTest, NotAllClientsReadyBeforeTimeout) {
  SwitchAdapterOnPartiallyPresentTimeoutTest();
}

// Make sure callback is called immediately after switch adapter off.
TEST_F(FlossDBusManagerTest, AdapterOffCallImmediately) {
  SwitchAdapterOnTest();
  SwitchAdapterOffTest();
}

// Make sure callback is called correctly after an adapter power cycle.
TEST_F(FlossDBusManagerTest, RestartAdapterClientPresent) {
  SwitchAdapterOnTest();
  SwitchAdapterOffTest();
  SwitchAdapterOnTest();
}

// Make sure callback is called correctly after an adapter power cycle.
TEST_F(FlossDBusManagerTest, RestartAdapterTriggerTimeout) {
  SwitchAdapterOnTest();
  SwitchAdapterOffTest();
  SwitchAdapterOnTimeoutTest();
}

}  // namespace floss
