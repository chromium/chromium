// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/geolocation_provider_impl.h"

#include <memory>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string16.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "services/device/geolocation/fake_location_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::MakeMatcher;
using testing::Matcher;
using testing::MatcherInterface;
using testing::MatchResultListener;

namespace device {
namespace {

class GeolocationObserver {
 public:
  virtual ~GeolocationObserver() = default;
  virtual void OnLocationUpdate(const mojom::Geoposition& position) = 0;
};

class MockGeolocationObserver : public GeolocationObserver {
 public:
  MOCK_METHOD1(OnLocationUpdate, void(const mojom::Geoposition& position));
};

class AsyncMockGeolocationObserver : public MockGeolocationObserver {
 public:
  void OnLocationUpdate(const mojom::Geoposition& position) override {
    MockGeolocationObserver::OnLocationUpdate(position);
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }
};

class MockGeolocationCallbackWrapper {
 public:
  MOCK_METHOD1(Callback, void(const mojom::Geoposition& position));
};

class GeopositionEqMatcher
    : public MatcherInterface<const mojom::Geoposition&> {
 public:
  explicit GeopositionEqMatcher(const mojom::Geoposition& expected)
      : expected_(expected) {}

  bool MatchAndExplain(const mojom::Geoposition& actual,
                       MatchResultListener* listener) const override {
    return actual.latitude == expected_.latitude &&
           actual.longitude == expected_.longitude &&
           actual.altitude == expected_.altitude &&
           actual.accuracy == expected_.accuracy &&
           actual.altitude_accuracy == expected_.altitude_accuracy &&
           actual.heading == expected_.heading &&
           actual.speed == expected_.speed &&
           actual.timestamp == expected_.timestamp &&
           actual.error_code == expected_.error_code &&
           actual.error_message == expected_.error_message;
  }

  void DescribeTo(::std::ostream* os) const override {
    *os << "which matches the expected position";
  }

  void DescribeNegationTo(::std::ostream* os) const override {
    *os << "which does not match the expected position";
  }

 private:
  mojom::Geoposition expected_;

  DISALLOW_COPY_AND_ASSIGN(GeopositionEqMatcher);
};

Matcher<const mojom::Geoposition&> GeopositionEq(
    const mojom::Geoposition& expected) {
  return MakeMatcher(new GeopositionEqMatcher(expected));
}

void DummyFunction(const LocationProvider* provider,
                   const mojom::Geoposition& position) {}

}  // namespace

class GeolocationProviderTest : public testing::Test {
 protected:
  GeolocationProviderTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI),
        arbitrator_(new FakeLocationProvider) {
    provider()->SetArbitratorForTesting(base::WrapUnique(arbitrator_));
  }

  ~GeolocationProviderTest() override = default;

  GeolocationProviderImpl* provider() {
    return GeolocationProviderImpl::GetInstance();
  }

  FakeLocationProvider* arbitrator() { return arbitrator_; }

  // Called on test thread.
  bool ProvidersStarted();
  void SendMockLocation(const mojom::Geoposition& position);

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
  FakeLocationProvider* arbitrator_;

  // True if |arbitrator_| is started.
  bool is_started_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationProviderTest);
};

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
  is_started_ = arbitrator()->state() != FakeLocationProvider::STOPPED;
}

void GeolocationProviderTest::SendMockLocation(
    const mojom::Geoposition& position) {
  DCHECK(provider()->IsRunning());
  DCHECK(thread_checker_.CalledOnValidThread());
  provider()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GeolocationProviderImpl::OnLocationUpdate,
                     base::Unretained(provider()), arbitrator_, position));
}

// Regression test for http://crbug.com/59377
TEST_F(GeolocationProviderTest, OnPermissionGrantedWithoutObservers) {
  // Clear |provider|'s arbitrator so the default arbitrator can be used.
  provider()->SetArbitratorForTesting(nullptr);
  EXPECT_FALSE(provider()->user_did_opt_into_location_services_for_testing());
  provider()->UserDidOptIntoLocationServices();
  EXPECT_TRUE(provider()->user_did_opt_into_location_services_for_testing());
  provider()->clear_user_did_opt_into_location_services_for_testing();
}

TEST_F(GeolocationProviderTest, StartStop) {
  EXPECT_FALSE(provider()->IsRunning());
  std::unique_ptr<GeolocationProvider::Subscription> subscription =
      provider()->AddLocationUpdateCallback(
          base::Bind(&DummyFunction, arbitrator()), false);
  EXPECT_TRUE(provider()->IsRunning());
  EXPECT_TRUE(ProvidersStarted());

  subscription.reset();

  EXPECT_FALSE(ProvidersStarted());
  EXPECT_TRUE(provider()->IsRunning());
}

TEST_F(GeolocationProviderTest, StalePositionNotSent) {
  mojom::Geoposition first_position;
  first_position.latitude = 12;
  first_position.longitude = 34;
  first_position.accuracy = 56;
  first_position.timestamp = base::Time::Now();

  AsyncMockGeolocationObserver first_observer;
  GeolocationProviderImpl::LocationUpdateCallback first_callback =
      base::Bind(&MockGeolocationObserver::OnLocationUpdate,
                 base::Unretained(&first_observer));
  EXPECT_CALL(first_observer, OnLocationUpdate(GeopositionEq(first_position)));
  std::unique_ptr<GeolocationProvider::Subscription> subscription =
      provider()->AddLocationUpdateCallback(first_callback, false);
  SendMockLocation(first_position);
  base::RunLoop().Run();

  subscription.reset();

  mojom::Geoposition second_position;
  second_position.latitude = 13;
  second_position.longitude = 34;
  second_position.accuracy = 56;
  second_position.timestamp = base::Time::Now();

  AsyncMockGeolocationObserver second_observer;

  // After adding a second observer, check that no unexpected position update
  // is sent.
  EXPECT_CALL(second_observer, OnLocationUpdate(testing::_)).Times(0);
  GeolocationProviderImpl::LocationUpdateCallback second_callback =
      base::Bind(&MockGeolocationObserver::OnLocationUpdate,
                 base::Unretained(&second_observer));
  std::unique_ptr<GeolocationProvider::Subscription> subscription2 =
      provider()->AddLocationUpdateCallback(second_callback, false);
  base::RunLoop().RunUntilIdle();

  // The second observer should receive the new position now.
  EXPECT_CALL(second_observer,
              OnLocationUpdate(GeopositionEq(second_position)));
  SendMockLocation(second_position);
  base::RunLoop().Run();

  subscription2.reset();
  EXPECT_FALSE(ProvidersStarted());
}

TEST_F(GeolocationProviderTest, OverrideLocationForTesting) {
  mojom::Geoposition position;
  position.error_code = mojom::Geoposition::ErrorCode::POSITION_UNAVAILABLE;
  provider()->OverrideLocationForTesting(position);
  // Adding an observer when the location is overridden should synchronously
  // update the observer with our overridden position.
  MockGeolocationObserver mock_observer;
  EXPECT_CALL(mock_observer, OnLocationUpdate(GeopositionEq(position)));
  GeolocationProviderImpl::LocationUpdateCallback callback =
      base::Bind(&MockGeolocationObserver::OnLocationUpdate,
                 base::Unretained(&mock_observer));
  std::unique_ptr<GeolocationProvider::Subscription> subscription =
      provider()->AddLocationUpdateCallback(callback, false);
  subscription.reset();
  // Wait for the providers to be stopped now that all clients are gone.
  EXPECT_FALSE(ProvidersStarted());
}

}  // namespace device
