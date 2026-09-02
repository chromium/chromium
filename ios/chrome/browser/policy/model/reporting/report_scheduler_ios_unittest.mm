// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/reporting/report_scheduler_ios.h"

#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/task/single_thread_task_runner.h"
#import "base/test/gmock_callback_support.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/mock_callback.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "components/device_signals/core/common/signals_features.h"
#import "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#import "components/enterprise/browser/reporting/chrome_profile_request_generator.h"
#import "components/enterprise/browser/reporting/common_pref_names.h"
#import "components/enterprise/browser/reporting/report_generation_config.h"
#import "components/enterprise/browser/reporting/report_request.h"
#import "components/enterprise/browser/reporting/report_type.h"
#import "components/enterprise/browser/reporting/reporting_features.h"
#import "components/policy/core/common/cloud/cloud_external_data_manager.h"
#import "components/policy/core/common/cloud/cloud_policy_constants.h"
#import "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#import "components/policy/core/common/cloud/mock_user_cloud_policy_store.h"
#import "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#import "components/policy/core/common/mock_policy_service.h"
#import "components/policy/core/common/schema_registry.h"
#import "components/policy/proto/device_management_backend.pb.h"
#import "ios/chrome/browser/policy/model/profile_policy_connector_mock.h"
#import "ios/chrome/browser/policy/model/reporting/features.h"
#import "ios/chrome/browser/policy/model/reporting/reporting_delegate_factory_ios.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "services/network/test/test_network_connection_tracker.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Invoke;
using ::testing::WithArgs;

namespace em = enterprise_management;

namespace enterprise_reporting {

namespace {

constexpr char kDMToken[] = "dm_token";
constexpr char kClientId[] = "client_id";
constexpr base::TimeDelta kUploadFrequency = base::Hours(12);
constexpr base::TimeDelta kNewUploadFrequency = base::Hours(10);

}  // namespace

ACTION_P(ScheduleGeneratorCallback, request_number) {
  ReportRequestQueue requests;
  for (int i = 0; i < request_number; i++) {
    requests.push(std::make_unique<ReportRequest>(ReportType::kBrowser));
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(arg0), std::move(requests)));
}

ACTION(ScheduleProfileRequestGeneratorCallback) {
  ReportRequestQueue requests;
  requests.push(std::make_unique<ReportRequest>(ReportType::kProfileReport));

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(arg0), std::move(requests)));
}

class MockReportGenerator : public ReportGenerator {
 public:
  explicit MockReportGenerator(ReportingDelegateFactoryIOS* delegate_factory)
      : ReportGenerator(delegate_factory) {}
  MockReportGenerator(const MockReportGenerator&) = delete;
  MockReportGenerator& operator=(const MockReportGenerator&) = delete;

  void Generate(ReportType report_type, ReportCallback callback) override {
    OnGenerate(report_type, callback);
  }
  MOCK_METHOD2(OnGenerate,
               void(ReportType report_type, ReportCallback& callback));
  MOCK_METHOD0(GenerateBasic, ReportRequestQueue());
};

class MockChromeProfileRequestGenerator : public ChromeProfileRequestGenerator {
 public:
  explicit MockChromeProfileRequestGenerator(
      ReportingDelegateFactoryIOS* delegate_factory)
      : ChromeProfileRequestGenerator(/*profile_path=*/base::FilePath(),
                                      delegate_factory) {}
  void Generate(ReportGenerationConfig generation_config,
                ReportCallback callback) override {
    OnGenerate(callback);
  }
  MOCK_METHOD1(OnGenerate, void(ReportCallback& callback));
};

class MockReportUploader : public ReportUploader {
 public:
  MockReportUploader() : ReportUploader(nullptr, 0) {}
  MockReportUploader(const MockReportUploader&) = delete;
  MockReportUploader& operator=(const MockReportUploader&) = delete;
  ~MockReportUploader() override = default;

  MOCK_METHOD3(SetRequestAndUpload,
               void(const ReportGenerationConfig& config,
                    ReportRequestQueue,
                    ReportCallback));
};

