// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/reporting/report_scheduler_ios.h"

#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/task/single_thread_task_runner.h"
#import "base/test/gmock_callback_support.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#import "components/enterprise/browser/reporting/common_pref_names.h"
#import "components/enterprise/browser/reporting/report_request.h"
#import "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#import "ios/chrome/browser/policy/model/reporting/features.h"
#import "ios/chrome/browser/policy/model/reporting/reporting_delegate_factory_ios.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
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
    requests.push(std::make_unique<ReportRequest>(ReportType::kFull));
  }
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
    uploader_ptr_ = std::make_unique<MockReportUploader>();
    uploader_ = uploader_ptr_.get();
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
      requests.push(std::make_unique<ReportRequest>(ReportType::kFull));
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
  std::unique_ptr<ReportScheduler> scheduler_;
  std::unique_ptr<policy::MockCloudPolicyClient> client_ptr_;
  raw_ptr<policy::MockCloudPolicyClient, DanglingUntriaged> client_;
  std::unique_ptr<MockReportUploader> uploader_ptr_;
  raw_ptr<MockReportUploader, DanglingUntriaged> uploader_;
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
    scheduler_->QueueReportUploaderForTesting(std::move(uploader_ptr_));
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

  void SetLastUploadInHour(base::TimeDelta gap) {
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
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kFull, _))
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
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kFull, _))
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
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kFull, _))
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
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kFull, _))
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
  SetLastUploadInHour(gap);
  SetReportFrequency(kUploadFrequency);

  EXPECT_CALL_SetupRegistration();
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kFull, _))
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
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kFull, _))
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
  SetLastUploadInHour(gap);
  SetReportFrequency(kUploadFrequency);

  EXPECT_CALL_SetupRegistration();
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kFull, _))
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
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kFull, _))
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
    scoped_feature_list_.InitAndEnableFeature(
        enterprise_reporting::kCloudProfileReporting);
    ReportSchedulerIOSTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();
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
    scheduler_->QueueReportUploaderForTesting(std::move(uploader_ptr_));
  }

  void ToggleCloudReport(bool enabled) override {
    profile_->GetPrefs()->SetBoolean(kCloudProfileReportingEnabled, enabled);
  }
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

}  // namespace enterprise_reporting
