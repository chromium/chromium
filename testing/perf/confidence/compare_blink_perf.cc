// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A utility to compare two runs of a style perftest, in the perftest
// output format, and compute confidence intervals. The simplest way
// to get the right information is to build a binary before and after
// changes and then run it continuously every-other for a while until
// you feel you have enough data:
//
//   rm -f old.txt new.txt; \
//   while :; do
//     taskset -c 2,4,6,8 ./out/Release/blink_perf___old \
//       --gtest_filter=StyleCalcPerfTest.\* 2>&1 | tee -a old.txt;
//     taskset -c 2,4,6,8 ./out/Release/blink_perf_tests \
//       --gtest_filter=StyleCalcPerfTest.\* 2>&1 | tee -a new.txt;
//   done
//
// and then run ./out/Release/compare_blink_perf old.txt new.txt.
// (Possibly under watch -n 1 if you want to look at data as it
// comes in, though beware of p-hacking.)
//
// TODO(sesse): Consider whether we should remove the first few
// runs, as they are frequently outliers.

#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "testing/perf/confidence/ratio_bootstrap_estimator.h"

#ifdef UNSAFE_BUFFERS_BUILD
// Not used with untrusted inputs.
#pragma allow_unsafe_buffers
#endif

using std::max;
using std::min;
using std::numeric_limits;
using std::pair;
using std::sort;
using std::string;
using std::unordered_map;
using std::vector;