class ReportSchedulerIOSTest : public PlatformTest,
                               public testing::WithParamInterface<bool> {
 public:
  ReportSchedulerIOSTest() = default;
  ReportSchedulerIOSTest(const ReportSchedulerIOSTest&) = delete;
  ReportSchedulerIOSTest& operator=(const ReportSchedulerIOSTest&) = delete;
  ~ReportSchedulerIOSTest() override = default;

  void SetUp() override {
    client_ptr_ = std::make_unique<policy::MockCloudPolicyClient>();
    client_ = client_ptr_.get();
    uploader_ = std::make_unique<MockReportUploader>();
  }

  void Init(bool policy_enabled,
            const std::string& dm_token,
            const std::string& client_id) {
    ToggleCloudReport(policy_enabled);
    storage_.SetDMToken(dm_token);
    storage_.SetClientId(client_id);
  }

  virtual void ToggleCloudReport(bool enabled) = 0;

  ReportRequestQueue CreateRequests(int number) {
    ReportRequestQueue requests;
    for (int i = 0; i < number; i++) {
      requests.push(std::make_unique<ReportRequest>(ReportType::kBrowser));
    }
    return requests;
  }

  void EXPECT_CALL_SetupRegistration() {
    EXPECT_CALL(*client_, SetupRegistration(kDMToken, kClientId, _));
  }

  void EXPECT_CALL_SetupRegistrationWithSetDMToken() {
    EXPECT_CALL(*client_, SetupRegistration(kDMToken, kClientId, _))
        .WillOnce(WithArgs<0>(
            Invoke(client_.get(), &policy::MockCloudPolicyClient::SetDMToken)));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;

  std::unique_ptr<TestProfileIOS> profile_;

  ReportingDelegateFactoryIOS report_delegate_factory_;
  std::unique_ptr<policy::MockCloudPolicyClient> client_ptr_;
  raw_ptr<policy::MockCloudPolicyClient> client_;
  std::unique_ptr<ReportScheduler> scheduler_;
  std::unique_ptr<MockReportUploader> uploader_;
  policy::FakeBrowserDMTokenStorage storage_;
  base::Time previous_set_last_upload_timestamp_;
  base::HistogramTester histogram_tester_;
};

class BrowserReportSchedulerIOSTest : public ReportSchedulerIOSTest {
 public:
  void SetUp() override {
    ReportSchedulerIOSTest::SetUp();
    generator_ptr_ =
        std::make_unique<MockReportGenerator>(&report_delegate_factory_);
    generator_ = generator_ptr_.get();
    Init(true, kDMToken, kClientId);
  }

  void CreateScheduler() {
    ReportScheduler::CreateParams params;
    params.client = client_.get();
    params.delegate = report_delegate_factory_.GetReportSchedulerDelegate();
    params.report_generator = std::move(generator_ptr_);
    scheduler_ = std::make_unique<ReportScheduler>(std::move(params));
    scheduler_->QueueReportUploaderForTesting(std::move(uploader_));
  }

  // If lastUploadTimestamp is updated recently, it should be updated as Now().
  // Otherwise, it should be same as previous set timestamp.
  void ExpectLastUploadTimestampUpdated(bool is_updated) {
    auto current_last_upload_timestamp =
        local_state()->GetTime(kLastUploadTimestamp);
    if (is_updated) {
      EXPECT_EQ(base::Time::Now(), current_last_upload_timestamp);
    } else {
      EXPECT_EQ(previous_set_last_upload_timestamp_,
                current_last_upload_timestamp);
    }
  }

  void SetLastUploadTimeAgo(base::TimeDelta gap) {
    previous_set_last_upload_timestamp_ = base::Time::Now() - gap;
    local_state()->SetTime(kLastUploadTimestamp,
                           previous_set_last_upload_timestamp_);
  }

  void SetReportFrequency(base::TimeDelta frequency) {
    local_state()->SetTimeDelta(kCloudReportingUploadFrequency, frequency);
  }

  void ToggleCloudReport(bool enabled) override {
    local_state()->SetBoolean(kCloudReportingEnabled, enabled);
  }

  PrefService* local_state() {
    return GetApplicationContext()->GetLocalState();
  }

  std::unique_ptr<MockReportGenerator> generator_ptr_;
  raw_ptr<MockReportGenerator> generator_;
};

TEST_F(BrowserReportSchedulerIOSTest, NoReportWithoutPolicy) {
  Init(false, kDMToken, kClientId);
  CreateScheduler();
  EXPECT_FALSE(scheduler_->IsNextReportScheduledForTesting());
}

TEST_F(BrowserReportSchedulerIOSTest, NoReportWithoutDMToken) {
  Init(true, "", kClientId);
  CreateScheduler();
  EXPECT_FALSE(scheduler_->IsNextReportScheduledForTesting());
}

TEST_F(BrowserReportSchedulerIOSTest, NoReportWithoutClientId) {
  Init(true, kDMToken, "");
  CreateScheduler();
  EXPECT_FALSE(scheduler_->IsNextReportScheduledForTesting());
}

TEST_F(BrowserReportSchedulerIOSTest, UploadReportSucceeded) {
  EXPECT_CALL_SetupRegistration();
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kBrowser, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(1)));
  EXPECT_CALL(*uploader_,
              SetRequestAndUpload(
                  ReportGenerationConfig(ReportTrigger::kTriggerTimer), _, _))
      .WillOnce(RunOnceCallback<2>(ReportUploader::kSuccess));

  CreateScheduler();
  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());

  // Run pending task.
  task_environment_.RunUntilIdle();

  // Next report is scheduled.
  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());
  ExpectLastUploadTimestampUpdated(true);

  ::testing::Mock::VerifyAndClearExpectations(client_);
  ::testing::Mock::VerifyAndClearExpectations(generator_);
}

