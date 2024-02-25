// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/application_delegate/mock_metrickit_metric_payload.h"

#import <Foundation/Foundation.h>
#import <MetricKit/MetricKit.h>

#import "base/strings/sys_string_conversions.h"
#import "components/version_info/version_info.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

id MockMXMetadata() {
  id metadata = OCMClassMock([MXMetaData class]);
  OCMStub([metadata applicationBuildVersion])
      .andReturn(base::SysUTF8ToNSString(version_info::GetVersionNumber()));
  return metadata;
}

id MockNSMeasurement(double value) {
  id mock_measurement = OCMClassMock([NSMeasurement class]);
  // Conversion is not supported.
  OCMStub([mock_measurement measurementByConvertingToUnit:[OCMArg any]])
      .andReturn(mock_measurement);
  OCMStub([mock_measurement doubleValue]).andReturn(value);
  return mock_measurement;
}

id MockMXMemoryMetric(NSDictionary* dictionary) {
  id memory_metric = OCMClassMock([MXMemoryMetric class]);
  NSNumber* suspended_memory =
      [dictionary objectForKey:@"averageSuspendedMemory"];
  if (suspended_memory) {
    id mock_average = OCMClassMock([MXAverage class]);
    id mock_measurement = MockNSMeasurement(suspended_memory.doubleValue);
    OCMClassMock([NSMeasurement class]);
    // Conversion is not supported.
    OCMStub([mock_average averageMeasurement]).andReturn(mock_measurement);
    OCMStub([memory_metric averageSuspendedMemory]).andReturn(mock_average);
  }

  NSNumber* peak_memory = [dictionary objectForKey:@"peakMemoryUsage"];
  if (peak_memory) {
    id mock_measurement = OCMClassMock([NSMeasurement class]);
    // Conversion is not supported.
    OCMStub([mock_measurement measurementByConvertingToUnit:[OCMArg any]])
        .andReturn(mock_measurement);
    OCMStub([mock_measurement doubleValue]).andReturn(peak_memory.doubleValue);
    OCMStub([memory_metric peakMemoryUsage]).andReturn(mock_measurement);
  }
  return memory_metric;
}

id MockMXAppRunTimeMetric(NSDictionary* dictionary) {
  id app_run_time = OCMClassMock([MXAppRunTimeMetric class]);
  NSNumber* cumulative_foreground =
      [dictionary objectForKey:@"cumulativeForegroundTime"];
  if (cumulative_foreground) {
    id mock_measurement = OCMClassMock([NSMeasurement class]);
    // Conversion is not supported.
    OCMStub([mock_measurement measurementByConvertingToUnit:[OCMArg any]])
        .andReturn(mock_measurement);
    OCMStub([mock_measurement doubleValue])
        .andReturn(cumulative_foreground.doubleValue);
    OCMStub([app_run_time cumulativeForegroundTime])
        .andReturn(mock_measurement);
  }
  NSNumber* cumulative_background =
      [dictionary objectForKey:@"cumulativeBackgroundTime"];
  if (cumulative_background) {
    id mock_measurement = MockNSMeasurement(cumulative_background.doubleValue);
    OCMStub([app_run_time cumulativeBackgroundTime])
        .andReturn(mock_measurement);
  }
  return app_run_time;
}

id MockMXHistogram(NSDictionary* dictionary, int delta) {
  id histogram = OCMClassMock([MXHistogram class]);
  OCMStub([histogram totalBucketCount]).andReturn(dictionary.count);
  NSMutableArray* buckets = [[NSMutableArray alloc] init];
  for (NSNumber* key in dictionary) {
    id bucket = OCMClassMock([MXHistogramBucket class]);
    OCMStub([bucket bucketStart])
        .andReturn(MockNSMeasurement(key.doubleValue - delta));
    OCMStub([bucket bucketEnd])
        .andReturn(MockNSMeasurement(key.doubleValue + delta));
    NSNumber* value = dictionary[key];
    OCMStub([bucket bucketCount]).andReturn(value.intValue);
    [buckets addObject:bucket];
  }

  // This uses `andDo` rather than `andReturn` since the objectEnumerator it
  // returns needs to change each time it's called.
  OCMStub([histogram bucketEnumerator]).andDo(^(NSInvocation* invocation) {
    NSEnumerator* enumerator = buckets.objectEnumerator;
    [invocation retainArguments];
    [invocation setReturnValue:&enumerator];
  });
  return histogram;
}

id MockMXAppLaunchMetric(NSDictionary* dictionary) {
  id app_launch = OCMClassMock([MXAppLaunchMetric class]);
  NSDictionary* first_draw =
      [dictionary objectForKey:@"histogrammedTimeToFirstDrawKey"];
  if (first_draw) {
    id first_draw_histogram = MockMXHistogram(first_draw, 5);
    OCMStub([app_launch histogrammedTimeToFirstDraw])
        .andReturn(first_draw_histogram);
  }

  NSDictionary* resume_time =
      [dictionary objectForKey:@"histogrammedResumeTime"];
  if (resume_time) {
    id resume_time_histogram = MockMXHistogram(resume_time, 5);
    OCMStub([app_launch histogrammedApplicationResumeTime])
        .andReturn(resume_time_histogram);
  }

  return app_launch;
}

id MockMXAppResponsivenessMetric(NSDictionary* dictionary) {
  id responsiveness = OCMClassMock([MXAppResponsivenessMetric class]);
  NSDictionary* hang_time =
      [dictionary objectForKey:@"histogrammedAppHangTime"];
  if (hang_time) {
    id hang_time_histogram = MockMXHistogram(hang_time, 5);
    OCMStub([responsiveness histogrammedApplicationHangTime])
        .andReturn(hang_time_histogram);
  }
  return responsiveness;
}

