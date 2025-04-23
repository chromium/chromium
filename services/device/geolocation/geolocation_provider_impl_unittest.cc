// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/geolocation_provider_impl.h"

#include <memory>
#include <string>

#include "base/at_exit.h"
#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "services/device/geolocation/fake_location_provider.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/public/cpp/geolocation/location_system_permission_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
#include "services/device/public/cpp/test/fake_geolocation_system_permission_manager.h"
#endif

namespace device {
namespace {

using ::base::test::TestFuture;
using ::device::LocationSystemPermissionStatus;

}  // namespace

class GeolocationProviderTest : public testing::Test {
 public:
  void SetUp() override {
#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
    if (features::IsOsLevelGeolocationPermissionSupportEnabled()) {
      fake_geolocation_system_permission_manager_ =
          std::make_unique<FakeGeolocationSystemPermissionManager>();
      GeolocationProviderImpl::SetGeolocationSystemPermissionManagerForTesting(
          static_cast<GeolocationSystemPermissionManager*>(
              fake_geolocation_system_permission_manager_.get()));
    }
#endif
  }

 protected:
  GeolocationProviderTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI) {
    mojom::Geoposition& position1 = *position_result1_->get_position();
    position1.latitude = 12;
    position1.longitude = 34;
    position1.accuracy = 56;
    position1.timestamp = base::Time::Now();

    mojom::Geoposition& position2 = *position_result2_->get_position();
    position2.latitude = 13;
    position2.longitude = 34;
    position2.accuracy = 56;
    position2.timestamp = base::Time::Now();

    feature_list_.InitWithFeatures(/*enabled_features=*/
                                   {
#if BUILDFLAG(IS_WIN)
                                       features::kWinSystemLocationPermission,
#endif  // BUILDFLAG(IS_WIN)
                                   },
                                   /*disabled_features=*/{});
  }

  GeolocationProviderTest(const GeolocationProviderTest&) = delete;
  GeolocationProviderTest& operator=(const GeolocationProviderTest&) = delete;

  ~GeolocationProviderTest() override = default;

  GeolocationProviderImpl* provider() {
    return GeolocationProviderImpl::GetInstance();
  }

  FakeLocationProvider* location_provider_manager() {
    return location_provider_manager_;
  }

  void SetSystemPermission(LocationSystemPermissionStatus status) {
#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
    fake_geolocation_system_permission_manager_->SetSystemPermission(status);
    RunUntilIdle();
#endif
  }
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  // Called on test thread.
  void SetFakeLocationProviderManager();
  bool ProvidersStarted();
  void SendMockLocation(const mojom::GeopositionResult& result);

  device::mojom::GeopositionResultPtr position_result1_ =
      mojom::GeopositionResult::NewPosition(mojom::Geoposition::New());

  device::mojom::GeopositionResultPtr position_result2_ =
      mojom::GeopositionResult::NewPosition(mojom::Geoposition::New());

  device::mojom::GeopositionResultPtr error_result_ =
      mojom::GeopositionResult::NewError(mojom::GeopositionError::New(
          mojom::GeopositionErrorCode::kPermissionDenied,
          GeolocationProviderImpl::kSystemPermissionDeniedErrorMessage,
          GeolocationProviderImpl::kSystemPermissionDeniedErrorTechnical));

 private:
  // Called on provider thread.
  bool GetProvidersStarted();

#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
  std::unique_ptr<FakeGeolocationSystemPermissionManager>
      fake_geolocation_system_permission_manager_;
#endif
  // |at_exit| must be initialized before all other variables so that it is
  // available to register with Singletons and can handle tear down when the
  // test completes.
  base::ShadowingAtExitManager at_exit_;

  base::test::SingleThreadTaskEnvironment task_environment_;

  base::ThreadChecker thread_checker_;

  // Owned by the GeolocationProviderImpl class.
  raw_ptr<FakeLocationProvider> location_provider_manager_ = nullptr;

  // True if |location_provider_manager_| is started.
  bool is_started_;

  base::test::ScopedFeatureList feature_list_;
};

