// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/geolocation/system_geolocation_source_apple.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "net/base/mock_network_change_notifier.h"
#include "services/device/public/cpp/geolocation/location_manager_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

namespace device {

namespace {

class MockObserver : public device::SystemGeolocationSource::PositionObserver {
 public:
  MOCK_METHOD(void,
              OnPositionUpdated,
              (const device::mojom::Geoposition&),
              (override));
  MOCK_METHOD(void,
              OnPositionError,
              (const device::mojom::GeopositionError&),
              (override));
};

}  // namespace

class SystemGeolocationSourceAppleTest : public testing::Test {
 public:
  void SetUp() override {
    // Create `SystemGeolocationSourceApple` for testing.
    source_ = std::make_unique<device::SystemGeolocationSourceApple>();
    delegate_ = source_->GetDelegateForTesting();

    // Create mocks.
    mock_core_location_manager_ = OCMClassMock([CLLocationManager class]);
    source_->SetLocationManagerForTesting(mock_core_location_manager_);
    mock_network_notifier_ = net::test::MockNetworkChangeNotifier::Create();
    mock_position_observer_ =
        std::make_unique<::testing::StrictMock<MockObserver>>();
  }

  void TearDown() override { task_environment_.RunUntilIdle(); }

  // Simulate CoreLocationProvider::StartProvider call.
  void SimulateStartProvider() {
    source_->AddPositionUpdateObserver(mock_position_observer_.get());
    source_->StartWatchingPosition(/*high_accuracy*/ true);
    base::RunLoop().RunUntilIdle();
  }

  // Simulate CoreLocationProvider::StopProvider call.
  void SimulateStopProvider() {
    source_->RemovePositionUpdateObserver(mock_position_observer_.get());
    source_->StopWatchingPosition();
    base::RunLoop().RunUntilIdle();
  }

  // Simulate a NetworkChange event with specified connection type.
  void SetConnectionType(net::NetworkChangeNotifier::ConnectionType type) {
    mock_network_notifier_->SetConnectionType(type);
    mock_network_notifier_->NotifyObserversOfNetworkChangeForTests(
        mock_network_notifier_->GetConnectionType());
  }

  void SimulateDidFailWithError(CLError error_code) {
    NSError* ns_error = [NSError errorWithDomain:kCLErrorDomain
                                            code:error_code
                                        userInfo:nil];
    [delegate_ locationManager:nil didFailWithError:ns_error];
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;

  // Real objects for testing.
  std::unique_ptr<device::SystemGeolocationSourceApple> source_;
  LocationManagerDelegate* delegate_;

  // Mocks.
  std::unique_ptr<MockObserver> mock_position_observer_;
  CLLocationManager* mock_core_location_manager_;
  std::unique_ptr<net::test::MockNetworkChangeNotifier> mock_network_notifier_;
};

// This test verifies that when Wi-Fi is always on and `kCLErrorLocationUnknown`
// is received, the error is propagated directly without fallback.
TEST_F(SystemGeolocationSourceAppleTest, ErrorLocationUnknownWhenWifiAlwaysOn) {
  // Simulate the Wi-Fi is enabled all the time.
  SystemGeolocationSourceApple::SetWifiStatusForTesting(true);

  OCMExpect([mock_core_location_manager_ startUpdatingLocation]);
  SimulateStartProvider();
  EXPECT_OCMOCK_VERIFY((id)mock_core_location_manager_);

  SimulateDidFailWithError(kCLErrorLocationUnknown);

  // Expect that non-fallback-able error code `kPositionUnavailable` is
  // notified.
  base::test::TestFuture<const device::mojom::GeopositionError&> error_future;
  EXPECT_CALL(*mock_position_observer_, OnPositionError)
      .WillOnce(base::test::InvokeFuture(error_future));
  // Wait for position observers to be notified.
  EXPECT_EQ(error_future.Get().error_code,
            device::mojom::GeopositionErrorCode::kPositionUnavailable);
}

// This test verifies that when Wi-Fi is disabled initially, the fallback
// mechanism is initiated directly.
TEST_F(SystemGeolocationSourceAppleTest, FallbackWhenWifiDisabledInitially) {
  // Simulate the Wi-Fi is disabled before starting position watching.
  SystemGeolocationSourceApple::SetWifiStatusForTesting(false);

  OCMExpect([mock_core_location_manager_ startUpdatingLocation]);
  SimulateStartProvider();
  EXPECT_OCMOCK_VERIFY((id)mock_core_location_manager_);

  SimulateDidFailWithError(kCLErrorLocationUnknown);

  // Expect that fallback-able error code `kWifiDisabled` is notified.
  base::test::TestFuture<const device::mojom::GeopositionError&> error_future;
  EXPECT_CALL(*mock_position_observer_, OnPositionError)
      .WillOnce(base::test::InvokeFuture(error_future));
  // Wait for position observers to be notified.
  EXPECT_EQ(error_future.Get().error_code,
            device::mojom::GeopositionErrorCode::kWifiDisabled);
}

// This test verifies that when Wi-Fi is enabled and then disabled, the fallback
// mechanism is triggered by a network change event.
TEST_F(SystemGeolocationSourceAppleTest, FallbackTriggeredByNetworkChanged) {
  // Simulate the Wi-Fi is enabled before start watching.
  SystemGeolocationSourceApple::SetWifiStatusForTesting(true);

  OCMExpect([mock_core_location_manager_ startUpdatingLocation]);
  SimulateStartProvider();
  EXPECT_OCMOCK_VERIFY((id)mock_core_location_manager_);

  // Simulate the Wi-Fi is disabled during position watching.
  SystemGeolocationSourceApple::SetWifiStatusForTesting(false);

  SimulateDidFailWithError(kCLErrorLocationUnknown);

  // Expect that fallback-able error code `kWifiDisabled` is notified.
  base::test::TestFuture<const device::mojom::GeopositionError&> error_future;
  EXPECT_CALL(*mock_position_observer_, OnPositionError)
      .WillOnce(base::test::InvokeFuture(error_future));

  // Simulate a network change event sequence: first with `CONNECTION_NONE` to
  // represent Wi-Fi being disabled, then with `CONNECTION_UNKNOWN` to indicate
  // a settled network state.
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_NONE);
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_UNKNOWN);

