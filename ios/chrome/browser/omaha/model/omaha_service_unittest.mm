// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omaha/model/omaha_service.h"

#import <regex.h>
#import <sys/types.h>

#import "base/check_op.h"
#import "base/functional/bind.h"
#import "base/run_loop.h"
#import "base/strings/stringprintf.h"
#import "base/task/sequenced_task_runner.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "components/metrics/metrics_pref_names.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
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

const int64_t kUnknownInstallDate = 2;

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
    OmahaService::ClearPersistentStateForTests();
  }

  OmahaServiceTest(const OmahaServiceTest&) = delete;
  OmahaServiceTest& operator=(const OmahaServiceTest&) = delete;

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

  void CleanService(OmahaService* service,
                    const std::string& last_sent_version) {
    service->ClearInstallRetryRequestId();
    service->number_of_tries_ = 0;
    if (last_sent_version.length() == 0)
      service->last_sent_version_ = base::Version("0.0.0.0");
    else
      service->last_sent_version_ = base::Version(last_sent_version);
    service->current_ping_time_ = base::Time();
    service->last_sent_time_ = base::Time();
    service->locale_lang_ = std::string();
  }

  std::string test_application_id() const {
    return ios::provider::GetOmahaApplicationId();
  }

 protected:
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;

 private:
  bool need_update_ = false;
  bool was_one_off_ = false;
  bool scheduled_callback_used_ = false;
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
};

TEST_F(OmahaServiceTest, PingMessageTest) {
  const char* expectedResult =
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
  service.StartInternal(base::SequencedTaskRunner::GetCurrentDefault());

  service.set_upgrade_recommended_callback(base::BindRepeating(
      &OmahaServiceTest::OnNeedUpdate, base::Unretained(this)));
  std::string content = service.GetPingContent(
      "requestId", "sessionId", std::string(version_info::GetVersionNumber()),
      GetChannelString(), base::Time::Now(), OmahaService::USAGE_PING);
  regex_t regex;
  regcomp(&regex, expectedResult, REG_NOSUB);
  int result = regexec(&regex, content.c_str(), 0, NULL, 0);
  regfree(&regex);
  EXPECT_EQ(0, result) << "Actual contents: " << content;
  EXPECT_FALSE(NeedUpdate());
}

TEST_F(OmahaServiceTest, PingMessageTestWithUnknownInstallDate) {
  const char* expectedResult =
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
  service.StartInternal(base::SequencedTaskRunner::GetCurrentDefault());

  service.set_upgrade_recommended_callback(base::BindRepeating(
      &OmahaServiceTest::OnNeedUpdate, base::Unretained(this)));
  std::string content = service.GetPingContent(
      "requestId", "sessionId", std::string(version_info::GetVersionNumber()),
      GetChannelString(), base::Time::FromTimeT(kUnknownInstallDate),
      OmahaService::USAGE_PING);
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

  // First install.
  OmahaService service(false);
  service.StartInternal(base::SequencedTaskRunner::GetCurrentDefault());

  service.set_upgrade_recommended_callback(base::BindRepeating(
      &OmahaServiceTest::OnNeedUpdate, base::Unretained(this)));
  CleanService(&service, "");
  std::string content = service.GetPingContent(
      "requestId", "sessionId", std::string(version_info::GetVersionNumber()),
      GetChannelString(), base::Time::Now(), OmahaService::INSTALL_EVENT);
  regmatch_t matches[2];
  regex_t regex;
  std::string expected_result =
      base::StringPrintf(kExpectedResultFormat, "" /* previous version */,
                         -1 /* install age */, 2 /* event type */);
  regcomp(&regex, expected_result.c_str(), REG_EXTENDED);
  int result = regexec(&regex, content.c_str(), std::size(matches), matches, 0);
  regfree(&regex);
  EXPECT_EQ(0, result) << "Actual contents: " << content;
  EXPECT_FALSE(NeedUpdate());

  // Update install.
  const char* kPreviousVersion = "0.5";
  CleanService(&service, kPreviousVersion);
  content = service.GetPingContent(
      "requestId", "sessionId", std::string(version_info::GetVersionNumber()),
      GetChannelString(), base::Time::Now(), OmahaService::INSTALL_EVENT);
  expected_result = base::StringPrintf(kExpectedResultFormat, kPreviousVersion,
                                       0 /* install age */, 3 /* event type */);
  regcomp(&regex, expected_result.c_str(), REG_EXTENDED);
  result = regexec(&regex, content.c_str(), std::size(matches), matches, 0);
  regfree(&regex);
  EXPECT_EQ(0, result) << "Actual contents: " << content;
  EXPECT_FALSE(NeedUpdate());
}

