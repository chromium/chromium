// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/test/task_environment.h"
#include "services/device/public/cpp/geolocation/geolocation_manager.h"
#include "services/device/public/cpp/geolocation/location_system_permission_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace {

class MockObserver : public device::GeolocationManager::PermissionObserver {
 public:
  MOCK_METHOD(void,
              OnSystemPermissionUpdated,
              (device::LocationSystemPermissionStatus),
              (override));
};

class SourceImpl : public device::SystemGeolocationSource {
 public:
  // device::SystemGeolocationSource
  void RegisterPermissionUpdateCallback(
      PermissionUpdateCallback callback) override {
    callback_ = callback;
  }

#if BUILDFLAG(IS_MAC)
  // This methods are not used in the tests, but need to be implemented on Mac.
  void RegisterPositionUpdateCallback(PositionUpdateCallback) override {}
  void StartWatchingPosition(bool) override {}
  void StopWatchingPosition() override {}
  void RequestPermission() override {}
#endif

  // Helper function to force observer notification (normally done by
  // implementations of PermissionProvider).
  void Update(device::LocationSystemPermissionStatus status) {
    callback_.Run(status);
  }

 private:
  PermissionUpdateCallback callback_ = base::DoNothing();
};

}  // namespace

class GeolocationPermissionTests : public testing::Test {
 public:
  ~GeolocationPermissionTests() override { task_environment_.RunUntilIdle(); }

  MockObserver& CreateMockObserver() {
    observers_.push_back(
        std::make_unique<::testing::StrictMock<MockObserver>>());
    return *observers_.back();
  }

 protected:
  std::vector<std::unique_ptr<MockObserver>> observers_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(GeolocationPermissionTests, TestAddObserver) {
  auto& observer1 = CreateMockObserver();
  auto& observer2 = CreateMockObserver();

  EXPECT_CALL(observer1, OnSystemPermissionUpdated(
                             device::LocationSystemPermissionStatus::kAllowed))
      .Times(2);
  EXPECT_CALL(observer2, OnSystemPermissionUpdated(
                             device::LocationSystemPermissionStatus::kAllowed))
      .Times(1);

  auto source_pointer = std::make_unique<SourceImpl>();
  SourceImpl& source = *source_pointer;
  device::GeolocationManager manager(std::move(source_pointer));

  // Test adding observers
  manager.AddObserver(&observer1);
  source.Update(device::LocationSystemPermissionStatus::kAllowed);
  manager.AddObserver(&observer2);
  source.Update(device::LocationSystemPermissionStatus::kAllowed);
}

TEST_F(GeolocationPermissionTests, TestRemoveObserver) {
  auto& observer1 = CreateMockObserver();
  auto& observer2 = CreateMockObserver();

  EXPECT_CALL(observer1, OnSystemPermissionUpdated(
                             device::LocationSystemPermissionStatus::kDenied))
      .Times(0);
  EXPECT_CALL(observer2, OnSystemPermissionUpdated(
                             device::LocationSystemPermissionStatus::kDenied))
      .Times(1);

  auto source_pointer = std::make_unique<SourceImpl>();
  SourceImpl& source = *source_pointer;
  device::GeolocationManager manager(std::move(source_pointer));

  manager.AddObserver(&observer1);
  manager.AddObserver(&observer2);

  // Test removing observers
  manager.RemoveObserver(&observer1);
  source.Update(device::LocationSystemPermissionStatus::kDenied);
}
