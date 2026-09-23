// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omaha/model/omaha_service.h"

#import <UIKit/UIKit.h>
#import <regex.h>
#import <sys/types.h>

#import "base/check.h"
#import "base/check_op.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/run_loop.h"
#import "base/strings/stringprintf.h"
#import "base/task/sequenced_task_runner.h"
#import "base/test/ios/wait_util.h"
#import "base/test/test_future.h"
#import "base/time/time.h"
#import "base/time/time_override.h"
#import "components/metrics/metrics_pref_names.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/omaha/model/omaha_persistent_state.h"
#import "ios/chrome/browser/omaha/model/omaha_ping.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/upgrade/model/upgrade_constants.h"
#import "ios/chrome/browser/upgrade/model/upgrade_recommended_details.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/public/provider/chrome/browser/omaha/omaha_api.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/thread/web_thread.h"
#import "net/http/http_status_code.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#import "services/network/public/mojom/url_response_head.mojom.h"
#import "services/network/test/test_url_loader_factory.h"
#import "services/network/test/test_utils.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

using ::base::test::TestFutureMode;
using ::testing::Eq;

const int64_t kUnknownInstallDate = 2;

base::Time GetTimeWithDelta(base::TimeDelta delta) {
  static base::Time base = base::subtle::TimeNowIgnoringOverride();
  return base + delta;
}

base::TimeTicks GetTimeTicksWithDelta(base::TimeDelta delta) {
  static base::TimeTicks base = base::subtle::TimeTicksNowIgnoringOverride();
  return base + delta;
}

// Returns whether `future` is ready and consume all values.
template <typename T>
bool IsReadyAndClear(base::test::TestFuture<T>& future) {
  const bool result = future.IsReady();
  future.Clear();
  return result;
}

// Expects `future` to be ready and returns the last captured value,
// dropping all other captured values (if any).
template <typename T>
T TakeLastValue(base::test::TestFuture<T>& future) {
  CHECK(future.IsReady());
  T result = future.Take();
  while (future.IsReady()) {
    result = future.Take();
  }
  return result;
}

}  // namespace

class OmahaServiceTest : public PlatformTest {
 public:
  OmahaServiceTest()
      : test_shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        need_update_(false) {
    GetApplicationContext()->GetLocalState()->SetInt64(
        metrics::prefs::kInstallDate, kUnknownInstallDate);
  }

  OmahaServiceTest(const OmahaServiceTest&) = delete;
  OmahaServiceTest& operator=(const OmahaServiceTest&) = delete;

  void TearDown() override {
    test_shared_url_loader_factory_->Detach();
    test_shared_url_loader_factory_ = nullptr;
  }

  void OnNeedUpdate(const UpgradeRecommendedDetails& details) {
    was_one_off_ = false;
    scheduled_callback_used_ = true;
    need_update_ = !details.is_up_to_date;
  }

  void OneOffCheck(const UpgradeRecommendedDetails& details) {
    was_one_off_ = true;
    need_update_ = !details.is_up_to_date;
  }

  bool WasOneOff() {
    bool was_one_off = was_one_off_;
    was_one_off_ = false;
    return was_one_off;
  }

  bool ScheduledCallbackUsed() { return scheduled_callback_used_; }

  bool NeedUpdate() {
    DCHECK_CURRENTLY_ON(web::WebThread::UI);
    if (!need_update_) {
      base::RunLoop().RunUntilIdle();
    }
    return need_update_;
  }

  const std::string GetResponseSuccess() {
    return std::string("<?xml version=\"1.0\"?><response protocol=\"3.0\" "
                       "server=\"prod\">"
                       "<daystart elapsed_days=\"4088\"/><app appid=\"") +
           test_application_id() +
           "\" status=\"ok\">"
           "<updatecheck status=\"noupdate\"/><ping status=\"ok\"/>"
           "</app></response>";
  }

  base::TimeDelta TimerRemainingTime(OmahaService* service) {
    return service->timer_.desired_run_time() - base::TimeTicks::Now();
  }

  std::string test_application_id() const {
    return ios::provider::GetOmahaApplicationId();
  }

  base::OnceCallback<scoped_refptr<network::SharedURLLoaderFactory>()>
  GetPendingSharedURLLoaderFactoryCallback() {
    using SharedURLLoaderFactoryScopedRefPtr =
        scoped_refptr<network::SharedURLLoaderFactory>;

    return base::BindOnce(
        [](SharedURLLoaderFactoryScopedRefPtr factory) { return factory; },
        test_shared_url_loader_factory_);
  }

 protected:
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      test_shared_url_loader_factory_;

 private:
  bool need_update_ = false;
  bool was_one_off_ = false;
  bool scheduled_callback_used_ = false;
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
};

