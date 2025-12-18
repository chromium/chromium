// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"

#import <memory>
#import <string>

#import "base/metrics/puma_histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/user_action_tester.h"
#import "components/metrics/demographics/demographic_metrics_test_utils.h"
#import "components/metrics/dwa/dwa_entry_builder.h"
#import "components/metrics/dwa/dwa_recorder.h"
#import "components/metrics/dwa/dwa_service.h"
#import "components/metrics/metrics_service.h"
#import "components/metrics/private_metrics/private_metrics_reporting_service.h"
#import "components/metrics/private_metrics/puma_service.h"
#import "components/metrics_services_manager/metrics_services_manager.h"
#import "components/network_time/network_time_tracker.h"
#import "components/regional_capabilities/regional_capabilities_country_id.h"
#import "components/ukm/ukm_service.h"
#import "components/ukm/ukm_test_helper.h"
#import "ios/chrome/browser/metrics/model/ios_chrome_metrics_service_accessor.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/test/app/histogram_test_util.h"
#import "ios/testing/nserror_util.h"
#import "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#import "third_party/metrics_proto/private_metrics/private_metrics.pb.h"
#import "third_party/metrics_proto/private_metrics/system_profiles/rc_coarse_system_profile.pb.h"
#import "third_party/metrics_proto/ukm/report.pb.h"
#import "third_party/zlib/google/compression_utils.h"

namespace {

bool g_metrics_enabled = false;

chrome_test_util::HistogramTester* g_histogram_tester = nullptr;
base::UserActionTester* g_user_action_tester = nullptr;

constexpr char kTestBooleanHistogram[] =
    "PUMA.PumaServiceTestHistogram.Boolean";
constexpr char kTestLinearHistogram[] = "PUMA.PumaServiceTestHistogram.Linear";
constexpr char kTestEnumHistogram[] = "PUMA.PumaServiceTestHistogram.Enum";

enum class TestEnum {
  kValueA = 0,
  kValueB = 1,
  kMaxValue = kValueB,
};

PrefService* GetLocalState() {
  return GetApplicationContext()->GetLocalState();
}

ukm::UkmService* GetUkmService() {
  return GetApplicationContext()->GetMetricsServicesManager()->GetUkmService();
}

metrics::dwa::DwaService* GetDwaService() {
  return GetApplicationContext()->GetMetricsServicesManager()->GetDwaService();
}

metrics::private_metrics::PumaService* GetPumaService() {
  return GetApplicationContext()->GetMetricsServicesManager()->GetPumaService();
}

metrics::MetricsService* GetMetricsService() {
  return GetApplicationContext()->GetMetricsService();
}

}  // namespace

@implementation MetricsAppInterface : NSObject

+ (void)overrideMetricsAndCrashReportingForTesting {
  IOSChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
      &g_metrics_enabled);

  // Give MSBB consent to the UKMService.
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  ukm_test_helper.SetMsbbConsent();
}

+ (void)stopOverridingMetricsAndCrashReportingForTesting {
  IOSChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
      nullptr);
}

+ (BOOL)setMetricsAndCrashReportingForTesting:(BOOL)enabled {
  BOOL previousValue = g_metrics_enabled;
  g_metrics_enabled = enabled;
  GetApplicationContext()
      ->GetMetricsServicesManager()
      ->UpdateUploadPermissions();
  return previousValue;
}

+ (BOOL)checkUKMRecordingEnabled:(BOOL)enabled {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());

  ConditionBlock condition = ^{
    return ukm_test_helper.IsRecordingEnabled() == enabled;
  };
  return base::test::ios::WaitUntilConditionOrTimeout(
      syncher::kSyncUKMOperationsTimeout, condition);
}

+ (BOOL)isReportUserNoisedUserBirthYearAndGenderEnabled {
  return ukm::UkmTestHelper::IsReportUserNoisedUserBirthYearAndGenderEnabled();
}

+ (uint64_t)UKMClientID {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  return ukm_test_helper.GetClientId();
}

+ (BOOL)UKMHasDummySource:(int64_t)sourceID {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  return ukm_test_helper.HasSource(sourceID);
}

+ (void)UKMRecordDummySource:(int64_t)sourceID {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  ukm_test_helper.RecordSourceForTesting(sourceID);
}

+ (void)updateNetworkTime:(base::Time)now {
  metrics::test::UpdateNetworkTime(
      now, GetApplicationContext()->GetNetworkTimeTracker());
}

+ (int)maximumEligibleBirthYearForTime:(base::Time)now {
  return metrics::test::GetMaximumEligibleBirthYear(now);
}

+ (void)buildAndStoreUKMLog {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  ukm_test_helper.BuildAndStoreLog();
}

+ (BOOL)hasUnsentUKMLogs {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  return ukm_test_helper.HasUnsentLogs();
}