TEST_F(OmahaServiceTest, SendPingSuccess) {
  base::Time now = base::Time::Now();
  OmahaService service(false);
  service.StartInternal(base::SequencedTaskRunner::GetCurrentDefault());

  service.set_upgrade_recommended_callback(base::BindRepeating(
      &OmahaServiceTest::OnNeedUpdate, base::Unretained(this)));
  service.InitializeURLLoaderFactory(test_shared_url_loader_factory_);
  CleanService(&service, std::string(version_info::GetVersionNumber()));

  service.SendPing();

  EXPECT_EQ(1, service.number_of_tries_);
  EXPECT_TRUE(service.current_ping_time_.is_null());
  EXPECT_GE(service.next_tries_time_, now + base::Minutes(54));
  EXPECT_LE(service.next_tries_time_, now + base::Hours(7));

  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), GetResponseSuccess());

  EXPECT_EQ(0, service.number_of_tries_);
  EXPECT_FALSE(service.current_ping_time_.is_null());
  EXPECT_EQ(service.current_ping_time_, service.next_tries_time_);
  EXPECT_GT(service.last_sent_time_, now);
  EXPECT_EQ(4088, service.last_server_date_);
  EXPECT_FALSE(NeedUpdate());
  EXPECT_FALSE(ScheduledCallbackUsed());
}

TEST_F(OmahaServiceTest, PingUpToDateUpdatesUserDefaults) {
  OmahaService service(false);
  service.StartInternal(base::SequencedTaskRunner::GetCurrentDefault());

  service.set_upgrade_recommended_callback(base::BindRepeating(
      &OmahaServiceTest::OnNeedUpdate, base::Unretained(this)));
  service.InitializeURLLoaderFactory(test_shared_url_loader_factory_);
  CleanService(&service, std::string(version_info::GetVersionNumber()));

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
  service.StartInternal(base::SequencedTaskRunner::GetCurrentDefault());

  service.set_upgrade_recommended_callback(base::BindRepeating(
      &OmahaServiceTest::OnNeedUpdate, base::Unretained(this)));
  service.InitializeURLLoaderFactory(test_shared_url_loader_factory_);
  CleanService(&service, std::string(version_info::GetVersionNumber()));

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
  base::Time now = base::Time::Now();
  OmahaService service(false);
  service.StartInternal(base::SequencedTaskRunner::GetCurrentDefault());

  service.set_upgrade_recommended_callback(base::BindRepeating(
      &OmahaServiceTest::OnNeedUpdate, base::Unretained(this)));
  service.InitializeURLLoaderFactory(test_shared_url_loader_factory_);
  CleanService(&service, std::string(version_info::GetVersionNumber()));

  service.SendPing();

  EXPECT_EQ(1, service.number_of_tries_);
  EXPECT_TRUE(service.current_ping_time_.is_null());
  EXPECT_GE(service.next_tries_time_, now + base::Minutes(54));
  EXPECT_LE(service.next_tries_time_, now + base::Hours(7));

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
  base::Time now = base::Time::Now();
  OmahaService service(false);
  service.StartInternal(base::SequencedTaskRunner::GetCurrentDefault());

  service.set_upgrade_recommended_callback(base::BindRepeating(
      &OmahaServiceTest::OnNeedUpdate, base::Unretained(this)));
  service.InitializeURLLoaderFactory(test_shared_url_loader_factory_);

  service.one_off_check_callback_ =
      base::BindOnce(^(UpgradeRecommendedDetails details) {
        OmahaServiceTest::OneOffCheck(details);
      });
  CleanService(&service, std::string(version_info::GetVersionNumber()));

  service.SendPing();

  EXPECT_EQ(1, service.number_of_tries_);
  EXPECT_TRUE(service.current_ping_time_.is_null());
  EXPECT_GE(service.next_tries_time_, now + base::Minutes(54));
  EXPECT_LE(service.next_tries_time_, now + base::Hours(7));

  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), GetResponseSuccess());

  EXPECT_EQ(0, service.number_of_tries_);
  EXPECT_FALSE(service.current_ping_time_.is_null());
  EXPECT_EQ(service.current_ping_time_, service.next_tries_time_);
  EXPECT_GT(service.last_sent_time_, now);
  EXPECT_EQ(4088, service.last_server_date_);
  EXPECT_FALSE(NeedUpdate());
  EXPECT_TRUE(WasOneOff());
}