TEST_F(OmahaServiceTest, PingMessageTest) {
  static constexpr char expectedResult[] =
      "<request protocol=\"3.0\" updater=\"iOS\" updaterversion=\"[^\"]*\""
      " updaterchannel=\"[^\"]*\" ismachine=\"1\" requestid=\"requestId\""
      " sessionid=\"sessionId\" hardware_class=\"[^\"]*\">"
      "<os platform=\"ios\" version=\"[0-9][0-9]*\\(\\.[0-9][0-9]*\\)*\""
      " arch=\"[^\"]*\"/>"
      "<app brand=\"[A-Z][A-Z][A-Z][A-Z]\" appid=\"{[^}]*}\" version=\"[^\"]*\""
      " nextversion=\"\" ap=\"[^\"]*\" lang=\"[^\"]*\" client=\"\""
      " installage=\"0\">"
      "<updatecheck/>"
      "<ping active=\"1\" ad=\"-2\" rd=\"-2\"/></app></request>";

  OmahaService service(false);
  service.StartInternal(OmahaPersistentState{},
                        GetPendingSharedURLLoaderFactoryCallback(),
                        base::BindRepeating(&OmahaServiceTest::OnNeedUpdate,
                                            base::Unretained(this)),
                        base::DoNothing());

  std::string content = service.GetPingContent(
      "requestId", "sessionId", std::string(version_info::GetVersionNumber()),
      GetChannelString(), base::Time::Now(), OmahaPingEvent::kUsagePing);
  regex_t regex;
  regcomp(&regex, expectedResult, REG_NOSUB);
  int result = regexec(&regex, content.c_str(), 0, NULL, 0);
  regfree(&regex);
  EXPECT_EQ(0, result) << "Actual contents: " << content;
  EXPECT_FALSE(NeedUpdate());
}

TEST_F(OmahaServiceTest, PingMessageTestWithUnknownInstallDate) {
  static constexpr char expectedResult[] =
      "<request protocol=\"3.0\" updater=\"iOS\" updaterversion=\"[^\"]*\""
      " updaterchannel=\"[^\"]*\" ismachine=\"1\" requestid=\"requestId\""
      " sessionid=\"sessionId\" hardware_class=\"[^\"]*\">"
      "<os platform=\"ios\" version=\"[0-9][0-9]*\\(\\.[0-9][0-9]*\\)*\""
      " arch=\"[^\"]*\"/>"
      "<app brand=\"[A-Z][A-Z][A-Z][A-Z]\" appid=\"{[^}]*}\" version=\"[^\"]*\""
      " nextversion=\"\" ap=\"[^\"]*\" lang=\"[^\"]*\" client=\"\">"
      "<updatecheck/>"
      "<ping active=\"1\" ad=\"-2\" rd=\"-2\"/></app></request>";

  OmahaService service(false);
  service.StartInternal(OmahaPersistentState{},
                        GetPendingSharedURLLoaderFactoryCallback(),
                        base::BindRepeating(&OmahaServiceTest::OnNeedUpdate,
                                            base::Unretained(this)),
                        base::DoNothing());

  std::string content = service.GetPingContent(
      "requestId", "sessionId", std::string(version_info::GetVersionNumber()),
      GetChannelString(), base::Time::FromTimeT(kUnknownInstallDate),
      OmahaPingEvent::kUsagePing);
  regex_t regex;
  regcomp(&regex, expectedResult, REG_NOSUB);
  int result = regexec(&regex, content.c_str(), 0, NULL, 0);
  regfree(&regex);
  EXPECT_EQ(0, result) << "Actual contents: " << content;
  EXPECT_FALSE(NeedUpdate());
}

TEST_F(OmahaServiceTest, InstallEventMessageTest) {
  static constexpr char kExpectedResultFormat[] =
      "<request protocol=\"3.0\" updater=\"iOS\" updaterversion=\"[^\"]*\""
      " updaterchannel=\"[^\"]*\" ismachine=\"1\" requestid=\"requestId\""
      " sessionid=\"sessionId\" hardware_class=\"[^\"]*\">"
      "<os platform=\"ios\" version=\"[0-9][0-9]*(\\.[0-9][0-9]*)*\""
      " arch=\"[^\"]*\"/>"
      "<app brand=\"[A-Z][A-Z][A-Z][A-Z]\" appid=\"{[^}]*}\" version=\"%s\""
      " nextversion=\"[^\"]*\" ap=\"[^\"]*\" lang=\"[^\"]*\" client=\"\""
      " installage=\"%d\">"
      "<event eventtype=\"%d\" eventresult=\"1\"/>"
      "<ping active=\"1\" ad=\"-2\" rd=\"-2\"/>"
      "</app></request>";

  struct TestCase {
    std::string_view previous_version;
    int expected_install_age;
    int expected_event_type;
  };

  constexpr auto kTestCases = std::to_array<TestCase>({
      {
          // First install.
          .previous_version = "",
          .expected_install_age = -1,
          .expected_event_type = 2,
      },
      {
          // Update install.
          .previous_version = "0.5",
          .expected_install_age = 0,
          .expected_event_type = 3,
      },
  });

  for (const TestCase& test_case : kTestCases) {
    OmahaService service(false);
    service.StartInternal(
        OmahaPersistentState{
            .last_sent_version = base::Version(test_case.previous_version),
        },
        GetPendingSharedURLLoaderFactoryCallback(),
        base::BindRepeating(&OmahaServiceTest::OnNeedUpdate,
                            base::Unretained(this)),
        base::DoNothing());

    std::string content = service.GetPingContent(
        "requestId", "sessionId", std::string(version_info::GetVersionNumber()),
        GetChannelString(), base::Time::Now(), OmahaPingEvent::kInstallEvent);
    regmatch_t matches[2];
    regex_t regex;
    std::string expected_result = base::StringPrintf(
        kExpectedResultFormat, test_case.previous_version,
        test_case.expected_install_age, test_case.expected_event_type);
    regcomp(&regex, expected_result.c_str(), REG_EXTENDED);
    int result =
        regexec(&regex, content.c_str(), std::size(matches), matches, 0);
    regfree(&regex);
    EXPECT_EQ(0, result) << "Actual contents: " << content;
    EXPECT_FALSE(NeedUpdate());
  }
}

