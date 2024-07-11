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
using ::testing::MakeMatcher;
using ::testing::Matcher;
using ::testing::MatcherInterface;
using ::testing::MatchResultListener;

std::string kSystemPermissoinDeniedErrorMessage =
    GeolocationProviderImpl::kSystemPermissionDeniedErrorMessage;

class GeolocationObserver {
 public:
  virtual ~GeolocationObserver() = default;
  virtual void OnLocationUpdate(const mojom::GeopositionResult& result) = 0;
};

class MockGeolocationObserver : public GeolocationObserver {
 public:
  MOCK_METHOD1(OnLocationUpdate, void(const mojom::GeopositionResult& result));
};

class AsyncMockGeolocationObserver : public MockGeolocationObserver {
 public:
  void OnLocationUpdate(const mojom::GeopositionResult& result) override {
    MockGeolocationObserver::OnLocationUpdate(result);
  }
};

class MockGeolocationCallbackWrapper {
 public:
  MOCK_METHOD1(Callback, void(const mojom::GeopositionResult& result));
};

class GeopositionResultEqMatcher
    : public MatcherInterface<const mojom::GeopositionResult&> {
 public:
  explicit GeopositionResultEqMatcher(mojom::GeopositionResultPtr expected)
      : expected_(std::move(expected)) {}

  GeopositionResultEqMatcher(const GeopositionResultEqMatcher&) = delete;
  GeopositionResultEqMatcher& operator=(const GeopositionResultEqMatcher&) =
      delete;

  bool MatchAndExplain(const mojom::GeopositionResult& actual,
                       MatchResultListener* listener) const override {
    return expected_->Equals(actual);
  }

  void DescribeTo(::std::ostream* os) const override {
    *os << "which matches the expected position";
  }

  void DescribeNegationTo(::std::ostream* os) const override {
    *os << "which does not match the expected position";
  }

 private:
  mojom::GeopositionResultPtr expected_;
};

Matcher<const mojom::GeopositionResult&> GeopositionResultEq(
    const mojom::GeopositionResult& expected) {
  return MakeMatcher(new GeopositionResultEqMatcher(expected.Clone()));
}

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
          kSystemPermissoinDeniedErrorMessage,
          ""));

 private:
  // Called on provider thread.
  void GetProvidersStarted();

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

  base::RunLoop run_loop;
  provider()->task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&GeolocationProviderTest::GetProvidersStarted,
                     base::Unretained(this)),
      run_loop.QuitClosure());
  run_loop.Run();
  return is_started_;
}

void GeolocationProviderTest::GetProvidersStarted() {
  DCHECK(provider()->task_runner()->BelongsToCurrentThread());
  is_started_ = location_provider_manager()->state() !=
                mojom::GeolocationDiagnostics::ProviderState::kStopped;
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
    base::RunLoop run_loop;

    AsyncMockGeolocationObserver first_observer;
    GeolocationProviderImpl::LocationUpdateCallback first_callback =
        base::BindRepeating(&MockGeolocationObserver::OnLocationUpdate,
                            base::Unretained(&first_observer));
    EXPECT_CALL(first_observer,
                OnLocationUpdate(GeopositionResultEq(*position_result1_)))
        .WillOnce([&run_loop]() { run_loop.Quit(); });
    base::CallbackListSubscription subscription =
        provider()->AddLocationUpdateCallback(first_callback, false);
    SendMockLocation(*position_result1_);
    run_loop.Run();
    subscription = {};
  }

  {
    base::RunLoop run_loop;
    AsyncMockGeolocationObserver second_observer;
    // After adding a second observer, check that no unexpected position update
    // is sent.
    EXPECT_CALL(second_observer, OnLocationUpdate(testing::_)).Times(0);
    GeolocationProviderImpl::LocationUpdateCallback second_callback =
        base::BindRepeating(&MockGeolocationObserver::OnLocationUpdate,
                            base::Unretained(&second_observer));
    base::CallbackListSubscription subscription2 =
        provider()->AddLocationUpdateCallback(second_callback, false);
    run_loop.RunUntilIdle();

    // The second observer should receive the new position now.
    EXPECT_CALL(second_observer,
                OnLocationUpdate(GeopositionResultEq(*position_result2_)))
        .WillOnce([&run_loop]() { run_loop.Quit(); });
    SendMockLocation(*position_result2_);
    run_loop.Run();
    subscription2 = {};
  }

  EXPECT_FALSE(ProvidersStarted());
}

