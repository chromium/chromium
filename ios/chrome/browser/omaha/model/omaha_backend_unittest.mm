// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omaha/model/omaha_backend.h"

#import <Foundation/Foundation.h>

#import <optional>

#import "base/check.h"
#import "base/check_deref.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/ref_counted.h"
#import "base/run_loop.h"
#import "base/strings/strcat.h"
#import "base/strings/string_number_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "base/time/time.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/omaha/model/omaha_persistent_state.h"
#import "ios/chrome/browser/upgrade/model/upgrade_recommended_details.h"
#import "ios/public/provider/chrome/browser/omaha/omaha_api.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#import "services/network/test/test_url_loader_factory.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

// Fake update URL string.
inline constexpr char kUpdateURL[] = "http://www.goo.fr/foo";

// Returns string corresponding to an up to date response.
std::string GetUpToDateServerResponse() {
  return base::StrCat({
      R"(<?xml version="1.0"?>)",
      R"(<response protocol="3.0" server="prod">)",
      R"(  <daystart elapsed_days="4088"/>)",
      R"(  <app status="ok" appid=")",
      ios::provider::GetOmahaApplicationId(),
      R"(">)",
      R"(    <updatecheck status="noupdate"/>)",
      R"(    <ping status="ok"/>)",
      R"(  </app>)",
      R"(</response>)",
  });
}

// Returns string corresponding an upgrade required response with `version`.
std::string GetOutOfDateServerResponse(const base::Version& version) {
  CHECK(version.IsValid());
  return base::StrCat({
      R"(<?xml version="1.0"?>)",
      R"(<response protocol="3.0" server="prod">)",
      R"(  <daystart elapsed_days="4088"/>)",
      R"(  <app status="ok" appid=")",
      ios::provider::GetOmahaApplicationId(),
      R"(">)",
      R"(    <updatecheck status="ok">)",
      R"(      <urls>)",
      R"(        <url codebase=")",
      kUpdateURL,
      R"("/>)",
      R"(      </urls>)",
      R"(      <manifest version=")",
      version.GetString(),
      R"(">)",
      R"(        <packages>)",
      R"(          <package hash="0" name="Chrome" required="true" size="0"/>)",
      R"(        </packages>)",
      R"(        <actions>)",
      R"(          <action event="update" run="Chrome"/>)",
      R"(          <action event="postinstall" osminversion="6.0"/>)",
      R"(        </actions>)",
      R"(      </manifest>)",
      R"(    </updatecheck>)",
      R"(    <ping status="ok"/>)",
      R"(  </app>)",
      R"(</response>)",
  });
}

// Returns a new minor version greater than `version`.
base::Version NewMinorVersion(const base::Version& version) {
  std::vector<uint32_t> components;
  for (uint32_t component : version.components()) {
    components.push_back(component);
    if (components.size() == 2) {
      ++components.back();
    }
  }
  return base::Version(std::move(components));
}

// Returns whether the timer is running according to debug information.
bool IsTimerRunning(const base::DictValue& debug) {
  const std::string* value = debug.FindString("timer_running");
  return value && *value == base::NumberToString(true);
}

// Returns the timer delay according to debug information.
std::optional<base::TimeDelta> GetTimerDelay(const base::DictValue& debug) {
  const std::string* value = debug.FindString("timer_current_delay");
  if (!value) {
    return std::nullopt;
  }

  uint64_t delay_in_seconds = 0;
  if (!base::StringToUint64(*value, &delay_in_seconds)) {
    return std::nullopt;
  }

  return base::Seconds(delay_in_seconds);
}

// Represents potential delta to apply to an OmahaPersistentState.
struct OmahaPersistentStateDelta {
  std::optional<base::TimeDelta> offset;
  std::optional<base::Version> version;
};

// Returns an OmahaPersistentState with `delta` (valid if all fields are
// std::nullopt).
OmahaPersistentState InitialState(OmahaPersistentStateDelta delta) {
  const base::TimeDelta offset = delta.offset.value_or(base::Seconds(0));
  const base::Time time = base::Time::Now() + offset;
  return {
      .next_ping_time = time,
      .last_ping_time = time,
      .last_response_time = time,
      .last_sent_version = delta.version.value_or(version_info::GetVersion()),
      .number_of_failures = 1,
      .last_server_date = 123,
  };
}

}  // anonymous namespace