id MockMXAppExitMetric(NSDictionary* dictionary) {
  id app_exit_metric = OCMClassMock([MXAppExitMetric class]);
  id foreground = OCMClassMock([MXForegroundExitData class]);
  NSDictionary* foreground_dict = dictionary[@"foregroundExitData"];
  OCMStub([foreground cumulativeNormalAppExitCount])
      .andReturn(
          [foreground_dict[@"cumulativeNormalAppExitCount"] integerValue]);
  OCMStub([foreground cumulativeAbnormalExitCount])
      .andReturn(
          [foreground_dict[@"cumulativeAbnormalExitCount"] integerValue]);
  OCMStub([foreground cumulativeAppWatchdogExitCount])
      .andReturn(
          [foreground_dict[@"cumulativeAppWatchdogExitCount"] integerValue]);
  OCMStub([foreground cumulativeMemoryResourceLimitExitCount])
      .andReturn([foreground_dict[@"cumulativeMemoryResourceLimitExitCount"]
          integerValue]);
  OCMStub([foreground cumulativeBadAccessExitCount])
      .andReturn(
          [foreground_dict[@"cumulativeBadAccessExitCount"] integerValue]);
  OCMStub([foreground cumulativeIllegalInstructionExitCount])
      .andReturn([foreground_dict[@"cumulativeIllegalInstructionExitCount"]
          integerValue]);
  OCMStub([app_exit_metric foregroundExitData]).andReturn(foreground);

  id background = OCMClassMock([MXBackgroundExitData class]);
  NSDictionary* background_dict = dictionary[@"backgroundExitData"];
  OCMStub([background cumulativeNormalAppExitCount])
      .andReturn(
          [background_dict[@"cumulativeNormalAppExitCount"] integerValue]);
  OCMStub([background cumulativeAbnormalExitCount])
      .andReturn(
          [background_dict[@"cumulativeAbnormalExitCount"] integerValue]);
  OCMStub([background cumulativeAppWatchdogExitCount])
      .andReturn(
          [background_dict[@"cumulativeAppWatchdogExitCount"] integerValue]);
  OCMStub([background cumulativeCPUResourceLimitExitCount])
      .andReturn([background_dict[@"cumulativeCPUResourceLimitExitCount"]
          integerValue]);
  OCMStub([background cumulativeMemoryResourceLimitExitCount])
      .andReturn([background_dict[@"cumulativeMemoryResourceLimitExitCount"]
          integerValue]);
  OCMStub([background cumulativeMemoryPressureExitCount])
      .andReturn(
          [background_dict[@"cumulativeMemoryPressureExitCount"] integerValue]);
  OCMStub([background cumulativeSuspendedWithLockedFileExitCount])
      .andReturn([background_dict[@"cumulativeSuspendedWithLockedFileExitCount"]
          integerValue]);
  OCMStub([background cumulativeBadAccessExitCount])
      .andReturn(
          [background_dict[@"cumulativeBadAccessExitCount"] integerValue]);
  OCMStub([background cumulativeIllegalInstructionExitCount])
      .andReturn([background_dict[@"cumulativeIllegalInstructionExitCount"]
          integerValue]);
  OCMStub([background cumulativeBackgroundTaskAssertionTimeoutExitCount])
      .andReturn(
          [background_dict[@"cumulativeBackgroundTaskAssertionTimeoutExitCount"]
              integerValue]);
  OCMStub([app_exit_metric backgroundExitData]).andReturn(background);

  return app_exit_metric;
}

id MockMetricPayload(NSDictionary* dictionary) {
  id mock_report = OCMClassMock([MXMetricPayload class]);
  NSDictionary* application_time_metrics_dict =
      [dictionary objectForKey:@"applicationTimeMetrics"];
  if (application_time_metrics_dict) {
    id application_time_metrics =
        MockMXAppRunTimeMetric(application_time_metrics_dict);
    OCMStub([mock_report applicationTimeMetrics])
        .andReturn(application_time_metrics);
  }
  NSDictionary* memory_metrics_dict =
      [dictionary objectForKey:@"memoryMetrics"];
  if (memory_metrics_dict) {
    id memory_metrics = MockMXMemoryMetric(memory_metrics_dict);
    OCMStub([mock_report memoryMetrics]).andReturn(memory_metrics);
  }
  NSDictionary* launch_metrics_dict =
      [dictionary objectForKey:@"applicationLaunchMetrics"];
  if (memory_metrics_dict) {
    id launch_metrics = MockMXAppLaunchMetric(launch_metrics_dict);
    OCMStub([mock_report applicationLaunchMetrics]).andReturn(launch_metrics);
  }
  NSDictionary* responsiveness_metrics_dict =
      [dictionary objectForKey:@"applicationResponsivenessMetrics"];
  if (responsiveness_metrics_dict) {
    id responsiveness_metrics =
        MockMXAppResponsivenessMetric(responsiveness_metrics_dict);
    OCMStub([mock_report applicationResponsivenessMetrics])
        .andReturn(responsiveness_metrics);
  }
  NSDictionary* exit_metrics_dict =
      [dictionary objectForKey:@"applicationExitMetrics"];
  if (exit_metrics_dict) {
    id exit_metrics = MockMXAppExitMetric(exit_metrics_dict);
    OCMStub([mock_report applicationExitMetrics]).andReturn(exit_metrics);
  }

  OCMStub([mock_report metaData]).andReturn(MockMXMetadata());
  return mock_report;
}
