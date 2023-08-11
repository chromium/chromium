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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {
namespace {

using ::base::test::TestFuture;
using ::testing::MakeMatcher;
using ::testing::Matcher;
using ::testing::MatcherInterface;
using ::testing::MatchResultListener;

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
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
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
 protected:
  GeolocationProviderTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI) {}

  GeolocationProviderTest(const GeolocationProviderTest&) = delete;
  GeolocationProviderTest& operator=(const GeolocationProviderTest&) = delete;

  ~GeolocationProviderTest() override = default;

  GeolocationProviderImpl* provider() {
    return GeolocationProviderImpl::GetInstance();
  }

  FakeLocationProvider* arbitrator() { return arbitrator_; }

  // Called on test thread.
  void SetFakeArbitrator();
  bool ProvidersStarted();
  void SendMockLocation(const mojom::GeopositionResult& result);

 private:
  // Called on provider thread.
  void GetProvidersStarted();

  // |at_exit| must be initialized before all other variables so that it is
  // available to register with Singletons and can handle tear down when the
  // test completes.
  base::ShadowingAtExitManager at_exit_;

  base::test::SingleThreadTaskEnvironment task_environment_;

  base::ThreadChecker thread_checker_;

  // Owned by the GeolocationProviderImpl class.
  raw_ptr<FakeLocationProvider, DanglingUntriaged> arbitrator_ = nullptr;

  // True if |arbitrator_| is started.
  bool is_started_;
};

void GeolocationProviderTest::SetFakeArbitrator() {
  ASSERT_FALSE(arbitrator_);
  auto arbitrator = std::make_unique<FakeLocationProvider>();
  arbitrator_ = arbitrator.get();
  provider()->SetArbitratorForTesting(std::move(arbitrator));
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
  is_started_ = arbitrator()->state() !=
                mojom::GeolocationDiagnostics::ProviderState::kStopped;
}

void GeolocationProviderTest::SendMockLocation(
    const mojom::GeopositionResult& result) {
  DCHECK(provider()->IsRunning());
  DCHECK(thread_checker_.CalledOnValidThread());
  provider()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&GeolocationProviderImpl::OnLocationUpdate,
                                base::Unretained(provider()), arbitrator_,
                                result.Clone()));
}

// Regression test for http://crbug.com/59377
TEST_F(GeolocationProviderTest, OnPermissionGrantedWithoutObservers) {
  EXPECT_FALSE(provider()->user_did_opt_into_location_services_for_testing());
  provider()->UserDidOptIntoLocationServices();
  EXPECT_TRUE(provider()->user_did_opt_into_location_services_for_testing());
  provider()->clear_user_did_opt_into_location_services_for_testing();
}

TEST_F(GeolocationProviderTest, StartStop) {
  SetFakeArbitrator();
  EXPECT_FALSE(provider()->IsRunning());
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
  SetFakeArbitrator();
  auto first_result =
      mojom::GeopositionResult::NewPosition(mojom::Geoposition::New());
  mojom::Geoposition& first_position = *first_result->get_position();
  first_position.latitude = 12;
  first_position.longitude = 34;
  first_position.accuracy = 56;
  first_position.timestamp = base::Time::Now();

  AsyncMockGeolocationObserver first_observer;
  GeolocationProviderImpl::LocationUpdateCallback first_callback =
      base::BindRepeating(&MockGeolocationObserver::OnLocationUpdate,
                          base::Unretained(&first_observer));
  EXPECT_CALL(first_observer,
              OnLocationUpdate(GeopositionResultEq(*first_result)));
  base::CallbackListSubscription subscription =
      provider()->AddLocationUpdateCallback(first_callback, false);
  SendMockLocation(*first_result);
  base::RunLoop().Run();

  subscription = {};

  auto second_result =
      mojom::GeopositionResult::NewPosition(mojom::Geoposition::New());
  mojom::Geoposition& second_position = *second_result->get_position();
  second_position.latitude = 13;
  second_position.longitude = 34;
  second_position.accuracy = 56;
  second_position.timestamp = base::Time::Now();

  AsyncMockGeolocationObserver second_observer;

  // After adding a second observer, check that no unexpected position update
  // is sent.
  EXPECT_CALL(second_observer, OnLocationUpdate(testing::_)).Times(0);
  GeolocationProviderImpl::LocationUpdateCallback second_callback =
      base::BindRepeating(&MockGeolocationObserver::OnLocationUpdate,
                          base::Unretained(&second_observer));
  base::CallbackListSubscription subscription2 =
      provider()->AddLocationUpdateCallback(second_callback, false);
  base::RunLoop().RunUntilIdle();

  // The second observer should receive the new position now.
  EXPECT_CALL(second_observer,
              OnLocationUpdate(GeopositionResultEq(*second_result)));
  SendMockLocation(*second_result);
  base::RunLoop().Run();

  subscription2 = {};
  EXPECT_FALSE(ProvidersStarted());
}

TEST_F(GeolocationProviderTest, OverrideLocationForTesting) {
  SetFakeArbitrator();
  auto result = mojom::GeopositionResult::NewError(mojom::GeopositionError::New(
      mojom::GeopositionErrorCode::kPositionUnavailable, "", ""));
  provider()->OverrideLocationForTesting(result->Clone());
  // Adding an observer when the location is overridden should synchronously
  // update the observer with our overridden position.
  MockGeolocationObserver mock_observer;
  EXPECT_CALL(mock_observer, OnLocationUpdate(GeopositionResultEq(*result)));
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
  SetFakeArbitrator();
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
  SetFakeArbitrator();
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

  // Add a subscription so the provider will be started.
  SetFakeArbitrator();
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
  base::RunLoop().RunUntilIdle();

  observer.Disconnect();
}

}  // namespace device