TEST_F(OmahaServiceTest, SendPingSuccess) {
  const base::Time now = base::Time::Now();
  base::test::TestFuture<OmahaPersistentState> future(TestFutureMode::kQueue);
  OmahaService service(false);
  service.StartInternal(
      OmahaPersistentState{.last_sent_version = version_info::GetVersion()},
      GetPendingSharedURLLoaderFactoryCallback(),
      base::BindRepeating(&OmahaServiceTest::OnNeedUpdate,
                          base::Unretained(this)),
      future.GetRepeatingCallback<const OmahaPersistentState&>());

  service.SendPing();

  {
    ASSERT_TRUE(future.IsReady());
    const OmahaPersistentState captured_state = TakeLastValue(future);
    EXPECT_TRUE(captured_state.last_ping_time.is_null());
    EXPECT_GE(captured_state.next_ping_time, now + base::Minutes(54));
    EXPECT_LE(captured_state.next_ping_time, now + base::Hours(7));
    EXPECT_EQ(captured_state.number_of_failures, 1);
  }

  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), GetResponseSuccess());

  {
    ASSERT_TRUE(future.IsReady());
    const OmahaPersistentState captured_state = TakeLastValue(future);
    EXPECT_EQ(captured_state.number_of_failures, 0);
    EXPECT_FALSE(captured_state.last_ping_time.is_null());
    EXPECT_EQ(captured_state.last_ping_time, captured_state.next_ping_time);
    EXPECT_GT(captured_state.last_response_time, now);
    EXPECT_EQ(captured_state.last_server_date, 4088);
  }

  EXPECT_FALSE(NeedUpdate());
  EXPECT_FALSE(ScheduledCallbackUsed());
}

TEST_F(OmahaServiceTest, PingUpToDateUpdatesUserDefaults) {
  OmahaService service(false);
  service.StartInternal(
      OmahaPersistentState{.last_sent_version = version_info::GetVersion()},
      GetPendingSharedURLLoaderFactoryCallback(),
      base::BindRepeating(&OmahaServiceTest::OnNeedUpdate,
                          base::Unretained(this)),
      base::DoNothing());

  service.SendPing();

  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), GetResponseSuccess());

  EXPECT_TRUE(
      [[NSUserDefaults standardUserDefaults] boolForKey:kIOSChromeUpToDateKey]);
  EXPECT_FALSE(NeedUpdate());
  EXPECT_FALSE(ScheduledCallbackUsed());
}

TEST_F(OmahaServiceTest, PingOutOfDateUpdatesUserDefaults) {
  OmahaService service(false);
  service.StartInternal(
      OmahaPersistentState{.last_sent_version = version_info::GetVersion()},
      GetPendingSharedURLLoaderFactoryCallback(),
      base::BindRepeating(&OmahaServiceTest::OnNeedUpdate,
                          base::Unretained(this)),
      base::DoNothing());

  service.SendPing();

  std::string response =
      std::string(
          "<?xml version=\"1.0\"?><response protocol=\"3.0\" server=\"prod\">"
          "<daystart elapsed_seconds=\"56754\"/><app appid=\"") +
      test_application_id() +
      "\" status=\"ok\">"
      "<updatecheck status=\"ok\"><urls>"
      "<url codebase=\"http://www.goo.fr/foo/\"/></urls>"
      "<manifest version=\"0.0.1075.1441\">"
      "<packages>"
      "<package hash=\"0\" name=\"Chrome\" required=\"true\" size=\"0\"/>"
      "</packages>"
      "<actions>"
      "<action event=\"update\" run=\"Chrome\"/>"
      "<action event=\"postinstall\" osminversion=\"6.0\"/>"
      "</actions>"
      "</manifest>"
      "</updatecheck><ping status=\"ok\"/>"
      "</app></response>";
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), response);

  EXPECT_FALSE(
      [[NSUserDefaults standardUserDefaults] boolForKey:kIOSChromeUpToDateKey]);
  EXPECT_TRUE(NeedUpdate());
  EXPECT_TRUE(ScheduledCallbackUsed());
}

TEST_F(OmahaServiceTest, CallbackForScheduledNotUsedOnErrorResponse) {
  const base::Time now = base::Time::Now();
  base::test::TestFuture<OmahaPersistentState> future(TestFutureMode::kQueue);
  OmahaService service(false);
  service.StartInternal(
      OmahaPersistentState{.last_sent_version = version_info::GetVersion()},
      GetPendingSharedURLLoaderFactoryCallback(),
      base::BindRepeating(&OmahaServiceTest::OnNeedUpdate,
                          base::Unretained(this)),
      future.GetRepeatingCallback<const OmahaPersistentState&>());

  service.SendPing();

  {
    ASSERT_TRUE(future.IsReady());
    const OmahaPersistentState captured_state = TakeLastValue(future);
    EXPECT_EQ(captured_state.number_of_failures, 1);
    EXPECT_TRUE(captured_state.last_ping_time.is_null());
    EXPECT_GE(captured_state.next_ping_time, now + base::Minutes(54));
    EXPECT_LE(captured_state.next_ping_time, now + base::Hours(7));
  }

  std::string response =
      std::string(
          "<?xml version=\"1.0\"?><response protocol=\"3.0\" server=\"prod\">"
          "<daystart elapsed_days=\"4088\"/><app appid=\"") +
      test_application_id() +
      "\" status=\"ok\">"
      "<updatecheck status=\"error\"/><ping status=\"ok\"/>"
      "</app></response>";

  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), response);

  EXPECT_FALSE(NeedUpdate());
  EXPECT_FALSE(ScheduledCallbackUsed());
}

