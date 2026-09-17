// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/geolocation_impl.h"

#include <memory>

#include "base/callback_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/network_change_notifier.h"
#include "services/device/geolocation/geolocation_context.h"
#include "services/device/geolocation/geolocation_provider.h"
#include "services/device/public/mojom/geolocation_client_id.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

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
    last_set_accuracy_ = enable_high_accuracy;
    add_callback_count_++;
    return callback_list_.Add(callback);
  }

  void OverrideLocationForTesting(mojom::GeopositionResultPtr result) override {
  }

  mojom::GeopositionResultPtr GetCachedPosition() override {
    return cached_result_ ? cached_result_.Clone() : nullptr;
  }

  void SimulateLocationUpdate(const mojom::GeopositionResult& result) {
    cached_result_ = result.Clone();
    callback_list_.Notify(result);
  }

  bool GetLastSetAccuracy() const { return last_set_accuracy_; }
  int GetAddCallbackCount() const { return add_callback_count_; }

 private:
  base::RepeatingCallbackList<void(const mojom::GeopositionResult&)>
      callback_list_;
  bool last_set_accuracy_;
  int add_callback_count_ = 0;
  mojom::GeopositionResultPtr cached_result_;
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
    BindGeolocation(/*has_precise_permission=*/true);
  }

  void TearDown() override {
    GeolocationProvider::SetInstanceForTesting(nullptr);
  }

  void BindGeolocation(bool has_precise_permission) {
    geolocation_.reset();
    geolocation_context_.BindGeolocation(
        geolocation_.BindNewPipeAndPassReceiver(),
        url::Origin::Create(GURL("https://test.com")),
        mojom::GeolocationClientId::kForTesting, has_precise_permission);
  }

  void FlushForTesting() { geolocation_.FlushForTesting(); }

  void OnPermissionUpdated(mojom::GeolocationPermissionLevel permission_level) {
    geolocation_context_.OnPermissionUpdated(
        url::Origin::Create(GURL("https://test.com")), permission_level);
  }

  bool GetLastSetAccuracy() {
    return geolocation_provider_.GetLastSetAccuracy();
  }

  int GetAddCallbackCount() {
    return geolocation_provider_.GetAddCallbackCount();
  }

  void SimulateLocationUpdate(const mojom::GeopositionResult& result) {
    geolocation_provider_.SimulateLocationUpdate(result);
  }

  void SetOverride(const mojom::GeopositionResult& result) {
    geolocation_context_.SetOverride(result.Clone());
  }

  void ClearOverride() { geolocation_context_.ClearOverride(); }

  mojom::GeopositionResultPtr MakeGeoposition(double latitude,
                                              double longitude) const {
    auto position = mojom::Geoposition::New();
    position->latitude = latitude;
    position->longitude = longitude;
    position->accuracy = 100;
    position->timestamp = base::Time::Now();
    return mojom::GeopositionResult::NewPosition(std::move(position));
  }

  mojom::GeopositionResultPtr MakeGeopositionError(
      mojom::GeopositionErrorCode error_code) const {
    return mojom::GeopositionResult::NewError(mojom::GeopositionError::New(
        error_code, /*error_message=*/"", /*error_technical=*/""));
  }

  const mojo::Remote<mojom::Geolocation>& geolocation() const {
    return geolocation_;
  }

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
  FlushForTesting();
  auto position = MakeGeoposition(37, -122);
  SimulateLocationUpdate(*position);
  EXPECT_EQ(future.Get(), position);
}

TEST_F(GeolocationImplTest, QueryNextPositionError) {
  // Simulate a location update, but the update is an error. The callback is
  // called with the error.
  TestFuture<mojom::GeopositionResultPtr> future;
  geolocation()->QueryNextPosition(future.GetCallback());
  FlushForTesting();
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
  FlushForTesting();
  EXPECT_FALSE(future.IsReady());
}

TEST_F(GeolocationImplTest, SetAndClearOverride) {
  // Simulate a location update.
  auto initial_position = MakeGeoposition(37, -122);
  SimulateLocationUpdate(*initial_position);
  FlushForTesting();

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
  FlushForTesting();
  EXPECT_FALSE(clear_future.IsReady());
}

TEST_F(GeolocationImplTest, SetAndClearOverrideWithoutUpdate) {
  // Query the position before the first location update. The callback is not
  // called.
  TestFuture<mojom::GeopositionResultPtr> error_future;
  geolocation()->QueryNextPosition(error_future.GetCallback());
  FlushForTesting();
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
  FlushForTesting();
  EXPECT_FALSE(clear_future.IsReady());
}

TEST_F(GeolocationImplTest, PermissionDenied) {
  TestFuture<mojom::GeopositionResultPtr> future;
  geolocation()->QueryNextPosition(future.GetCallback());
  FlushForTesting();

  OnPermissionUpdated(mojom::GeolocationPermissionLevel::kDenied);

  auto result = future.Take();
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error()->error_code,
            mojom::GeopositionErrorCode::kPermissionDenied);
}

