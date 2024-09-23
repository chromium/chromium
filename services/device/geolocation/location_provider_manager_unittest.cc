// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/device/geolocation/location_provider_manager.h"

#include <memory>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "services/device/geolocation/fake_location_provider.h"
#include "services/device/geolocation/fake_position_cache.h"
#include "services/device/public/cpp/device_features.h"
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
  position.timestamp = base::Time::Now();
  ASSERT_TRUE(ValidateGeoposition(position));
  provider->HandlePositionChanged(std::move(result));
}

void SetErrorPosition(FakeLocationProvider* provider,
                      mojom::GeopositionErrorCode error_code) {
  auto result = mojom::GeopositionResult::NewError(mojom::GeopositionError::New(
      error_code, /*error_message=*/"", /*error_technical=*/""));
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
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : LocationProviderManager(
            std::move(provider_getter),
            /*geolocation_system_permission_manager=*/nullptr,
            std::move(url_loader_factory),
            std::string() /* api_key */,
            std::make_unique<FakePositionCache>(),
            /*internals_updated_closure=*/base::DoNothing(),
            /*network_request_callback=*/base::DoNothing(),
            /*network_response_callback=*/base::DoNothing()) {
    SetUpdateCallback(callback);
  }

  std::unique_ptr<LocationProvider> NewNetworkLocationProvider(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& api_key) override {
    return std::make_unique<FakeLocationProvider>();
  }

  std::unique_ptr<LocationProvider> NewSystemLocationProvider() override {
    return std::make_unique<FakeLocationProvider>();
  }

  mojom::GeolocationDiagnostics::ProviderState state() {
    mojom::GeolocationDiagnostics diagnostics;
    FillDiagnostics(diagnostics);
    return diagnostics.provider_state;
  }
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
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
    const LocationProvider::LocationProviderUpdateCallback callback =
        base::BindRepeating(&MockLocationObserver::OnLocationUpdate,
                            base::Unretained(observer_.get()));
    location_provider_manager_ =
        std::make_unique<TestingLocationProviderManager>(
            callback, std::move(provider_getter),
            std::move(url_loader_factory));
  }

  // Default parameter `kNetworkOnly` is configured if the test does not
  // explicitly call `SetExperimentMode`.
  void SetUp() override {
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>(
        features::kLocationProviderManager);
  }

  // testing::Test
  void TearDown() override { scoped_feature_list_.reset(); }

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

  FakeLocationProvider* network_location_provider() {
    return static_cast<FakeLocationProvider*>(
        location_provider_manager_->network_location_provider_.get());
  }

  FakeLocationProvider* platform_location_provider() {
    return static_cast<FakeLocationProvider*>(
        location_provider_manager_->platform_location_provider_.get());
  }

  // Configure the `kLocationProviderManager` feature for testing with a
  // specific LocationProviderManagerMode. Ensures that only valid initial modes
  // (as defined in the FeatureParam options) are used, simplifying test setup
  // and avoiding invalid configurations.
  bool SetExperimentMode(mojom::LocationProviderManagerMode mode) {
    auto options =
        base::make_span(features::kLocationProviderManagerParam.options,
                        features::kLocationProviderManagerParam.option_count);
    auto it = std::ranges::find(
        options, mode,
        &base::FeatureParam<
            device::mojom::LocationProviderManagerMode>::Option::value);

    // An invalid initial mode was specified, return false to indicate failure.
    if (it == options.end()) {
      return false;
    }

    scoped_feature_list_.reset();
    base::FieldTrialParams parameters;
    parameters[features::kLocationProviderManagerParam.name] = it->name;
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitAndEnableFeatureWithParameters(
        features::kLocationProviderManager, parameters);
    base::RunLoop().RunUntilIdle();
    return true;
  }

  const std::unique_ptr<MockLocationObserver> observer_;
  std::unique_ptr<TestingLocationProviderManager> location_provider_manager_;
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
};