TEST_F(OmahaServiceTest, OneOffSuccess) {
  const base::Time now = base::Time::Now();
  base::test::TestFuture<OmahaPersistentState> future(TestFutureMode::kQueue);
  OmahaService service(false);
  service.StartInternal(
      OmahaPersistentState{.last_sent_version = version_info::GetVersion()},
      GetPendingSharedURLLoaderFactoryCallback(),
      base::BindRepeating(&OmahaServiceTest::OnNeedUpdate,
                          base::Unretained(this)),
      future.GetRepeatingCallback<const OmahaPersistentState&>());

  service.CheckNowOnIOThread(
      base::BindOnce(&OmahaServiceTest::OneOffCheck, base::Unretained(this)));

  {
    ASSERT_TRUE(future.IsReady());
    const OmahaPersistentState captured_state = TakeLastValue(future);
    EXPECT_EQ(captured_state.number_of_failures, 1);
    EXPECT_TRUE(captured_state.last_ping_time.is_null());
    EXPECT_GE(captured_state.next_ping_time, now + base::Minutes(54));
    EXPECT_LE(captured_state.next_ping_time, now + base::Hours(7));
  }

  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), GetResponseSuccess());

  {
    ASSERT_TRUE(future.IsReady());
    const OmahaPersistentState captured_state = TakeLastValue(future);
    EXPECT_EQ(captured_state.number_of_failures, 0);
    EXPECT_FALSE(captured_state.last_ping_time.is_null());
    EXPECT_EQ(captured_state.last_ping_time, captured_state.next_ping_time);
    EXPECT_GT(captured_state.last_response_time, now);
    EXPECT_EQ(captured_state.last_server_date, 4088);
  }

  EXPECT_FALSE(NeedUpdate());
  EXPECT_TRUE(WasOneOff());
}

TEST_F(OmahaServiceTest, OngoingPingOneOffCallbackUsed) {
  const base::Time now = base::Time::Now();
  base::test::TestFuture<OmahaPersistentState> future(TestFutureMode::kQueue);
  OmahaService service(false);
  service.StartInternal(
      OmahaPersistentState{.last_sent_version = version_info::GetVersion()},
      GetPendingSharedURLLoaderFactoryCallback(),
      base::BindRepeating(&OmahaServiceTest::OnNeedUpdate,
                          base::Unretained(this)),
      future.GetRepeatingCallback<const OmahaPersistentState&>());

  service.SendPing();

  {
    ASSERT_TRUE(future.IsReady());
    const OmahaPersistentState captured_state = TakeLastValue(future);
    EXPECT_EQ(captured_state.number_of_failures, 1);
    EXPECT_TRUE(captured_state.last_ping_time.is_null());
    EXPECT_GE(captured_state.next_ping_time, now + base::Minutes(54));
    EXPECT_LE(captured_state.next_ping_time, now + base::Hours(7));
  }

  // One off callback set during ongoing ping, it should now be used for
  // response.
  service.CheckNowOnIOThread(
      base::BindOnce(&OmahaServiceTest::OneOffCheck, base::Unretained(this)));

  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), GetResponseSuccess());

  {
    ASSERT_TRUE(future.IsReady());
    const OmahaPersistentState captured_state = TakeLastValue(future);
    EXPECT_EQ(captured_state.number_of_failures, 0);
    EXPECT_FALSE(captured_state.last_ping_time.is_null());
    EXPECT_EQ(captured_state.last_ping_time, captured_state.next_ping_time);
    EXPECT_GT(captured_state.last_response_time, now);
    EXPECT_EQ(captured_state.last_server_date, 4088);
  }

  EXPECT_FALSE(NeedUpdate());
  EXPECT_TRUE(WasOneOff());
}

TEST_F(OmahaServiceTest, OneOffCallbackUsedOnlyOnce) {
  const base::Time now = base::Time::Now();
  base::test::TestFuture<OmahaPersistentState> future(TestFutureMode::kQueue);
  OmahaService service(false);
  service.StartInternal(
      OmahaPersistentState{.last_sent_version = version_info::GetVersion()},
      GetPendingSharedURLLoaderFactoryCallback(),
      base::BindRepeating(&OmahaServiceTest::OnNeedUpdate,
                          base::Unretained(this)),
      future.GetRepeatingCallback<const OmahaPersistentState&>());

  service.CheckNowOnIOThread(
      base::BindOnce(&OmahaServiceTest::OneOffCheck, base::Unretained(this)));

  {
    ASSERT_TRUE(future.IsReady());
    const OmahaPersistentState captured_state = TakeLastValue(future);
    EXPECT_EQ(captured_state.number_of_failures, 1);
    EXPECT_TRUE(captured_state.last_ping_time.is_null());
    EXPECT_GE(captured_state.next_ping_time, now + base::Minutes(54));
    EXPECT_LE(captured_state.next_ping_time, now + base::Hours(7));
  }

  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), GetResponseSuccess());

  {
    ASSERT_TRUE(future.IsReady());
    const OmahaPersistentState captured_state = TakeLastValue(future);
    EXPECT_EQ(captured_state.number_of_failures, 0);
    EXPECT_FALSE(captured_state.last_ping_time.is_null());
    EXPECT_EQ(captured_state.last_ping_time, captured_state.next_ping_time);
    EXPECT_GT(captured_state.last_response_time, now);
    EXPECT_EQ(captured_state.last_server_date, 4088);
  }

  EXPECT_FALSE(NeedUpdate());
  EXPECT_TRUE(WasOneOff());

  service.SendPing();

  // State must have been saved.
  EXPECT_TRUE(IsReadyAndClear(future));

  pending_request = test_url_loader_factory_.GetPendingRequest(0);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), GetResponseSuccess());

  EXPECT_FALSE(NeedUpdate());
  EXPECT_FALSE(WasOneOff());

  // State must have been saved.
  EXPECT_TRUE(IsReadyAndClear(future));
}