TEST_F(GeolocationImplTest, OnPermissionUpdated) {
  // Initially, with kPrecise permission, high accuracy request is accepted.
  geolocation()->SetHighAccuracyHint(true);
  FlushForTesting();
  EXPECT_TRUE(GetLastSetAccuracy());

  // Updated to kApproximate permission , original high accuracy should be
  // disabled.
  OnPermissionUpdated(mojom::GeolocationPermissionLevel::kApproximate);
  FlushForTesting();
  EXPECT_FALSE(GetLastSetAccuracy());

  // Given that current permission is kApproximate, the request for high
  // accuracy should be ignored.
  geolocation()->SetHighAccuracyHint(true);
  FlushForTesting();
  EXPECT_FALSE(GetLastSetAccuracy());

  // Change back to kPrecise, high accuracy should be re-enabled.
  OnPermissionUpdated(mojom::GeolocationPermissionLevel::kPrecise);
  FlushForTesting();
  EXPECT_TRUE(GetLastSetAccuracy());

  // With kPrecise permission, the request for low accuracy should still be
  // acceptable.
  geolocation()->SetHighAccuracyHint(false);
  FlushForTesting();
  EXPECT_FALSE(GetLastSetAccuracy());
}

TEST_F(GeolocationImplTest, EffectiveHighAccuracy) {
  // A subscription is created when `GeolocationImpl` is firstly created
  // through `GeolocationContext::BindGeolocation` which invokes
  // `GeolocationImpl::StartListeningForUpdates`.
  EXPECT_EQ(1, GetAddCallbackCount());

  // Set high accuracy to false. The `effective_high_accuracy_` is false before
  // the first granted `SetHighAccuracyHint(true)` is called. A repeated
  // `SetHighAccuracyHint(false)` should not create new subscription.
  geolocation()->SetHighAccuracyHint(false);
  FlushForTesting();
  EXPECT_FALSE(GetLastSetAccuracy());
  EXPECT_EQ(1, GetAddCallbackCount());

  // Set high accuracy to true. A new subscription should be created.
  // effective_high_accuracy will be true.
  geolocation()->SetHighAccuracyHint(true);
  FlushForTesting();
  EXPECT_TRUE(GetLastSetAccuracy());
  EXPECT_EQ(2, GetAddCallbackCount());

  // Set high accuracy to true again. No new subscription should be created.
  geolocation()->SetHighAccuracyHint(true);
  FlushForTesting();
  EXPECT_TRUE(GetLastSetAccuracy());
  EXPECT_EQ(2, GetAddCallbackCount());

  // Change permission to approximate. A new subscription should be created.
  // effective_high_accuracy will be false.
  OnPermissionUpdated(mojom::GeolocationPermissionLevel::kApproximate);
  FlushForTesting();
  EXPECT_FALSE(GetLastSetAccuracy());
  EXPECT_EQ(3, GetAddCallbackCount());

  // Change permission to approximate again. No new subscription should be
  // created.
  OnPermissionUpdated(mojom::GeolocationPermissionLevel::kApproximate);
  FlushForTesting();
  EXPECT_FALSE(GetLastSetAccuracy());
  EXPECT_EQ(3, GetAddCallbackCount());

  // Change permission to precise. A new subscription should be created.
  // effective_high_accuracy will be true, because high_accuracy is still true.
  OnPermissionUpdated(mojom::GeolocationPermissionLevel::kPrecise);
  FlushForTesting();
  EXPECT_TRUE(GetLastSetAccuracy());
  EXPECT_EQ(4, GetAddCallbackCount());

  // Set high accuracy to false. A new subscription should be created.
  geolocation()->SetHighAccuracyHint(false);
  FlushForTesting();
  EXPECT_FALSE(GetLastSetAccuracy());
  EXPECT_EQ(5, GetAddCallbackCount());

  // Set high accuracy to false again. No new subscription should be created.
  geolocation()->SetHighAccuracyHint(false);
  FlushForTesting();
  EXPECT_FALSE(GetLastSetAccuracy());
  EXPECT_EQ(5, GetAddCallbackCount());
}

TEST_F(GeolocationImplTest, QueryCachedPositionSuccess) {
  auto position = MakeGeoposition(37, -122);
  SimulateLocationUpdate(*position);

  TestFuture<mojom::GeopositionResultPtr> geolocation_result;
  geolocation()->QueryCachedPosition(geolocation_result.GetCallback());

  EXPECT_EQ(geolocation_result.Get()->get_position()->latitude, 37);
}