void GeolocationProviderTest::SetFakeLocationProviderManager() {
  ASSERT_FALSE(location_provider_manager_);
  auto location_provider_manager = std::make_unique<FakeLocationProvider>();
  location_provider_manager_ = location_provider_manager.get();
  provider()->SetLocationProviderManagerForTesting(
      std::move(location_provider_manager));
}

bool GeolocationProviderTest::ProvidersStarted() {
  DCHECK(provider()->IsRunning());
  DCHECK(thread_checker_.CalledOnValidThread());

  TestFuture<bool> future;
  provider()->task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GeolocationProviderTest::GetProvidersStarted,
                     base::Unretained(this)),
      future.GetCallback());
  return future.Get();
}

bool GeolocationProviderTest::GetProvidersStarted() {
  DCHECK(provider()->task_runner()->BelongsToCurrentThread());
  is_started_ = location_provider_manager()->state() !=
                mojom::GeolocationDiagnostics::ProviderState::kStopped;
  return is_started_;
}

void GeolocationProviderTest::SendMockLocation(
    const mojom::GeopositionResult& result) {
  DCHECK(provider()->IsRunning());
  DCHECK(thread_checker_.CalledOnValidThread());
  provider()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&GeolocationProviderImpl::OnLocationUpdate,
                                base::Unretained(provider()),
                                location_provider_manager_, result.Clone()));
}

// Regression test for http://crbug.com/59377
TEST_F(GeolocationProviderTest, OnPermissionGrantedWithoutObservers) {
  EXPECT_FALSE(provider()->user_did_opt_into_location_services_for_testing());
  provider()->UserDidOptIntoLocationServices();
  EXPECT_TRUE(provider()->user_did_opt_into_location_services_for_testing());
  provider()->clear_user_did_opt_into_location_services_for_testing();
}

TEST_F(GeolocationProviderTest, StartStop) {
  SetFakeLocationProviderManager();
  EXPECT_FALSE(provider()->IsRunning());
  SetSystemPermission(LocationSystemPermissionStatus::kAllowed);
  base::CallbackListSubscription subscription =
      provider()->AddLocationUpdateCallback(base::DoNothing(),
                                            /*enable_high_accuracy=*/false);
  EXPECT_TRUE(provider()->IsRunning());
  EXPECT_TRUE(ProvidersStarted());

  subscription = {};

  EXPECT_FALSE(ProvidersStarted());
  EXPECT_TRUE(provider()->IsRunning());
}

TEST_F(GeolocationProviderTest, StalePositionNotSent) {
  SetFakeLocationProviderManager();
  SetSystemPermission(LocationSystemPermissionStatus::kAllowed);

  {
    TestFuture<mojom::GeopositionResultPtr> future1;

    base::MockCallback<GeolocationProviderImpl::LocationUpdateCallback>
        mock_callback1;
    EXPECT_CALL(mock_callback1, Run)
        .WillOnce([&](const mojom::GeopositionResult& result) {
          future1.SetValue(result.Clone());
        });

    base::CallbackListSubscription subscription =
        provider()->AddLocationUpdateCallback(mock_callback1.Get(), false);
    SendMockLocation(*position_result1_);
    EXPECT_EQ(future1.Get()->get_position(), position_result1_->get_position());
    subscription = {};
  }

  {
    base::MockCallback<GeolocationProviderImpl::LocationUpdateCallback>
        mock_callback2;

    // After adding a second callback, check that no unexpected position update
    // is sent.
    EXPECT_CALL(mock_callback2, Run).Times(0);
    base::CallbackListSubscription subscription2 =
        provider()->AddLocationUpdateCallback(mock_callback2.Get(), false);
    base::RunLoop().RunUntilIdle();

    // The second callback should receive the new position now.
    TestFuture<mojom::GeopositionResultPtr> future2;
    EXPECT_CALL(mock_callback2, Run)
        .WillOnce([&](const mojom::GeopositionResult& result) {
          future2.SetValue(result.Clone());
        });
    SendMockLocation(*position_result2_);
    EXPECT_EQ(future2.Get()->get_position(), position_result2_->get_position());
    subscription2 = {};
  }

  EXPECT_FALSE(ProvidersStarted());
}