TEST_F(OmahaServiceTest, ScheduledPingDuringOneOffDropped) {
  const base::Time now = base::Time::Now();
  base::test::TestFuture<OmahaPersistentState> future(TestFutureMode::kQueue);
  OmahaService service(false);
  service.StartInternal(
      OmahaPersistentState{.last_sent_version = version_info::GetVersion()},
      GetPendingSharedURLLoaderFactoryCallback(),
      base::BindRepeating(&OmahaServiceTest::OnNeedUpdate,
                          base::Unretained(this)),
      future.GetRepeatingCallback<const OmahaPersistentState&>());

  service.CheckNowOnIOThread(
      base::BindOnce(&OmahaServiceTest::OneOffCheck, base::Unretained(this)));

  {
    ASSERT_TRUE(future.IsReady());
    const OmahaPersistentState captured_state = TakeLastValue(future);
    EXPECT_EQ(captured_state.number_of_failures, 1);
    EXPECT_TRUE(captured_state.last_ping_time.is_null());
    EXPECT_GE(captured_state.next_ping_time, now + base::Minutes(54));
    EXPECT_LE(captured_state.next_ping_time, now + base::Hours(7));
  }

  service.SendPing();

  // Ping during one-off should be dropped, nothing should change.
  EXPECT_FALSE(IsReadyAndClear(future));

  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), GetResponseSuccess());

  {
    ASSERT_TRUE(future.IsReady());
    const OmahaPersistentState captured_state = TakeLastValue(future);
    EXPECT_EQ(captured_state.number_of_failures, 0);
    EXPECT_FALSE(captured_state.last_ping_time.is_null());
    EXPECT_EQ(captured_state.last_ping_time, captured_state.next_ping_time);
    EXPECT_GT(captured_state.last_response_time, now);
    EXPECT_EQ(captured_state.last_server_date, 4088);
  }

  EXPECT_FALSE(NeedUpdate());
  EXPECT_TRUE(WasOneOff());
}

TEST_F(OmahaServiceTest, ParseAndEchoLastServerDate) {
  base::test::TestFuture<OmahaPersistentState> future(TestFutureMode::kQueue);
  OmahaService service(false);
  service.StartInternal(
      OmahaPersistentState{.last_sent_version = version_info::GetVersion()},
      GetPendingSharedURLLoaderFactoryCallback(),
      base::BindRepeating(&OmahaServiceTest::OnNeedUpdate,
                          base::Unretained(this)),
      future.GetRepeatingCallback<const OmahaPersistentState&>());

  service.SendPing();
  future.Clear();

  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), GetResponseSuccess());

  {
    ASSERT_TRUE(future.IsReady());
    const OmahaPersistentState captured_state = TakeLastValue(future);
    EXPECT_EQ(captured_state.last_server_date, 4088);
  }

  const char* expectedResult =
      "<request protocol=\"3.0\" updater=\"iOS\" updaterversion=\"[^\"]*\""
      " updaterchannel=\"[^\"]*\" ismachine=\"1\" requestid=\"requestId\""
      " sessionid=\"sessionId\" hardware_class=\"[^\"]*\">"
      "<os platform=\"ios\" version=\"[0-9][0-9]*\\(\\.[0-9][0-9]*\\)*\""
      " arch=\"[^\"]*\"/>"
      "<app brand=\"[A-Z][A-Z][A-Z][A-Z]\" appid=\"{[^}]*}\" version=\"[^\"]*\""
      " nextversion=\"\" ap=\"[^\"]*\" lang=\"[^\"]*\" client=\"\">"
      "<updatecheck/>"
      "<ping active=\"1\" ad=\"4088\" rd=\"4088\"/></app></request>";

  std::string content = service.GetPingContent(
      "requestId", "sessionId", std::string(version_info::GetVersionNumber()),
      GetChannelString(), base::Time::FromTimeT(kUnknownInstallDate),
      OmahaPingEvent::kUsagePing);
  regex_t regex;
  regcomp(&regex, expectedResult, REG_NOSUB);
  int result = regexec(&regex, content.c_str(), 0, nullptr, 0);
  regfree(&regex);
  EXPECT_EQ(0, result) << "Actual contents: " << content;
}

