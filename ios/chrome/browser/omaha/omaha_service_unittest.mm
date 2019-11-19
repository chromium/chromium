// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omaha/omaha_service.h"

#include <regex.h>
#include <sys/types.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state_manager.h"
#include "ios/chrome/browser/install_time_util.h"
#include "ios/chrome/common/channel_info.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_state_manager.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/omaha/omaha_service_provider.h"
#include "ios/web/public/test/web_task_environment.h"
#include "ios/web/public/thread/web_thread.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char kUserDataDir[] = FILE_PATH_LITERAL(".");

}  // namespace

class OmahaServiceTest : public PlatformTest {
 public:
  OmahaServiceTest()
      : test_shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        need_update_(false),
        scoped_browser_state_manager_(
            std::make_unique<TestChromeBrowserStateManager>(
                base::FilePath(kUserDataDir))) {
    GetApplicationContext()->GetLocalState()->SetInt64(
        metrics::prefs::kInstallDate, install_time_util::kUnknownInstallDate);
    OmahaService::ClearPersistentStateForTests();
  }

  void OnNeedUpdate(const UpgradeRecommendedDetails& details) {
    need_update_ = true;
  }

  bool NeedUpdate() {
    DCHECK_CURRENTLY_ON(web::WebThread::UI);
    if (!need_update_) {
      base::RunLoop().RunUntilIdle();
    }
    return need_update_;
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
    return ios::GetChromeBrowserProvider()
        ->GetOmahaServiceProvider()
        ->GetApplicationID();
  }

 protected:
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;

 private:
  bool need_update_;
  IOSChromeScopedTestingChromeBrowserStateManager scoped_browser_state_manager_;
  web::WebTaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(OmahaServiceTest);
};

TEST_F(OmahaServiceTest, PingMessageTest) {
  const char* expectedResult =
      "<request protocol=\"3.0\" version=\"iOS-1.0.0.0\" ismachine=\"1\" "
      "requestid=\"requestId\" sessionid=\"sessionId\""
      " hardware_class=\"[^\"]*\">"
      "<os platform=\"ios\" version=\"[0-9][0-9]*\\(\\.[0-9][0-9]*\\)*\""
      " arch=\"[^\"]*\"/>"
      "<app version=\"[^\"]*\" nextversion=\"\" lang=\"[^\"]*\""
      " brand=\"[A-Z][A-Z][A-Z][A-Z]\" client=\"\" appid=\"{[^}]*}\""
      " installage=\"0\">"
      "<updatecheck tag=\"[^\"]*\"/>"
      "<ping active=\"1\"/></app></request>";

  OmahaService service(false);
  service.set_upgrade_recommended_callback(
      base::Bind(&OmahaServiceTest::OnNeedUpdate, base::Unretained(this)));
  std::string content = service.GetPingContent(
      "requestId", "sessionId", version_info::GetVersionNumber(),
      GetChannelString(), base::Time::Now(), OmahaService::USAGE_PING);
  regex_t regex;
  regcomp(&regex, expectedResult, REG_NOSUB);
  int result = regexec(&regex, content.c_str(), 0, NULL, 0);
  regfree(&regex);
  EXPECT_EQ(0, result);
  EXPECT_FALSE(NeedUpdate());
}

TEST_F(OmahaServiceTest, PingMessageTestWithUnknownInstallDate) {
  const char* expectedResult =
      "<request protocol=\"3.0\" version=\"iOS-1.0.0.0\" ismachine=\"1\" "
      "requestid=\"requestId\" sessionid=\"sessionId\""
      " hardware_class=\"[^\"]*\">"
      "<os platform=\"ios\" version=\"[0-9][0-9]*\\(\\.[0-9][0-9]*\\)*\""
      " arch=\"[^\"]*\"/>"
      "<app version=\"[^\"]*\" nextversion=\"\" lang=\"[^\"]*\""
      " brand=\"[A-Z][A-Z][A-Z][A-Z]\" client=\"\" appid=\"{[^}]*}\">"
      "<updatecheck tag=\"[^\"]*\"/>"
      "<ping active=\"1\"/></app></request>";

  OmahaService service(false);
  service.set_upgrade_recommended_callback(
      base::Bind(&OmahaServiceTest::OnNeedUpdate, base::Unretained(this)));
  std::string content = service.GetPingContent(
      "requestId", "sessionId", version_info::GetVersionNumber(),
      GetChannelString(),
      base::Time::FromTimeT(install_time_util::kUnknownInstallDate),
      OmahaService::USAGE_PING);
  regex_t regex;
  regcomp(&regex, expectedResult, REG_NOSUB);
  int result = regexec(&regex, content.c_str(), 0, NULL, 0);
  regfree(&regex);
  EXPECT_EQ(0, result);
  EXPECT_FALSE(NeedUpdate());
}

