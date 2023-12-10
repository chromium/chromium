// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/metrics/ios_password_manager_visits_recorder.h"
#import "base/test/metrics/histogram_tester.h"
#import "ios/chrome/browser/passwords/model/metrics/ios_password_manager_metrics.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using base::HistogramTester;

namespace {
// Verifies that a given number of password manager visits have been recorded.
void CheckPasswordManagerVisitMetricsCount(
    int count,
    const HistogramTester& histogram_tester) {
  histogram_tester.ExpectUniqueSample(
      /*name=*/password_manager::kPasswordManagerSurfaceVisitHistogramName,
      /*sample=*/password_manager::PasswordManagerSurface::kPasswordList,
      /*count=*/count);
}

}  // namespace

namespace password_manager {

using IOSPasswordManagerVisitsRecorderTest = PlatformTest;

// Validates the recorder only logs visits the first time it is used.
TEST_F(IOSPasswordManagerVisitsRecorderTest, VisitMetricsRecordedOnlyOnce) {
  HistogramTester histogram_tester;
  IOSPasswordManagerVisitsRecorder* visits_recorder =
      [[IOSPasswordManagerVisitsRecorder alloc]
          initWithPasswordManagerSurface:
              password_manager::PasswordManagerSurface::kPasswordList];

  CheckPasswordManagerVisitMetricsCount(0, histogram_tester);

  [visits_recorder maybeRecordVisitMetric];
  // The first invocation should log a visit.
  CheckPasswordManagerVisitMetricsCount(1, histogram_tester);

  // Any subsequent calls should be no-op;
  [visits_recorder maybeRecordVisitMetric];

  CheckPasswordManagerVisitMetricsCount(1, histogram_tester);
}

}  // namespace password_manager
