// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/geolocation_impl.h"

#include <memory>

#include "base/callback_list.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/with_feature_override.h"
#include "base/time/time.h"
#include "components/content_settings/core/common/features.h"
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

class GeolocationImplTestWithApproxLocation
    : public base::test::WithFeatureOverride,
      public GeolocationImplTest {
 public:
  GeolocationImplTestWithApproxLocation()
      : base::test::WithFeatureOverride(
            content_settings::features::kApproximateGeolocationPermission) {}
};

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(GeolocationImplTestWithApproxLocation);

TEST_P(GeolocationImplTestWithApproxLocation, SetAndClearOverride) {
  if (IsParamFeatureEnabled()) {
    // When `kApproximateGeolocationPermission` is enabled, the overridden
    // position's `is_precise` flag is updated based on
    // `high_accuracy_hint_ && has_precise_permission_` to meet the spec
    // requirement.
    geolocation()->SetHighAccuracyHint(true);
  }
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

TEST_P(GeolocationImplTestWithApproxLocation,
       SetAndClearOverrideWithoutUpdate) {
  if (IsParamFeatureEnabled()) {
    // When `kApproximateGeolocationPermission` is enabled, the overridden
    // position's `is_precise` flag is updated based on
    // `high_accuracy_hint_ && has_precise_permission_` to meet the spec
    // requirement.
    geolocation()->SetHighAccuracyHint(true);
  }
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

// Verifies that calling GeolocationContext::QueryCachedPosition()
// returns cached position without subscribing to GeolocationProvider or
// starting the underlying OS location provider.
TEST_F(GeolocationImplTest, QueryCachedPositionDoesNotStartProvider) {
  int initial_callbacks = GetAddCallbackCount();

  GeolocationContext context;

  // Calling QueryCachedPosition when cache is empty returns an error.
  TestFuture<mojom::GeopositionResultPtr> empty_cache_future;
  context.QueryCachedPosition(empty_cache_future.GetCallback());
  EXPECT_TRUE(empty_cache_future.Get()->is_error());
  EXPECT_EQ(empty_cache_future.Get()->get_error()->error_code,
            mojom::GeopositionErrorCode::kPositionUnavailable);

  // Simulate a precise location update to populate the cache.
  auto position = MakeGeoposition(37, -122);
  position->get_position()->is_precise = true;
  SimulateLocationUpdate(*position);

  // Querying cached position should return the precise cached position without
  // dropping it.
  TestFuture<mojom::GeopositionResultPtr> cached_future;
  context.QueryCachedPosition(cached_future.GetCallback());
  EXPECT_EQ(cached_future.Get(), position);
  EXPECT_TRUE(cached_future.Get()->is_position() &&
              cached_future.Get()->get_position()->is_precise);

  // Context override should take precedence.
  auto override_position = MakeGeoposition(41, 74);
  context.SetOverride(override_position.Clone());
  TestFuture<mojom::GeopositionResultPtr> override_future;
  context.QueryCachedPosition(override_future.GetCallback());
  EXPECT_EQ(override_future.Get(), override_position);
  context.ClearOverride();

  TestFuture<mojom::GeopositionResultPtr> restored_cached_future;
  context.QueryCachedPosition(restored_cached_future.GetCallback());
  EXPECT_EQ(restored_cached_future.Get(), position);

  // Querying cached position should never start the provider.
  EXPECT_EQ(GetAddCallbackCount(), initial_callbacks);
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

TEST_P(GeolocationImplTestWithApproxLocation,
       PermissionDowngradeToApproximatePreservesOverride) {
  if (IsParamFeatureEnabled()) {
    // When `kApproximateGeolocationPermission` is enabled, the overridden
    // position's `is_precise` flag is updated based on
    // `high_accuracy_hint_ && has_precise_permission_` to meet the spec
    // requirement.
    geolocation()->SetHighAccuracyHint(true);
    FlushForTesting();
  }

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

TEST_F(GeolocationImplTest, NullFieldsOnApproximatePosition) {
  auto position = MakeGeoposition(37, -122);
  position->get_position()->altitude = 100.0;
  position->get_position()->altitude_accuracy = 50.0;
  position->get_position()->heading = 90.0;
  position->get_position()->speed = 10.0;
  position->get_position()->is_precise = false;

  TestFuture<mojom::GeopositionResultPtr> future;
  geolocation()->QueryNextPosition(future.GetCallback());
  FlushForTesting();
  SimulateLocationUpdate(*position);

  auto expected_position = position.Clone();
  expected_position->get_position()->altitude = mojom::kBadAltitude;
  expected_position->get_position()->altitude_accuracy = mojom::kBadAccuracy;
  expected_position->get_position()->heading = mojom::kBadHeading;
  expected_position->get_position()->speed = mojom::kBadSpeed;
  EXPECT_EQ(future.Take(), expected_position);

  position->get_position()->is_precise = true;
  geolocation()->QueryNextPosition(future.GetCallback());
  FlushForTesting();
  SimulateLocationUpdate(*position);

  EXPECT_EQ(future.Take(), position);
}

TEST_F(GeolocationImplTest, SetOverrideWithLowAccuracyHint) {
  base::test::ScopedFeatureList scoped_feature_list(
      content_settings::features::kApproximateGeolocationPermission);

  // Simulate an initial location update so SetOverride reports a position.
  auto initial_position = MakeGeoposition(37, -122);
  SimulateLocationUpdate(*initial_position);
  FlushForTesting();

  auto override_position = MakeGeoposition(41, 74);
  override_position->get_position()->altitude = 100.0;
  override_position->get_position()->altitude_accuracy = 50.0;
  override_position->get_position()->heading = 90.0;
  override_position->get_position()->speed = 10.0;
  override_position->get_position()->is_precise = true;

  // With `high_accuracy_hint_` defaulting to `false`, `SetOverride` should
  // downgrade `is_precise` to `false` and sanitize `altitude`,
  // `altitude_accuracy`, `heading`, and `speed` to `mojom::kBad*`.
  TestFuture<mojom::GeopositionResultPtr> future;
  geolocation()->QueryNextPosition(future.GetCallback());
  SetOverride(*override_position);

  auto expected_position = override_position.Clone();
  expected_position->get_position()->is_precise = false;
  expected_position->get_position()->altitude = mojom::kBadAltitude;
  expected_position->get_position()->altitude_accuracy = mojom::kBadAccuracy;
  expected_position->get_position()->heading = mojom::kBadHeading;
  expected_position->get_position()->speed = mojom::kBadSpeed;
  EXPECT_EQ(future.Take(), expected_position);

  // Even when `high_accuracy_hint_` is `true`, an override explicitly set with
  // `is_precise = false` should remain approximate (`is_precise = false`).
  geolocation()->SetHighAccuracyHint(true);
  FlushForTesting();
  override_position->get_position()->is_precise = false;
  geolocation()->QueryNextPosition(future.GetCallback());
  SetOverride(*override_position);
  EXPECT_EQ(future.Take(), expected_position);
}

}  // namespace device
