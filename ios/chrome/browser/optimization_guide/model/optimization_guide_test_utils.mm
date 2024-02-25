// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/optimization_guide/model/optimization_guide_test_utils.h"

#import "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "testing/gtest/include/gtest/gtest.h"

void RetryForHistogramUntilCountReached(
    const base::HistogramTester* histogram_tester,
    const std::string& histogram_name,
    int count) {
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        base::RunLoop().RunUntilIdle();
        int total = 0;
        for (const auto& bucket :
             histogram_tester->GetAllSamples(histogram_name)) {
          total += bucket.count;
        }
        return total >= count;
      }));
}