TEST_F(BrowserReportSchedulerIOSTest, UploadReportTransientError) {
  EXPECT_CALL_SetupRegistration();
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kBrowser, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(1)));
  EXPECT_CALL(*uploader_,
              SetRequestAndUpload(
                  ReportGenerationConfig(ReportTrigger::kTriggerTimer), _, _))
      .WillOnce(RunOnceCallback<2>(ReportUploader::kTransientError));

  CreateScheduler();
  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());

  // Run pending task.
  task_environment_.RunUntilIdle();

  // Next report is scheduled.
  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());
  ExpectLastUploadTimestampUpdated(true);

  ::testing::Mock::VerifyAndClearExpectations(client_);
  ::testing::Mock::VerifyAndClearExpectations(generator_);
}

TEST_F(BrowserReportSchedulerIOSTest, UploadReportPersistentError) {
  EXPECT_CALL_SetupRegistrationWithSetDMToken();
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kBrowser, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(1)));
  EXPECT_CALL(*uploader_,
              SetRequestAndUpload(
                  ReportGenerationConfig(ReportTrigger::kTriggerTimer), _, _))
      .WillOnce(RunOnceCallback<2>(ReportUploader::kPersistentError));

  CreateScheduler();
  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());

  // Run pending task.
  task_environment_.RunUntilIdle();

  // Next report is not scheduled.
  EXPECT_FALSE(scheduler_->IsNextReportScheduledForTesting());
  ExpectLastUploadTimestampUpdated(false);

  // Turn off and on reporting to resume.
  ToggleCloudReport(false);
  ToggleCloudReport(true);
  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());

  ::testing::Mock::VerifyAndClearExpectations(client_);
  ::testing::Mock::VerifyAndClearExpectations(generator_);
}

TEST_F(BrowserReportSchedulerIOSTest, NoReportGenerate) {
  EXPECT_CALL_SetupRegistrationWithSetDMToken();
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kBrowser, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(0)));
  EXPECT_CALL(*uploader_, SetRequestAndUpload(_, _, _)).Times(0);

  CreateScheduler();
  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());

  // Run pending task.
  task_environment_.RunUntilIdle();

  // Next report is not scheduled.
  EXPECT_FALSE(scheduler_->IsNextReportScheduledForTesting());
  ExpectLastUploadTimestampUpdated(false);

  // Turn off and on reporting to resume.
  ToggleCloudReport(false);
  ToggleCloudReport(true);
  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());

  ::testing::Mock::VerifyAndClearExpectations(client_);
  ::testing::Mock::VerifyAndClearExpectations(generator_);
}

TEST_F(BrowserReportSchedulerIOSTest, TimerDelayWithLastUploadTimestamp) {
  const base::TimeDelta gap = base::Hours(10);
  SetLastUploadTimeAgo(gap);
  SetReportFrequency(kUploadFrequency);

  EXPECT_CALL_SetupRegistration();
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kBrowser, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(1)));
  EXPECT_CALL(*uploader_,
              SetRequestAndUpload(
                  ReportGenerationConfig(ReportTrigger::kTriggerTimer), _, _))
      .WillOnce(RunOnceCallback<2>(ReportUploader::kSuccess));

  CreateScheduler();
  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());

  base::TimeDelta next_report_delay = kUploadFrequency - gap;
  task_environment_.FastForwardBy(next_report_delay - base::Seconds(1));
  ExpectLastUploadTimestampUpdated(false);
  task_environment_.FastForwardBy(base::Seconds(1));
  ExpectLastUploadTimestampUpdated(true);

  ::testing::Mock::VerifyAndClearExpectations(client_);
  ::testing::Mock::VerifyAndClearExpectations(generator_);
}

TEST_F(BrowserReportSchedulerIOSTest, TimerDelayWithoutLastUploadTimestamp) {
  EXPECT_CALL_SetupRegistration();
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kBrowser, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(1)));
  EXPECT_CALL(*uploader_,
              SetRequestAndUpload(
                  ReportGenerationConfig(ReportTrigger::kTriggerTimer), _, _))
      .WillOnce(RunOnceCallback<2>(ReportUploader::kSuccess));

  CreateScheduler();
  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());

  ExpectLastUploadTimestampUpdated(false);
  task_environment_.RunUntilIdle();
  ExpectLastUploadTimestampUpdated(true);

  ::testing::Mock::VerifyAndClearExpectations(client_);
}

TEST_F(BrowserReportSchedulerIOSTest, TimerDelayUpdate) {
  const base::TimeDelta gap = base::Hours(5);
  SetLastUploadTimeAgo(gap);
  SetReportFrequency(kUploadFrequency);

  EXPECT_CALL_SetupRegistration();
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kBrowser, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(1)));
  EXPECT_CALL(*uploader_,
              SetRequestAndUpload(
                  ReportGenerationConfig(ReportTrigger::kTriggerTimer), _, _))
      .WillOnce(RunOnceCallback<2>(ReportUploader::kSuccess));

  CreateScheduler();
  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());

  SetReportFrequency(kNewUploadFrequency);

  // The report should be re-scheduled, moving the time forward with the new
  // interval.
  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());

  base::TimeDelta next_report_delay = kNewUploadFrequency - gap;
  task_environment_.FastForwardBy(next_report_delay - base::Seconds(1));
  ExpectLastUploadTimestampUpdated(false);
  task_environment_.FastForwardBy(base::Seconds(1));
  ExpectLastUploadTimestampUpdated(true);

  ::testing::Mock::VerifyAndClearExpectations(client_);
  ::testing::Mock::VerifyAndClearExpectations(generator_);
}