TEST_F(OmahaServiceTest, SendInstallEventSuccess) {
  const base::Time now = base::Time::Now();
  base::test::TestFuture<OmahaPersistentState> future(TestFutureMode::kQueue);
  OmahaService service(false);
  service.StartInternal(
      OmahaPersistentState{}, GetPendingSharedURLLoaderFactoryCallback(),
      base::BindRepeating(&OmahaServiceTest::OnNeedUpdate,
                          base::Unretained(this)),
      future.GetRepeatingCallback<const OmahaPersistentState&>());

  service.SendPing();

  {
    ASSERT_TRUE(future.IsReady());
    const OmahaPersistentState captured_state = TakeLastValue(future);
    EXPECT_EQ(captured_state.number_of_failures, 1);
    EXPECT_TRUE(captured_state.last_ping_time.is_null());
    EXPECT_GE(captured_state.next_ping_time, now + base::Minutes(54));
    EXPECT_LE(captured_state.next_ping_time, now + base::Hours(7));
  }

  std::string response =
      std::string(
          "<?xml version=\"1.0\"?><response protocol=\"3.0\" server=\"prod\">"
          "<daystart elapsed_seconds=\"56754\"/><app appid=\"") +
      test_application_id() +
      "\" status=\"ok\">"
      "<event status=\"ok\"/>"
      "<ping status=\"ok\"/>"
      "</app></response>";

  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), response);

  {
    ASSERT_TRUE(future.IsReady());
    const OmahaPersistentState captured_state = TakeLastValue(future);
    EXPECT_FALSE(captured_state.last_ping_time.is_null());
    EXPECT_GT(captured_state.last_response_time, now);
  }

  EXPECT_FALSE(NeedUpdate());
}

TEST_F(OmahaServiceTest, SendPingReceiveUpdate) {
  const base::Time now = base::Time::Now();
  base::test::TestFuture<OmahaPersistentState> future(TestFutureMode::kQueue);
  OmahaService service(false);
  service.StartInternal(
      OmahaPersistentState{.last_sent_version = version_info::GetVersion()},
      GetPendingSharedURLLoaderFactoryCallback(),
      base::BindRepeating(&OmahaServiceTest::OnNeedUpdate,
                          base::Unretained(this)),
      future.GetRepeatingCallback<const OmahaPersistentState&>());

  service.SendPing();

  {
    ASSERT_TRUE(future.IsReady());
    const OmahaPersistentState captured_state = TakeLastValue(future);
    EXPECT_EQ(captured_state.number_of_failures, 1);
    EXPECT_TRUE(captured_state.last_ping_time.is_null());
    EXPECT_GE(captured_state.next_ping_time, now + base::Minutes(54));
    EXPECT_LE(captured_state.next_ping_time, now + base::Hours(7));
  }

  std::string response =
      std::string(
          "<?xml version=\"1.0\"?><response protocol=\"3.0\" server=\"prod\">"
          "<daystart elapsed_seconds=\"56754\"/><app appid=\"") +
      test_application_id() +
      "\" status=\"ok\">"
      "<updatecheck status=\"ok\"><urls>"
      "<url codebase=\"http://www.goo.fr/foo/\"/></urls>"
      "<manifest version=\"0.0.1075.1441\">"
      "<packages>"
      "<package hash=\"0\" name=\"Chrome\" required=\"true\" size=\"0\"/>"
      "</packages>"
      "<actions>"
      "<action event=\"update\" run=\"Chrome\"/>"
      "<action event=\"postinstall\" osminversion=\"6.0\"/>"
      "</actions>"
      "</manifest>"
      "</updatecheck><ping status=\"ok\"/>"
      "</app></response>";
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), response);

  {
    ASSERT_TRUE(future.IsReady());
    const OmahaPersistentState captured_state = TakeLastValue(future);
    EXPECT_EQ(captured_state.number_of_failures, 0);
    EXPECT_FALSE(captured_state.last_ping_time.is_null());
    EXPECT_EQ(captured_state.last_ping_time, captured_state.next_ping_time);
    EXPECT_GT(captured_state.last_response_time, now);
  }

  EXPECT_TRUE(NeedUpdate());
}

TEST_F(OmahaServiceTest, SendPingFailure) {
  const base::Time now = base::Time::Now();
  base::test::TestFuture<OmahaPersistentState> future(TestFutureMode::kQueue);
  OmahaService service(false);
  service.StartInternal(
      OmahaPersistentState{.last_sent_version = version_info::GetVersion()},
      GetPendingSharedURLLoaderFactoryCallback(),
      base::BindRepeating(&OmahaServiceTest::OnNeedUpdate,
                          base::Unretained(this)),
      future.GetRepeatingCallback<const OmahaPersistentState&>());

  service.SendPing();

  base::Time next_tries_time;
  {
    // Tries with a non 200 result.
    ASSERT_TRUE(future.IsReady());
    const OmahaPersistentState captured_state = TakeLastValue(future);
    EXPECT_EQ(captured_state.number_of_failures, 1);
    EXPECT_TRUE(captured_state.last_ping_time.is_null());
    EXPECT_GE(captured_state.next_ping_time, now + base::Minutes(54));
    EXPECT_LE(captured_state.next_ping_time, now + base::Hours(7));
    next_tries_time = captured_state.next_ping_time;
  }

  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  auto url_response_head =
      network::CreateURLResponseHead(net::HTTP_BAD_REQUEST);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url, network::URLLoaderCompletionStatus(net::OK),
      std::move(url_response_head), std::string());

  EXPECT_FALSE(future.IsReady());
  EXPECT_FALSE(NeedUpdate());

  // Tries with an incorrect xml message.
  service.SendPing();

  {
    ASSERT_TRUE(future.IsReady());
    const OmahaPersistentState captured_state = TakeLastValue(future);
    EXPECT_EQ(captured_state.number_of_failures, 2);
    EXPECT_TRUE(captured_state.last_ping_time.is_null());
    EXPECT_GE(captured_state.next_ping_time, now + base::Minutes(54));
    EXPECT_LE(captured_state.next_ping_time, now + base::Hours(7));
    next_tries_time = captured_state.next_ping_time;
  }

  pending_request = test_url_loader_factory_.GetPendingRequest(0);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "Incorrect Message");

  EXPECT_FALSE(future.IsReady());
  EXPECT_FALSE(NeedUpdate());
}