// Test fixture for OmahaBackend.
class OmahaBackendTest : public PlatformTest {
 public:
  using SharedURLLoaderFactoryScopedRefPtr =
      scoped_refptr<network::SharedURLLoaderFactory>;

  // Creates a backend with default values.
  std::unique_ptr<OmahaBackend> CreateBackend(bool auto_schedule) {
    return std::make_unique<OmahaBackend>(
        /*locale_lang=*/"en",
        /*app_install=*/base::Time::Now(),
        /*omaha_server_url=*/ios::provider::GetOmahaUpdateServerURL(),
        /*auto_schedule=*/auto_schedule);
  }

  // Returns a PendingSharedURLLoaderFactoryCallback allowing to capture
  // network requests (for use by the OmahaBackend::Start(...) method).
  base::OnceCallback<SharedURLLoaderFactoryScopedRefPtr()>
  GetPendingSharedURLLoaderFactoryCallback() {
    return base::BindOnce(
        [](SharedURLLoaderFactoryScopedRefPtr factory) { return factory; },
        shared_url_loader_factory_);
  }

  // Returns whether the SharedURLLoader has captured any request.
  bool HasPendingRequest() {
    return url_loader_factory_.GetPendingRequest(0) != nullptr;
  }

  // Reply the the pending request with `response`. It is an error to call
  // this method if HasPendingRequest() is false. This method will drain
  // the current TaskRunner, so it must not be used with an auto scheduling
  // OmahaBackend.
  void SimulateResponseForPendingRequest(std::string response) {
    using PendingRequest = network::TestURLLoaderFactory::PendingRequest;
    PendingRequest* pending_request = url_loader_factory_.GetPendingRequest(0);
    url_loader_factory_.SimulateResponseForPendingRequest(
        CHECK_DEREF(pending_request).request.url.spec(), std::move(response));
  }