TEST_F(OmahaServiceTest, OngoingPingOneOffCallbackUsed) {
  base::Time now = base::Time::Now();
  OmahaService service(false);
  service.StartInternal(base::SequencedTaskRunner::GetCurrentDefault());

  service.set_upgrade_recommended_callback(base::BindRepeating(
      &OmahaServiceTest::OnNeedUpdate, base::Unretained(this)));
  service.InitializeURLLoaderFactory(test_shared_url_loader_factory_);
  CleanService(&service, std::string(version_info::GetVersionNumber()));

  service.SendPing();

  EXPECT_EQ(1, service.number_of_tries_);
  EXPECT_TRUE(service.current_ping_time_.is_null());
  EXPECT_GE(service.next_tries_time_, now + base::Minutes(54));
  EXPECT_LE(service.next_tries_time_, now + base::Hours(7));

  // One off callback set during ongoing ping, it should now be used for
  // response.
  service.one_off_check_callback_ =
      base::BindOnce(^(UpgradeRecommendedDetails details) {
        OmahaServiceTest::OneOffCheck(details);
      });

  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), GetResponseSuccess());

  EXPECT_EQ(0, service.number_of_tries_);
  EXPECT_FALSE(service.current_ping_time_.is_null());
  EXPECT_EQ(service.current_ping_time_, service.next_tries_time_);
  EXPECT_GT(service.last_sent_time_, now);
  EXPECT_EQ(4088, service.last_server_date_);
  EXPECT_FALSE(NeedUpdate());
  EXPECT_TRUE(WasOneOff());
}

TEST_F(OmahaServiceTest, OneOffCallbackUsedOnlyOnce) {
  base::Time now = base::Time::Now();
  OmahaService service(false);
  service.StartInternal(base::SequencedTaskRunner::GetCurrentDefault());

  service.set_upgrade_recommended_callback(base::BindRepeating(
      &OmahaServiceTest::OnNeedUpdate, base::Unretained(this)));
  service.InitializeURLLoaderFactory(test_shared_url_loader_factory_);

  service.one_off_check_callback_ =
      base::BindOnce(^(UpgradeRecommendedDetails details) {
        OmahaServiceTest::OneOffCheck(details);
      });
  CleanService(&service, std::string(version_info::GetVersionNumber()));

  service.SendPing();

  EXPECT_EQ(1, service.number_of_tries_);
  EXPECT_TRUE(service.current_ping_time_.is_null());
  EXPECT_GE(service.next_tries_time_, now + base::Minutes(54));
  EXPECT_LE(service.next_tries_time_, now + base::Hours(7));

  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), GetResponseSuccess());

  EXPECT_EQ(0, service.number_of_tries_);
  EXPECT_FALSE(service.current_ping_time_.is_null());
  EXPECT_EQ(service.current_ping_time_, service.next_tries_time_);
  EXPECT_GT(service.last_sent_time_, now);
  EXPECT_EQ(4088, service.last_server_date_);
  EXPECT_FALSE(NeedUpdate());
  EXPECT_TRUE(WasOneOff());

  service.SendPing();

  pending_request = test_url_loader_factory_.GetPendingRequest(0);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), GetResponseSuccess());

  EXPECT_FALSE(NeedUpdate());
  EXPECT_FALSE(WasOneOff());
}

