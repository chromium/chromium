// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier_linux.h"

#include <unordered_set>

#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "net/base/address_map_linux.h"
#include "net/base/address_tracker_linux.h"
#include "net/dns/dns_config_service.h"
#include "net/dns/system_dns_config_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

class NetworkChangeNotifierLinuxTest : public testing::Test {
 public:
  NetworkChangeNotifierLinuxTest() = default;
  NetworkChangeNotifierLinuxTest(const NetworkChangeNotifierLinuxTest&) =
      delete;
  NetworkChangeNotifierLinuxTest& operator=(
      const NetworkChangeNotifierLinuxTest&) = delete;
  ~NetworkChangeNotifierLinuxTest() override = default;

  void CreateNotifier() {
    // Use a noop DNS notifier.
    dns_config_notifier_ = std::make_unique<SystemDnsConfigChangeNotifier>(
        nullptr /* task_runner */, nullptr /* dns_config_service */);
    notifier_ = std::make_unique<NetworkChangeNotifierLinux>(
        std::unordered_set<std::string>());
  }

  void TearDown() override { base::RunLoop().RunUntilIdle(); }

 protected:
  base::test::TaskEnvironment task_environment_;

  // Allows us to allocate our own NetworkChangeNotifier for unit testing.
  NetworkChangeNotifier::DisableForTest disable_for_test_;
  std::unique_ptr<SystemDnsConfigChangeNotifier> dns_config_notifier_;
  std::unique_ptr<NetworkChangeNotifierLinux> notifier_;
};

// https://crbug.com/1441671
TEST_F(NetworkChangeNotifierLinuxTest, AddressTrackerLinuxSetDiffCallback) {
  CreateNotifier();
  AddressMapOwnerLinux* address_map_owner = notifier_->GetAddressMapOwner();
  ASSERT_TRUE(address_map_owner);
  internal::AddressTrackerLinux* address_tracker_linux =
      address_map_owner->GetAddressTrackerLinux();
  ASSERT_TRUE(address_tracker_linux);
  address_tracker_linux->GetInitialDataAndStartRecordingDiffs();
  address_tracker_linux->SetDiffCallback(base::DoNothing());
}

}  // namespace net