TEST_F(GeolocationProviderTest, OverrideLocationForTesting) {
  SetFakeLocationProviderManager();
  SetSystemPermission(LocationSystemPermissionStatus::kAllowed);

  provider()->OverrideLocationForTesting(error_result_->Clone());
  // Adding a callback when the location is overridden should synchronously
  // invoke the callback with our overridden position.
  base::MockCallback<GeolocationProviderImpl::LocationUpdateCallback>
      mock_callback;
  EXPECT_CALL(mock_callback, Run);
  base::CallbackListSubscription subscription =
      provider()->AddLocationUpdateCallback(mock_callback.Get(), false);
  subscription = {};
  // Wait for the providers to be stopped now that all clients are gone.
  EXPECT_FALSE(ProvidersStarted());
}

namespace {

class MockGeolocationInternalsObserver
    : public mojom::GeolocationInternalsObserver {
 public:
  MOCK_METHOD(void,
              OnDiagnosticsChanged,
              (mojom::GeolocationDiagnosticsPtr),
              (override));
  void OnNetworkLocationRequested(
      std::vector<mojom::AccessPointDataPtr> request) override {}
  void OnNetworkLocationReceived(
      mojom::NetworkLocationResponsePtr response) override {}

  void Bind(
      mojo::PendingReceiver<mojom::GeolocationInternalsObserver> receiver) {
    receiver_.Bind(std::move(receiver));
  }
  void Disconnect() { receiver_.reset(); }

 private:
  mojo::Receiver<mojom::GeolocationInternalsObserver> receiver_{this};
};

}  // namespace

TEST_F(GeolocationProviderTest, InitializeWhileObservingDiagnostics) {
  // Add the observer and wait for the initial update.
  MockGeolocationInternalsObserver observer;
  mojo::PendingRemote<mojom::GeolocationInternalsObserver> remote;
  observer.Bind(remote.InitWithNewPipeAndPassReceiver());
  TestFuture<mojom::GeolocationDiagnosticsPtr> add_observer_future;
  provider()->AddInternalsObserver(std::move(remote),
                                   add_observer_future.GetCallback());
  // AddInternalsObserver invokes the callback with nullptr if the API
  // implementation is not yet initialized.
  EXPECT_FALSE(add_observer_future.Get());
  EXPECT_FALSE(provider()->IsRunning());

  // Add a subscription so the provider will be initialized and started.
  TestFuture<mojom::GeolocationDiagnosticsPtr> provider_started_future;
  EXPECT_CALL(observer, OnDiagnosticsChanged).WillOnce([&](auto diagnostics) {
    provider_started_future.SetValue(std::move(diagnostics));
  });
  SetFakeLocationProviderManager();
  SetSystemPermission(LocationSystemPermissionStatus::kAllowed);
  base::CallbackListSubscription subscription =
      provider()->AddLocationUpdateCallback(base::DoNothing(),
                                            /*enable_high_accuracy=*/false);
  EXPECT_TRUE(ProvidersStarted());

  // Starting the provider updates diagnostics.
  ASSERT_TRUE(provider_started_future.Get());
  EXPECT_EQ(provider_started_future.Get()->provider_state,
            mojom::GeolocationDiagnostics::ProviderState::kLowAccuracy);

  // Calling OnInternalsUpdated updates diagnostics.
  TestFuture<mojom::GeolocationDiagnosticsPtr> internals_updated_future;
  EXPECT_CALL(observer, OnDiagnosticsChanged).WillOnce([&](auto diagnostics) {
    internals_updated_future.SetValue(std::move(diagnostics));
  });
  provider()->SimulateInternalsUpdatedForTesting();
  ASSERT_TRUE(internals_updated_future.Get());
  EXPECT_EQ(internals_updated_future.Get()->provider_state,
            mojom::GeolocationDiagnostics::ProviderState::kLowAccuracy);

  // Stopping the provider updates diagnostics.
  TestFuture<mojom::GeolocationDiagnosticsPtr> provider_stopped_future;
  EXPECT_CALL(observer, OnDiagnosticsChanged).WillOnce([&](auto diagnostics) {
    provider_stopped_future.SetValue(std::move(diagnostics));
  });
  subscription = {};
  EXPECT_FALSE(ProvidersStarted());
  ASSERT_TRUE(provider_stopped_future.Get());
  EXPECT_EQ(provider_stopped_future.Get()->provider_state,
            mojom::GeolocationDiagnostics::ProviderState::kStopped);
}