TEST_F(OmahaServiceTest, PersistStatesTest) {
  const base::Time now = base::Time::Now();
  base::test::TestFuture<OmahaPersistentState> future(TestFutureMode::kQueue);
  OmahaService service(false);
  service.StartInternal(
      OmahaPersistentState{.next_ping_time = now + base::Seconds(2),
                           .last_ping_time = now + base::Seconds(3),
                           .last_response_time = now - base::Seconds(1),
                           .last_sent_version = version_info::GetVersion(),
                           .number_of_failures = 5},
      GetPendingSharedURLLoaderFactoryCallback(),
      base::BindRepeating(&OmahaServiceTest::OnNeedUpdate,
                          base::Unretained(this)),
      future.GetRepeatingCallback<const OmahaPersistentState&>());

  service.PersistStates();

  {
    ASSERT_TRUE(future.IsReady());
    const OmahaPersistentState captured_state = TakeLastValue(future);
    EXPECT_EQ(captured_state.number_of_failures, 5);
    EXPECT_EQ(captured_state.last_response_time, now - base::Seconds(1));
    EXPECT_EQ(captured_state.next_ping_time, now + base::Seconds(2));
    EXPECT_EQ(captured_state.last_ping_time, now + base::Seconds(3));
    EXPECT_EQ(captured_state.last_sent_version, version_info::GetVersion());
  }
}

TEST_F(OmahaServiceTest, BackoffTest) {
  for (int i = 1; i < 100; ++i) {
    // Testing multiple times for a given number of retries, as the method has
    // a random part.
    for (int j = 0; j < 2; ++j) {
      EXPECT_GE(OmahaService::GetBackOff(i), base::Hours(1) - base::Minutes(6));
      EXPECT_LE(OmahaService::GetBackOff(i), base::Hours(6));
    }
  }
}

// Tests that an active ping is scheduled immediately after a successful install
// event send.
TEST_F(OmahaServiceTest, ActivePingAfterInstallEventTest) {
  const base::Time now = base::Time::Now();
  base::test::TestFuture<OmahaPersistentState> future(TestFutureMode::kQueue);
  OmahaService service(false);
  service.StartInternal(
      OmahaPersistentState{}, GetPendingSharedURLLoaderFactoryCallback(),
      base::BindRepeating(&OmahaServiceTest::OnNeedUpdate,
                          base::Unretained(this)),
      future.GetRepeatingCallback<const OmahaPersistentState&>());

  service.SendPing();

  {
    ASSERT_TRUE(future.IsReady());
    const OmahaPersistentState captured_state = TakeLastValue(future);
    EXPECT_EQ(captured_state.number_of_failures, 1);
    EXPECT_TRUE(captured_state.last_ping_time.is_null());
    EXPECT_GE(captured_state.next_ping_time, now + base::Minutes(54));
    EXPECT_LE(captured_state.next_ping_time, now + base::Hours(7));
  }

  std::string response =
      std::string(
          "<?xml version=\"1.0\"?><response protocol=\"3.0\" server=\"prod\">"
          "<daystart elapsed_seconds=\"0\"/><app appid=\"") +
      test_application_id() +
      "\" status=\"ok\">"
      "<event status=\"ok\"/>"
      "<ping status=\"ok\"/>"
      "</app></response>";
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), response);

  {
    ASSERT_TRUE(future.IsReady());
    const OmahaPersistentState captured_state = TakeLastValue(future);
    EXPECT_EQ(captured_state.number_of_failures, 1);
    EXPECT_LT(captured_state.last_ping_time - now, base::Minutes(1));
    EXPECT_GT(captured_state.next_ping_time, captured_state.last_ping_time);
  }

  EXPECT_FALSE(NeedUpdate());
}

// Tests that active pings are not sent in rapid succession.
TEST_F(OmahaServiceTest, NonSpammingTest) {
  const base::Time now = base::Time::Now();
  base::test::TestFuture<OmahaPersistentState> future(TestFutureMode::kQueue);
  OmahaService service(false);
  service.StartInternal(
      OmahaPersistentState{.last_sent_version = version_info::GetVersion()},
      GetPendingSharedURLLoaderFactoryCallback(),
      base::BindRepeating(&OmahaServiceTest::OnNeedUpdate,
                          base::Unretained(this)),
      future.GetRepeatingCallback<const OmahaPersistentState&>());

  service.SendPing();

  {
    ASSERT_TRUE(future.IsReady());
    const OmahaPersistentState captured_state = TakeLastValue(future);
    EXPECT_EQ(captured_state.number_of_failures, 1);
    EXPECT_TRUE(captured_state.last_ping_time.is_null());
    EXPECT_GE(captured_state.next_ping_time, now + base::Minutes(54));
    EXPECT_LE(captured_state.next_ping_time, now + base::Hours(7));
  }

  std::string response =
      std::string(
          "<?xml version=\"1.0\"?><response protocol=\"3.0\" server=\"prod\">"
          "<daystart elapsed_seconds=\"0\"/><app appid=\"") +
      test_application_id() +
      "\" status=\"ok\">"
      "<updatecheck status=\"noupdate\"/><ping status=\"ok\"/>"
      "</app></response>";
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), response);

  {
    ASSERT_TRUE(future.IsReady());
    const OmahaPersistentState captured_state = TakeLastValue(future);
    EXPECT_EQ(captured_state.number_of_failures, 0);
    EXPECT_FALSE(captured_state.last_ping_time.is_null());
    EXPECT_GE(captured_state.next_ping_time - now, base::Hours(2));
    EXPECT_GT(captured_state.last_response_time, now);
  }

  EXPECT_FALSE(NeedUpdate());
}

