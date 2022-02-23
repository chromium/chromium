// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/perf/perf_result_reporter.h"

#include <ostream>
#include <vector>

#include "base/check.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "testing/perf/perf_test.h"

namespace {

// These characters mess with either the stdout parsing or the dashboard itself.
static const base::NoDestructor<std::vector<std::string>> kInvalidCharacters{
    {"/", ":", "="}};

void CheckForInvalidCharacters(const std::string& str) {
  for (const auto& invalid : *kInvalidCharacters) {
    CHECK(str.find(invalid) == std::string::npos)
        << "Given invalid character for perf names '" << invalid << "'";
  }
}

}  // namespace

namespace perf_test {

PerfResultReporter::PerfResultReporter(const std::string& metric_basename,
                                       const std::string& story_name)
    : metric_basename_(metric_basename), story_name_(story_name) {
  CheckForInvalidCharacters(metric_basename_);
  CheckForInvalidCharacters(story_name_);
}

PerfResultReporter::~PerfResultReporter() = default;

void PerfResultReporter::RegisterFyiMetric(const std::string& metric_suffix,
                                           const std::string& units) {
  RegisterMetric(metric_suffix, units, false);
}

void PerfResultReporter::RegisterImportantMetric(
    const std::string& metric_suffix,
    const std::string& units) {
  RegisterMetric(metric_suffix, units, true);
}

void PerfResultReporter::AddResult(const std::string& metric_suffix,
                                   size_t value) const {
  auto info = GetMetricInfoOrFail(metric_suffix);

  PrintResult(metric_basename_, metric_suffix, story_name_, value, info.units,
              info.important);
}

void PerfResultReporter::AddResult(const std::string& metric_suffix,
                                   double value) const {
  auto info = GetMetricInfoOrFail(metric_suffix);

  PrintResult(metric_basename_, metric_suffix, story_name_, value, info.units,
              info.important);
}

void PerfResultReporter::AddResult(const std::string& metric_suffix,
                                   const std::string& value) const {
  auto info = GetMetricInfoOrFail(metric_suffix);

  PrintResult(metric_basename_, metric_suffix, story_name_, value, info.units,
              info.important);
}

void PerfResultReporter::AddResult(const std::string& metric_suffix,
                                   base::TimeDelta value) const {
  auto info = GetMetricInfoOrFail(metric_suffix);

  // Decide what time unit to convert the TimeDelta into. Units are based on
  // the legacy units in
  // https://cs.chromium.org/chromium/src/third_party/catapult/tracing/tracing/value/legacy_unit_info.py?q=legacy_unit_info
  double time = 0;
  if (info.units == "seconds") {
    time = value.InSecondsF();
  } else if (info.units == "ms" || info.units == "milliseconds") {
    time = value.InMillisecondsF();
  } else if (info.units == "us") {
    time = value.InMicrosecondsF();
  } else if (info.units == "ns") {
    time = value.InNanoseconds();
  } else {
    NOTREACHED() << "Attempted to use AddResult with a TimeDelta when "
                 << "registered unit for metric " << metric_suffix << " is "
                 << info.units;
  }

  PrintResult(metric_basename_, metric_suffix, story_name_, time, info.units,
              info.important);
}

void PerfResultReporter::AddResultList(const std::string& metric_suffix,
                                       const std::string& values) const {
  auto info = GetMetricInfoOrFail(metric_suffix);

  PrintResultList(metric_basename_, metric_suffix, story_name_, values,
                  info.units, info.important);
}

void PerfResultReporter::AddResultMeanAndError(
    const std::string& metric_suffix,
    const std::string& mean_and_error) {
  auto info = GetMetricInfoOrFail(metric_suffix);

  PrintResultMeanAndError(metric_basename_, metric_suffix, story_name_,
                          mean_and_error, info.units, info.important);
}

bool PerfResultReporter::GetMetricInfo(const std::string& metric_suffix,
                                       MetricInfo* out) const {
  auto iter = metric_map_.find(metric_suffix);
  if (iter == metric_map_.end()) {
    return false;
  }

  *out = iter->second;
  return true;
}

void PerfResultReporter::RegisterMetric(const std::string& metric_suffix,
                                        const std::string& units,
                                        bool important) {
  CheckForInvalidCharacters(metric_suffix);
  CHECK(metric_map_.count(metric_suffix) == 0);
  metric_map_.insert({metric_suffix, {units, important}});
}

MetricInfo PerfResultReporter::GetMetricInfoOrFail(
    const std::string& metric_suffix) const {
  MetricInfo info;
  CHECK(GetMetricInfo(metric_suffix, &info))
      << "Attempted to use unregistered metric " << metric_suffix;
  return info;
}

}  // namespace perf_test
