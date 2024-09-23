// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/geolocation/geolocation_system_permission_manager.h"

#include <memory>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/public/cpp/geolocation/location_system_permission_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace {

class MockObserver
    : public device::GeolocationSystemPermissionManager::PermissionObserver {
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

  // These methods are not used in the tests, but need to be implemented.
#if BUILDFLAG(IS_APPLE)
  void StartWatchingPosition(bool) override {}
  void StopWatchingPosition() override {}
  void AddPositionUpdateObserver(PositionObserver* observer) override {}
  void RemovePositionUpdateObserver(PositionObserver* observer) override {}
#endif  // BUILDFLAG(IS_APPLE)

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN)
  void RequestPermission() override {}
#endif  // BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN)

  // Helper function to force observer notification (normally done by
  // implementations of PermissionProvider).
  void Update(device::LocationSystemPermissionStatus status) {
    callback_.Run(status);
  }

 private:
  PermissionUpdateCallback callback_ = base::DoNothing();
};

}  // namespace

class GeolocationSystemPermissionTests : public testing::Test {
 public:
  GeolocationSystemPermissionTests() {
#if BUILDFLAG(IS_WIN)
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kWinSystemLocationPermission},
        /*disabled_features=*/{});
#endif  // BUILDFLAG(IS_WIN)
  }

  GeolocationSystemPermissionTests(const GeolocationSystemPermissionTests&) =
      delete;
  GeolocationSystemPermissionTests& operator=(
      const GeolocationSystemPermissionTests&) = delete;

  ~GeolocationSystemPermissionTests() override {
    task_environment_.RunUntilIdle();
  }

  MockObserver& CreateMockObserver() {
    observers_.push_back(
        std::make_unique<::testing::StrictMock<MockObserver>>());
    return *observers_.back();
  }

 protected:
  std::vector<std::unique_ptr<MockObserver>> observers_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(GeolocationSystemPermissionTests, TestAddObserver) {
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
  device::GeolocationSystemPermissionManager manager(std::move(source_pointer));

  // Test adding observers
  manager.AddObserver(&observer1);
  source.Update(device::LocationSystemPermissionStatus::kAllowed);
  manager.AddObserver(&observer2);
  source.Update(device::LocationSystemPermissionStatus::kAllowed);
}

TEST_F(GeolocationSystemPermissionTests, TestRemoveObserver) {
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
  device::GeolocationSystemPermissionManager manager(std::move(source_pointer));

  manager.AddObserver(&observer1);
  manager.AddObserver(&observer2);

  // Test removing observers
  manager.RemoveObserver(&observer1);
  source.Update(device::LocationSystemPermissionStatus::kDenied);
}