TEST_F(GeolocationProviderTest, OverrideLocationForTesting) {
  SetFakeLocationProviderManager();
  SetSystemPermission(LocationSystemPermissionStatus::kAllowed);

  provider()->OverrideLocationForTesting(error_result_->Clone());
  // Adding an observer when the location is overridden should synchronously
  // update the observer with our overridden position.
  MockGeolocationObserver mock_observer;
  EXPECT_CALL(mock_observer,
              OnLocationUpdate(GeopositionResultEq(*error_result_)));
  GeolocationProviderImpl::LocationUpdateCallback callback =
      base::BindRepeating(&MockGeolocationObserver::OnLocationUpdate,
                          base::Unretained(&mock_observer));
  base::CallbackListSubscription subscription =
      provider()->AddLocationUpdateCallback(callback, false);
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

TEST_F(GeolocationProviderTest, DiagnosticsObserverDisabled) {
  // Disable the diagnostics observer feature.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kGeolocationDiagnosticsObserver});
  base::RunLoop loop;
  SetFakeLocationProviderManager();
  SetSystemPermission(LocationSystemPermissionStatus::kAllowed);

  // Add a subscription so the provider will be started.
  base::CallbackListSubscription subscription =
      provider()->AddLocationUpdateCallback(base::DoNothing(),
                                            /*enable_high_accuracy=*/true);
  EXPECT_TRUE(ProvidersStarted());

  // Add an observer. The initial diagnostics are null.
  MockGeolocationInternalsObserver observer;
  mojo::PendingRemote<mojom::GeolocationInternalsObserver> remote;
  observer.Bind(remote.InitWithNewPipeAndPassReceiver());
  TestFuture<mojom::GeolocationDiagnosticsPtr> future;
  provider()->AddInternalsObserver(std::move(remote), future.GetCallback());
  EXPECT_FALSE(future.Get());

  // Call OnInternalsUpdated. The observer is not notified.
  EXPECT_CALL(observer, OnDiagnosticsChanged).Times(0);
  provider()->SimulateInternalsUpdatedForTesting();
  loop.RunUntilIdle();

  observer.Disconnect();
}

#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
TEST_F(GeolocationProviderTest, StartProviderAfterSystemPermissionGranted) {
  SetFakeLocationProviderManager();

  // The default system permission state is kUndetermined. Adding a location
  // observer should not start provider and observer's callback should not be
  // called.
  MockGeolocationObserver mock_observer;
  EXPECT_CALL(mock_observer, OnLocationUpdate).Times(0);
  GeolocationProviderImpl::LocationUpdateCallback callback =
      base::BindRepeating(&MockGeolocationObserver::OnLocationUpdate,
                          base::Unretained(&mock_observer));
  base::CallbackListSubscription subscription =
      provider()->AddLocationUpdateCallback(callback,
                                            /*enable_high_accuracy=*/true);

  // Verify that the provider hasn't started yet due to permission is not
  // granted.
  EXPECT_FALSE(ProvidersStarted());

  // Simulate system permission being granted. Provider should now be active.
  SetSystemPermission(LocationSystemPermissionStatus::kAllowed);
  EXPECT_TRUE(ProvidersStarted());

  TestFuture<mojom::GeopositionResultPtr> future;
  EXPECT_CALL(mock_observer, OnLocationUpdate)
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

  MockGeolocationObserver mock_observer;
  TestFuture<mojom::GeopositionResultPtr> future;

  // Expect that the observer should be notified with permission denied error
  // when subscription is created.
  EXPECT_CALL(mock_observer, OnLocationUpdate)
      .WillOnce([&](const mojom::GeopositionResult& result) {
        future.SetValue(result.Clone());
      });

  GeolocationProviderImpl::LocationUpdateCallback callback =
      base::BindRepeating(&MockGeolocationObserver::OnLocationUpdate,
                          base::Unretained(&mock_observer));
  base::CallbackListSubscription subscription =
      provider()->AddLocationUpdateCallback(callback,
                                            /*enable_high_accuracy=*/true);

  // Verify that callback should be invoked with permission denied error and
  // provider is not started.
  EXPECT_EQ(future.Take()->get_error(), error_result_->get_error());
  EXPECT_FALSE(ProvidersStarted());
}

TEST_F(GeolocationProviderTest,
       ReportPermissionDeniedOnSystemPermissionDenied) {
  SetFakeLocationProviderManager();

  // Set system permission state from kUndetermined to kAllowed.
  SetSystemPermission(LocationSystemPermissionStatus::kAllowed);

  MockGeolocationObserver mock_observer;
  GeolocationProviderImpl::LocationUpdateCallback callback =
      base::BindRepeating(&MockGeolocationObserver::OnLocationUpdate,
                          base::Unretained(&mock_observer));
  base::CallbackListSubscription subscription =
      provider()->AddLocationUpdateCallback(callback,
                                            /*enable_high_accuracy=*/true);

  // Verify that provider is started when subscription is created when system
  // permission is granted.
  EXPECT_TRUE(ProvidersStarted());

  TestFuture<mojom::GeopositionResultPtr> position_future;
  EXPECT_CALL(mock_observer, OnLocationUpdate)
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
  EXPECT_CALL(mock_observer, OnLocationUpdate)
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

  // Create 1st observer and expected the callback1 is invoked with permission
  // denied error when system permission is denied.
  MockGeolocationObserver mock_observer1;
  GeolocationProviderImpl::LocationUpdateCallback callback1 =
      base::BindRepeating(&MockGeolocationObserver::OnLocationUpdate,
                          base::Unretained(&mock_observer1));
  EXPECT_CALL(mock_observer1, OnLocationUpdate)
      .WillOnce([&](const mojom::GeopositionResult& result) {
        error_future.SetValue(result.Clone());
      });

  base::CallbackListSubscription subscription1 =
      provider()->AddLocationUpdateCallback(callback1,
                                            /*enable_high_accuracy=*/true);
  EXPECT_EQ(error_future.Get()->get_error(), error_result_->get_error());
  subscription1 = {};
  EXPECT_FALSE(ProvidersStarted());

  // Set system permission state from kDenied to kAllowed.
  SetSystemPermission(LocationSystemPermissionStatus::kAllowed);

  // Created 2nd observer and subscription after system permission is set to
  // kAllowed.
  MockGeolocationObserver mock_observer2;
  GeolocationProviderImpl::LocationUpdateCallback callback2 =
      base::BindRepeating(&MockGeolocationObserver::OnLocationUpdate,
                          base::Unretained(&mock_observer2));
  base::CallbackListSubscription subscription2 =
      provider()->AddLocationUpdateCallback(callback2,
                                            /*enable_high_accuracy=*/true);

  // Verify that provider is started when subscription2 is created when system
  // permission is granted.
  EXPECT_TRUE(ProvidersStarted());

  TestFuture<mojom::GeopositionResultPtr> position_future;
  EXPECT_CALL(mock_observer2, OnLocationUpdate)
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