+ (BOOL)UKMReportHasBirthYear:(int)year
                       gender:(metrics::UserDemographicsProto::Gender)gender {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  std::unique_ptr<ukm::Report> report = ukm_test_helper.GetUkmReport();
  int noisedBirthYear =
      metrics::test::GetNoisedBirthYear(GetLocalState(), year);

  return report && gender == report->user_demographics().gender() &&
         noisedBirthYear == report->user_demographics().birth_year();
}

+ (BOOL)UKMReportHasUserDemographics {
  ukm::UkmTestHelper ukm_test_helper(GetUkmService());
  std::unique_ptr<ukm::Report> report = ukm_test_helper.GetUkmReport();
  return report && report->has_user_demographics();
}

+ (void)buildAndStoreUMALog {
  metrics::test::BuildAndStoreLog(GetMetricsService());
}

+ (BOOL)hasUnsentUMALogs {
  return metrics::test::HasUnsentLogs(GetMetricsService());
}

+ (BOOL)UMALogHasBirthYear:(int)year
                    gender:(metrics::UserDemographicsProto::Gender)gender {
  if (![self UMALogHasUserDemographics]) {
    return NO;
  }
  std::unique_ptr<metrics::ChromeUserMetricsExtension> log =
      metrics::test::GetLastUmaLog(GetMetricsService());
  int noisedBirthYear =
      metrics::test::GetNoisedBirthYear(GetLocalState(), year);

  return noisedBirthYear == log->user_demographics().birth_year() &&
         gender == log->user_demographics().gender();
}

+ (BOOL)UMALogHasUserDemographics {
  if (![self hasUnsentUMALogs]) {
    return NO;
  }
  std::unique_ptr<metrics::ChromeUserMetricsExtension> log =
      metrics::test::GetLastUmaLog(GetMetricsService());
  return log && log->has_user_demographics();
}

+ (BOOL)checkDWARecordingEnabled:(BOOL)enabled {
  ConditionBlock condition = ^{
    return metrics::dwa::DwaRecorder::Get()->IsEnabled() == enabled;
  };
  return base::test::ios::WaitUntilConditionOrTimeout(
      syncher::kSyncDWAOperationsTimeout, condition);
}

+ (BOOL)DWARecorderAllowedForAllProfiles:(BOOL)state {
  ConditionBlock condition = ^{
    return GetApplicationContext()
               ->GetMetricsServicesManager()
               ->IsDwaAllowedForAllProfiles() == state;
  };
  return base::test::ios::WaitUntilConditionOrTimeout(
      syncher::kSyncDWAOperationsTimeout, condition);
}

+ (BOOL)DWARecorderHasEntries:(BOOL)state {
  ConditionBlock condition = ^{
    return metrics::dwa::DwaRecorder::Get()->HasEntries() == state;
  };
  return base::test::ios::WaitUntilConditionOrTimeout(
      syncher::kSyncDWAOperationsTimeout, condition);
}

+ (BOOL)hasUnsentDWALogs:(BOOL)state {
  ConditionBlock condition = ^{
    return GetDwaService()->unsent_log_store()->has_unsent_logs() == state;
  };
  return base::test::ios::WaitUntilConditionOrTimeout(
      syncher::kSyncDWAOperationsTimeout, condition);
}

+ (void)recordTestDWAEntryMetric {
  dwa::DwaEntryBuilder builder("Kangaroo.Jumped");
  builder.SetContent("https://adtech.com");
  builder.SetMetric("Length", 5);
  builder.Record(metrics::dwa::DwaRecorder::Get());
}

+ (void)DWAServiceFlushCall {
  GetDwaService()->Flush(
      metrics::MetricsLogsEventManager::CreateReason::kPeriodic);
}

+ (void)clearDWARecorder {
  metrics::dwa::DwaRecorder::Get()->Purge();
}

+ (BOOL)isPumaReportingEnabled {
  return GetPumaService()->reporting_service()->reporting_active();
}

+ (BOOL)hasUnsentPumaLogs {
  return GetPumaService()
      ->reporting_service()
      ->unsent_log_store()
      ->has_unsent_logs();
}

+ (void)purgePumaLogs {
  GetPumaService()->reporting_service()->unsent_log_store()->Purge();
}

+ (void)recordTestPumaMetric {
  base::PumaHistogramBoolean(base::PumaType::kRc, kTestBooleanHistogram, true);
  base::PumaHistogramExactLinear(base::PumaType::kRc, kTestLinearHistogram, 50,
                                 101);
  base::PumaHistogramEnumeration(base::PumaType::kRc, kTestEnumHistogram,
                                 TestEnum::kValueA);
}

+ (void)flushPumaService {
  GetPumaService()->Flush(
      metrics::MetricsLogsEventManager::CreateReason::kPeriodic);
}