TEST_F(OmahaServiceTest, InstallEventMessageTest) {
  const char* kExpectedResultFormat =
      "<request protocol=\"3.0\" version=\"iOS-1.0.0.0\" ismachine=\"1\" "
      "requestid=\"requestId\" sessionid=\"sessionId\""
      " hardware_class=\"[^\"]*\">"
      "<os platform=\"ios\" version=\"[0-9][0-9]*(\\.[0-9][0-9]*)*\""
      " arch=\"[^\"]*\"/>"
      "<app version=\"%s\" nextversion=\"[^\"]*\" lang=\"[^\"]*\""
      " brand=\"[A-Z][A-Z][A-Z][A-Z]\" client=\"\" appid=\"{[^}]*}\""
      " installage=\"%d\">"
      "<event eventtype=\"%d\" eventresult=\"1\"/>"
      "</app></request>";

  // First install.
  OmahaService service(false);
  service.set_upgrade_recommended_callback(
      base::Bind(&OmahaServiceTest::OnNeedUpdate, base::Unretained(this)));
  CleanService(&service, "");
  std::string content = service.GetPingContent(
      "requestId", "sessionId", version_info::GetVersionNumber(),
      GetChannelString(), base::Time::Now(), OmahaService::INSTALL_EVENT);
  regmatch_t matches[2];
  regex_t regex;
  std::string expected_result =
      base::StringPrintf(kExpectedResultFormat, "" /* previous version */,
                         -1 /* install age */, 2 /* event type */);
  regcomp(&regex, expected_result.c_str(), REG_EXTENDED);
  int result =
      regexec(&regex, content.c_str(), base::size(matches), matches, 0);
  regfree(&regex);
  EXPECT_EQ(0, result) << "Actual contents: " << content;
  EXPECT_FALSE(NeedUpdate());

  // Update install.
  const char* kPreviousVersion = "0.5";
  CleanService(&service, kPreviousVersion);
  content = service.GetPingContent(
      "requestId", "sessionId", version_info::GetVersionNumber(),
      GetChannelString(), base::Time::Now(), OmahaService::INSTALL_EVENT);
  expected_result = base::StringPrintf(kExpectedResultFormat, kPreviousVersion,
                                       0 /* install age */, 3 /* event type */);
  regcomp(&regex, expected_result.c_str(), REG_EXTENDED);
  result = regexec(&regex, content.c_str(), base::size(matches), matches, 0);
  regfree(&regex);
  EXPECT_EQ(0, result) << "Actual contents: " << content;
  EXPECT_FALSE(NeedUpdate());
}

TEST_F(OmahaServiceTest, SendPingSuccess) {
  base::Time now = base::Time::Now();
  OmahaService service(false);
  service.set_upgrade_recommended_callback(
      base::Bind(&OmahaServiceTest::OnNeedUpdate, base::Unretained(this)));
  service.InitializeURLLoaderFactory(test_shared_url_loader_factory_);
  CleanService(&service, version_info::GetVersionNumber());

  service.SendPing();

  EXPECT_EQ(1, service.number_of_tries_);
  EXPECT_TRUE(service.current_ping_time_.is_null());
  EXPECT_GE(service.next_tries_time_, now + base::TimeDelta::FromMinutes(54));
  EXPECT_LE(service.next_tries_time_, now + base::TimeDelta::FromHours(7));

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

  EXPECT_EQ(0, service.number_of_tries_);
  EXPECT_FALSE(service.current_ping_time_.is_null());
  EXPECT_EQ(service.current_ping_time_, service.next_tries_time_);
  EXPECT_GT(service.last_sent_time_, now);
  EXPECT_FALSE(NeedUpdate());
}