TEST_F(OmahaServiceTest, ScheduledPingDuringOneOffDropped) {
  base::Time now = base::Time::Now();
  OmahaService service(false);
  service.StartInternal(base::SequencedTaskRunner::GetCurrentDefault());

  service.set_upgrade_recommended_callback(base::BindRepeating(
      &OmahaServiceTest::OnNeedUpdate, base::Unretained(this)));
  service.InitializeURLLoaderFactory(test_shared_url_loader_factory_);

  service.one_off_check_callback_ =
      base::BindOnce(^(UpgradeRecommendedDetails details) {
        OmahaServiceTest::OneOffCheck(details);
      });
  CleanService(&service, std::string(version_info::GetVersionNumber()));

  service.SendPing();

  EXPECT_EQ(1, service.number_of_tries_);
  EXPECT_TRUE(service.current_ping_time_.is_null());
  EXPECT_GE(service.next_tries_time_, now + base::Minutes(54));
  EXPECT_LE(service.next_tries_time_, now + base::Hours(7));

  service.SendPing();

  // Ping during one-off should be dropped, nothing should change.
  EXPECT_EQ(1, service.number_of_tries_);
  EXPECT_TRUE(service.current_ping_time_.is_null());
  EXPECT_GE(service.next_tries_time_, now + base::Minutes(54));
  EXPECT_LE(service.next_tries_time_, now + base::Hours(7));

  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), GetResponseSuccess());

  EXPECT_EQ(0, service.number_of_tries_);
  EXPECT_FALSE(service.current_ping_time_.is_null());
  EXPECT_EQ(service.current_ping_time_, service.next_tries_time_);
  EXPECT_GT(service.last_sent_time_, now);
  EXPECT_EQ(4088, service.last_server_date_);
  EXPECT_FALSE(NeedUpdate());
  EXPECT_TRUE(WasOneOff());
}

TEST_F(OmahaServiceTest, ParseAndEchoLastServerDate) {
  OmahaService service(false);
  service.StartInternal(base::SequencedTaskRunner::GetCurrentDefault());

  service.set_upgrade_recommended_callback(base::BindRepeating(
      &OmahaServiceTest::OnNeedUpdate, base::Unretained(this)));
  service.InitializeURLLoaderFactory(test_shared_url_loader_factory_);
  CleanService(&service, std::string(version_info::GetVersionNumber()));

  service.SendPing();

  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), GetResponseSuccess());

  EXPECT_EQ(4088, service.last_server_date_);

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
      OmahaService::USAGE_PING);
  regex_t regex;
  regcomp(&regex, expectedResult, REG_NOSUB);
  int result = regexec(&regex, content.c_str(), 0, nullptr, 0);
  regfree(&regex);
  EXPECT_EQ(0, result) << "Actual contents: " << content;
}

TEST_F(OmahaServiceTest, SendInstallEventSuccess) {
  base::Time now = base::Time::Now();
  OmahaService service(false);
  service.StartInternal(base::SequencedTaskRunner::GetCurrentDefault());

  service.set_upgrade_recommended_callback(base::BindRepeating(
      &OmahaServiceTest::OnNeedUpdate, base::Unretained(this)));
  service.InitializeURLLoaderFactory(test_shared_url_loader_factory_);
  CleanService(&service, "");

  service.SendPing();

  EXPECT_EQ(1, service.number_of_tries_);
  EXPECT_TRUE(service.current_ping_time_.is_null());
  EXPECT_GE(service.next_tries_time_, now + base::Minutes(54));
  EXPECT_LE(service.next_tries_time_, now + base::Hours(7));

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

  EXPECT_FALSE(service.current_ping_time_.is_null());
  EXPECT_GT(service.last_sent_time_, now);
  EXPECT_FALSE(NeedUpdate());
}

