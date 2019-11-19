// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/win/location_provider_winrt.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/win/core_winrt_util.h"
#include "services/device/geolocation/win/fake_geocoordinate_winrt.h"
#include "services/device/geolocation/win/fake_geolocator_winrt.h"
#include "services/device/public/cpp/geolocation/geoposition.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {
namespace {
using ABI::Windows::Devices::Geolocation::IGeolocator;
using ABI::Windows::Devices::Geolocation::PositionStatus;

class MockLocationObserver {
 public:
  MockLocationObserver(base::OnceClosure update_called)
      : update_called_(std::move(update_called)) {}
  ~MockLocationObserver() = default;

  void InvalidateLastPosition() {
    last_position_.error_code = mojom::Geoposition::ErrorCode::NONE;
    EXPECT_FALSE(ValidateGeoposition(last_position_));
  }

  void OnLocationUpdate(const LocationProvider* provider,
                        const mojom::Geoposition& position) {
    last_position_ = position;
    on_location_update_called_ = true;
    std::move(update_called_).Run();
  }

  mojom::Geoposition last_position() { return last_position_; }

  bool on_location_update_called() { return on_location_update_called_; }

 private:
  base::OnceClosure update_called_;
  mojom::Geoposition last_position_;
  bool on_location_update_called_ = false;
};

}  // namespace

class TestingLocationProviderWinrt : public LocationProviderWinrt {
 public:
  TestingLocationProviderWinrt(
      std::unique_ptr<FakeGeocoordinateData> position_data,
      PositionStatus position_status)
      : position_data_(std::move(position_data)),
        position_status_(position_status) {}

  bool HasPermissionBeenGrantedForTest() { return permission_granted_; }

  bool IsHighAccuracyEnabled() { return enable_high_accuracy_; }

  base::Optional<EventRegistrationToken> GetStatusChangedToken() {
    return status_changed_token_;
  }

  base::Optional<EventRegistrationToken> GetPositionChangedToken() {
    return position_changed_token_;
  }

  HRESULT GetGeolocator(IGeolocator** geo_locator) override {
    *geo_locator = Microsoft::WRL::Make<FakeGeolocatorWinrt>(
                       std::move(position_data_), position_status_)
                       .Detach();
    return S_OK;
  }

 private:
  std::unique_ptr<FakeGeocoordinateData> position_data_;
  const PositionStatus position_status_;
};

class LocationProviderWinrtTest : public testing::Test {
 public:
  static void SetUpTestSuite() {
    base::win::RoInitialize(RO_INIT_TYPE::RO_INIT_MULTITHREADED);
  }

 protected:
  LocationProviderWinrtTest()
      : observer_(
            std::make_unique<MockLocationObserver>(run_loop_.QuitClosure())),
        callback_(base::BindRepeating(&MockLocationObserver::OnLocationUpdate,
                                      base::Unretained(observer_.get()))) {}

  void InitializeProvider(
      PositionStatus position_status = PositionStatus::PositionStatus_Ready) {
    auto test_data = FakeGeocoordinateData();
    test_data.longitude = 0;
    test_data.latitude = 0;
    test_data.accuracy = 0;
    InitializeProvider(test_data, position_status);
  }

  void InitializeProvider(
      FakeGeocoordinateData position_data,
      PositionStatus position_status = PositionStatus::PositionStatus_Ready) {
    provider_ = std::make_unique<TestingLocationProviderWinrt>(
        std::make_unique<FakeGeocoordinateData>(position_data),
        position_status);
    provider_->SetUpdateCallback(callback_);
  }

  base::test::TaskEnvironment task_environment_;
  base::RunLoop run_loop_;
  const std::unique_ptr<MockLocationObserver> observer_;
  const LocationProvider::LocationProviderUpdateCallback callback_;
  std::unique_ptr<TestingLocationProviderWinrt> provider_;
};

TEST_F(LocationProviderWinrtTest, CreateDestroy) {
  InitializeProvider();
  EXPECT_TRUE(provider_);
  provider_.reset();
}

TEST_F(LocationProviderWinrtTest, OnPermissionGranted) {
  InitializeProvider();
  EXPECT_FALSE(provider_->HasPermissionBeenGrantedForTest());
  provider_->OnPermissionGranted();
  EXPECT_TRUE(provider_->HasPermissionBeenGrantedForTest());
}

TEST_F(LocationProviderWinrtTest, SetAccuracyOptions) {
  InitializeProvider();
  provider_->StartProvider(/*enable_high_accuracy=*/false);
  EXPECT_EQ(false, provider_->IsHighAccuracyEnabled());
  provider_->StartProvider(/*enable_high_accuracy=*/true);
  EXPECT_EQ(true, provider_->IsHighAccuracyEnabled());
}

