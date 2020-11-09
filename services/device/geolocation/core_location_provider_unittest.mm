// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/core_location_provider.h"

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "services/device/public/cpp/geolocation/geoposition.h"
#include "testing/gtest/include/gtest/gtest.h"

@interface FakeCLLocationManager : NSObject {
  CLLocationAccuracy _desiredAccuracy;
  id<CLLocationManagerDelegate> _delegate;
  bool _updating;
}
@property(assign, nonatomic) CLLocationAccuracy desiredAccuracy;
@property(weak, nonatomic) id<CLLocationManagerDelegate> delegate;
// CLLocationManager implementation.
- (void)stopUpdatingLocation;
- (void)startUpdatingLocation;
- (bool)updating;

// Utility functions.
- (void)fakeUpdatePosition:(CLLocation*)test_location;
- (void)fakeUpdatePermission:(CLAuthorizationStatus)status;
@end

@implementation FakeCLLocationManager
@synthesize desiredAccuracy = _desiredAccuracy;
@synthesize delegate = _delegate;
- (instancetype)init {
  self = [super init];
  _updating = false;
  return self;
}

- (void)stopUpdatingLocation {
  _updating = false;
}

- (void)startUpdatingLocation {
  _updating = true;
}

- (bool)updating {
  return _updating;
}

- (void)fakeUpdatePosition:(CLLocation*)testLocation {
  [_delegate locationManager:(id)self didUpdateLocations:@[ testLocation ]];
}

- (void)fakeUpdatePermission:(CLAuthorizationStatus)status {
  [_delegate locationManager:(id)self didChangeAuthorizationStatus:status];
}

@end

namespace device {

class CoreLocationProviderTest : public testing::Test {
 public:
  std::unique_ptr<CoreLocationProvider> provider_;

 protected:
  CoreLocationProviderTest() {}

  void InitializeProvider() {
    fake_location_manager_ = [[FakeCLLocationManager alloc] init];
    provider_ = std::make_unique<CoreLocationProvider>();
    provider_->SetManagerForTesting((id)fake_location_manager_);
  }

  bool IsUpdating() { return [fake_location_manager_ updating]; }

  // updates the position synchronously
  void FakeUpdatePosition(CLLocation* location) {
    [fake_location_manager_ fakeUpdatePosition:location];
  }

  const mojom::Geoposition& GetLatestPosition() {
    return provider_->GetPosition();
  }

  base::test::TaskEnvironment task_environment_;
  const LocationProvider::LocationProviderUpdateCallback callback_;
  FakeCLLocationManager* fake_location_manager_;
};

TEST_F(CoreLocationProviderTest, CreateDestroy) {
  InitializeProvider();
  EXPECT_TRUE(provider_);
  provider_.reset();
}

TEST_F(CoreLocationProviderTest, StartAndStopUpdating) {
  InitializeProvider();
  if (@available(macOS 10.12.0, *)) {
    [fake_location_manager_
        fakeUpdatePermission:kCLAuthorizationStatusAuthorizedAlways];
  } else {
    [fake_location_manager_
        fakeUpdatePermission:kCLAuthorizationStatusAuthorized];
  }
  provider_->StartProvider(/*high_accuracy=*/true);
  EXPECT_TRUE(IsUpdating());
  EXPECT_EQ([fake_location_manager_ desiredAccuracy], kCLLocationAccuracyBest);
  provider_->StopProvider();
  EXPECT_FALSE(IsUpdating());
  provider_.reset();
}

TEST_F(CoreLocationProviderTest, DontStartUpdatingIfPermissionDenied) {
  InitializeProvider();
  [fake_location_manager_ fakeUpdatePermission:kCLAuthorizationStatusDenied];
  provider_->StartProvider(/*high_accuracy=*/true);
  EXPECT_FALSE(IsUpdating());
}

TEST_F(CoreLocationProviderTest, DontStartUpdatingUntilPermissionGranted) {
  InitializeProvider();
  provider_->StartProvider(/*high_accuracy=*/true);
  EXPECT_FALSE(IsUpdating());
  [fake_location_manager_ fakeUpdatePermission:kCLAuthorizationStatusDenied];
  EXPECT_FALSE(IsUpdating());
  if (@available(macOS 10.12.0, *)) {
    [fake_location_manager_
        fakeUpdatePermission:kCLAuthorizationStatusAuthorizedAlways];
  } else {
    [fake_location_manager_
        fakeUpdatePermission:kCLAuthorizationStatusAuthorized];
  }
  EXPECT_TRUE(IsUpdating());
  provider_.reset();
}

TEST_F(CoreLocationProviderTest, GetPositionUpdates) {
  InitializeProvider();
  if (@available(macOS 10.12.0, *)) {
    [fake_location_manager_
        fakeUpdatePermission:kCLAuthorizationStatusAuthorizedAlways];
  } else {
    [fake_location_manager_
        fakeUpdatePermission:kCLAuthorizationStatusAuthorized];
  }
  provider_->StartProvider(/*high_accuracy=*/true);
  EXPECT_TRUE(IsUpdating());
  EXPECT_EQ([fake_location_manager_ desiredAccuracy], kCLLocationAccuracyBest);

  // test info
  double latitude = 147.147;
  double longitude = 101.101;
  double altitude = 417.417;
  double accuracy = 10.5;
  double altitude_accuracy = 15.5;
  NSDate* mac_timestamp = [NSDate date];

  CLLocationCoordinate2D coors;
  coors.latitude = latitude;
  coors.longitude = longitude;
  CLLocation* test_mac_location =
      [[CLLocation alloc] initWithCoordinate:coors
                                    altitude:altitude
                          horizontalAccuracy:accuracy
                            verticalAccuracy:altitude_accuracy
                                   timestamp:mac_timestamp];
  mojom::Geoposition test_position;
  test_position.latitude = latitude;
  test_position.longitude = longitude;
  test_position.altitude = altitude;
  test_position.accuracy = accuracy;
  test_position.altitude_accuracy = altitude_accuracy;
  test_position.timestamp =
      base::Time::FromDoubleT(mac_timestamp.timeIntervalSince1970);

  bool update_callback_called = false;
  provider_->SetUpdateCallback(
      base::BindLambdaForTesting([&](const LocationProvider* provider,
                                     const mojom::Geoposition& position) {
        update_callback_called = true;
        EXPECT_TRUE(test_position.Equals(position));
      }));

  FakeUpdatePosition(test_mac_location);

  EXPECT_TRUE(update_callback_called);
  EXPECT_TRUE(GetLatestPosition().Equals(test_position));

  provider_->StopProvider();
  EXPECT_FALSE(IsUpdating());
  provider_.reset();
}

}  // namespace device
