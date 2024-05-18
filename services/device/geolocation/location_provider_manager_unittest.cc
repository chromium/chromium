// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/location_provider_manager.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "services/device/geolocation/fake_location_provider.h"
#include "services/device/geolocation/fake_position_cache.h"
#include "services/device/public/cpp/geolocation/geoposition.h"
#include "services/device/public/cpp/geolocation/location_provider.h"
#include "services/device/public/mojom/geoposition.mojom.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::NiceMock;

namespace device {
namespace {

std::unique_ptr<LocationProvider> NullLocationProvider() {
  return nullptr;
}

class MockLocationObserver {
 public:
  virtual ~MockLocationObserver() = default;
  void OnLocationUpdate(const LocationProvider* provider,
                        mojom::GeopositionResultPtr result) {
    last_result_ = std::move(result);
  }

  const mojom::GeopositionResult* last_result() { return last_result_.get(); }

 private:
  mojom::GeopositionResultPtr last_result_;
};

double g_fake_time_now_secs = 1;

base::Time GetTimeNowForTest() {
  return base::Time::FromSecondsSinceUnixEpoch(g_fake_time_now_secs);
}

void AdvanceTimeNow(const base::TimeDelta& delta) {
  g_fake_time_now_secs += delta.InSecondsF();
}

void SetPositionFix(FakeLocationProvider* provider,
                    double latitude,
                    double longitude,
                    double accuracy) {
  auto result =
      mojom::GeopositionResult::NewPosition(mojom::Geoposition::New());
  auto& position = *result->get_position();
  position.latitude = latitude;
  position.longitude = longitude;
  position.accuracy = accuracy;
  position.timestamp = GetTimeNowForTest();
  ASSERT_TRUE(ValidateGeoposition(position));
  provider->HandlePositionChanged(std::move(result));
}

// TODO(lethalantidote): Populate a Geoposition in the class from kConstants
// and then just copy that with "=" versus using a helper function.
void SetReferencePosition(FakeLocationProvider* provider) {
  SetPositionFix(provider, 51.0, -0.1, 400);
}

}  // namespace

class TestingLocationProviderManager : public LocationProviderManager {
 public:
  TestingLocationProviderManager(
      LocationProviderUpdateCallback callback,
      const CustomLocationProviderCallback& provider_getter,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      bool should_use_system_location_provider = false)
      : LocationProviderManager(
            std::move(provider_getter),
            /*geolocation_system_permission_manager=*/nullptr,
            std::move(url_loader_factory),
            std::string() /* api_key */,
            std::make_unique<FakePositionCache>(),
            /*internals_updated_closure=*/base::DoNothing(),
            /*network_request_callback=*/base::DoNothing(),
            /*network_response_callback=*/base::DoNothing()),
        should_use_system_location_provider_(
            should_use_system_location_provider) {
    SetUpdateCallback(callback);
  }

  base::Time GetTimeNow() const override { return GetTimeNowForTest(); }

  std::unique_ptr<LocationProvider> NewNetworkLocationProvider(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& api_key) override {
    auto provider = std::make_unique<FakeLocationProvider>();
    network_location_provider_ = provider->GetWeakPtr();
    return provider;
  }

  std::unique_ptr<LocationProvider> NewSystemLocationProvider() override {
    if (!should_use_system_location_provider_) {
      return nullptr;
    }

    system_location_provider_ = new FakeLocationProvider;
    return base::WrapUnique(system_location_provider_.get());
  }

  mojom::GeolocationDiagnostics::ProviderState state() {
    mojom::GeolocationDiagnostics diagnostics;
    FillDiagnostics(diagnostics);
    return diagnostics.provider_state;
  }

  base::WeakPtr<FakeLocationProvider> network_location_provider_;
  raw_ptr<FakeLocationProvider> system_location_provider_ = nullptr;
  bool should_use_system_location_provider_;
};

class GeolocationLocationProviderManagerTest : public testing::Test {
 protected:
  GeolocationLocationProviderManagerTest()
      : observer_(new MockLocationObserver),
        url_loader_factory_(new network::TestSharedURLLoaderFactory()) {}

  // Initializes |location_provider_manager_| with the specified
  // |url_loader_factory|, which may be null.
  void InitializeLocationProviderManager(
      CustomLocationProviderCallback provider_getter,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      bool should_use_system_location_provider = false) {
    const LocationProvider::LocationProviderUpdateCallback callback =
        base::BindRepeating(&MockLocationObserver::OnLocationUpdate,
                            base::Unretained(observer_.get()));
    location_provider_manager_ =
        std::make_unique<TestingLocationProviderManager>(
            callback, std::move(provider_getter), std::move(url_loader_factory),
            should_use_system_location_provider);
  }