// Tests when OnPermissionGranted() called location update is provided.
TEST_F(LocationProviderWinrtTest, HasPermissions) {
  auto test_data = FakeGeocoordinateData();
  test_data.longitude = 1;
  test_data.latitude = 2;
  test_data.accuracy = 3;
  test_data.speed = 4;

  InitializeProvider(test_data);
  provider_->OnPermissionGranted();
  provider_->StartProvider(/*enable_high_accuracy=*/false);

  EXPECT_FALSE(observer_->on_location_update_called());
  EXPECT_FALSE(ValidateGeoposition(observer_->last_position()));

  EXPECT_TRUE(provider_->GetStatusChangedToken().has_value());
  EXPECT_TRUE(provider_->GetPositionChangedToken().has_value());

  run_loop_.Run();

  EXPECT_TRUE(observer_->on_location_update_called());
  auto position = observer_->last_position();
  EXPECT_TRUE(ValidateGeoposition(position));
  EXPECT_EQ(position.latitude, test_data.latitude);
  EXPECT_EQ(position.longitude, test_data.longitude);
  EXPECT_EQ(position.accuracy, test_data.accuracy);
  EXPECT_EQ(position.altitude, device::mojom::kBadAltitude);
  EXPECT_EQ(position.altitude_accuracy, device::mojom::kBadAccuracy);
  EXPECT_EQ(position.speed, test_data.speed.value());
  EXPECT_EQ(position.heading, device::mojom::kBadHeading);
}

// Tests when OnPermissionGranted() called location update is provided with all
// possible values populated.
TEST_F(LocationProviderWinrtTest, HasPermissionsAllValues) {
  auto test_data = FakeGeocoordinateData();
  test_data.longitude = 1;
  test_data.latitude = 2;
  test_data.accuracy = 3;
  test_data.altitude = 4;
  test_data.altitude_accuracy = 5;
  test_data.heading = 6;
  test_data.speed = 7;

  InitializeProvider(test_data);
  provider_->OnPermissionGranted();
  provider_->StartProvider(/*enable_high_accuracy=*/false);

  EXPECT_FALSE(observer_->on_location_update_called());
  EXPECT_FALSE(ValidateGeoposition(observer_->last_position()));

  EXPECT_TRUE(provider_->GetStatusChangedToken().has_value());
  EXPECT_TRUE(provider_->GetPositionChangedToken().has_value());

  run_loop_.Run();

  EXPECT_TRUE(observer_->on_location_update_called());
  auto position = observer_->last_position();
  EXPECT_TRUE(ValidateGeoposition(position));
  EXPECT_EQ(position.latitude, test_data.latitude);
  EXPECT_EQ(position.longitude, test_data.longitude);
  EXPECT_EQ(position.accuracy, test_data.accuracy);
  EXPECT_EQ(position.altitude, test_data.altitude.value());
  EXPECT_EQ(position.altitude_accuracy, test_data.altitude_accuracy.value());
  EXPECT_EQ(position.speed, test_data.speed.value());
  EXPECT_EQ(position.heading, test_data.heading.value());
}

// Tests when provider is stopped and started quickly access errors
// do not occur and location update is not called.
TEST_F(LocationProviderWinrtTest, StartStopProviderRunTasks) {
  InitializeProvider();
  provider_->OnPermissionGranted();
  provider_->StartProvider(/*enable_high_accuracy=*/false);
  provider_->StopProvider();

  EXPECT_FALSE(observer_->on_location_update_called());
  EXPECT_FALSE(ValidateGeoposition(observer_->last_position()));

  run_loop_.RunUntilIdle();

  EXPECT_FALSE(observer_->on_location_update_called());
  EXPECT_FALSE(provider_->GetStatusChangedToken().has_value());
  EXPECT_FALSE(provider_->GetPositionChangedToken().has_value());
}

// Tests when OnPermissionGranted() has not been called location update
// is not provided.
TEST_F(LocationProviderWinrtTest, NoPermissions) {
  InitializeProvider();
  provider_->StartProvider(/*enable_high_accuracy=*/false);

  EXPECT_FALSE(observer_->on_location_update_called());
  EXPECT_FALSE(ValidateGeoposition(observer_->last_position()));

  run_loop_.RunUntilIdle();

  EXPECT_FALSE(observer_->on_location_update_called());
  EXPECT_FALSE(provider_->GetStatusChangedToken().has_value());
  EXPECT_FALSE(provider_->GetPositionChangedToken().has_value());
}

// Tests when a PositionStatus_Disabled is returned from the OS indicating
// access to location on the OS is disabled, a permission denied is returned.
TEST_F(LocationProviderWinrtTest, PositionStatusDisabledOsPermissions) {
  InitializeProvider(PositionStatus::PositionStatus_Disabled);
  provider_->OnPermissionGranted();
  provider_->StartProvider(/*enable_high_accuracy=*/false);

  EXPECT_FALSE(observer_->on_location_update_called());
  EXPECT_FALSE(ValidateGeoposition(observer_->last_position()));

  EXPECT_TRUE(provider_->GetStatusChangedToken().has_value());
  EXPECT_TRUE(provider_->GetPositionChangedToken().has_value());

  run_loop_.Run();

  EXPECT_TRUE(observer_->on_location_update_called());
  auto position = observer_->last_position();
  EXPECT_FALSE(ValidateGeoposition(position));
  EXPECT_EQ(position.error_code,
            mojom::Geoposition::ErrorCode::PERMISSION_DENIED);
}
}  // namespace device