TEST_F(BrowserReportSchedulerIOSTest, IgnoreFrequencyWithoutReportEnabled) {
  Init(false, kDMToken, kClientId);
  CreateScheduler();
  EXPECT_FALSE(scheduler_->IsNextReportScheduledForTesting());

  SetReportFrequency(kUploadFrequency);
  EXPECT_FALSE(scheduler_->IsNextReportScheduledForTesting());

  // Toggle reporting on and off.
  EXPECT_CALL_SetupRegistration();
  ToggleCloudReport(true);
  ToggleCloudReport(false);

  EXPECT_FALSE(scheduler_->IsNextReportScheduledForTesting());

  SetReportFrequency(kNewUploadFrequency);

  EXPECT_FALSE(scheduler_->IsNextReportScheduledForTesting());
}

TEST_F(BrowserReportSchedulerIOSTest,
       ReportingIsDisabledWhileNewReportIsScheduledButNotPosted) {
  EXPECT_CALL_SetupRegistration();

  CreateScheduler();
  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());

  // Run pending task.
  task_environment_.RunUntilIdle();

  ToggleCloudReport(false);

  // Next report is not scheduled.
  EXPECT_FALSE(scheduler_->IsNextReportScheduledForTesting());
  ExpectLastUploadTimestampUpdated(false);

  ::testing::Mock::VerifyAndClearExpectations(client_);
  ::testing::Mock::VerifyAndClearExpectations(generator_);
}

TEST_F(BrowserReportSchedulerIOSTest,
       ReportingIsDisabledWhileNewReportIsPosted) {
  EXPECT_CALL_SetupRegistration();
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kBrowser, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(1)));
  EXPECT_CALL(*uploader_,
              SetRequestAndUpload(
                  ReportGenerationConfig(ReportTrigger::kTriggerTimer), _, _))
      .WillOnce(RunOnceCallback<2>(ReportUploader::kSuccess));

  CreateScheduler();
  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());

  // Run pending task.
  task_environment_.RunUntilIdle();

  ToggleCloudReport(false);

  // Run pending task.
  task_environment_.RunUntilIdle();

  ExpectLastUploadTimestampUpdated(true);
  // Next report is not scheduled.
  EXPECT_FALSE(scheduler_->IsNextReportScheduledForTesting());

  ::testing::Mock::VerifyAndClearExpectations(client_);
  ::testing::Mock::VerifyAndClearExpectations(generator_);
}

class ProfileReportSchedulerIOSTest : public ReportSchedulerIOSTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{enterprise_reporting::kCloudProfileReporting,
                              enterprise_reporting::kUploadReportOnProfileOpen},
        /*disabled_features=*/{});
    ReportSchedulerIOSTest::SetUp();

    enterprise_management::PolicyData profile_policy_data;
    profile_policy_data.set_request_token(kDMToken);
    profile_policy_data.set_device_id(kClientId);

    auto store = std::make_unique<policy::MockUserCloudPolicyStore>(
        policy::dm_protocol::GetChromeUserPolicyType());
    store->set_policy_data_for_testing(
        std::make_unique<enterprise_management::PolicyData>(
            std::move(profile_policy_data)));

    auto cloud_policy_manager =
        std::make_unique<policy::UserCloudPolicyManager>(
            std::move(store), /*extension_install_store=*/nullptr,
            base::FilePath(),
            /*cloud_external_data_manager=*/nullptr,
            base::SingleThreadTaskRunner::GetCurrentDefault(),
            network::TestNetworkConnectionTracker::CreateGetter());

    TestProfileIOS::Builder builder;
    builder.SetPolicyConnector(std::make_unique<ProfilePolicyConnectorMock>(
        std::make_unique<policy::MockPolicyService>(), &schema_registry_));
    builder.SetUserCloudPolicyManager(std::move(cloud_policy_manager));
    profile_ = std::move(builder).Build();

    profile_request_generator_ptr_ =
        std::make_unique<MockChromeProfileRequestGenerator>(
            &report_delegate_factory_);
    profile_request_generator_ = profile_request_generator_ptr_.get();

    Init(true, kDMToken, kClientId);
  }

  void CreateScheduler(bool require_policy_fetch_with_profile_id) {
    ReportScheduler::CreateParams params;
    params.client = client_.get();
    params.delegate =
        report_delegate_factory_.GetReportSchedulerDelegate(profile_.get());
    params.require_policy_fetch_with_profile_id =
        require_policy_fetch_with_profile_id;
    scheduler_ = std::make_unique<ReportScheduler>(std::move(params));
    scheduler_->QueueReportUploaderForTesting(std::move(uploader_));
  }

  void CreateSchedulerForProfileReporting(
      bool require_policy_fetch_with_profile_id = false) {
    ReportScheduler::CreateParams params;
    params.client = client_.get();
    params.delegate =
        report_delegate_factory_.GetReportSchedulerDelegate(profile_.get());
    params.require_policy_fetch_with_profile_id =
        require_policy_fetch_with_profile_id;
    if (profile_->GetPrefs()->GetTime(kLastUploadTimestamp).is_null()) {
      SetLastUploadTimeAgo(base::Seconds(0));
    }
    params.profile_request_generator =
        std::move(profile_request_generator_ptr_);
    scheduler_ = std::make_unique<ReportScheduler>(std::move(params));
    scheduler_->QueueReportUploaderForTesting(std::move(uploader_));
  }

  void SetLastUploadTimeAgo(base::TimeDelta gap) {
    previous_set_last_upload_timestamp_ = base::Time::Now() - gap;
    profile_->GetPrefs()->SetTime(kLastUploadTimestamp,
                                  previous_set_last_upload_timestamp_);
  }

  void ToggleCloudReport(bool enabled) override {
    profile_->GetPrefs()->SetBoolean(kCloudProfileReportingEnabled, enabled);
  }

  std::unique_ptr<MockChromeProfileRequestGenerator>
      profile_request_generator_ptr_;
  raw_ptr<MockChromeProfileRequestGenerator> profile_request_generator_;

  policy::SchemaRegistry schema_registry_;
};