  // testing::Test
  void TearDown() override {}

  void CheckLastPositionInfo(double latitude,
                             double longitude,
                             double accuracy) {
    const auto* result = observer_->last_result();
    EXPECT_TRUE(result && result->is_position());
    if (result && result->is_position()) {
      const mojom::Geoposition& geoposition = *result->get_position();
      EXPECT_DOUBLE_EQ(latitude, geoposition.latitude);
      EXPECT_DOUBLE_EQ(longitude, geoposition.longitude);
      EXPECT_DOUBLE_EQ(accuracy, geoposition.accuracy);
    }
  }

  base::TimeDelta SwitchOnFreshnessCliff() {
    // Add 1, to ensure it meets any greater-than test.
    return LocationProviderManager::kFixStaleTimeoutTimeDelta +
           base::Milliseconds(1);
  }

  FakeLocationProvider* network_location_provider() {
    return location_provider_manager_->network_location_provider_.get();
  }

  FakeLocationProvider* system_location_provider() {
    return location_provider_manager_->system_location_provider_;
  }

  const std::unique_ptr<MockLocationObserver> observer_;
  std::unique_ptr<TestingLocationProviderManager> location_provider_manager_;
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

// Basic test of the text fixture.
TEST_F(GeolocationLocationProviderManagerTest, CreateDestroy) {
  InitializeLocationProviderManager(base::BindRepeating(&NullLocationProvider),
                                    nullptr);
  EXPECT_TRUE(location_provider_manager_);
  EXPECT_EQ(location_provider_manager_->state(),
            mojom::GeolocationDiagnostics::ProviderState::kStopped);
  location_provider_manager_.reset();
  SUCCEED();
}

// Tests OnPermissionGranted().
TEST_F(GeolocationLocationProviderManagerTest, OnPermissionGranted) {
  InitializeLocationProviderManager(base::BindRepeating(&NullLocationProvider),
                                    nullptr);
  EXPECT_FALSE(location_provider_manager_->HasPermissionBeenGrantedForTest());
  location_provider_manager_->OnPermissionGranted();
  EXPECT_TRUE(location_provider_manager_->HasPermissionBeenGrantedForTest());
  // Can't check the provider has been notified without going through the
  // motions to create the provider (see next test).
  EXPECT_FALSE(network_location_provider());
  EXPECT_FALSE(system_location_provider());
}

// Tests basic operation (single position fix) network location provider.
TEST_F(GeolocationLocationProviderManagerTest, NormalUsageNetwork) {
  InitializeLocationProviderManager(base::BindRepeating(&NullLocationProvider),
                                    url_loader_factory_);
  ASSERT_TRUE(location_provider_manager_);

  EXPECT_FALSE(network_location_provider());
  EXPECT_FALSE(system_location_provider());
  location_provider_manager_->StartProvider(false);

  ASSERT_TRUE(network_location_provider());
  EXPECT_FALSE(system_location_provider());
  EXPECT_EQ(mojom::GeolocationDiagnostics::ProviderState::kLowAccuracy,
            network_location_provider()->state());
  EXPECT_FALSE(observer_->last_result());

  SetReferencePosition(network_location_provider());

  ASSERT_TRUE(observer_->last_result());
  if (observer_->last_result()->is_position()) {
    ASSERT_TRUE(network_location_provider()->GetPosition());
    EXPECT_EQ(
        network_location_provider()->GetPosition()->get_position()->latitude,
        observer_->last_result()->get_position()->latitude);
  }

  EXPECT_FALSE(network_location_provider()->is_permission_granted());
  EXPECT_FALSE(location_provider_manager_->HasPermissionBeenGrantedForTest());
  location_provider_manager_->OnPermissionGranted();
  EXPECT_TRUE(location_provider_manager_->HasPermissionBeenGrantedForTest());
  EXPECT_TRUE(network_location_provider()->is_permission_granted());
}

// Tests basic operation (single position fix) system location provider.
TEST_F(GeolocationLocationProviderManagerTest, NormalUsageSystem) {
  InitializeLocationProviderManager(base::BindRepeating(&NullLocationProvider),
                                    url_loader_factory_, true);
  ASSERT_TRUE(location_provider_manager_);

  EXPECT_FALSE(network_location_provider());
  EXPECT_FALSE(system_location_provider());
  location_provider_manager_->StartProvider(false);

  EXPECT_FALSE(network_location_provider());
  ASSERT_TRUE(system_location_provider());
  EXPECT_EQ(mojom::GeolocationDiagnostics::ProviderState::kLowAccuracy,
            system_location_provider()->state());
  EXPECT_FALSE(observer_->last_result());

  SetReferencePosition(system_location_provider());

  ASSERT_TRUE(observer_->last_result());
  if (observer_->last_result()->is_position()) {
    ASSERT_TRUE(system_location_provider()->GetPosition());
    EXPECT_EQ(
        system_location_provider()->GetPosition()->get_position()->latitude,
        observer_->last_result()->get_position()->latitude);
  }

  EXPECT_FALSE(system_location_provider()->is_permission_granted());
  EXPECT_FALSE(location_provider_manager_->HasPermissionBeenGrantedForTest());
  location_provider_manager_->OnPermissionGranted();
  EXPECT_TRUE(location_provider_manager_->HasPermissionBeenGrantedForTest());
  EXPECT_TRUE(system_location_provider()->is_permission_granted());
}

// Tests basic operation (single position fix) with no network location
// provider, no system location provider and a custom system location provider.
TEST_F(GeolocationLocationProviderManagerTest, CustomSystemProviderOnly) {
  FakeLocationProvider* fake_location_provider = nullptr;
  InitializeLocationProviderManager(
      base::BindLambdaForTesting([&]() -> std::unique_ptr<LocationProvider> {
        auto provider = std::make_unique<FakeLocationProvider>();
        fake_location_provider = provider.get();
        return provider;
      }),
      nullptr, true);
  ASSERT_TRUE(location_provider_manager_);

  EXPECT_FALSE(network_location_provider());
  EXPECT_FALSE(system_location_provider());
  location_provider_manager_->StartProvider(false);

  EXPECT_FALSE(network_location_provider());
  EXPECT_FALSE(system_location_provider());
  EXPECT_EQ(mojom::GeolocationDiagnostics::ProviderState::kLowAccuracy,
            fake_location_provider->state());
  EXPECT_FALSE(observer_->last_result());

  SetReferencePosition(fake_location_provider);

  ASSERT_TRUE(observer_->last_result());
  if (observer_->last_result()->is_position()) {
    ASSERT_TRUE(fake_location_provider->GetPosition());
    EXPECT_EQ(fake_location_provider->GetPosition()->get_position()->latitude,
              observer_->last_result()->get_position()->latitude);
  }

  EXPECT_FALSE(fake_location_provider->is_permission_granted());
  EXPECT_FALSE(location_provider_manager_->HasPermissionBeenGrantedForTest());
  location_provider_manager_->OnPermissionGranted();
  EXPECT_TRUE(location_provider_manager_->HasPermissionBeenGrantedForTest());
  EXPECT_TRUE(fake_location_provider->is_permission_granted());
}

// Tests flipping from Low to High accuracy mode as requested by a location
// observer.
TEST_F(GeolocationLocationProviderManagerTest, SetObserverOptions) {
  InitializeLocationProviderManager(base::BindRepeating(&NullLocationProvider),
                                    url_loader_factory_);
  location_provider_manager_->StartProvider(false);
  ASSERT_TRUE(network_location_provider());
  EXPECT_FALSE(system_location_provider());
  EXPECT_EQ(mojom::GeolocationDiagnostics::ProviderState::kLowAccuracy,
            network_location_provider()->state());
  SetReferencePosition(network_location_provider());
  EXPECT_EQ(mojom::GeolocationDiagnostics::ProviderState::kLowAccuracy,
            network_location_provider()->state());
  location_provider_manager_->StartProvider(true);
  EXPECT_EQ(mojom::GeolocationDiagnostics::ProviderState::kHighAccuracy,
            network_location_provider()->state());
}

// Verifies that the location_provider_manager_ doesn't retain pointers to old
// providers after it has stopped and then restarted (crbug.com/240956).
TEST_F(GeolocationLocationProviderManagerTest, TwoOneShotsIsNewPositionBetter) {
  InitializeLocationProviderManager(base::BindRepeating(&NullLocationProvider),
                                    url_loader_factory_);
  location_provider_manager_->StartProvider(false);
  ASSERT_TRUE(network_location_provider());
  EXPECT_FALSE(system_location_provider());

  // Set the initial position.
  SetPositionFix(network_location_provider(), 3, 139, 100);
  CheckLastPositionInfo(3, 139, 100);

  // Restart providers to simulate a one-shot request.
  location_provider_manager_->StopProvider();

  // To test 240956, perform a throwaway alloc.
  // This convinces the allocator to put the providers in a new memory location.
  std::unique_ptr<FakeLocationProvider> dummy_provider(
      new FakeLocationProvider);

  location_provider_manager_->StartProvider(false);

  // Advance the time a short while to simulate successive calls.
  AdvanceTimeNow(base::Milliseconds(5));

  // Update with a less accurate position to verify 240956.
  SetPositionFix(network_location_provider(), 3, 139, 150);
  CheckLastPositionInfo(3, 139, 150);
}

}  // namespace device