TEST_F(GeolocationProviderTest, MultipleDiagnosticsObservers) {
  // Add a subscription so the provider will be started.
  SetFakeLocationProviderManager();
  SetSystemPermission(LocationSystemPermissionStatus::kAllowed);
  base::CallbackListSubscription subscription =
      provider()->AddLocationUpdateCallback(base::DoNothing(),
                                            /*enable_high_accuracy=*/true);
  EXPECT_TRUE(ProvidersStarted());

  // Add two internals observers. Both receive diagnostics indicating the
  // provider is started.
  MockGeolocationInternalsObserver observer1;
  {
    mojo::PendingRemote<mojom::GeolocationInternalsObserver> remote;
    observer1.Bind(remote.InitWithNewPipeAndPassReceiver());
    TestFuture<mojom::GeolocationDiagnosticsPtr> future;
    provider()->AddInternalsObserver(std::move(remote), future.GetCallback());
    ASSERT_TRUE(future.Get());
    EXPECT_EQ(future.Get()->provider_state,
              mojom::GeolocationDiagnostics::ProviderState::kHighAccuracy);
  }
  MockGeolocationInternalsObserver observer2;
  {
    mojo::PendingRemote<mojom::GeolocationInternalsObserver> remote;
    observer2.Bind(remote.InitWithNewPipeAndPassReceiver());
    TestFuture<mojom::GeolocationDiagnosticsPtr> future;
    provider()->AddInternalsObserver(std::move(remote), future.GetCallback());
    ASSERT_TRUE(future.Get());
    EXPECT_EQ(future.Get()->provider_state,
              mojom::GeolocationDiagnostics::ProviderState::kHighAccuracy);
  }

  // Call OnInternalsUpdated. Both observers are notified.
  {
    base::RunLoop loop;
    auto barrier_closure = base::BarrierClosure(2, loop.QuitClosure());
    mojom::GeolocationDiagnosticsPtr diagnostics1;
    EXPECT_CALL(observer1, OnDiagnosticsChanged)
        .WillOnce([&](auto diagnostics) {
          diagnostics1 = std::move(diagnostics);
          barrier_closure.Run();
        });
    mojom::GeolocationDiagnosticsPtr diagnostics2;
    EXPECT_CALL(observer2, OnDiagnosticsChanged)
        .WillOnce([&](auto diagnostics) {
          diagnostics2 = std::move(diagnostics);
          barrier_closure.Run();
        });
    provider()->SimulateInternalsUpdatedForTesting();
    loop.Run();
    ASSERT_TRUE(diagnostics1);
    EXPECT_EQ(diagnostics1->provider_state,
              mojom::GeolocationDiagnostics::ProviderState::kHighAccuracy);
    ASSERT_TRUE(diagnostics2);
    EXPECT_EQ(diagnostics2->provider_state,
              mojom::GeolocationDiagnostics::ProviderState::kHighAccuracy);
  }

  // Disconnect observer1.
  observer1.Disconnect();

  // Call OnInternalsUpdated. Only observer2 is notified.
  {
    TestFuture<mojom::GeolocationDiagnosticsPtr> future;
    EXPECT_CALL(observer2, OnDiagnosticsChanged)
        .WillOnce(
            [&](auto diagnostics) { future.SetValue(std::move(diagnostics)); });
    provider()->SimulateInternalsUpdatedForTesting();
    ASSERT_TRUE(future.Get());
    EXPECT_EQ(future.Get()->provider_state,
              mojom::GeolocationDiagnostics::ProviderState::kHighAccuracy);
  }
}