  // Wait for position observers to be notified.
  EXPECT_EQ(error_future.Get().error_code,
            device::mojom::GeopositionErrorCode::kWifiDisabled);
}

// This test verifies that if the fallback mechanism times out while waiting
// for a network change event, a `kPositionUnavailable` error is propagated.
TEST_F(SystemGeolocationSourceAppleTest, OnNetworkChangedTimeout) {
  // Simulate the Wi-Fi is enabled before start watching.
  SystemGeolocationSourceApple::SetWifiStatusForTesting(true);

  OCMExpect([mock_core_location_manager_ startUpdatingLocation]);
  SimulateStartProvider();
  EXPECT_OCMOCK_VERIFY((id)mock_core_location_manager_);

  // Simulate the Wi-Fi is disabled during position watching.
  SystemGeolocationSourceApple::SetWifiStatusForTesting(false);

  SimulateDidFailWithError(kCLErrorLocationUnknown);

  // Simulate a single OnNetworkChanged event with `CONNECTION_NONE` to mimic a
  // network change without a subsequent settled state.
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_NONE);

  // This waits for the network change event timer to expire.
  // `kNetworkChangeTimeoutMs` is currently set to 1000 milliseconds, so we wait
  // for 1200 milliseconds to ensure the timer has definitely expired.
  base::PlatformThreadBase::Sleep(
      base::Milliseconds(source_->kNetworkChangeTimeoutMs + 200));

  // Expect that `OnPositionError` is called with `kPositionUnavailable` because
  // the network event timer timeouted.
  base::test::TestFuture<const device::mojom::GeopositionError&> error_future;
  EXPECT_CALL(*mock_position_observer_, OnPositionError)
      .WillOnce(base::test::InvokeFuture(error_future));

  // Wait for timeout handler to be finished.
  EXPECT_EQ(error_future.Get().error_code,
            device::mojom::GeopositionErrorCode::kPositionUnavailable);

  // Simulate the second OnNetworkChanged event with `CONNECTION_UNKNOWN` to
  // mimic a network change settle, but this should be no-op since the timeout
  // handler has been invoked.
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_UNKNOWN);

  // Wait to ensure position observers is not notified by a OnNetworkChanged
  // event arrived after timeout.
  base::RunLoop().RunUntilIdle();
}

