// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/location_arbitrator.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
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

std::unique_ptr<LocationProvider> GetCustomLocationProviderForTest(
    std::unique_ptr<LocationProvider> provider) {
  return provider;
}

class MockLocationObserver {
 public:
  virtual ~MockLocationObserver() = default;
  void InvalidateLastPosition() {
    last_position_.latitude = 100;
    last_position_.error_code = mojom::Geoposition::ErrorCode::NONE;
    ASSERT_FALSE(ValidateGeoposition(last_position_));
  }
  void OnLocationUpdate(const LocationProvider* provider,
                        const mojom::Geoposition& position) {
    last_position_ = position;
  }

  mojom::Geoposition last_position() { return last_position_; }

 private:
  mojom::Geoposition last_position_;
};

double g_fake_time_now_secs = 1;

base::Time GetTimeNowForTest() {
  return base::Time::FromDoubleT(g_fake_time_now_secs);
}

void AdvanceTimeNow(const base::TimeDelta& delta) {
  g_fake_time_now_secs += delta.InSecondsF();
}

void SetPositionFix(FakeLocationProvider* provider,
                    double latitude,
                    double longitude,
                    double accuracy) {
  mojom::Geoposition position;
  position.error_code = mojom::Geoposition::ErrorCode::NONE;
  position.latitude = latitude;
  position.longitude = longitude;
  position.accuracy = accuracy;
  position.timestamp = GetTimeNowForTest();
  ASSERT_TRUE(ValidateGeoposition(position));
  provider->HandlePositionChanged(position);
}

// TODO(lethalantidote): Populate a Geoposition in the class from kConstants
// and then just copy that with "=" versus using a helper function.
void SetReferencePosition(FakeLocationProvider* provider) {
  SetPositionFix(provider, 51.0, -0.1, 400);
}

}  // namespace

class TestingLocationArbitrator : public LocationArbitrator {
 public:
  TestingLocationArbitrator(
      const LocationProviderUpdateCallback& callback,
      const CustomLocationProviderCallback& provider_getter,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      bool should_use_system_location_provider = false)
      : LocationArbitrator(provider_getter,
                           std::move(url_loader_factory),
                           std::string() /* api_key */,
                           std::make_unique<FakePositionCache>()),
        should_use_system_location_provider_(
            should_use_system_location_provider) {
    SetUpdateCallback(callback);
  }

  base::Time GetTimeNow() const override { return GetTimeNowForTest(); }

  std::unique_ptr<LocationProvider> NewNetworkLocationProvider(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& api_key) override {
    network_location_provider_ = new FakeLocationProvider;
    return base::WrapUnique(network_location_provider_);
  }

  std::unique_ptr<LocationProvider> NewSystemLocationProvider() override {
    if (!should_use_system_location_provider_) {
      return nullptr;
    }

    system_location_provider_ = new FakeLocationProvider;
    return base::WrapUnique(system_location_provider_);
  }

  FakeLocationProvider* network_location_provider_ = nullptr;
  FakeLocationProvider* system_location_provider_ = nullptr;
  bool should_use_system_location_provider_;
};

class GeolocationLocationArbitratorTest : public testing::Test {
 protected:
  GeolocationLocationArbitratorTest()
      : observer_(new MockLocationObserver),
        url_loader_factory_(new network::TestSharedURLLoaderFactory()) {}

  // Initializes |arbitrator_| with the specified |url_loader_factory|, which
  // may be null.
  void InitializeArbitrator(
      const CustomLocationProviderCallback& provider_getter,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      bool should_use_system_location_provider = false) {
    const LocationProvider::LocationProviderUpdateCallback callback =
        base::BindRepeating(&MockLocationObserver::OnLocationUpdate,
                            base::Unretained(observer_.get()));
    arbitrator_.reset(new TestingLocationArbitrator(
        callback, provider_getter, std::move(url_loader_factory),
        should_use_system_location_provider));
  }

  // testing::Test
  void TearDown() override {}

  void CheckLastPositionInfo(double latitude,
                             double longitude,
                             double accuracy) {
    mojom::Geoposition geoposition = observer_->last_position();
    EXPECT_TRUE(ValidateGeoposition(geoposition));
    EXPECT_DOUBLE_EQ(latitude, geoposition.latitude);
    EXPECT_DOUBLE_EQ(longitude, geoposition.longitude);
    EXPECT_DOUBLE_EQ(accuracy, geoposition.accuracy);
  }