TEST_F(GeolocationImplTest, QueryCachedPositionFailure) {
  TestFuture<mojom::GeopositionResultPtr> geolocation_result;
  geolocation()->QueryCachedPosition(geolocation_result.GetCallback());

  EXPECT_TRUE(geolocation_result.Get()->is_error());

  // Now simulate an update and verify it works.
  auto position = MakeGeoposition(37, -122);
  SimulateLocationUpdate(*position);

  TestFuture<mojom::GeopositionResultPtr> geolocation_result2;
  geolocation()->QueryCachedPosition(geolocation_result2.GetCallback());

  EXPECT_EQ(geolocation_result2.Get()->get_position()->latitude, 37);
}

TEST_F(GeolocationImplTest, QueryCachedPositionMultipleUpdates) {
  auto position1 = MakeGeoposition(37, -122);
  SimulateLocationUpdate(*position1);

  TestFuture<mojom::GeopositionResultPtr> result1;
  geolocation()->QueryCachedPosition(result1.GetCallback());
  EXPECT_EQ(result1.Get()->get_position()->latitude, 37);

  auto position2 = MakeGeoposition(38, -123);
  SimulateLocationUpdate(*position2);

  TestFuture<mojom::GeopositionResultPtr> result2;
  geolocation()->QueryCachedPosition(result2.GetCallback());

  EXPECT_EQ(result2.Get()->get_position()->latitude, 38);
}

TEST_F(GeolocationImplTest, QueryCachedPositionDoesNotStartProvider) {
  int initial_callbacks = GetAddCallbackCount();

  TestFuture<mojom::GeopositionResultPtr> future;
  geolocation()->QueryCachedPosition(future.GetCallback());

  // Wait for the callback to be called.
  EXPECT_TRUE(future.Wait());

  // The callback count should not increase because we didn't start listening.
  EXPECT_EQ(GetAddCallbackCount(), initial_callbacks);
}

TEST_F(GeolocationImplTest, QueryCachedPositionWithOverride) {
  auto initial_position = MakeGeoposition(37, -122);
  SimulateLocationUpdate(*initial_position);

  auto override_position = MakeGeoposition(41, 74);
  SetOverride(*override_position);

  TestFuture<mojom::GeopositionResultPtr> future;
  geolocation()->QueryCachedPosition(future.GetCallback());

  EXPECT_EQ(future.Get(), override_position);
}

TEST_F(GeolocationImplTest,
       QueryCachedPositionApproximatePermissionPreciseResult) {
  BindGeolocation(/*has_precise_permission=*/false);

  auto position = MakeGeoposition(37, -122);
  position->get_position()->is_precise = true;
  SimulateLocationUpdate(*position);

  TestFuture<mojom::GeopositionResultPtr> geolocation_result;
  geolocation()->QueryCachedPosition(geolocation_result.GetCallback());

  EXPECT_TRUE(geolocation_result.Get()->is_error());
  EXPECT_EQ(geolocation_result.Get()->get_error()->error_code,
            mojom::GeopositionErrorCode::kPositionUnavailable);
}

TEST_F(GeolocationImplTest,
       QueryCachedPositionApproximatePermissionApproximateResult) {
  BindGeolocation(/*has_precise_permission=*/false);

  auto position = MakeGeoposition(37, -122);
  position->get_position()->is_precise = false;
  SimulateLocationUpdate(*position);

  TestFuture<mojom::GeopositionResultPtr> geolocation_result;
  geolocation()->QueryCachedPosition(geolocation_result.GetCallback());

  EXPECT_TRUE(geolocation_result.Get()->is_position());
  EXPECT_EQ(geolocation_result.Get()->get_position()->latitude, 37);
}

TEST_F(GeolocationImplTest,
       PermissionDowngradeToApproximateInvalidatesPreciseResult) {
  // Start with precise permission and low accuracy requested.
  geolocation()->SetHighAccuracyHint(false);
  FlushForTesting();

  // Simulate a precise location update.
  auto precise_position = MakeGeoposition(37, -122);
  precise_position->get_position()->is_precise = true;
  SimulateLocationUpdate(*precise_position);
  FlushForTesting();

  // Downgrade permission to approximate. Because high_accuracy_hint is false,
  // effective_high_accuracy does not change (remains false), but the cached
  // precise result must still be invalidated.
  OnPermissionUpdated(mojom::GeolocationPermissionLevel::kApproximate);
  FlushForTesting();

  // QueryNextPosition should not be immediately fulfilled with the stale
  // precise position.
  TestFuture<mojom::GeopositionResultPtr> future;
  geolocation()->QueryNextPosition(future.GetCallback());
  FlushForTesting();
  EXPECT_FALSE(future.IsReady());

  // Simulate an approximate location update; it should fulfill the query.
  auto approx_position = MakeGeoposition(38, -123);
  approx_position->get_position()->is_precise = false;
  SimulateLocationUpdate(*approx_position);

  EXPECT_EQ(future.Get(), approx_position);
}