// Profile reporting without require_policy_fetch_with_profile_id, schedule
// reports right away.
TEST_F(ProfileReportSchedulerIOSTest, NoRequirePolicyFetchWithProfileId) {
  EXPECT_CALL_SetupRegistrationWithSetDMToken();
  EXPECT_CALL(*uploader_, SetRequestAndUpload(_, _, _)).Times(0);

  ToggleCloudReport(true);
  CreateScheduler(/*require_policy_fetch_with_profile_id=*/false);
  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());

  ::testing::Mock::VerifyAndClearExpectations(client_);
}

// kPoliciesEverFetchedWithProfileId starts false, schedule reports when it
// flips to true.
TEST_F(ProfileReportSchedulerIOSTest, RequirePolicyFetchWithProfileId) {
  EXPECT_CALL_SetupRegistrationWithSetDMToken();
  EXPECT_CALL(*uploader_, SetRequestAndUpload(_, _, _)).Times(0);

  ToggleCloudReport(true);
  CreateScheduler(/*require_policy_fetch_with_profile_id=*/true);
  EXPECT_FALSE(scheduler_->IsNextReportScheduledForTesting());

  // Flip kPoliciesEverFetchedWithProfileId to true, this should enable
  // scheduling.
  profile_->GetPrefs()->SetBoolean(kPoliciesEverFetchedWithProfileId, true);
  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());

  ::testing::Mock::VerifyAndClearExpectations(client_);
}

// kPoliciesEverFetchedWithProfileId starts true, schedule reports right away.
TEST_F(ProfileReportSchedulerIOSTest,
       RequirePolicyFetchWithProfileIdAlreadyTrue) {
  EXPECT_CALL_SetupRegistrationWithSetDMToken();
  EXPECT_CALL(*uploader_, SetRequestAndUpload(_, _, _)).Times(0);

  ToggleCloudReport(true);
  profile_->GetPrefs()->SetBoolean(kPoliciesEverFetchedWithProfileId, true);
  CreateScheduler(/*require_policy_fetch_with_profile_id=*/true);
  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());

  ::testing::Mock::VerifyAndClearExpectations(client_);
}

// kPoliciesEverFetchedWithProfileId starts true, but the policy starts false.
// Schedule reports when the policy flips to ture.
TEST_F(ProfileReportSchedulerIOSTest,
       RequirePolicyFetchWithProfileIdPolicyChanges) {
  EXPECT_CALL_SetupRegistrationWithSetDMToken();
  EXPECT_CALL(*uploader_, SetRequestAndUpload(_, _, _)).Times(0);

  ToggleCloudReport(false);
  profile_->GetPrefs()->SetBoolean(kPoliciesEverFetchedWithProfileId, true);
  CreateScheduler(/*require_policy_fetch_with_profile_id=*/true);
  EXPECT_FALSE(scheduler_->IsNextReportScheduledForTesting());

  // Flip kCloudProfileReportingEnabled to true, this should enable
  // scheduling.
  ToggleCloudReport(true);
  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());

  ::testing::Mock::VerifyAndClearExpectations(client_);
}

// Verifies that `GetProfileDMToken` and `GetProfileClientId` return an empty
// token and string when the profile does not have a `UserCloudPolicyManager`.
TEST_F(ProfileReportSchedulerIOSTest,
       GetProfileDMTokenAndClientId_NullPolicyManager) {
  TestProfileIOS::Builder builder;
  std::unique_ptr<TestProfileIOS> profile_without_mgr =
      std::move(builder).Build();
  ReportSchedulerIOS scheduler(profile_without_mgr.get());
  EXPECT_TRUE(scheduler.GetProfileDMToken().is_empty());
  EXPECT_TRUE(scheduler.GetProfileClientId().empty());
}

