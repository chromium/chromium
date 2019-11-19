// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_BASE_PERF_TEST_IOS_H_
#define IOS_CHROME_TEST_BASE_PERF_TEST_IOS_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"
#include "base/time/time.h"
#import "ios/chrome/test/block_cleanup_test.h"
#include "ios/web/public/test/scoped_testing_web_client.h"
#include "ios/web/public/test/web_task_environment.h"

typedef base::TimeDelta (^TimedActionBlock)(int index);

// Base class for PerfTest providing some common functions that are
// generally useful.
class PerfTest : public BlockCleanupTest {
 public:
  explicit PerfTest(std::string testGroup);
  PerfTest(std::string testGroup,
           std::string firstLabel,
           std::string averageLabel,
           bool isWaterfall,
           bool verbose,
           int repeat);
  ~PerfTest() override;

  // Convenience methods to display the performance timing data.
  // All recorded values will show up on the bot performance graphs.  If
  // |isWaterfall_| is true, the performance data recorded will show up on the
  // waterfall. In the majority of the cases, |isWaterfall_| should be false.
  virtual void LogPerfValue(std::string testName,
                            double value,
                            std::string unit);
  virtual void LogPerfTiming(std::string testName, base::TimeDelta elapsed);

  // Utility method to run a test multiple times.
  // The first run is counted separately to account for possible lazy
  // initialization overhead. Subsequent run times are averaged to provide
  // reliable timings for comparison.
  virtual void RepeatTimedRuns(std::string testName,
                               TimedActionBlock timedAction,
                               ProceduralBlock postAction);
  virtual void RepeatTimedRuns(std::string testName,
                               TimedActionBlock timedAction,
                               ProceduralBlock postAction,
                               int repeatCount);

  // Computes the average time, and, optionally, returns the maximum and
  // minimum times seen.
  static base::TimeDelta CalculateAverage(base::TimeDelta* times,
                                          int count,
                                          base::TimeDelta* min_time,
                                          base::TimeDelta* max_time);

 private:
  // Name for this group of perf tests.
  std::string testGroup_;
  // The label added to the first test of multiple tests.
  std::string firstLabel_;
  // The label added to the average number for multiple tests.
  std::string averageLabel_;
  // Whether the test result should appear in the waterfall.
  bool isWaterfall_;
  // Flag to show generate more output.
  bool verbose_;
  // Sets number of times to repeat a test when ran with RepeatTimedRuns.
  int repeatCount_;
  // The threads used for testing.
  web::WebTaskEnvironment task_environment_;
  // The WebClient for testing purposes.
  web::ScopedTestingWebClient web_client_;
};

#endif  // IOS_CHROME_TEST_BASE_PERF_TEST_IOS_H_