TEST_F(OmahaServiceTest, SendInstallEventSuccess) {
  base::Time now = base::Time::Now();
  OmahaService service(false);
  service.set_upgrade_recommended_callback(
      base::Bind(&OmahaServiceTest::OnNeedUpdate, base::Unretained(this)));
  service.InitializeURLLoaderFactory(test_shared_url_loader_factory_);
  CleanService(&service, "");

  service.SendPing();

  EXPECT_EQ(1, service.number_of_tries_);
  EXPECT_TRUE(service.current_ping_time_.is_null());
  EXPECT_GE(service.next_tries_time_, now + base::TimeDelta::FromMinutes(54));
  EXPECT_LE(service.next_tries_time_, now + base::TimeDelta::FromHours(7));

  std::string response =
      std::string(
          "<?xml version=\"1.0\"?><response protocol=\"3.0\" server=\"prod\">"
          "<daystart elapsed_seconds=\"56754\"/><app appid=\"") +
      test_application_id() +
      "\" status=\"ok\">"
      "<event status=\"ok\"/>"
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
  service.set_upgrade_recommended_callback(
      base::Bind(&OmahaServiceTest::OnNeedUpdate, base::Unretained(this)));
  service.InitializeURLLoaderFactory(test_shared_url_loader_factory_);
  CleanService(&service, version_info::GetVersionNumber());

  service.SendPing();

  EXPECT_EQ(1, service.number_of_tries_);
  EXPECT_TRUE(service.current_ping_time_.is_null());
  EXPECT_GE(service.next_tries_time_, now + base::TimeDelta::FromMinutes(54));
  EXPECT_LE(service.next_tries_time_, now + base::TimeDelta::FromHours(7));

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
  service.set_upgrade_recommended_callback(
      base::Bind(&OmahaServiceTest::OnNeedUpdate, base::Unretained(this)));
  service.InitializeURLLoaderFactory(test_shared_url_loader_factory_);
  CleanService(&service, version_info::GetVersionNumber());

  service.SendPing();

  // Tries with a non 200 result.
  EXPECT_EQ(1, service.number_of_tries_);
  EXPECT_TRUE(service.current_ping_time_.is_null());
  EXPECT_GE(service.next_tries_time_, now + base::TimeDelta::FromMinutes(54));
  EXPECT_LE(service.next_tries_time_, now + base::TimeDelta::FromHours(7));
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
  EXPECT_GE(service.next_tries_time_, now + base::TimeDelta::FromMinutes(54));
  EXPECT_LE(service.next_tries_time_, now + base::TimeDelta::FromHours(7));
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
  std::string version_string = version_info::GetVersionNumber();
  base::Time now = base::Time::Now();
  OmahaService service(false);
  service.set_upgrade_recommended_callback(
      base::Bind(&OmahaServiceTest::OnNeedUpdate, base::Unretained(this)));
  service.number_of_tries_ = 5;
  service.last_sent_time_ = now - base::TimeDelta::FromSeconds(1);
  service.next_tries_time_ = now + base::TimeDelta::FromSeconds(2);
  service.current_ping_time_ = now + base::TimeDelta::FromSeconds(3);
  service.last_sent_version_ = base::Version(version_string);
  service.PersistStates();

  OmahaService service2(false);
  EXPECT_EQ(service.number_of_tries_, 5);
  EXPECT_EQ(service2.last_sent_time_, now - base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(service2.next_tries_time_, now + base::TimeDelta::FromSeconds(2));
  EXPECT_EQ(service2.current_ping_time_, now + base::TimeDelta::FromSeconds(3));
  EXPECT_EQ(service.last_sent_version_.GetString(), version_string);
}

TEST_F(OmahaServiceTest, BackoffTest) {
  for (int i = 1; i < 100; ++i) {
    // Testing multiple times for a given number of retries, as the method has
    // a random part.
    for (int j = 0; j < 2; ++j) {
      EXPECT_GE(OmahaService::GetBackOff(i).InSeconds(), 3600 - 360);
      EXPECT_LE(OmahaService::GetBackOff(i).InSeconds(), 6 * 3600);
    }
  }
}

// Tests that an active ping is scheduled immediately after a successful install
// event send.
TEST_F(OmahaServiceTest, ActivePingAfterInstallEventTest) {
  base::Time now = base::Time::Now();
  OmahaService service(false);
  service.set_upgrade_recommended_callback(
      base::Bind(&OmahaServiceTest::OnNeedUpdate, base::Unretained(this)));
  service.InitializeURLLoaderFactory(test_shared_url_loader_factory_);
  CleanService(&service, "");

  service.SendPing();

  EXPECT_EQ(1, service.number_of_tries_);
  EXPECT_TRUE(service.current_ping_time_.is_null());
  EXPECT_GE(service.next_tries_time_, now + base::TimeDelta::FromMinutes(54));
  EXPECT_LE(service.next_tries_time_, now + base::TimeDelta::FromHours(7));

  std::string response =
      std::string(
          "<?xml version=\"1.0\"?><response protocol=\"3.0\" server=\"prod\">"
          "<daystart elapsed_seconds=\"0\"/><app appid=\"") +
      test_application_id() +
      "\" status=\"ok\">"
      "<event status=\"ok\"/>"
      "</app></response>";
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), response);

  EXPECT_EQ(1, service.number_of_tries_);
  EXPECT_LT(service.current_ping_time_ - now, base::TimeDelta::FromMinutes(1));
  EXPECT_GT(service.next_tries_time_, service.current_ping_time_);
  EXPECT_FALSE(NeedUpdate());
}

// Tests that active pings are not sent in rapid succession.
TEST_F(OmahaServiceTest, NonSpammingTest) {
  base::Time now = base::Time::Now();
  OmahaService service(false);
  service.set_upgrade_recommended_callback(
      base::Bind(&OmahaServiceTest::OnNeedUpdate, base::Unretained(this)));
  service.InitializeURLLoaderFactory(test_shared_url_loader_factory_);
  CleanService(&service, version_info::GetVersionNumber());

  service.SendPing();

  EXPECT_EQ(1, service.number_of_tries_);
  EXPECT_TRUE(service.current_ping_time_.is_null());
  EXPECT_GE(service.next_tries_time_, now + base::TimeDelta::FromMinutes(54));
  EXPECT_LE(service.next_tries_time_, now + base::TimeDelta::FromHours(7));

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
  EXPECT_GE(service.next_tries_time_ - now, base::TimeDelta::FromHours(2));
  EXPECT_GT(service.last_sent_time_, now);
  EXPECT_FALSE(NeedUpdate());
}

TEST_F(OmahaServiceTest, InstallRetryTest) {
  OmahaService service(false);
  service.set_upgrade_recommended_callback(
      base::Bind(&OmahaServiceTest::OnNeedUpdate, base::Unretained(this)));
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