// Profile reporting is enabled and policy has been fetched. A report should be
// uploaded on profile open.
TEST_F(ProfileReportSchedulerIOSTest,
       UploadReportOnProfileOpen_ProfileReportingEnabled) {
  EXPECT_CALL_SetupRegistrationWithSetDMToken();
  EXPECT_CALL(*profile_request_generator_, OnGenerate(_))
      .WillOnce(WithArgs<0>(ScheduleProfileRequestGeneratorCallback()));
  EXPECT_CALL(*uploader_,
              SetRequestAndUpload(
                  ReportGenerationConfig(ReportTrigger::kTriggerProfileOpened,
                                         ReportType::kProfileReport,
                                         SecuritySignalsMode::kNoSignals,
                                         /*use_cookies=*/false),
                  _, _))
      .WillOnce(RunOnceCallback<2>(ReportUploader::kSuccess));

  ToggleCloudReport(true);
  profile_->GetPrefs()->SetBoolean(kPoliciesEverFetchedWithProfileId, true);
  SetLastUploadTimeAgo(base::Hours(1));
  CreateSchedulerForProfileReporting(
      /*require_policy_fetch_with_profile_id=*/true);

  // Fast forward to let the profile open report complete.
  task_environment_.FastForwardBy(base::TimeDelta());

  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());
  base::Time current_last_upload_timestamp =
      profile_->GetPrefs()->GetTime(kLastUploadTimestamp);
  EXPECT_EQ(base::Time::Now(), current_last_upload_timestamp);

  ::testing::Mock::VerifyAndClearExpectations(client_);
  ::testing::Mock::VerifyAndClearExpectations(profile_request_generator_);
}

// Profile reporting and security signals reporting are enabled with cookies.
TEST_F(ProfileReportSchedulerIOSTest,
       UploadReportOnProfileOpen_WithSecuritySignalsAndCookies) {
  base::test::ScopedFeatureList signals_feature_list;
  signals_feature_list.InitWithFeatures(
      /*enabled_features=*/{enterprise_reporting::kIOSSignalSharingEnabled,
                            enterprise_signals::features::
                                kProfileSignalsReportingEnabled},
      /*disabled_features=*/{});

  EXPECT_CALL_SetupRegistrationWithSetDMToken();
  EXPECT_CALL(*profile_request_generator_, OnGenerate(_))
      .WillOnce(WithArgs<0>(ScheduleProfileRequestGeneratorCallback()));
  EXPECT_CALL(*uploader_,
              SetRequestAndUpload(
                  ReportGenerationConfig(ReportTrigger::kTriggerProfileOpened,
                                         ReportType::kProfileReport,
                                         SecuritySignalsMode::kSignalsAttached,
                                         /*use_cookies=*/true),
                  _, _))
      .WillOnce(RunOnceCallback<2>(ReportUploader::kSuccess));

  ToggleCloudReport(true);
  profile_->GetPrefs()->SetBoolean(kPoliciesEverFetchedWithProfileId, true);
  profile_->GetPrefs()->SetBoolean(kUserSecuritySignalsReporting, true);
  profile_->GetPrefs()->SetBoolean(kUserSecurityAuthenticatedReporting, true);
  SetLastUploadTimeAgo(base::Hours(1));
  CreateSchedulerForProfileReporting(
      /*require_policy_fetch_with_profile_id=*/true);

  task_environment_.FastForwardBy(base::TimeDelta());

  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());
  base::Time current_last_upload_timestamp =
      profile_->GetPrefs()->GetTime(kLastUploadTimestamp);
  EXPECT_EQ(base::Time::Now(), current_last_upload_timestamp);

  ::testing::Mock::VerifyAndClearExpectations(client_);
  ::testing::Mock::VerifyAndClearExpectations(profile_request_generator_);
}

// Profile reporting is disabled, but security signals reporting is enabled.
TEST_F(ProfileReportSchedulerIOSTest,
       UploadReportOnProfileOpen_OnlySecuritySignals) {
  base::test::ScopedFeatureList signals_feature_list;
  signals_feature_list.InitWithFeatures(
      /*enabled_features=*/{enterprise_reporting::kIOSSignalSharingEnabled,
                            enterprise_signals::features::
                                kProfileSignalsReportingEnabled},
      /*disabled_features=*/{});

  EXPECT_CALL_SetupRegistrationWithSetDMToken();
  EXPECT_CALL(*profile_request_generator_, OnGenerate(_))
      .WillOnce(WithArgs<0>(ScheduleProfileRequestGeneratorCallback()));
  EXPECT_CALL(*uploader_,
              SetRequestAndUpload(
                  ReportGenerationConfig(ReportTrigger::kTriggerProfileOpened,
                                         ReportType::kProfileReport,
                                         SecuritySignalsMode::kSignalsOnly,
                                         /*use_cookies=*/false),
                  _, _))
      .WillOnce(RunOnceCallback<2>(ReportUploader::kSuccess));

  ToggleCloudReport(false);
  profile_->GetPrefs()->SetBoolean(kUserSecuritySignalsReporting, true);
  SetLastUploadTimeAgo(base::Hours(1));
  CreateSchedulerForProfileReporting(
      /*require_policy_fetch_with_profile_id=*/false);

  task_environment_.FastForwardBy(base::TimeDelta());

  EXPECT_FALSE(scheduler_->IsNextReportScheduledForTesting());
  base::Time current_last_upload_timestamp =
      profile_->GetPrefs()->GetTime(kLastUploadTimestamp);
  EXPECT_EQ(base::Time::Now(), current_last_upload_timestamp);

  ::testing::Mock::VerifyAndClearExpectations(client_);
  ::testing::Mock::VerifyAndClearExpectations(profile_request_generator_);
}