+ (NSDictionary*)lastPumaRcProfile {
  metrics::private_metrics::PrivateMetricsReportingService* reporting_service =
      GetPumaService()->reporting_service();
  if (!reporting_service) {
    return nil;
  }

  metrics::UnsentLogStore* log_store = reporting_service->unsent_log_store();
  log_store->StageNextLog();

  if (log_store->staged_log().empty()) {
    return nil;
  }

  std::string uncompressed_log_data;
  if (!compression::GzipUncompress(log_store->staged_log(),
                                   &uncompressed_log_data)) {
    return nil;
  }

  private_metrics::PrivateMetricEndpointPayload payload;
  if (!payload.ParseFromString(uncompressed_log_data)) {
    return nil;
  }

  if (!payload.has_private_uma_report() ||
      !payload.private_uma_report().has_rc_profile()) {
    return nil;
  }

  const auto& rc_profile = payload.private_uma_report().rc_profile();

  return @{
    @"platform" : @(rc_profile.platform()),
    @"milestone" : @(rc_profile.milestone()),
    @"profile_country_id" : @(rc_profile.profile_country_id()),
    @"channel" : @(rc_profile.channel()),
  };
}

+ (NSError*)setupHistogramTester {
  if (g_histogram_tester) {
    return testing::NSErrorWithLocalizedDescription(
        @"Cannot setup two histogram testers.");
  }
  g_histogram_tester = new chrome_test_util::HistogramTester();
  return nil;
}

+ (NSError*)releaseHistogramTester {
  if (!g_histogram_tester) {
    return testing::NSErrorWithLocalizedDescription(
        @"Cannot release histogram tester.");
  }
  delete g_histogram_tester;
  g_histogram_tester = nullptr;
  return nil;
}

+ (NSError*)expectTotalCount:(int)count forHistogram:(NSString*)histogram {
  if (!g_histogram_tester) {
    return testing::NSErrorWithLocalizedDescription(
        @"setupHistogramTester must be called before testing metrics.");
  }
  __block NSString* error = nil;

  g_histogram_tester->ExpectTotalCount(base::SysNSStringToUTF8(histogram),
                                       count, ^(NSString* e) {
                                         error = e;
                                       });
  if (error) {
    return testing::NSErrorWithLocalizedDescription(error);
  }
  return nil;
}

+ (NSError*)expectCount:(int)count
              forBucket:(int)bucket
           forHistogram:(NSString*)histogram {
  if (!g_histogram_tester) {
    return testing::NSErrorWithLocalizedDescription(
        @"setupHistogramTester must be called before testing metrics.");
  }
  __block NSString* error = nil;

  g_histogram_tester->ExpectBucketCount(base::SysNSStringToUTF8(histogram),
                                        bucket, count, ^(NSString* e) {
                                          error = e;
                                        });
  if (error) {
    return testing::NSErrorWithLocalizedDescription(error);
  }
  return nil;
}

+ (NSError*)expectUniqueSampleWithCount:(int)count
                              forBucket:(int)bucket
                           forHistogram:(NSString*)histogram {
  if (!g_histogram_tester) {
    return testing::NSErrorWithLocalizedDescription(
        @"setupHistogramTester must be called before testing metrics.");
  }
  __block NSString* error = nil;

  g_histogram_tester->ExpectUniqueSample(base::SysNSStringToUTF8(histogram),
                                         bucket, count, ^(NSString* e) {
                                           error = e;
                                         });
  if (error) {
    return testing::NSErrorWithLocalizedDescription(error);
  }
  return nil;
}

+ (NSError*)expectSum:(NSInteger)sum forHistogram:(NSString*)histogram {
  if (!g_histogram_tester) {
    return testing::NSErrorWithLocalizedDescription(
        @"setupHistogramTester must be called before testing metrics.");
  }
  std::unique_ptr<base::HistogramSamples> samples =
      g_histogram_tester->GetHistogramSamplesSinceCreation(
          base::SysNSStringToUTF8(histogram));
  if (samples->sum() != sum) {
    return testing::NSErrorWithLocalizedDescription([NSString
        stringWithFormat:
            @"Sum of histogram %@ mismatch. Expected %ld, Observed: %ld",
            histogram, static_cast<long>(sum),
            static_cast<long>(samples->sum())]);
  }
  return nil;
}

+ (NSError*)setupUserActionTester {
  if (g_user_action_tester) {
    return testing::NSErrorWithLocalizedDescription(
        @"Cannot setup two user action testers.");
  }
  g_user_action_tester = new base::UserActionTester();
  return nil;
}

+ (NSError*)releaseUserActionTester {
  if (!g_user_action_tester) {
    return testing::NSErrorWithLocalizedDescription(
        @"Cannot release user action tester.");
  }
  delete g_user_action_tester;
  g_user_action_tester = nullptr;
  return nil;
}

+ (NSError*)expectCount:(int)expectedCount forUserAction:(NSString*)userAction {
  if (!g_user_action_tester) {
    return testing::NSErrorWithLocalizedDescription(
        @"setupHistogramTester must be called before testing metrics.");
  }

  int count =
      g_user_action_tester->GetActionCount(base::SysNSStringToUTF8(userAction));
  if (expectedCount != count) {
    NSString* errorString =
        [NSString stringWithFormat:@"Expected %i count of %@. Got %i instead.",
                                   expectedCount, userAction, count];
    return testing::NSErrorWithLocalizedDescription(errorString);
  }
  return nil;
}

@end