#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
TEST_F(GeolocationProviderTest, StartProviderAfterSystemPermissionGranted) {
  SetFakeLocationProviderManager();

  // The default system permission state is kUndetermined. Adding a location
  // callback should not start provider and the callback should not be
  // called.
  base::MockCallback<GeolocationProviderImpl::LocationUpdateCallback>
      mock_callback;
  EXPECT_CALL(mock_callback, Run).Times(0);
  base::CallbackListSubscription subscription =
      provider()->AddLocationUpdateCallback(mock_callback.Get(),
                                            /*enable_high_accuracy=*/true);

  // Verify that the provider hasn't started yet due to permission is not
  // granted.
  EXPECT_FALSE(ProvidersStarted());

  // Simulate system permission being granted. Provider should now be active.
  SetSystemPermission(LocationSystemPermissionStatus::kAllowed);
  EXPECT_TRUE(ProvidersStarted());

  TestFuture<mojom::GeopositionResultPtr> future;
  EXPECT_CALL(mock_callback, Run)
      .WillOnce([&](const mojom::GeopositionResult& result) {
        future.SetValue(result.Clone());
      });

  // Trigger a location update with the sample data.
  SendMockLocation(*position_result1_);

  // Verify that the mock observer received the correct update.
  EXPECT_EQ(future.Get()->get_position(), position_result1_->get_position());

  subscription = {};
  EXPECT_FALSE(ProvidersStarted());
}

TEST_F(GeolocationProviderTest, AddCallbackWhenSystemPermissionDenied) {
  SetFakeLocationProviderManager();

  // Set system permission state from kUndetermined to kDenied.
  SetSystemPermission(LocationSystemPermissionStatus::kDenied);

  base::MockCallback<GeolocationProviderImpl::LocationUpdateCallback>
      mock_callback;
  TestFuture<mojom::GeopositionResultPtr> future;

  // Expect that the callback should be invoked with permission denied error
  // when subscription is created.
  EXPECT_CALL(mock_callback, Run)
      .WillOnce([&](const mojom::GeopositionResult& result) {
        future.SetValue(result.Clone());
      });

  base::CallbackListSubscription subscription =
      provider()->AddLocationUpdateCallback(mock_callback.Get(),
                                            /*enable_high_accuracy=*/true);

  // Verify that callback should be invoked with permission denied error and
  // provider is not started.
  EXPECT_EQ(future.Take()->get_error(), error_result_->get_error());
  EXPECT_FALSE(ProvidersStarted());
}

TEST_F(GeolocationProviderTest, AddCallbackOnPermissionGrantAfterDenied) {
  SetFakeLocationProviderManager();

  // Initially set system permission to denied, then grant it.
  SetSystemPermission(LocationSystemPermissionStatus::kDenied);
  SetSystemPermission(LocationSystemPermissionStatus::kAllowed);

  base::MockCallback<GeolocationProviderImpl::LocationUpdateCallback>
      mock_callback;

  // Expect that the observer should NOT be notified with permission denied
  // error because the cached |result_| should has been cleared when system
  // permission state changed from kDenied to kAllowed.
  EXPECT_CALL(mock_callback, Run).Times(0);
  base::CallbackListSubscription subscription =
      provider()->AddLocationUpdateCallback(mock_callback.Get(),
                                            /*enable_high_accuracy=*/true);
  base::RunLoop().RunUntilIdle();

  // Expect that the observer is notified now when location update is simulated.
  // TestFuture<mojom::GeopositionResultPtr> future;
  TestFuture<mojom::GeopositionResultPtr> future;
  EXPECT_CALL(mock_callback, Run)
      .WillOnce([&](const mojom::GeopositionResult& result) {
        future.SetValue(result.Clone());
      });

  //  Now simulate a location update and expect that position result to be
  //  equal.
  SendMockLocation(*position_result1_);
  EXPECT_EQ(future.Get()->get_position(), position_result1_->get_position());
}