// When `kUploadReportOnProfileOpen` is disabled, no report is uploaded on
// profile open and the periodic timer is used instead.
TEST_F(ProfileReportSchedulerIOSTest,
       UploadReportOnProfileOpenDisabled_PeriodicTimerTriggered) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{enterprise_reporting::kCloudProfileReporting},
      /*disabled_features=*/{enterprise_reporting::kUploadReportOnProfileOpen});

  EXPECT_CALL_SetupRegistrationWithSetDMToken();
  EXPECT_CALL(*profile_request_generator_, OnGenerate(_))
      .WillOnce(WithArgs<0>(ScheduleProfileRequestGeneratorCallback()));
  EXPECT_CALL(*uploader_,
              SetRequestAndUpload(
                  ReportGenerationConfig(ReportTrigger::kTriggerTimer,
                                         ReportType::kProfileReport,
                                         SecuritySignalsMode::kNoSignals,
                                         /*use_cookies=*/false),
                  _, _))
      .WillOnce(RunOnceCallback<2>(ReportUploader::kSuccess));

  ToggleCloudReport(true);
  profile_->GetPrefs()->SetBoolean(kPoliciesEverFetchedWithProfileId, true);
  SetLastUploadTimeAgo(base::Hours(25));
  CreateSchedulerForProfileReporting(
      /*require_policy_fetch_with_profile_id=*/true);

  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());
  task_environment_.FastForwardBy(base::TimeDelta());

  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());
  EXPECT_EQ(base::Time::Now(),
            profile_->GetPrefs()->GetTime(kLastUploadTimestamp));

  ::testing::Mock::VerifyAndClearExpectations(client_);
  ::testing::Mock::VerifyAndClearExpectations(profile_request_generator_);
}

// When `kPoliciesEverFetchedWithProfileId` is false, no report is uploaded on
// profile open. Scheduling only starts when the pref flips to true.
TEST_F(ProfileReportSchedulerIOSTest,
       RequirePolicyFetchWithProfileId_NotFetchedYet_NoProfileOpenReport) {
  EXPECT_CALL_SetupRegistrationWithSetDMToken();
  EXPECT_CALL(*profile_request_generator_, OnGenerate(_)).Times(0);
  EXPECT_CALL(*uploader_, SetRequestAndUpload(_, _, _)).Times(0);

  ToggleCloudReport(true);
  profile_->GetPrefs()->SetBoolean(kPoliciesEverFetchedWithProfileId, false);
  CreateSchedulerForProfileReporting(
      /*require_policy_fetch_with_profile_id=*/true);

  // Fast forward - no report should be triggered on profile open because
  // `IsReportingEnabled()` is false.
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_FALSE(scheduler_->IsNextReportScheduledForTesting());

  // When `kPoliciesEverFetchedWithProfileId` flips to true, the periodic timer
  // should start.
  profile_->GetPrefs()->SetBoolean(kPoliciesEverFetchedWithProfileId, true);
  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());

  ::testing::Mock::VerifyAndClearExpectations(client_);
  ::testing::Mock::VerifyAndClearExpectations(profile_request_generator_);
}

// Security reports are enabled when the UserSecuritySignalsReporting policy is
// enabled and kIOSSignalSharingEnabled is enabled, but cookie-based
// authentication is disabled by default.
TEST_F(ProfileReportSchedulerIOSTest, UserSecuritySignalsReportingPolicyEnabled) {
  base::test::ScopedFeatureList signals_feature_list;
  signals_feature_list.InitWithFeatures(
      /*enabled_features=*/{enterprise_reporting::kIOSSignalSharingEnabled,
                           enterprise_signals::features::
                               kProfileSignalsReportingEnabled},
      /*disabled_features=*/{});

  ToggleCloudReport(true);
  profile_->GetPrefs()->SetBoolean(kUserSecuritySignalsReporting, true);

  std::unique_ptr<ReportScheduler::Delegate> delegate =
      report_delegate_factory_.GetReportSchedulerDelegate(profile_.get());
  EXPECT_TRUE(delegate->AreSecurityReportsEnabled());
  EXPECT_FALSE(delegate->UseCookiesInUploads());
}