  base::TimeDelta SwitchOnFreshnessCliff() {
    // Add 1, to ensure it meets any greater-than test.
    return LocationArbitrator::kFixStaleTimeoutTimeDelta +
           base::TimeDelta::FromMilliseconds(1);
  }

  FakeLocationProvider* network_location_provider() {
    return arbitrator_->network_location_provider_;
  }

  FakeLocationProvider* system_location_provider() {
    return arbitrator_->system_location_provider_;
  }

  const std::unique_ptr<MockLocationObserver> observer_;
  std::unique_ptr<TestingLocationArbitrator> arbitrator_;
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

// Basic test of the text fixture.
TEST_F(GeolocationLocationArbitratorTest, CreateDestroy) {
  InitializeArbitrator(
      base::BindRepeating(&GetCustomLocationProviderForTest, nullptr), nullptr);
  EXPECT_TRUE(arbitrator_);
  arbitrator_.reset();
  SUCCEED();
}

// Tests OnPermissionGranted().
TEST_F(GeolocationLocationArbitratorTest, OnPermissionGranted) {
  InitializeArbitrator(
      base::BindRepeating(&GetCustomLocationProviderForTest, nullptr), nullptr);
  EXPECT_FALSE(arbitrator_->HasPermissionBeenGrantedForTest());
  arbitrator_->OnPermissionGranted();
  EXPECT_TRUE(arbitrator_->HasPermissionBeenGrantedForTest());
  // Can't check the provider has been notified without going through the
  // motions to create the provider (see next test).
  EXPECT_FALSE(network_location_provider());
  EXPECT_FALSE(system_location_provider());
}

// Tests basic operation (single position fix) network location provider.
TEST_F(GeolocationLocationArbitratorTest, NormalUsageNetwork) {
  InitializeArbitrator(
      base::BindRepeating(&GetCustomLocationProviderForTest, nullptr),
      url_loader_factory_);
  ASSERT_TRUE(arbitrator_);

  EXPECT_FALSE(network_location_provider());
  EXPECT_FALSE(system_location_provider());
  arbitrator_->StartProvider(false);

  ASSERT_TRUE(network_location_provider());
  EXPECT_FALSE(system_location_provider());
  EXPECT_EQ(FakeLocationProvider::LOW_ACCURACY,
            network_location_provider()->state_);
  EXPECT_FALSE(ValidateGeoposition(observer_->last_position()));
  EXPECT_EQ(mojom::Geoposition::ErrorCode::NONE,
            observer_->last_position().error_code);

  SetReferencePosition(network_location_provider());

  EXPECT_TRUE(ValidateGeoposition(observer_->last_position()) ||
              observer_->last_position().error_code !=
                  mojom::Geoposition::ErrorCode::NONE);
  EXPECT_EQ(network_location_provider()->GetPosition().latitude,
            observer_->last_position().latitude);

  EXPECT_FALSE(network_location_provider()->is_permission_granted());
  EXPECT_FALSE(arbitrator_->HasPermissionBeenGrantedForTest());
  arbitrator_->OnPermissionGranted();
  EXPECT_TRUE(arbitrator_->HasPermissionBeenGrantedForTest());
  EXPECT_TRUE(network_location_provider()->is_permission_granted());
}

// Tests basic operation (single position fix) system location provider.
TEST_F(GeolocationLocationArbitratorTest, NormalUsageSystem) {
  InitializeArbitrator(
      base::BindRepeating(&GetCustomLocationProviderForTest, nullptr),
      url_loader_factory_, true);
  ASSERT_TRUE(arbitrator_);

  EXPECT_FALSE(network_location_provider());
  EXPECT_FALSE(system_location_provider());
  arbitrator_->StartProvider(false);

  EXPECT_FALSE(network_location_provider());
  ASSERT_TRUE(system_location_provider());
  EXPECT_EQ(FakeLocationProvider::LOW_ACCURACY,
            system_location_provider()->state_);
  EXPECT_FALSE(ValidateGeoposition(observer_->last_position()));
  EXPECT_EQ(mojom::Geoposition::ErrorCode::NONE,
            observer_->last_position().error_code);

  SetReferencePosition(system_location_provider());

  EXPECT_TRUE(ValidateGeoposition(observer_->last_position()) ||
              observer_->last_position().error_code !=
                  mojom::Geoposition::ErrorCode::NONE);
  EXPECT_EQ(system_location_provider()->GetPosition().latitude,
            observer_->last_position().latitude);

  EXPECT_FALSE(system_location_provider()->is_permission_granted());
  EXPECT_FALSE(arbitrator_->HasPermissionBeenGrantedForTest());
  arbitrator_->OnPermissionGranted();
  EXPECT_TRUE(arbitrator_->HasPermissionBeenGrantedForTest());
  EXPECT_TRUE(system_location_provider()->is_permission_granted());
}

// Tests basic operation (single position fix) with no network location
// provider, no system location provider and a custom system location provider.
TEST_F(GeolocationLocationArbitratorTest, CustomSystemProviderOnly) {
  auto provider = std::make_unique<FakeLocationProvider>();
  auto* fake_location_provider = provider.get();
  InitializeArbitrator(base::BindRepeating(&GetCustomLocationProviderForTest,
                                           base::Passed(&provider)),
                       nullptr, true);
  ASSERT_TRUE(arbitrator_);

  EXPECT_FALSE(network_location_provider());
  EXPECT_FALSE(system_location_provider());
  arbitrator_->StartProvider(false);

  EXPECT_FALSE(network_location_provider());
  EXPECT_FALSE(system_location_provider());
  EXPECT_EQ(FakeLocationProvider::LOW_ACCURACY, fake_location_provider->state_);
  EXPECT_FALSE(ValidateGeoposition(observer_->last_position()));
  EXPECT_EQ(mojom::Geoposition::ErrorCode::NONE,
            observer_->last_position().error_code);

  SetReferencePosition(fake_location_provider);

  EXPECT_TRUE(ValidateGeoposition(observer_->last_position()) ||
              observer_->last_position().error_code !=
                  mojom::Geoposition::ErrorCode::NONE);
  EXPECT_EQ(fake_location_provider->GetPosition().latitude,
            observer_->last_position().latitude);

  EXPECT_FALSE(fake_location_provider->is_permission_granted());
  EXPECT_FALSE(arbitrator_->HasPermissionBeenGrantedForTest());
  arbitrator_->OnPermissionGranted();
  EXPECT_TRUE(arbitrator_->HasPermissionBeenGrantedForTest());
  EXPECT_TRUE(fake_location_provider->is_permission_granted());
}

// Tests flipping from Low to High accuracy mode as requested by a location
// observer.
TEST_F(GeolocationLocationArbitratorTest, SetObserverOptions) {
  InitializeArbitrator(
      base::BindRepeating(&GetCustomLocationProviderForTest, nullptr),
      url_loader_factory_);
  arbitrator_->StartProvider(false);
  ASSERT_TRUE(network_location_provider());
  EXPECT_FALSE(system_location_provider());
  EXPECT_EQ(FakeLocationProvider::LOW_ACCURACY,
            network_location_provider()->state_);
  SetReferencePosition(network_location_provider());
  EXPECT_EQ(FakeLocationProvider::LOW_ACCURACY,
            network_location_provider()->state_);
  arbitrator_->StartProvider(true);
  EXPECT_EQ(FakeLocationProvider::HIGH_ACCURACY,
            network_location_provider()->state_);
}

// Verifies that the arbitrator doesn't retain pointers to old providers after
// it has stopped and then restarted (crbug.com/240956).
TEST_F(GeolocationLocationArbitratorTest, TwoOneShotsIsNewPositionBetter) {
  InitializeArbitrator(
      base::BindRepeating(&GetCustomLocationProviderForTest, nullptr),
      url_loader_factory_);
  arbitrator_->StartProvider(false);
  ASSERT_TRUE(network_location_provider());
  EXPECT_FALSE(system_location_provider());

  // Set the initial position.
  SetPositionFix(network_location_provider(), 3, 139, 100);
  CheckLastPositionInfo(3, 139, 100);

  // Restart providers to simulate a one-shot request.
  arbitrator_->StopProvider();

  // To test 240956, perform a throwaway alloc.
  // This convinces the allocator to put the providers in a new memory location.
  std::unique_ptr<FakeLocationProvider> dummy_provider(
      new FakeLocationProvider);

  arbitrator_->StartProvider(false);

  // Advance the time a short while to simulate successive calls.
  AdvanceTimeNow(base::TimeDelta::FromMilliseconds(5));

  // Update with a less accurate position to verify 240956.
  SetPositionFix(network_location_provider(), 3, 139, 150);
  CheckLastPositionInfo(3, 139, 150);
}

}  // namespace device