  // Simulate `time_skipped` passing while the app was backgrounded.
  void SimulateEnteringForeground(base::TimeDelta time_skipped) {
    task_environment_.SuspendedAdvanceClock(time_skipped);

    [[NSNotificationCenter defaultCenter]
        postNotificationName:@"UIApplicationWillEnterForegroundNotification"
                      object:nil];

    // OmahaBackend use post-task to handle the notification, so post another
    // task here and wait for completion before returning. As it is posted
    // after the one from OmahaBackend and the TaskRunner is sequenced, it
    // will execute after.
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  // Using a fake time source ensure the code can reliably manipule time.
  using TimeSource = base::test::TaskEnvironment::TimeSource;
  base::test::TaskEnvironment task_environment_{TimeSource::MOCK_TIME};

  network::TestURLLoaderFactory url_loader_factory_;
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      shared_url_loader_factory_{url_loader_factory_.GetSafeWeakWrapper()};
};

// Tests that OmahaBackend will not persist state if it is valid.
TEST_F(OmahaBackendTest, Start_ValidState) {
  auto backend = CreateBackend(/*auto_schedule=*/false);

  base::test::TestFuture<OmahaPersistentState> future;
  backend->Start(InitialState({}), GetPendingSharedURLLoaderFactoryCallback(),
                 /*upgrade_recommended_callback=*/base::DoNothing(),
                 future.GetRepeatingCallback<const OmahaPersistentState&>());

  EXPECT_FALSE(future.IsReady());
}

// Tests that OmahaBackend will fix the state if it looks like the time
// has been changed on the device.
TEST_F(OmahaBackendTest, Start_StateWithInvalidTime) {
  auto backend = CreateBackend(/*auto_schedule=*/false);

  base::test::TestFuture<OmahaPersistentState> future;
  backend->Start(InitialState({.offset = base::Days(30)}),
                 GetPendingSharedURLLoaderFactoryCallback(),
                 /*upgrade_recommended_callback=*/base::DoNothing(),
                 future.GetRepeatingCallback<const OmahaPersistentState&>());

  ASSERT_TRUE(future.IsReady());
  const OmahaPersistentState state = future.Take();
  EXPECT_EQ(state.last_response_time, base::Time::Now());
  EXPECT_EQ(state.next_ping_time, base::Time::Now() + base::Hours(5));
}

// Tests that OmahaBackend will ping the server immediately if the
// version is obsolete and will persist the state if needed.
TEST_F(OmahaBackendTest, Start_ObsoleteVersion) {
  auto backend = CreateBackend(/*auto_schedule=*/false);

  base::test::TestFuture<OmahaPersistentState> future;
  backend->Start(InitialState({.version = base::Version()}),
                 GetPendingSharedURLLoaderFactoryCallback(),
                 /*upgrade_recommended_callback=*/base::DoNothing(),
                 future.GetRepeatingCallback<const OmahaPersistentState&>());

  ASSERT_TRUE(future.IsReady());
  const OmahaPersistentState state = future.Take();
  EXPECT_EQ(state.next_ping_time, base::Time::Now());
  EXPECT_EQ(state.number_of_failures, 0);
}

// Tests that OmahaBackend invokes the UpgradeRecommendedDetails with a
// value describing that the application is up to date when the response
// from the Omaha server says so.
TEST_F(OmahaBackendTest, Ping_ResponseUpToDate) {
  auto backend = CreateBackend(/*auto_schedule=*/false);

  base::test::TestFuture<UpgradeRecommendedDetails> future;
  backend->Start(
      InitialState({}), GetPendingSharedURLLoaderFactoryCallback(),
      future.GetRepeatingCallback<const UpgradeRecommendedDetails&>(),
      /*save_persistent_state_callback=*/base::DoNothing());
  EXPECT_FALSE(future.IsReady());

  // Request sending a ping now.
  backend->CheckNow();
  EXPECT_FALSE(future.IsReady());

  // Send a fake response indicating the app is up to date.
  ASSERT_TRUE(HasPendingRequest());
  SimulateResponseForPendingRequest(GetUpToDateServerResponse());

  // OmahaBackend should have invoked `upgrade_recommended_callback`
  // with details that indicate the app is up to date.
  ASSERT_TRUE(future.IsReady());

  const UpgradeRecommendedDetails details = future.Take();
  EXPECT_EQ(details.upgrade_url, GURL());
  EXPECT_EQ(details.next_version, std::string());
  EXPECT_EQ(details.is_up_to_date, true);
}

// Tests that OmahaBackend invokes the UpgradeRecommendedDetails with a
// value describing that the application needs to be updated, includes
// an URL and a version when the response from the Omaha server says so.
TEST_F(OmahaBackendTest, Ping_ResponseUpgradeRequired) {
  auto backend = CreateBackend(/*auto_schedule=*/false);

  base::test::TestFuture<UpgradeRecommendedDetails> future;
  backend->Start(
      InitialState({}), GetPendingSharedURLLoaderFactoryCallback(),
      future.GetRepeatingCallback<const UpgradeRecommendedDetails&>(),
      /*save_persistent_state_callback=*/base::DoNothing());
  EXPECT_FALSE(future.IsReady());

  // Request sending a ping now.
  backend->CheckNow();
  EXPECT_FALSE(future.IsReady());

  // Send a fake response indicating the app is up to date.
  ASSERT_TRUE(HasPendingRequest());
  const base::Version new_version = NewMinorVersion(version_info::GetVersion());
  SimulateResponseForPendingRequest(GetOutOfDateServerResponse(new_version));

  // OmahaBackend should have invoked `upgrade_recommended_callback`
  // with details that indicate the app is out of date, including an
  // upgrade URL and a version number.
  ASSERT_TRUE(future.IsReady());

  const UpgradeRecommendedDetails details = future.Take();
  EXPECT_EQ(details.upgrade_url, GURL(kUpdateURL));
  EXPECT_EQ(details.next_version, new_version.GetString());
  EXPECT_EQ(details.is_up_to_date, false);
}

// Tests that OmahaBackend drops invalid responses from the Omaha server.
TEST_F(OmahaBackendTest, Ping_InvalidResponse) {
  auto backend = CreateBackend(/*auto_schedule=*/false);

  base::test::TestFuture<UpgradeRecommendedDetails> future;
  backend->Start(
      InitialState({}), GetPendingSharedURLLoaderFactoryCallback(),
      future.GetRepeatingCallback<const UpgradeRecommendedDetails&>(),
      /*save_persistent_state_callback=*/base::DoNothing());
  EXPECT_FALSE(future.IsReady());

  // Request sending a ping now.
  backend->CheckNow();
  EXPECT_FALSE(future.IsReady());

  // Send a fake response that is not valid
  ASSERT_TRUE(HasPendingRequest());
  SimulateResponseForPendingRequest("Not a valid xml document!");

  // OmahaBackend should not invoke the `upgrade_recommended_callback`
  // when the response is invalid.
  EXPECT_FALSE(future.IsReady());
}

// Tests that OmahaBackend persist the state when sending ping and when
// receiving response from the service.
TEST_F(OmahaBackendTest, Ping_PersistState) {
  auto backend = CreateBackend(/*auto_schedule=*/false);

  const OmahaPersistentState initial_state = InitialState({});
  base::test::TestFuture<OmahaPersistentState> future;
  backend->Start(initial_state, GetPendingSharedURLLoaderFactoryCallback(),
                 /*upgrade_recommended_callback=*/base::DoNothing(),
                 future.GetRepeatingCallback<const OmahaPersistentState&>());
  EXPECT_FALSE(future.IsReady());

  // Request sending a ping now.
  backend->CheckNow();

  {
    ASSERT_TRUE(future.IsReady());
    const OmahaPersistentState state = future.Take();
    EXPECT_GT(state.number_of_failures, initial_state.number_of_failures);
    EXPECT_GE(state.next_ping_time, base::Time::Now() + base::Minutes(54));
    EXPECT_LE(state.next_ping_time, base::Time::Now() + base::Hours(7));
  }

  // Send a fake response indicating the app is up to date.
  ASSERT_TRUE(HasPendingRequest());
  SimulateResponseForPendingRequest(GetUpToDateServerResponse());

  {
    ASSERT_TRUE(future.IsReady());
    const OmahaPersistentState state = future.Take();
    EXPECT_EQ(state.number_of_failures, 0);
    EXPECT_EQ(state.last_response_time, base::Time::Now());
    EXPECT_EQ(state.last_ping_time, state.next_ping_time);
    EXPECT_EQ(state.last_server_date, 4088);
  }
}

// Tests that OmahaBackend resynchronize the timer when the application
// enters foreground (as the time ticks may not be in sync with clock
// clock time).
TEST_F(OmahaBackendTest, RescheduleTimerOnEnteringForeground) {
  auto backend = CreateBackend(/*auto_schedule=*/true);
  backend->Start(InitialState({.offset = base::Minutes(30)}),
                 GetPendingSharedURLLoaderFactoryCallback(),
                 /*upgrade_recommended_callback=*/base::DoNothing(),
                 /*save_persistent_state_callback=*/base::DoNothing());

  // The next ping should have been scheduled according to the data
  // from the initial state.
  {
    const base::DictValue debug = backend->GetDebugInformation();
    EXPECT_TRUE(IsTimerRunning(debug));
    EXPECT_EQ(GetTimerDelay(debug), base::Minutes(30));
  }

  // Simulate time advancing while the app was backgrounded.
  SimulateEnteringForeground(base::Minutes(5));

  // OmahaBackend should have rescheduled the timer.
  {
    const base::DictValue debug = backend->GetDebugInformation();
    EXPECT_TRUE(IsTimerRunning(debug));
    EXPECT_EQ(GetTimerDelay(debug), base::Minutes(25));
  }

  // Simulate time advancing while the app was backgrounded.
  SimulateEnteringForeground(base::Minutes(30));

  // OmahaBackend should have sent the ping request and stopped the timer.
  {
    EXPECT_FALSE(IsTimerRunning(backend->GetDebugInformation()));
    EXPECT_TRUE(HasPendingRequest());
  }
}

// Tests that OmahaBackend has no dangling pointer due to the notification.
TEST_F(OmahaBackendTest, RescheduleTimerOnEnteringForeground_NoDanglingPtr) {
  auto backend = CreateBackend(/*auto_schedule=*/true);
  backend->Start(InitialState({.offset = base::Minutes(30)}),
                 GetPendingSharedURLLoaderFactoryCallback(),
                 /*upgrade_recommended_callback=*/base::DoNothing(),
                 /*save_persistent_state_callback=*/base::DoNothing());

  // The next ping should have been scheduled according to the data
  // from the initial state.
  {
    const base::DictValue debug = backend->GetDebugInformation();
    EXPECT_TRUE(IsTimerRunning(debug));
    EXPECT_EQ(GetTimerDelay(debug), base::Minutes(30));
  }

  // Post the notification. This should schedule a callback.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:@"UIApplicationWillEnterForegroundNotification"
                    object:nil];

  // Destroy the backend before the callback is executed, then wait
  // for the callback to execute. This should not crash.
  backend.reset();

  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}