// Basic test of the text fixture.
TEST_F(GeolocationLocationProviderManagerTest, CreateDestroy) {
  InitializeLocationProviderManager(base::BindRepeating(&NullLocationProvider),
                                    /*url_loader_factory=*/nullptr);
  EXPECT_TRUE(location_provider_manager_);
  EXPECT_EQ(location_provider_manager_->state(),
            mojom::GeolocationDiagnostics::ProviderState::kStopped);
  location_provider_manager_.reset();
  SUCCEED();
}

// Tests OnPermissionGranted().
TEST_F(GeolocationLocationProviderManagerTest, OnPermissionGranted) {
  InitializeLocationProviderManager(base::BindRepeating(&NullLocationProvider),
                                    /*url_loader_factory=*/nullptr);
  EXPECT_FALSE(location_provider_manager_->HasPermissionBeenGrantedForTest());
  location_provider_manager_->OnPermissionGranted();
  EXPECT_TRUE(location_provider_manager_->HasPermissionBeenGrantedForTest());
  // Can't check the provider has been notified without going through the
  // motions to create the provider (see next test).
  EXPECT_FALSE(network_location_provider());
  EXPECT_FALSE(platform_location_provider());
}

#if !BUILDFLAG(IS_ANDROID)
// Tests basic operation (valid position and error position update) for network
// location provider.
TEST_F(GeolocationLocationProviderManagerTest, NetworkOnly) {
  InitializeLocationProviderManager(base::BindRepeating(&NullLocationProvider),
                                    url_loader_factory_);
  ASSERT_TRUE(location_provider_manager_);

  EXPECT_FALSE(network_location_provider());
  EXPECT_FALSE(platform_location_provider());
  location_provider_manager_->StartProvider(false);

  ASSERT_TRUE(network_location_provider());
  EXPECT_FALSE(platform_location_provider());
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

  // In `kNetworkOnly` mode, an error position is reported directly.
  SetErrorPosition(network_location_provider(),
                   mojom::GeopositionErrorCode::kPositionUnavailable);
  EXPECT_TRUE(network_location_provider()->GetPosition()->is_error());
  EXPECT_TRUE(observer_->last_result()->is_error());
  EXPECT_EQ(network_location_provider()->GetPosition()->get_error(),
            observer_->last_result()->get_error());
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
// Tests basic operation (valid position and error position update) for system
// location provider.
TEST_F(GeolocationLocationProviderManagerTest, PlatformOnly) {
  ASSERT_TRUE(
      SetExperimentMode(mojom::LocationProviderManagerMode::kPlatformOnly));
  InitializeLocationProviderManager(base::BindRepeating(&NullLocationProvider),
                                    url_loader_factory_);
  ASSERT_TRUE(location_provider_manager_);

  EXPECT_FALSE(network_location_provider());
  EXPECT_FALSE(platform_location_provider());
  location_provider_manager_->StartProvider(false);

  EXPECT_FALSE(network_location_provider());
  ASSERT_TRUE(platform_location_provider());
  EXPECT_EQ(mojom::GeolocationDiagnostics::ProviderState::kLowAccuracy,
            platform_location_provider()->state());
  EXPECT_FALSE(observer_->last_result());

  SetReferencePosition(platform_location_provider());

  ASSERT_TRUE(observer_->last_result());
  if (observer_->last_result()->is_position()) {
    ASSERT_TRUE(platform_location_provider()->GetPosition());
    EXPECT_EQ(
        platform_location_provider()->GetPosition()->get_position()->latitude,
        observer_->last_result()->get_position()->latitude);
  }

  EXPECT_FALSE(platform_location_provider()->is_permission_granted());
  EXPECT_FALSE(location_provider_manager_->HasPermissionBeenGrantedForTest());
  location_provider_manager_->OnPermissionGranted();
  EXPECT_TRUE(location_provider_manager_->HasPermissionBeenGrantedForTest());
  EXPECT_TRUE(platform_location_provider()->is_permission_granted());

  // In `kPlatformOnly` mode, an error position is reported directly.
  SetErrorPosition(platform_location_provider(),
                   mojom::GeopositionErrorCode::kPositionUnavailable);
  EXPECT_TRUE(platform_location_provider()->GetPosition()->is_error());
  EXPECT_TRUE(observer_->last_result()->is_error());
  EXPECT_EQ(platform_location_provider()->GetPosition()->get_error(),
            observer_->last_result()->get_error());
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)

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
      /*url_loader_factory=*/nullptr);
  ASSERT_TRUE(location_provider_manager_);

  EXPECT_FALSE(network_location_provider());
  EXPECT_FALSE(platform_location_provider());
  location_provider_manager_->StartProvider(false);

  EXPECT_FALSE(network_location_provider());
  EXPECT_FALSE(platform_location_provider());
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