TEST_F(OmahaServiceTest, SendPingReceiveUpdate) {
  base::Time now = base::Time::Now();
  OmahaService service(false);
  service.StartInternal(base::SequencedTaskRunner::GetCurrentDefault());

  service.set_upgrade_recommended_callback(base::BindRepeating(
      &OmahaServiceTest::OnNeedUpdate, base::Unretained(this)));
  service.InitializeURLLoaderFactory(test_shared_url_loader_factory_);
  CleanService(&service, std::string(version_info::GetVersionNumber()));

  service.SendPing();

  EXPECT_EQ(1, service.number_of_tries_);
  EXPECT_TRUE(service.current_ping_time_.is_null());
  EXPECT_GE(service.next_tries_time_, now + base::Minutes(54));
  EXPECT_LE(service.next_tries_time_, now + base::Hours(7));

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

  EXPECT_EQ(0, service.number_of_tries_);
  EXPECT_FALSE(service.current_ping_time_.is_null());
  EXPECT_EQ(service.current_ping_time_, service.next_tries_time_);
  EXPECT_GT(service.last_sent_time_, now);
  EXPECT_TRUE(NeedUpdate());
}

TEST_F(OmahaServiceTest, SendPingFailure) {
  base::Time now = base::Time::Now();
  OmahaService service(false);
  service.StartInternal(base::SequencedTaskRunner::GetCurrentDefault());

  service.set_upgrade_recommended_callback(base::BindRepeating(
      &OmahaServiceTest::OnNeedUpdate, base::Unretained(this)));
  service.InitializeURLLoaderFactory(test_shared_url_loader_factory_);
  CleanService(&service, std::string(version_info::GetVersionNumber()));

  service.SendPing();

  // Tries with a non 200 result.
  EXPECT_EQ(1, service.number_of_tries_);
  EXPECT_TRUE(service.current_ping_time_.is_null());
  EXPECT_GE(service.next_tries_time_, now + base::Minutes(54));
  EXPECT_LE(service.next_tries_time_, now + base::Hours(7));
  base::Time next_tries_time = service.next_tries_time_;

  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  auto url_response_head =
      network::CreateURLResponseHead(net::HTTP_BAD_REQUEST);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url, network::URLLoaderCompletionStatus(net::OK),
      std::move(url_response_head), std::string());

  EXPECT_EQ(1, service.number_of_tries_);
  EXPECT_TRUE(service.current_ping_time_.is_null());
  EXPECT_EQ(next_tries_time, service.next_tries_time_);
  EXPECT_LT(service.last_sent_time_, now);
  EXPECT_FALSE(NeedUpdate());

  // Tries with an incorrect xml message.
  service.SendPing();

  EXPECT_EQ(2, service.number_of_tries_);
  EXPECT_TRUE(service.current_ping_time_.is_null());
  EXPECT_GE(service.next_tries_time_, now + base::Minutes(54));
  EXPECT_LE(service.next_tries_time_, now + base::Hours(7));
  next_tries_time = service.next_tries_time_;

  pending_request = test_url_loader_factory_.GetPendingRequest(0);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "Incorrect Message");

  EXPECT_EQ(2, service.number_of_tries_);
  EXPECT_TRUE(service.current_ping_time_.is_null());
  EXPECT_EQ(next_tries_time, service.next_tries_time_);
  EXPECT_LT(service.last_sent_time_, now);
  EXPECT_FALSE(NeedUpdate());
}