namespace {

string BeautifyCategory(const string& category) {
  if (category == "BlinkStyleInitialCalcTime") {
    return "Initial style (µs)";
  } else if (category == "BlinkStyleRecalcTime") {
    return "Recalc style (µs)";
  } else if (category == "BlinkStyleParseTime") {
    return "Parse (µs)";
  } else {
    return category;
  }
}

bool CodeUnitCompareIgnoringASCIICaseLessThan(const string& a,
                                              const string& b) {
  return lexicographical_compare(
      a.begin(), a.end(), b.begin(), b.end(),
      [](char c1, char c2) { return std::tolower(c1) < std::tolower(c2); });
}

// The structure is e.g. BlinkStyleParseTime -> Video -> [100 us, 90 us, ...]
unordered_map<string, unordered_map<string, vector<double>>> ReadFile(
    const char* filename) {
  unordered_map<string, unordered_map<string, vector<double>>> measurements;

  FILE* fp = fopen(filename, "r");
  if (fp == nullptr) {
    perror(filename);
    exit(1);
  }
  while (!feof(fp)) {
    char buf[4096];
    if (fgets(buf, sizeof(buf), fp) == nullptr) {
      break;
    }
    string str(buf);
    if (str.length() > 1 && str[str.length() - 1] == '\n') {
      str.resize(str.length() - 1);
    }
    if (str.length() > 1 && str[str.length() - 1] == '\r') {
      str.resize(str.length() - 1);
    }

    // A result line looks like: *RESULT BlinkStyleParseTime: Video= 11061 us
    vector<string> cols{""};
    for (char ch : str) {
      if (ch == ' ') {
        cols.push_back("");
      } else {
        cols.back().push_back(ch);
      }
    }
    if (cols.size() != 5 || cols[0] != "*RESULT" || !cols[1].ends_with(":") ||
        !cols[2].ends_with("=") || cols[4] != "us") {
      continue;
    }

    string category = cols[1];
    category.resize(category.length() - 1);
    category = BeautifyCategory(category);

    string benchmark = cols[2];
    benchmark.resize(benchmark.length() - 1);

    double val;
    if (!base::StringToDouble(cols[3], &val)) {
      continue;
    }
    measurements[category][benchmark].push_back(val);
  }

  fclose(fp);
  return measurements;
}

// Find the number of trials, for display.
void FindNumberOfTrials(
    const unordered_map<string, unordered_map<string, vector<double>>>&
        measurements,
    unsigned& min_num_trials,
    unsigned& max_num_trials) {
  for (const auto& [category, entry] : measurements) {
    for (const auto& [benchmark, samples] : entry) {
      min_num_trials = min<unsigned>(min_num_trials, samples.size());
      max_num_trials = max<unsigned>(max_num_trials, samples.size());
    }
  }
}

struct Label {
  string benchmark;
  size_t data_index;
};

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3) {
    fprintf(stderr, "USAGE: compare_blink_perf OLD_LOG NEW_LOG\n");
    exit(1);
  }

  unordered_map<string, unordered_map<string, vector<double>>> before =
      ReadFile(argv[1]);
  unordered_map<string, unordered_map<string, vector<double>>> after =
      ReadFile(argv[2]);

  unsigned min_num_trials = numeric_limits<unsigned>::max();
  unsigned max_num_trials = numeric_limits<unsigned>::min();
  FindNumberOfTrials(before, min_num_trials, max_num_trials);
  FindNumberOfTrials(after, min_num_trials, max_num_trials);
  if (min_num_trials == max_num_trials) {
    printf("%u trial(s) on each side.\n", min_num_trials);
  } else {
    printf("%u–%u trial(s) on each side.\n", min_num_trials, max_num_trials);
  }

  // Now pair up the data. (The estimator treats them as unpaired,
  // but currently needs them to be of the same length within a single
  // benchmark.) We do one run per category, so that we can get
  // geometric means over them (RatioBootstrapEstimator doesn't support
  // arbitrary grouping).
  vector<string> sorted_categories;
  for (const auto& [category, entry] : before) {
    sorted_categories.push_back(category);
  }
  sort(sorted_categories.begin(), sorted_categories.end(),
       [](const string& a, const string& b) {
         return CodeUnitCompareIgnoringASCIICaseLessThan(a, b);
       });
  for (const string& category : sorted_categories) {
    vector<Label> labels;
    vector<vector<RatioBootstrapEstimator::Sample>> data;
    for (const auto& [benchmark, before_samples] :
         before.find(category)->second) {
      const auto after_entry = after.find(category);
      if (after_entry == after.end()) {
        continue;
      }
      const auto after_samples = after_entry->second.find(benchmark);
      if (after_samples == after_entry->second.end()) {
        continue;
      }

      vector<RatioBootstrapEstimator::Sample> samples;
      for (unsigned i = 0;
           i < std::min(before_samples.size(), after_samples->second.size());
           ++i) {
        samples.push_back({before_samples[i], after_samples->second[i]});
      }
      labels.emplace_back(Label{benchmark, data.size()});
      data.push_back(std::move(samples));
    }

    RatioBootstrapEstimator estimator(base::RandUint64());
    const unsigned kNumResamples = 2000;
    vector<RatioBootstrapEstimator::Estimate> estimates =
        estimator.ComputeRatioEstimates(data, kNumResamples,
                                        /*confidence_level=*/0.95,
                                        /*compute_geometric_mean=*/true);

    // Sort the labels for display.
    sort(labels.begin(), labels.end(), [](const Label& a, const Label& b) {
      return CodeUnitCompareIgnoringASCIICaseLessThan(a.benchmark, b.benchmark);
    });

    printf("\n");
    printf("%-20s %9s %9s %7s %17s\n", category.c_str(), "Before", "After",
           "Perf", "95% CI (BCa)");
    printf(
        "=================== ========= ========= ======= "
        "=================\n");
    for (const Label& label : labels) {
      // RatioBootstrapEstimator doesn't give us the plain means, so compute
      // that by hand.
      double sum_before = 0.0, sum_after = 0.0;
      for (const RatioBootstrapEstimator::Sample& sample :
           data[label.data_index]) {
        sum_before += sample.before;
        sum_after += sample.after;
      }
      double mean_before = sum_before / data[label.data_index].size();
      double mean_after = sum_after / data[label.data_index].size();

      const RatioBootstrapEstimator::Estimate& estimate =
          estimates[label.data_index];
      printf("%-19s %9.0f %9.0f %+6.1f%%  [%+5.1f%%, %+5.1f%%]\n",
             label.benchmark.c_str(), mean_before, mean_after,
             100.0 * (estimate.point_estimate - 1.0),
             100.0 * (estimate.lower - 1.0), 100.0 * (estimate.upper - 1.0));
    }

    const RatioBootstrapEstimator::Estimate& estimate = estimates[data.size()];
    printf("%-19s %9s %9s %+6.1f%%  [%+5.1f%%, %+5.1f%%]\n", "Geometric mean",
           "", "", 100.0 * (estimate.point_estimate - 1.0),
           100.0 * (estimate.lower - 1.0), 100.0 * (estimate.upper - 1.0));
  }
}
