// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/geolocation/model/geolocation_manager.h"

#import <CoreLocation/CoreLocation.h>

#import "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/geolocation/model/authorization_status_cache_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

@interface FakeCLLocationManagerDelegate : NSObject <CLLocationManagerDelegate>

@property(nonatomic) int delegateCallbackCount;

@end

@implementation FakeCLLocationManagerDelegate

- (instancetype)init {
  self = [super init];
  if (self) {
    _delegateCallbackCount = 0;
  }
  return self;
}

- (void)locationManagerDidChangeAuthorization:
    (CLLocationManager*)locationManager {
  self.delegateCallbackCount++;
}

@end

namespace {

class GeolocationManagerTest : public PlatformTest {
 public:
  GeolocationManagerTest() {}

  ~GeolocationManagerTest() override {}

  void SetUp() override {
    authorization_status_cache_util::ClearAuthorizationStatusForTesting();
  }

  void TearDown() override {
    authorization_status_cache_util::ClearAuthorizationStatusForTesting();
  }

 private:
  base::test::TaskEnvironment task_environment_;
};

// Tests that the cache util clears the cache correctly.
TEST_F(GeolocationManagerTest, AuthorizationStatusCacheUtilClear) {
  authorization_status_cache_util::SetAuthorizationStatus(
      kCLAuthorizationStatusNotDetermined);
  authorization_status_cache_util::ClearAuthorizationStatusForTesting();
  ASSERT_FALSE(authorization_status_cache_util::GetAuthorizationStatus());
}

// Tests that the cache util sets and retrives the cache values correctly.
TEST_F(GeolocationManagerTest, AuthorizationStatusCacheUtilSetAndRetrieve) {
  authorization_status_cache_util::SetAuthorizationStatus(
      kCLAuthorizationStatusNotDetermined);
  std::optional<CLAuthorizationStatus> status =
      authorization_status_cache_util::GetAuthorizationStatus();
  EXPECT_TRUE(status.has_value());
  EXPECT_EQ(kCLAuthorizationStatusNotDetermined, status.value());

  authorization_status_cache_util::SetAuthorizationStatus(
      kCLAuthorizationStatusDenied);
  status = authorization_status_cache_util::GetAuthorizationStatus();
  EXPECT_TRUE(status.has_value());
  EXPECT_EQ(kCLAuthorizationStatusDenied, status.value());

  authorization_status_cache_util::SetAuthorizationStatus(
      kCLAuthorizationStatusAuthorizedWhenInUse);
  status = authorization_status_cache_util::GetAuthorizationStatus();
  EXPECT_TRUE(status.has_value());
  EXPECT_EQ(kCLAuthorizationStatusAuthorizedWhenInUse, status.value());
}

// Tests that the internal CLLocationManager calls its delegate after creation.
TEST_F(GeolocationManagerTest, LocationUpdatesOnCreation) {
  FakeCLLocationManagerDelegate* delegate =
      [[FakeCLLocationManagerDelegate alloc] init];
  ASSERT_EQ(delegate.delegateCallbackCount, 0);

  CLLocationManager* manager = [[CLLocationManager alloc] init];
  manager.delegate = delegate;

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForFileOperationTimeout, ^bool() {
        return delegate.delegateCallbackCount > 0;
      }));
}

// Tests that GeolocationManager caches its value correctly and prefers to
// return recent authorization status values over the cached status.
TEST_F(GeolocationManagerTest, GeolocationManagerCache) {
  ASSERT_FALSE(authorization_status_cache_util::GetAuthorizationStatus());

  // Create GeolocationManager so that it will update the cached value.
  __unused GeolocationManager* manager = [GeolocationManager createForTesting];

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForFileOperationTimeout, ^bool() {
        return authorization_status_cache_util::GetAuthorizationStatus()
            .has_value();
      }));

  CLAuthorizationStatus currentStatus = manager.authorizationStatus;
  authorization_status_cache_util::ClearAuthorizationStatusForTesting();

  // Clearning cached value should not affect returned value becuase
  // GeolocationManager should prefer returning the vlaue returned from this run
  // instead of the value from the cache.
  EXPECT_EQ(manager.authorizationStatus, currentStatus);
}

}  // namespace
