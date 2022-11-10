// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/application_delegate/metric_kit_subscriber.h"

#import <Foundation/Foundation.h>
#import <MetricKit/MetricKit.h>

#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/ios/ios_util.h"
#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "components/crash/core/app/crashpad.h"
#import "components/crash/core/common/reporter_running_ios.h"
#import "ios/chrome/app/application_delegate/mock_metrickit_metric_payload.h"
#import "testing/platform_test.h"
#import "third_party/crashpad/crashpad/client/crash_report_database.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class MetricKitSubscriberTest : public PlatformTest {
 public:
  void SetUp() override {
    ASSERT_FALSE(crash_reporter::internal::GetCrashReportDatabase());
    ASSERT_TRUE(database_dir_.CreateUniqueTempDir());
    database_dir_path_ = database_dir_.GetPath();
    database_ = crashpad::CrashReportDatabase::Initialize(database_dir_path_);
    crash_reporter::internal::SetCrashReportDatabaseForTesting(
        database_.get(), &database_dir_path_);

    std::vector<crash_reporter::Report> reports;
    crash_reporter::GetReports(&reports);
    ASSERT_EQ(reports.size(), 0u);
    crash_reporter::SetCrashpadRunning(true);
  }

  void TearDown() override {
    crash_reporter::internal::SetCrashReportDatabaseForTesting(nullptr,
                                                               nullptr);
    crash_reporter::SetCrashpadRunning(false);
  }

  auto Database() { return database_.get(); }

 private:
  base::ScopedTempDir database_dir_;
  base::FilePath database_dir_path_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<crashpad::CrashReportDatabase> database_;
};

TEST_F(MetricKitSubscriberTest, Metrics) {
  base::HistogramTester tester;
  MXMetricPayload* mock_report = MockMetricPayload(@{
    @"applicationTimeMetrics" :
        @{@"cumulativeForegroundTime" : @1, @"cumulativeBackgroundTime" : @2},
    @"memoryMetrics" :
        @{@"peakMemoryUsage" : @3, @"averageSuspendedMemory" : @4},
    @"applicationLaunchMetrics" : @{
      @"histogrammedResumeTime" : @{@25 : @5, @35 : @7},
      @"histogrammedTimeToFirstDrawKey" : @{@5 : @2, @15 : @4}
    },
    @"applicationExitMetrics" : @{
      @"backgroundExitData" : @{
        @"cumulativeAppWatchdogExitCount" : @1,
        @"cumulativeMemoryResourceLimitExitCount" : @2,
        // These two entries are present in the simulated payload but not in
        // the SDK.
        // @"cumulativeBackgroundURLSessionCompletionTimeoutExitCount" : @3,
        // @"cumulativeBackgroundFetchCompletionTimeoutExitCount" : @4,
        @"cumulativeAbnormalExitCount" : @5,
        @"cumulativeSuspendedWithLockedFileExitCount" : @6,
        @"cumulativeIllegalInstructionExitCount" : @7,
        @"cumulativeMemoryPressureExitCount" : @8,
        @"cumulativeBadAccessExitCount" : @9,
        @"cumulativeCPUResourceLimitExitCount" : @10,
        @"cumulativeBackgroundTaskAssertionTimeoutExitCount" : @11,
        @"cumulativeNormalAppExitCount" : @12
      },
      @"foregroundExitData" : @{
        @"cumulativeBadAccessExitCount" : @13,
        @"cumulativeAbnormalExitCount" : @14,
        @"cumulativeMemoryResourceLimitExitCount" : @15,
        @"cumulativeNormalAppExitCount" : @16,
        // This entry is present in the simulated payload but not in the SDK.
        // @"cumulativeCPUResourceLimitExitCount" : @17,
        @"cumulativeIllegalInstructionExitCount" : @18,
        @"cumulativeAppWatchdogExitCount" : @19
      }
    },
  });
  NSArray* array = @[ mock_report ];
  [[MetricKitSubscriber sharedInstance] didReceiveMetricPayloads:array];
  tester.ExpectUniqueTimeSample("IOS.MetricKit.ForegroundTimePerDay",
                                base::Seconds(1), 1);
  tester.ExpectUniqueTimeSample("IOS.MetricKit.BackgroundTimePerDay",
                                base::Seconds(2), 1);
  tester.ExpectUniqueSample("IOS.MetricKit.PeakMemoryUsage", 3, 1);
  tester.ExpectUniqueSample("IOS.MetricKit.AverageSuspendedMemory", 4, 1);

  tester.ExpectTotalCount("IOS.MetricKit.ApplicationResumeTime", 12);
  tester.ExpectBucketCount("IOS.MetricKit.ApplicationResumeTime", 25, 5);
  tester.ExpectBucketCount("IOS.MetricKit.ApplicationResumeTime", 35, 7);

  tester.ExpectTotalCount("IOS.MetricKit.TimeToFirstDraw", 6);
  tester.ExpectBucketCount("IOS.MetricKit.TimeToFirstDraw", 5, 2);
  tester.ExpectBucketCount("IOS.MetricKit.TimeToFirstDraw", 15, 4);

  tester.ExpectTotalCount("IOS.MetricKit.BackgroundExitData", 71);
  tester.ExpectBucketCount("IOS.MetricKit.BackgroundExitData", 2, 1);
  tester.ExpectBucketCount("IOS.MetricKit.BackgroundExitData", 4, 2);
  tester.ExpectBucketCount("IOS.MetricKit.BackgroundExitData", 1, 5);
  tester.ExpectBucketCount("IOS.MetricKit.BackgroundExitData", 6, 6);
  tester.ExpectBucketCount("IOS.MetricKit.BackgroundExitData", 8, 7);
  tester.ExpectBucketCount("IOS.MetricKit.BackgroundExitData", 5, 8);
  tester.ExpectBucketCount("IOS.MetricKit.BackgroundExitData", 7, 9);
  tester.ExpectBucketCount("IOS.MetricKit.BackgroundExitData", 3, 10);
  tester.ExpectBucketCount("IOS.MetricKit.BackgroundExitData", 9, 11);
  tester.ExpectBucketCount("IOS.MetricKit.BackgroundExitData", 0, 12);

  tester.ExpectTotalCount("IOS.MetricKit.ForegroundExitData", 95);
  tester.ExpectBucketCount("IOS.MetricKit.ForegroundExitData", 7, 13);
  tester.ExpectBucketCount("IOS.MetricKit.ForegroundExitData", 1, 14);
  tester.ExpectBucketCount("IOS.MetricKit.ForegroundExitData", 4, 15);
  tester.ExpectBucketCount("IOS.MetricKit.ForegroundExitData", 0, 16);
  tester.ExpectBucketCount("IOS.MetricKit.ForegroundExitData", 8, 18);
  tester.ExpectBucketCount("IOS.MetricKit.ForegroundExitData", 2, 19);
}