TEST_F(OmahaServiceTest, PersistStatesTest) {
  std::string version_string(version_info::GetVersionNumber());
  base::Time now = base::Time::Now();
  OmahaService service(false);
  service.StartInternal(base::SequencedTaskRunner::GetCurrentDefault());
  base::test::ios::SpinRunLoopWithMinDelay(base::Milliseconds(1));

  service.set_upgrade_recommended_callback(base::BindRepeating(
      &OmahaServiceTest::OnNeedUpdate, base::Unretained(this)));
  service.number_of_tries_ = 5;
  service.last_sent_time_ = now - base::Seconds(1);
  service.next_tries_time_ = now + base::Seconds(2);
  service.current_ping_time_ = now + base::Seconds(3);
  service.last_sent_version_ = base::Version(version_string);
  service.PersistStates();
  base::test::ios::SpinRunLoopWithMinDelay(base::Milliseconds(1));

  OmahaService service2(false);
  service2.StartInternal(base::SequencedTaskRunner::GetCurrentDefault());
  base::test::ios::SpinRunLoopWithMinDelay(base::Milliseconds(1));

  EXPECT_EQ(service.number_of_tries_, 5);
  EXPECT_EQ(service2.last_sent_time_, now - base::Seconds(1));
  EXPECT_EQ(service2.next_tries_time_, now + base::Seconds(2));
  EXPECT_EQ(service2.current_ping_time_, now + base::Seconds(3));
  EXPECT_EQ(service.last_sent_version_.GetString(), version_string);
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
  base::Time now = base::Time::Now();
  OmahaService service(false);
  service.StartInternal(base::SequencedTaskRunner::GetCurrentDefault());

  service.set_upgrade_recommended_callback(base::BindRepeating(
      &OmahaServiceTest::OnNeedUpdate, base::Unretained(this)));
  service.InitializeURLLoaderFactory(test_shared_url_loader_factory_);
  CleanService(&service, "");

  service.SendPing();

  EXPECT_EQ(1, service.number_of_tries_);
  EXPECT_TRUE(service.current_ping_time_.is_null());
  EXPECT_GE(service.next_tries_time_, now + base::Minutes(54));
  EXPECT_LE(service.next_tries_time_, now + base::Hours(7));

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

  EXPECT_EQ(1, service.number_of_tries_);
  EXPECT_LT(service.current_ping_time_ - now, base::Minutes(1));
  EXPECT_GT(service.next_tries_time_, service.current_ping_time_);
  EXPECT_FALSE(NeedUpdate());
}

// Tests that active pings are not sent in rapid succession.
TEST_F(OmahaServiceTest, NonSpammingTest) {
  base::Time now = base::Time::Now();
  OmahaService service(false);
  service.StartInternal(base::SequencedTaskRunner::GetCurrentDefault());

  service.set_upgrade_recommended_callback(base::BindRepeating(
      &OmahaServiceTest::OnNeedUpdate, base::Unretained(this)));
  service.InitializeURLLoaderFactory(test_shared_url_loader_factory_);
  CleanService(&service, std::string(version_info::GetVersionNumber()));

  service.SendPing();

  EXPECT_EQ(1, service.number_of_tries_);
  EXPECT_TRUE(service.current_ping_time_.is_null());
  EXPECT_GE(service.next_tries_time_, now + base::Minutes(54));
  EXPECT_LE(service.next_tries_time_, now + base::Hours(7));

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

  EXPECT_EQ(0, service.number_of_tries_);
  EXPECT_FALSE(service.current_ping_time_.is_null());
  EXPECT_GE(service.next_tries_time_ - now, base::Hours(2));
  EXPECT_GT(service.last_sent_time_, now);
  EXPECT_FALSE(NeedUpdate());
}

TEST_F(OmahaServiceTest, InstallRetryTest) {
  OmahaService service(false);
  service.StartInternal(base::SequencedTaskRunner::GetCurrentDefault());

  service.set_upgrade_recommended_callback(base::BindRepeating(
      &OmahaServiceTest::OnNeedUpdate, base::Unretained(this)));
  service.InitializeURLLoaderFactory(test_shared_url_loader_factory_);
  CleanService(&service, "");

  EXPECT_FALSE(service.IsNextPingInstallRetry());
  std::string id1 = service.GetNextPingRequestId(OmahaService::INSTALL_EVENT);
  EXPECT_TRUE(service.IsNextPingInstallRetry());
  ASSERT_EQ(id1, service.GetNextPingRequestId(OmahaService::INSTALL_EVENT));

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
  id1 = service.GetNextPingRequestId(OmahaService::USAGE_PING);
  ASSERT_NE(id1, service.GetNextPingRequestId(OmahaService::USAGE_PING));
}