TEST_F(OmahaServiceTest, InstallRetryTest) {
  OmahaService service(false);
  service.StartInternal(OmahaPersistentState{},
                        GetPendingSharedURLLoaderFactoryCallback(),
                        base::BindRepeating(&OmahaServiceTest::OnNeedUpdate,
                                            base::Unretained(this)),
                        base::DoNothing());

  EXPECT_FALSE(service.IsNextPingInstallRetry());
  std::string id1 = service.GetNextPingRequestId(OmahaPingEvent::kInstallEvent);
  EXPECT_TRUE(service.IsNextPingInstallRetry());
  ASSERT_EQ(id1, service.GetNextPingRequestId(OmahaPingEvent::kInstallEvent));

  service.SendPing();

  std::string response =
      std::string(
          "<?xml version=\"1.0\"?><response protocol=\"3.0\" server=\"prod\">"
          "<daystart elapsed_seconds=\"56754\"/><app appid=\"") +
      test_application_id() +
      "\" status=\"ok\">"
      "<updatecheck status=\"noupdate\"/><ping status=\"ok\"/>"
      "</app></response>";
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), response);

  EXPECT_FALSE(service.IsNextPingInstallRetry());
  id1 = service.GetNextPingRequestId(OmahaPingEvent::kUsagePing);
  ASSERT_NE(id1, service.GetNextPingRequestId(OmahaPingEvent::kUsagePing));
}

TEST_F(OmahaServiceTest, ResyncTimerAfterSystemSuspend) {
  base::test::TestFuture<OmahaPersistentState> future(TestFutureMode::kQueue);
  OmahaService service(true);
  service.StartInternal(
      OmahaPersistentState{.last_sent_version = version_info::GetVersion()},
      GetPendingSharedURLLoaderFactoryCallback(),
      base::BindRepeating(&OmahaServiceTest::OnNeedUpdate,
                          base::Unretained(this)),
      future.GetRepeatingCallback<const OmahaPersistentState&>());

  {
    base::subtle::ScopedTimeClockOverrides clock_overrides(
        []() { return GetTimeWithDelta(base::Hours(0)); },
        []() { return GetTimeTicksWithDelta(base::Hours(0)); }, nullptr);

    // Sending a successful ping will schedule another ping in the future.
    service.SendPing();
    future.Clear();

    auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        pending_request->request.url.spec(), GetResponseSuccess());

    ASSERT_TRUE(future.IsReady());
    const OmahaPersistentState captured_state = TakeLastValue(future);
    EXPECT_GE(TimerRemainingTime(&service), base::Minutes(299));
    EXPECT_EQ(captured_state.last_response_time, base::Time::Now());
  }

  // Simulate two hours of system suspend time.
  {
    base::subtle::ScopedTimeClockOverrides clock_overrides(
        []() { return GetTimeWithDelta(base::Hours(2)); },
        []() { return GetTimeTicksWithDelta(base::Hours(0)); }, nullptr);

    // Before the resync, the timer should still have ~5 hours of
    // running time left.
    EXPECT_GT(TimerRemainingTime(&service), base::Minutes(299));
    EXPECT_LT(TimerRemainingTime(&service), base::Minutes(301));

    // After the resync, there should be ~3 hours of running time
    // left, since the wall clock advanced by 2 hours.
    service.ResyncTimerIfNeeded();

    EXPECT_GT(TimerRemainingTime(&service), base::Minutes(179));
    EXPECT_LT(TimerRemainingTime(&service), base::Minutes(181));
    ASSERT_FALSE(future.IsReady());
  }

  // Simulate six hours of wall clock time, two hours of that suspended.
  {
    base::subtle::ScopedTimeClockOverrides clock_overrides(
        []() { return GetTimeWithDelta(base::Hours(6)); },
        []() { return GetTimeTicksWithDelta(base::Hours(2)); }, nullptr);

    // Before the resync, the timer should still have ~1 hour of
    // running time left.
    EXPECT_GT(TimerRemainingTime(&service), base::Minutes(59));
    EXPECT_LT(TimerRemainingTime(&service), base::Minutes(61));

    // After the resync, the timer should fire.
    service.ResyncTimerIfNeeded();
    auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        pending_request->request.url.spec(), GetResponseSuccess());

    ASSERT_TRUE(future.IsReady());
    const OmahaPersistentState captured_state = TakeLastValue(future);
    EXPECT_GT(TimerRemainingTime(&service), base::Minutes(299));
    EXPECT_LT(TimerRemainingTime(&service), base::Minutes(301));
    EXPECT_EQ(captured_state.last_response_time, base::Time::Now());
  }

  // Spin the runloop to clear any pending tasks.
  base::RunLoop().RunUntilIdle();
}