TEST_F(GeolocationProviderTest,
       ReportPermissionDeniedOnSystemPermissionDenied) {
  SetFakeLocationProviderManager();

  // Set system permission state from kUndetermined to kAllowed.
  SetSystemPermission(LocationSystemPermissionStatus::kAllowed);

  base::MockCallback<GeolocationProviderImpl::LocationUpdateCallback>
      mock_callback;
  base::CallbackListSubscription subscription =
      provider()->AddLocationUpdateCallback(mock_callback.Get(),
                                            /*enable_high_accuracy=*/true);

  // Verify that provider is started when subscription is created when system
  // permission is granted.
  EXPECT_TRUE(ProvidersStarted());

  TestFuture<mojom::GeopositionResultPtr> position_future;
  EXPECT_CALL(mock_callback, Run)
      .WillOnce([&](const mojom::GeopositionResult& result) {
        position_future.SetValue(result.Clone());
      });

  // Simulate a location update and expect that position result to be equal.
  SendMockLocation(*position_result1_);
  EXPECT_EQ(position_future.Get()->get_position(),
            position_result1_->get_position());

  TestFuture<mojom::GeopositionResultPtr> error_future;

  // Set system permission state from kAllowed to kDenied. Expect that callback
  // is invoked with permission denied error.
  EXPECT_CALL(mock_callback, Run)
      .WillOnce([&](const mojom::GeopositionResult& result) {
        error_future.SetValue(result.Clone());
      });
  SetSystemPermission(LocationSystemPermissionStatus::kDenied);
  EXPECT_EQ(error_future.Get()->get_error(), error_result_->get_error());

  // Clear subscription and expect that provider is stopped.
  subscription = {};
  EXPECT_FALSE(ProvidersStarted());
}

TEST_F(GeolocationProviderTest,
       SystemPermissionAllowedAfterSystemPermissionDenied) {
  SetFakeLocationProviderManager();

  // Set system permission state from kUndetermined to kDenied.
  SetSystemPermission(LocationSystemPermissionStatus::kDenied);

  TestFuture<mojom::GeopositionResultPtr> error_future;

  // Create 1st callback and expected to be invoked with permission
  // denied error when system permission is denied.
  base::MockCallback<GeolocationProviderImpl::LocationUpdateCallback>
      mock_callback1;
  EXPECT_CALL(mock_callback1, Run)
      .WillOnce([&](const mojom::GeopositionResult& result) {
        error_future.SetValue(result.Clone());
      });

  base::CallbackListSubscription subscription1 =
      provider()->AddLocationUpdateCallback(mock_callback1.Get(),
                                            /*enable_high_accuracy=*/true);
  EXPECT_EQ(error_future.Get()->get_error(), error_result_->get_error());
  subscription1 = {};
  EXPECT_FALSE(ProvidersStarted());

  // Set system permission state from kDenied to kAllowed.
  SetSystemPermission(LocationSystemPermissionStatus::kAllowed);

  // Created 2nd callback and subscription after system permission is set to
  // kAllowed.
  base::MockCallback<GeolocationProviderImpl::LocationUpdateCallback>
      mock_callback2;
  base::CallbackListSubscription subscription2 =
      provider()->AddLocationUpdateCallback(mock_callback2.Get(),
                                            /*enable_high_accuracy=*/true);

  // Verify that provider is started when subscription2 is created when system
  // permission is granted.
  EXPECT_TRUE(ProvidersStarted());

  TestFuture<mojom::GeopositionResultPtr> position_future;
  EXPECT_CALL(mock_callback2, Run)
      .WillOnce([&](const mojom::GeopositionResult& result) {
        position_future.SetValue(result.Clone());
      });

  // Simulate a location update and expect that position result to be equal.
  SendMockLocation(*position_result1_);
  EXPECT_EQ(position_future.Get()->get_position(),
            position_result1_->get_position());

  subscription2 = {};
  EXPECT_FALSE(ProvidersStarted());
}
#endif  // BUILDFLAG(IS_APPLE) ||
        // BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)

}  // namespace device
