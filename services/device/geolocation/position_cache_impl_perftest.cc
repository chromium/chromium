// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/position_cache_impl.h"

#include <memory>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "services/device/geolocation/position_cache_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace {

constexpr char kMetricPrefixPositionCacheImpl[] = "PositionCacheImpl.";
constexpr char kMetricAddTimeMs[] = "add_time";
constexpr char kMetricFindTimeMs[] = "find_time";
constexpr char kStoryBaseline[] = "baseline_story";

perf_test::PerfResultReporter SetUpReporter(const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefixPositionCacheImpl, story);
  reporter.RegisterImportantMetric(kMetricAddTimeMs, "ms");
  reporter.RegisterImportantMetric(kMetricFindTimeMs, "ms");
  return reporter;
}

}  // namespace

namespace device {

class PositionCacheImplPerfTest : public ::testing::Test {
 public:
  PositionCacheImplPerfTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        cache_(task_environment_.GetMockTickClock()) {}

  void SetUp() override {
    data_.reserve(kBatchSize);
    for (size_t i = 0; i < kBatchSize; ++i)
      data_.push_back(std::make_pair(testing::CreateDefaultUniqueWifiData(),
                                     testing::CreateGeoposition(i % 90)));
  }

 protected:
  static constexpr size_t kBatchSize = 5000;
  std::vector<std::pair<WifiData, mojom::Geoposition>> data_;
  base::test::TaskEnvironment task_environment_;
  PositionCacheImpl cache_;
};

TEST_F(PositionCacheImplPerfTest, Adding) {
  base::Time start = base::Time::Now();
  for (const auto& pair : data_)
    cache_.CachePosition(pair.first, pair.second);
  base::Time end = base::Time::Now();
  auto reporter = SetUpReporter(kStoryBaseline);
  reporter.AddResult(kMetricAddTimeMs, base::TimeDelta(end - start));
}

TEST_F(PositionCacheImplPerfTest, Finding) {
  for (const auto& pair : data_)
    cache_.CachePosition(pair.first, pair.second);
  base::Time start = base::Time::Now();
  for (const auto& pair : data_)
    cache_.FindPosition(pair.first);
  base::Time end = base::Time::Now();
  auto reporter = SetUpReporter(kStoryBaseline);
  reporter.AddResult(kMetricFindTimeMs, base::TimeDelta(end - start));
}
}  // namespace device