// This test verifies that if the fallback timer is canceled by
// `StopWatchingPosition` call while waiting for a network change event.
TEST_F(SystemGeolocationSourceAppleTest, FallbackCanceledByStopWatching) {
  // Simulate the Wi-Fi is enabled before start watching.
  SystemGeolocationSourceApple::SetWifiStatusForTesting(true);

  OCMExpect([mock_core_location_manager_ startUpdatingLocation]);
  SimulateStartProvider();
  EXPECT_OCMOCK_VERIFY((id)mock_core_location_manager_);

  // Simulate the Wi-Fi is disabled during position watching.
  SystemGeolocationSourceApple::SetWifiStatusForTesting(false);

  SimulateDidFailWithError(kCLErrorLocationUnknown);

  // Simulate first OnNetworkChanged event with `CONNECTION_NONE` when Wi-Fi is
  // just disabled.
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_NONE);

  SimulateStopProvider();

  // Simulate the second OnNetworkChanged event with `CONNECTION_UNKNOWN` to
  // mimic a network change settle, but this should be no-op since watching
  // position has been stopped.
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_UNKNOWN);

  // Expect that `OnPositionError` is not called because location provider is
  // already stopped.
  EXPECT_CALL(*mock_position_observer_, OnPositionError).Times(0);

  // Wait to ensure that `OnPosisionError` is not invoked because provider has
  // been stopped.
  base::RunLoop().RunUntilIdle();
}

// This test verifies that if the fallback is canceled by `didFailWithError`
// call while waiting for a network change event.
TEST_F(SystemGeolocationSourceAppleTest, FallbackCanceledByDidFailWithError) {
  // Simulate the Wi-Fi is enabled before start watching.
  SystemGeolocationSourceApple::SetWifiStatusForTesting(true);

  OCMExpect([mock_core_location_manager_ startUpdatingLocation]);
  SimulateStartProvider();
  EXPECT_OCMOCK_VERIFY((id)mock_core_location_manager_);

  // Simulate the Wi-Fi is disabled during position watching.
  SystemGeolocationSourceApple::SetWifiStatusForTesting(false);

  // Simulate first NSError wihch is kCLErrorLocationUnknown + Wi-Fi disabled so
  // it can be fallbacked.
  SimulateDidFailWithError(kCLErrorLocationUnknown);

  // Simulate first OnNetworkChanged event with `CONNECTION_NONE` when Wi-Fi is
  // just disabled.
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_NONE);

  // Simulate second NSError wihch is critical and can not be fallbacked.
  SimulateDidFailWithError(kCLErrorDenied);

  // Simulate second OnNetworkChanged event with `CONNECTION_UNKNOWN` when
  // network change settle, but this should be no-op since the timer has been
  // canceld.
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_UNKNOWN);

  // Expected that `OnPositionError` is only called once with `kPermissionDenie`
  // instead of `kWifiDisabled`.
  base::test::TestFuture<const device::mojom::GeopositionError&> error_future;
  EXPECT_CALL(*mock_position_observer_, OnPositionError)
      .WillOnce(base::test::InvokeFuture(error_future));
  // Wait for position observers to be notified.
  EXPECT_EQ(error_future.Get().error_code,
            device::mojom::GeopositionErrorCode::kPermissionDenied);
}

// This test verifies that if the fallback error code is only sent once.
TEST_F(SystemGeolocationSourceAppleTest, FallbackTriggeredOnlyOnce) {
  // Simulate the Wi-Fi is enabled before start watching.
  SystemGeolocationSourceApple::SetWifiStatusForTesting(true);

  OCMExpect([mock_core_location_manager_ startUpdatingLocation]);
  SimulateStartProvider();
  EXPECT_OCMOCK_VERIFY((id)mock_core_location_manager_);

  // Simulate the Wi-Fi is disabled during position watching.
  SystemGeolocationSourceApple::SetWifiStatusForTesting(false);

  SimulateDidFailWithError(kCLErrorLocationUnknown);

  // Simulate first OnNetworkChanged event with `CONNECTION_NONE` when Wi-Fi is
  // just disabled.
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_NONE);

  // Simulate second NSError with same `kCLErrorLocationUnknown` error code.
  SimulateDidFailWithError(kCLErrorLocationUnknown);

  // Simulate second OnNetworkChanged event with `CONNECTION_UNKNOWN` when
  // network change settle, but this should be no-op since the timer has been
  // canceld.
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_UNKNOWN);

  // Expected that `OnPositionError` is only called once with `kWifiDisabled`
  // even with two `didFailWithError` called.
  base::test::TestFuture<const device::mojom::GeopositionError&> error_future;
  EXPECT_CALL(*mock_position_observer_, OnPositionError)
      .WillOnce(base::test::InvokeFuture(error_future));

  // Wait for position observers to be notified.
  EXPECT_EQ(error_future.Get().error_code,
            device::mojom::GeopositionErrorCode::kWifiDisabled);
}

}  // namespace device