#if !BUILDFLAG(IS_ANDROID)
// Tests flipping from Low to High accuracy mode as requested by a location
// observer.
TEST_F(GeolocationLocationProviderManagerTest, SetObserverOptions) {
  InitializeLocationProviderManager(base::BindRepeating(&NullLocationProvider),
                                    url_loader_factory_);
  location_provider_manager_->StartProvider(false);
  ASSERT_TRUE(network_location_provider());
  EXPECT_FALSE(platform_location_provider());
  EXPECT_EQ(mojom::GeolocationDiagnostics::ProviderState::kLowAccuracy,
            network_location_provider()->state());
  SetReferencePosition(network_location_provider());
  EXPECT_EQ(mojom::GeolocationDiagnostics::ProviderState::kLowAccuracy,
            network_location_provider()->state());
  location_provider_manager_->StartProvider(true);
  EXPECT_EQ(mojom::GeolocationDiagnostics::ProviderState::kHighAccuracy,
            network_location_provider()->state());
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC)
// This test fallback mechanism by simulating a `kWifiDisabled` error code
// reported from platform location provider. Fallback is currently only
// supported on macOS.
TEST_F(GeolocationLocationProviderManagerTest, HybridPlatformFallback) {
  ASSERT_TRUE(
      SetExperimentMode(mojom::LocationProviderManagerMode::kHybridPlatform));
  InitializeLocationProviderManager(base::BindRepeating(&NullLocationProvider),
                                    url_loader_factory_);
  ASSERT_TRUE(location_provider_manager_);

  EXPECT_FALSE(network_location_provider());
  EXPECT_FALSE(platform_location_provider());
  location_provider_manager_->StartProvider(false);

  EXPECT_FALSE(network_location_provider());
  ASSERT_TRUE(platform_location_provider());
  EXPECT_EQ(mojom::GeolocationDiagnostics::ProviderState::kLowAccuracy,
            platform_location_provider()->state());
  EXPECT_FALSE(observer_->last_result());

  // Simulate a `kWifiDisabled` error which will initiate fallback mode.
  SetErrorPosition(platform_location_provider(),
                   mojom::GeopositionErrorCode::kWifiDisabled);

  EXPECT_EQ(mojom::LocationProviderManagerMode::kHybridFallbackNetwork,
            location_provider_manager_->provider_manager_mode_);
  ASSERT_FALSE(observer_->last_result());
  EXPECT_FALSE(platform_location_provider());
  EXPECT_TRUE(network_location_provider());
  EXPECT_EQ(mojom::GeolocationDiagnostics::ProviderState::kLowAccuracy,
            network_location_provider()->state());

  // Stop provider and ensure that provider manager mode is reset.
  location_provider_manager_->StopProvider();
  EXPECT_EQ(mojom::LocationProviderManagerMode::kHybridPlatform,
            location_provider_manager_->provider_manager_mode_);
}

#endif  // BUILDFLAG(IS_MAC)

}  // namespace device