TEST_F(GeolocationImplTest,
       PermissionDowngradeToApproximatePreservesApproximateResult) {
  // Start with precise permission.
  geolocation()->SetHighAccuracyHint(false);
  FlushForTesting();

  // Simulate an approximate location update.
  auto approx_position = MakeGeoposition(37, -122);
  approx_position->get_position()->is_precise = false;
  SimulateLocationUpdate(*approx_position);
  FlushForTesting();

  // Downgrade permission to approximate. Since the cached result is already
  // approximate, it should not be invalidated.
  OnPermissionUpdated(mojom::GeolocationPermissionLevel::kApproximate);
  FlushForTesting();

  TestFuture<mojom::GeopositionResultPtr> future;
  geolocation()->QueryNextPosition(future.GetCallback());

  EXPECT_EQ(future.Get(), approx_position);
}

TEST_F(GeolocationImplTest,
       QueryNextPositionApproximatePermissionPreciseResult) {
  BindGeolocation(/*has_precise_permission=*/false);

  TestFuture<mojom::GeopositionResultPtr> future;
  geolocation()->QueryNextPosition(future.GetCallback());
  FlushForTesting();

  // Simulate a precise location update received from provider.
  auto precise_position = MakeGeoposition(37, -122);
  precise_position->get_position()->is_precise = true;
  SimulateLocationUpdate(*precise_position);
  FlushForTesting();

  // The precise position must be dropped when the client only has approximate
  // permission, so the query should remain pending.
  EXPECT_FALSE(future.IsReady());

  // An approximate location update should fulfill the query.
  auto approx_position = MakeGeoposition(38, -123);
  approx_position->get_position()->is_precise = false;
  SimulateLocationUpdate(*approx_position);

  EXPECT_EQ(future.Get(), approx_position);
}

TEST_F(GeolocationImplTest, PermissionDowngradeToApproximatePreservesOverride) {
  // Set an override with a precise position.
  auto override_position = MakeGeoposition(41, 74);
  override_position->get_position()->is_precise = true;
  SetOverride(*override_position);

  // Downgrade permission to approximate.
  OnPermissionUpdated(mojom::GeolocationPermissionLevel::kApproximate);
  FlushForTesting();

  // The overridden position should be preserved and returned without
  // violating the precise permission check.
  TestFuture<mojom::GeopositionResultPtr> future;
  geolocation()->QueryNextPosition(future.GetCallback());

  EXPECT_EQ(future.Get(), override_position);
}

TEST_F(GeolocationImplTest, ClearOverrideResumesSubscription) {
  BindGeolocation(/*has_precise_permission=*/false);

  // Call SetOverride with a precise position.
  auto override_position = MakeGeoposition(41, 74);
  override_position->get_position()->is_precise = true;
  SetOverride(*override_position);

  // Call ClearOverride to clear the position override.
  ClearOverride();
  FlushForTesting();

  // Call QueryNextPosition to trigger position acquisition.
  TestFuture<mojom::GeopositionResultPtr> future;
  geolocation()->QueryNextPosition(future.GetCallback());
  FlushForTesting();
  EXPECT_FALSE(future.IsReady());

  // Call SimulateLocationUpdate with an approximate position.
  auto approx_position = MakeGeoposition(37, -122);
  approx_position->get_position()->is_precise = false;
  SimulateLocationUpdate(*approx_position);

  // Check that the QueryNextPosition callback receives the approximate
  // position.
  EXPECT_EQ(future.Get(), approx_position);
}

TEST_F(GeolocationImplTest,
       PermissionDowngradeWhileRequestActiveDropsPreciseUpdate) {
  // Start with precise location permission (SetUp binds with precise=true).
  // Call QueryNextPosition to request a position.
  TestFuture<mojom::GeopositionResultPtr> future;
  geolocation()->QueryNextPosition(future.GetCallback());
  FlushForTesting();
  EXPECT_FALSE(future.IsReady());

  // Call OnPermissionUpdated to simulate a downgrade to approximate location
  // permission while the request is active.
  OnPermissionUpdated(mojom::GeolocationPermissionLevel::kApproximate);
  FlushForTesting();

  // Simulate a location update with a precise position; it should be ignored.
  auto precise_position = MakeGeoposition(37, -122);
  precise_position->get_position()->is_precise = true;
  SimulateLocationUpdate(*precise_position);
  FlushForTesting();
  EXPECT_FALSE(future.IsReady());

  // Simulate another location update with an approximate position.
  auto approx_position = MakeGeoposition(38, -123);
  approx_position->get_position()->is_precise = false;
  SimulateLocationUpdate(*approx_position);

  // Check that the QueryNextPosition callback receives only the approximate
  // position.
  EXPECT_EQ(future.Get(), approx_position);
}

}  // namespace device