TEST_F(MetricKitSubscriberTest, SaveDiagnosticReport) {
  id mock_report = OCMClassMock([MXDiagnosticPayload class]);
  NSDate* date = [NSDate date];
  std::string file_data("report content");
  NSData* data = [NSData dataWithBytes:file_data.c_str()
                                length:file_data.size()];
  NSDateFormatter* formatter = [[NSDateFormatter alloc] init];
  [formatter setDateFormat:@"yyyyMMdd_HHmmss"];
  [formatter setTimeZone:[NSTimeZone timeZoneWithName:@"UTC"]];
  OCMStub([mock_report timeStampEnd]).andReturn(date);
  OCMStub([mock_report JSONRepresentation]).andReturn(data);
  NSArray* array = @[ mock_report ];

  id mock_diagnostic = OCMClassMock([MXCrashDiagnostic class]);
  OCMStub([mock_diagnostic JSONRepresentation]).andReturn(data);
  NSArray* mock_diagnostics = @[ mock_diagnostic ];
  OCMStub([mock_report crashDiagnostics]).andReturn(mock_diagnostics);
  [[MetricKitSubscriber sharedInstance] didReceiveDiagnosticPayloads:array];

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForFileOperationTimeout, ^bool() {
        base::RunLoop().RunUntilIdle();
        std::vector<crash_reporter::Report> reports;
        crash_reporter::GetReports(&reports);
        return reports.size() == 1;
      }));

  std::vector<crash_reporter::Report> reports;
  crash_reporter::GetReports(&reports);
  ASSERT_EQ(reports.size(), 1u);

  std::unique_ptr<const crashpad::CrashReportDatabase::UploadReport>
      upload_report;
  crashpad::UUID uuid;
  uuid.InitializeFromString(reports[0].local_id);
  EXPECT_EQ(Database()->GetReportForUploading(uuid, &upload_report),
            crashpad::CrashReportDatabase::kNoError);

  std::map<std::string, crashpad::FileReader*> attachments =
      upload_report->GetAttachments();
  EXPECT_EQ(attachments.size(), 1u);
  ASSERT_NE(attachments.find("MetricKit"), attachments.end());
  char result_buffer[sizeof(file_data)];
  attachments["MetricKit"]->Read(result_buffer, sizeof(result_buffer));

  NSData* result_data = [NSData dataWithBytes:result_buffer
                                       length:sizeof(result_buffer)];

  NSError* error = nil;
  result_data =
      [result_data decompressedDataUsingAlgorithm:NSDataCompressionAlgorithmZlib
                                            error:&error];
  ASSERT_NE(result_data, nil);
  EXPECT_EQ(memcmp([data bytes], [result_data bytes], data.length), 0);
}