// Cookie-based authentication for uploads is enabled when both the
// UserSecuritySignalsReporting and UserSecurityAuthenticatedReporting policies
// are enabled.
TEST_F(ProfileReportSchedulerIOSTest,
       UserSecurityAuthenticatedReportingPolicyEnabled) {
  base::test::ScopedFeatureList signals_feature_list;
  signals_feature_list.InitWithFeatures(
      /*enabled_features=*/{enterprise_reporting::kIOSSignalSharingEnabled,
                           enterprise_signals::features::
                               kProfileSignalsReportingEnabled},
      /*disabled_features=*/{});

  ToggleCloudReport(true);
  profile_->GetPrefs()->SetBoolean(kUserSecuritySignalsReporting, true);
  profile_->GetPrefs()->SetBoolean(kUserSecurityAuthenticatedReporting, true);

  std::unique_ptr<ReportScheduler::Delegate> delegate =
      report_delegate_factory_.GetReportSchedulerDelegate(profile_.get());
  EXPECT_TRUE(delegate->AreSecurityReportsEnabled());
  EXPECT_TRUE(delegate->UseCookiesInUploads());
}

// Security reports are disabled when kIOSSignalSharingEnabled is disabled,
// even if UserSecuritySignalsReporting policy is enabled.
TEST_F(ProfileReportSchedulerIOSTest, IOSSignalSharingFeatureDisabled) {
  base::test::ScopedFeatureList signals_feature_list;
  signals_feature_list.InitWithFeatures(
      /*enabled_features=*/{enterprise_signals::features::
                               kProfileSignalsReportingEnabled},
      /*disabled_features=*/{enterprise_reporting::kIOSSignalSharingEnabled});

  ToggleCloudReport(true);
  profile_->GetPrefs()->SetBoolean(kUserSecuritySignalsReporting, true);

  std::unique_ptr<ReportScheduler::Delegate> delegate =
      report_delegate_factory_.GetReportSchedulerDelegate(profile_.get());
  EXPECT_FALSE(delegate->AreSecurityReportsEnabled());
}

// Tests that OnReportEventTriggered runs the callback with
// ReportTrigger::kTriggerSecurity when security reports are enabled.
TEST_F(ProfileReportSchedulerIOSTest, OnReportEventTriggered) {
  base::test::ScopedFeatureList signals_feature_list;
  signals_feature_list.InitWithFeatures(
      /*enabled_features=*/{enterprise_reporting::kIOSSignalSharingEnabled,
                           enterprise_signals::features::
                               kProfileSignalsReportingEnabled},
      /*disabled_features=*/{});

  ToggleCloudReport(true);
  profile_->GetPrefs()->SetBoolean(kUserSecuritySignalsReporting, true);

  ReportSchedulerIOS scheduler(profile_.get());
  base::MockRepeatingCallback<void(ReportTrigger)> callback;
  scheduler.SetReportTriggerCallback(callback.Get());

  EXPECT_CALL(callback, Run(ReportTrigger::kTriggerSecurity)).Times(1);
  scheduler.OnReportEventTriggered(SecurityReportTrigger::kTimer);
}

// Tests that OnReportEventTriggered does not run the callback when security
// reports are disabled.
TEST_F(ProfileReportSchedulerIOSTest,
       OnReportEventTriggered_SecurityReportsDisabled) {
  base::test::ScopedFeatureList signals_feature_list;
  signals_feature_list.InitWithFeatures(
      /*enabled_features=*/{enterprise_reporting::kIOSSignalSharingEnabled,
                           enterprise_signals::features::
                               kProfileSignalsReportingEnabled},
      /*disabled_features=*/{});

  ToggleCloudReport(true);
  profile_->GetPrefs()->SetBoolean(kUserSecuritySignalsReporting, false);

  ReportSchedulerIOS scheduler(profile_.get());
  base::MockRepeatingCallback<void(ReportTrigger)> callback;
  scheduler.SetReportTriggerCallback(callback.Get());

  EXPECT_CALL(callback, Run(_)).Times(0);
  scheduler.OnReportEventTriggered(SecurityReportTrigger::kTimer);
}

// Tests that OnReportEventTriggered does not crash when the callback is null.
TEST_F(ProfileReportSchedulerIOSTest, OnReportEventTriggered_NullCallback) {
  base::test::ScopedFeatureList signals_feature_list;
  signals_feature_list.InitWithFeatures(
      /*enabled_features=*/{enterprise_reporting::kIOSSignalSharingEnabled,
                           enterprise_signals::features::
                               kProfileSignalsReportingEnabled},
      /*disabled_features=*/{});

  ToggleCloudReport(true);
  profile_->GetPrefs()->SetBoolean(kUserSecuritySignalsReporting, true);

  ReportSchedulerIOS scheduler(profile_.get());
  // Callback is null by default; this should not crash.
  scheduler.OnReportEventTriggered(SecurityReportTrigger::kTimer);
}

// Tests that GetCookieManager returns the profile's cookie manager for a
// profile report scheduler, and nullptr for a browser-level scheduler.
TEST_F(ProfileReportSchedulerIOSTest, GetCookieManager) {
  ReportSchedulerIOS profile_scheduler(profile_.get());
  EXPECT_EQ(profile_scheduler.GetCookieManager(), profile_->GetCookieManager());
  EXPECT_NE(profile_scheduler.GetCookieManager(), nullptr);

  ReportSchedulerIOS browser_scheduler(nullptr);
  EXPECT_EQ(browser_scheduler.GetCookieManager(), nullptr);
}

}  // namespace enterprise_reporting
