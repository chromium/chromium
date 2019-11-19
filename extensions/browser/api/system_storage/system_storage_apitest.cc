// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <vector>

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/storage_monitor/storage_monitor.h"
#include "components/storage_monitor/test_storage_monitor.h"
#include "extensions/browser/api/system_storage/storage_api_test_util.h"
#include "extensions/browser/api/system_storage/storage_info_provider.h"
#include "extensions/shell/test/shell_apitest.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"

namespace {

using extensions::StorageUnitInfoList;
using extensions::test::TestStorageUnitInfo;
using extensions::test::kRemovableStorageData;
using storage_monitor::StorageMonitor;
using storage_monitor::TestStorageMonitor;

const struct TestStorageUnitInfo kTestingData[] = {
    {"dcim:device:001", "0xbeaf", 4098, 1},
    {"path:device:002", "/home", 4098, 2},
    {"path:device:003", "/data", 10000, 3}};

}  // namespace

class TestStorageInfoProvider : public extensions::StorageInfoProvider {
 public:
  TestStorageInfoProvider(const struct TestStorageUnitInfo* testing_data,
                          size_t n);

 private:
  ~TestStorageInfoProvider() override;

  // StorageInfoProvider implementations.
  double GetStorageFreeSpaceFromTransientIdAsync(
      const std::string& transient_id) override;

  std::vector<struct TestStorageUnitInfo> testing_data_;
};

TestStorageInfoProvider::TestStorageInfoProvider(
    const struct TestStorageUnitInfo* testing_data,
    size_t n)
    : testing_data_(testing_data, testing_data + n) {
}

TestStorageInfoProvider::~TestStorageInfoProvider() {
}

double TestStorageInfoProvider::GetStorageFreeSpaceFromTransientIdAsync(
    const std::string& transient_id) {
  std::string device_id =
      StorageMonitor::GetInstance()->GetDeviceIdForTransientId(transient_id);
  for (size_t i = 0; i < testing_data_.size(); ++i) {
    if (testing_data_[i].device_id == device_id) {
      return static_cast<double>(testing_data_[i].available_capacity);
    }
  }
  return -1;
}

class SystemStorageApiTest : public extensions::ShellApiTest {
 public:
  SystemStorageApiTest() {}
  ~SystemStorageApiTest() override {}

  void SetUpOnMainThread() override {
    ShellApiTest::SetUpOnMainThread();
    TestStorageMonitor::CreateForBrowserTests();
  }

  void SetUpAllMockStorageDevices() {
    for (size_t i = 0; i < base::size(kTestingData); ++i) {
      AttachRemovableStorage(kTestingData[i]);
    }
  }

  void AttachRemovableStorage(
      const struct TestStorageUnitInfo& removable_storage_info) {
    StorageMonitor::GetInstance()->receiver()->ProcessAttach(
        extensions::test::BuildStorageInfoFromTestStorageUnitInfo(
            removable_storage_info));
  }

  void DetachRemovableStorage(const std::string& id) {
    StorageMonitor::GetInstance()->receiver()->ProcessDetach(id);
  }
};

IN_PROC_BROWSER_TEST_F(SystemStorageApiTest, Storage) {
  SetUpAllMockStorageDevices();
  TestStorageInfoProvider* provider =
      new TestStorageInfoProvider(kTestingData, base::size(kTestingData));
  extensions::StorageInfoProvider::InitializeForTesting(provider);
  std::vector<std::unique_ptr<ExtensionTestMessageListener>>
      device_ids_listeners;
  for (size_t i = 0; i < base::size(kTestingData); ++i) {
    device_ids_listeners.push_back(
        std::make_unique<ExtensionTestMessageListener>(
            StorageMonitor::GetInstance()->GetTransientIdForDeviceId(
                kTestingData[i].device_id),
            false));
  }
  ASSERT_TRUE(RunAppTest("system/storage")) << message_;
  for (size_t i = 0; i < device_ids_listeners.size(); ++i)
    EXPECT_TRUE(device_ids_listeners[i]->WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(SystemStorageApiTest, StorageAttachment) {
  extensions::ResultCatcher catcher;
  ExtensionTestMessageListener attach_listener("attach", false);
  ExtensionTestMessageListener detach_listener("detach", false);

  EXPECT_TRUE(LoadApp("system/storage_attachment"));
  // Simulate triggering onAttached event.
  ASSERT_TRUE(attach_listener.WaitUntilSatisfied());

  AttachRemovableStorage(kRemovableStorageData);

  std::string removable_storage_transient_id =
      StorageMonitor::GetInstance()->GetTransientIdForDeviceId(
          kRemovableStorageData.device_id);
  ExtensionTestMessageListener detach_device_id_listener(
      removable_storage_transient_id, false);

  // Simulate triggering onDetached event.
  ASSERT_TRUE(detach_listener.WaitUntilSatisfied());
  DetachRemovableStorage(kRemovableStorageData.device_id);

  ASSERT_TRUE(detach_device_id_listener.WaitUntilSatisfied());

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}
