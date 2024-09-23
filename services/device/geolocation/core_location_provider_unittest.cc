// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/core_location_provider.h"

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "services/device/public/cpp/geolocation/geoposition.h"
#include "services/device/public/cpp/geolocation/system_geolocation_source.h"
#include "services/device/public/cpp/test/fake_geolocation_system_permission_manager.h"
#include "services/device/public/cpp/test/fake_system_geolocation_source.h"
#include "services/device/public/mojom/geolocation_internals.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

using ::base::test::TestFuture;

class CoreLocationProviderTest : public testing::Test {
 public:
  std::unique_ptr<CoreLocationProvider> provider_;

  void SetUp() override {
    fake_geolocation_system_permission_manager_ =
        std::make_unique<FakeGeolocationSystemPermissionManager>();
    fake_system_geolocation_source_ =
        &fake_geolocation_system_permission_manager_
             ->SystemGeolocationSourceForTest();
    provider_ = std::make_unique<CoreLocationProvider>(
        *fake_system_geolocation_source_);
  }

 protected:
  CoreLocationProviderTest() = default;

  bool IsUpdating() {
    return static_cast<FakeSystemGeolocationSource*>(
               fake_system_geolocation_source_)
        ->watching_position();
  }

  // Updates the position synchronously.
  void FakeUpdatePosition(const mojom::Geoposition& result) {
    static_cast<FakeSystemGeolocationSource*>(fake_system_geolocation_source_)
        ->FakePositionUpdatedForTesting(result);
  }

  // Updates the error synchronously.
  void FakeUpdateError(const mojom::GeopositionError& error) {
    static_cast<FakeSystemGeolocationSource*>(fake_system_geolocation_source_)
        ->FakePositionErrorForTesting(error);
  }

  const mojom::GeopositionResult* GetLatestPosition() {
    return provider_->GetPosition();
  }

  mojom::GeolocationDiagnostics::ProviderState GetProviderState() {
    mojom::GeolocationDiagnostics diagnostics;
    provider_->FillDiagnostics(diagnostics);
    return diagnostics.provider_state;
  }

  base::test::TaskEnvironment task_environment_;
  const LocationProvider::LocationProviderUpdateCallback callback_;
  std::unique_ptr<FakeGeolocationSystemPermissionManager>
      fake_geolocation_system_permission_manager_;
  raw_ptr<SystemGeolocationSource> fake_system_geolocation_source_ = nullptr;
};

TEST_F(CoreLocationProviderTest, CreateDestroy) {
  EXPECT_TRUE(provider_);
  provider_.reset();
}

TEST_F(CoreLocationProviderTest, StartAndStopUpdating) {
  provider_->StartProvider(/*high_accuracy=*/true);
  EXPECT_TRUE(IsUpdating());
  EXPECT_EQ(GetProviderState(),
            mojom::GeolocationDiagnostics::ProviderState::kHighAccuracy);
  provider_->StopProvider();
  EXPECT_FALSE(IsUpdating());
  EXPECT_EQ(GetProviderState(),
            mojom::GeolocationDiagnostics::ProviderState::kStopped);
  provider_.reset();
}

TEST_F(CoreLocationProviderTest, GetPositionUpdates) {
  provider_->StartProvider(/*high_accuracy=*/true);
  EXPECT_TRUE(IsUpdating());
  EXPECT_EQ(GetProviderState(),
            mojom::GeolocationDiagnostics::ProviderState::kHighAccuracy);

  // test info
  double latitude = 147.147;
  double longitude = 101.101;
  double altitude = 417.417;
  double accuracy = 10.5;
  double altitude_accuracy = 15.5;

  auto test_position = mojom::Geoposition::New();
  test_position->latitude = latitude;
  test_position->longitude = longitude;
  test_position->altitude = altitude;
  test_position->accuracy = accuracy;
  test_position->altitude_accuracy = altitude_accuracy;
  test_position->timestamp = base::Time::Now();

  TestFuture<const LocationProvider*, mojom::GeopositionResultPtr>
      location_update_future;
  provider_->SetUpdateCallback(location_update_future.GetRepeatingCallback());
  FakeUpdatePosition(*test_position);
  auto [provider, result] = location_update_future.Take();
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_position());
  EXPECT_TRUE(test_position.Equals(result->get_position()));
  ASSERT_TRUE(GetLatestPosition());
  ASSERT_TRUE(GetLatestPosition()->is_position());
  EXPECT_TRUE(GetLatestPosition()->get_position().Equals(test_position));

  provider_->StopProvider();
  EXPECT_FALSE(IsUpdating());
  EXPECT_EQ(GetProviderState(),
            mojom::GeolocationDiagnostics::ProviderState::kStopped);
  provider_.reset();
}

TEST_F(CoreLocationProviderTest, GetPositionError) {
  provider_->StartProvider(/*high_accuracy=*/true);
  EXPECT_TRUE(IsUpdating());
  EXPECT_EQ(GetProviderState(),
            mojom::GeolocationDiagnostics::ProviderState::kHighAccuracy);

  auto error = mojom::GeopositionError::New();
  error->error_code = mojom::GeopositionErrorCode::kPermissionDenied;

  TestFuture<const LocationProvider*, mojom::GeopositionResultPtr>
      location_update_future;
  provider_->SetUpdateCallback(location_update_future.GetRepeatingCallback());
  FakeUpdateError(*error);
  auto [provider, result] = location_update_future.Take();
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_error());
  EXPECT_TRUE(error.Equals(result->get_error()));
  ASSERT_TRUE(GetLatestPosition());
  ASSERT_TRUE(GetLatestPosition()->is_error());
  EXPECT_TRUE(GetLatestPosition()->get_error().Equals(error));

  provider_->StopProvider();
  EXPECT_FALSE(IsUpdating());
  EXPECT_EQ(GetProviderState(),
            mojom::GeolocationDiagnostics::ProviderState::kStopped);
  provider_.reset();
}

}  // namespace device
