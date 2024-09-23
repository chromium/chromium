// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/geolocation_impl.h"

#include <memory>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/network_change_notifier.h"
#include "services/device/geolocation/geolocation_context.h"
#include "services/device/geolocation/geolocation_provider.h"
#include "services/device/public/mojom/geolocation_client_id.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

using ::base::test::TestFuture;

// Fake implementation of GeolocationProvider that can simulate location
// updates.
class FakeGeolocationProvider : public GeolocationProvider {
 public:
  FakeGeolocationProvider() = default;
  FakeGeolocationProvider(const FakeGeolocationProvider&) = delete;
  FakeGeolocationProvider& operator=(const FakeGeolocationProvider&) = delete;
  ~FakeGeolocationProvider() override = default;

  base::CallbackListSubscription AddLocationUpdateCallback(
      const LocationUpdateCallback& callback,
      bool enable_high_accuracy) override {
    location_update_callback_ = callback;
    return {};
  }

  bool HighAccuracyLocationInUse() override { return false; }

  void OverrideLocationForTesting(mojom::GeopositionResultPtr result) override {
  }

  void SimulateLocationUpdate(const mojom::GeopositionResult& result) {
    if (location_update_callback_) {
      location_update_callback_.Run(result);
    }
  }

 private:
  LocationUpdateCallback location_update_callback_;
};

}  // namespace

class GeolocationImplTest : public testing::Test {
 public:
  GeolocationImplTest()
      : network_change_notifier_(
            net::NetworkChangeNotifier::CreateMockIfNeeded()) {}
  GeolocationImplTest(const GeolocationImplTest&) = delete;
  GeolocationImplTest& operator=(const GeolocationImplTest&) = delete;
  ~GeolocationImplTest() override = default;

  void SetUp() override {
    GeolocationProvider::SetInstanceForTesting(&geolocation_provider_);
    geolocation_context_.BindGeolocation(
        geolocation_.BindNewPipeAndPassReceiver(), GURL(),
        mojom::GeolocationClientId::kForTesting);
  }

  void TearDown() override {
    GeolocationProvider::SetInstanceForTesting(nullptr);
  }

  void SimulateLocationUpdate(const mojom::GeopositionResult& result) {
    geolocation_provider_.SimulateLocationUpdate(result);
  }

  void SetOverride(const mojom::GeopositionResult& result) {
    geolocation_context_.SetOverride(result.Clone());
  }

  void ClearOverride() { geolocation_context_.ClearOverride(); }

  mojom::GeopositionResultPtr MakeGeoposition(double latitude,
                                              double longitude) {
    auto position = mojom::Geoposition::New();
    position->latitude = latitude;
    position->longitude = longitude;
    position->accuracy = 100;
    position->timestamp = base::Time::Now();
    return mojom::GeopositionResult::NewPosition(std::move(position));
  }

  mojom::GeopositionResultPtr MakeGeopositionError(
      mojom::GeopositionErrorCode error_code) {
    return mojom::GeopositionResult::NewError(mojom::GeopositionError::New(
        error_code, /*error_message=*/"", /*error_technical=*/""));
  }

  const mojo::Remote<mojom::Geolocation>& geolocation() { return geolocation_; }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  FakeGeolocationProvider geolocation_provider_;
  GeolocationContext geolocation_context_;
  mojo::Remote<mojom::Geolocation> geolocation_;
};

TEST_F(GeolocationImplTest, QueryNextPosition) {
  // Simulate a location update. The estimate returned from QueryNextPosition
  // should match the update.
  TestFuture<mojom::GeopositionResultPtr> future;
  geolocation()->QueryNextPosition(future.GetCallback());
  base::RunLoop().RunUntilIdle();
  auto position = MakeGeoposition(37, -122);
  SimulateLocationUpdate(*position);
  EXPECT_EQ(future.Get(), position);
}

TEST_F(GeolocationImplTest, QueryNextPositionError) {
  // Simulate a location update, but the update is an error. The callback is
  // called with the error.
  TestFuture<mojom::GeopositionResultPtr> future;
  geolocation()->QueryNextPosition(future.GetCallback());
  base::RunLoop().RunUntilIdle();
  auto error =
      MakeGeopositionError(mojom::GeopositionErrorCode::kPositionUnavailable);
  SimulateLocationUpdate(*error);
  EXPECT_EQ(future.Get(), error);
}

TEST_F(GeolocationImplTest, QueryNextPositionWithoutUpdate) {
  // Query the position before the first location update. The callback is not
  // called.
  TestFuture<mojom::GeopositionResultPtr> future;
  geolocation()->QueryNextPosition(future.GetCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(future.IsReady());
}

TEST_F(GeolocationImplTest, SetAndClearOverride) {
  // Simulate a location update.
  auto initial_position = MakeGeoposition(37, -122);
  SimulateLocationUpdate(*initial_position);
  base::RunLoop().RunUntilIdle();

  auto override_position = MakeGeoposition(41, 74);
  // Set the position override. The callback is called with the overridden
  // position.
  TestFuture<mojom::GeopositionResultPtr> override_future;
  geolocation()->QueryNextPosition(override_future.GetCallback());
  SetOverride(*override_position);
  EXPECT_EQ(override_future.Get(), override_position);

  // Clear the override. The callback is not called.
  TestFuture<mojom::GeopositionResultPtr> clear_future;
  geolocation()->QueryNextPosition(clear_future.GetCallback());
  ClearOverride();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(clear_future.IsReady());
}

TEST_F(GeolocationImplTest, SetAndClearOverrideWithoutUpdate) {
  // Query the position before the first location update. The callback is not
  // called.
  TestFuture<mojom::GeopositionResultPtr> error_future;
  geolocation()->QueryNextPosition(error_future.GetCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(error_future.IsReady());

  // Set the position override. The callback is called with a GeopositionError.
  auto override_position = MakeGeoposition(41, 74);
  SetOverride(*override_position);
  ASSERT_TRUE(error_future.Get()->is_error());
  EXPECT_EQ(error_future.Get()->get_error()->error_code,
            mojom::GeopositionErrorCode::kPositionUnavailable);

  // Query the position again. The callback is called with the overridden
  // position.
  TestFuture<mojom::GeopositionResultPtr> override_future;
  geolocation()->QueryNextPosition(override_future.GetCallback());
  EXPECT_EQ(override_future.Get(), override_position);

  // Clear the override. The callback is not called.
  TestFuture<mojom::GeopositionResultPtr> clear_future;
  geolocation()->QueryNextPosition(clear_future.GetCallback());
  ClearOverride();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(clear_future.IsReady());
}

}  // namespace device
