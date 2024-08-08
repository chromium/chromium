// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A small tool to take MotionMark/Speedometer CSV files from Pinpoint
// and compute confidence intervals. Not intended as a general CSV reader
// (we don't do things like escaping and quoting).
//
// _ci refers to confidence intervals, not continuous integration.

#include <stdio.h>
#include <stdlib.h>

#include <ranges>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/rand_util.h"
#include "base/strings/string_split.h"
#include "testing/perf/confidence/ratio_bootstrap_estimator.h"

#ifdef UNSAFE_BUFFERS_BUILD
// Not used with untrusted inputs.
#pragma allow_unsafe_buffers
#endif

using std::pair;
using std::string;
using std::string_view;
using std::unordered_map;
using std::vector;

namespace {

vector<string_view> SplitCSVLine(string_view str) {
  if (str.length() > 1 && str[str.length() - 1] == '\r') {
    str = str.substr(0, str.size() - 1);
  }
  return base::SplitStringPiece(str, ",", base::KEEP_WHITESPACE,
                                base::SPLIT_WANT_ALL);
}

vector<unordered_map<string, string>> ReadCSV(const char* filename) {
  string contents;
  if (!base::ReadFileToString(
          base::FilePath::FromUTF8Unsafe(string_view(filename)), &contents)) {
    perror(filename);
    exit(1);
  }

  vector<string_view> lines = base::SplitStringPiece(
      contents, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  vector<string_view> headers = SplitCSVLine(lines[0]);
  if (headers.empty()) {
    fprintf(stderr, "%s: Empty header line!\n", filename);
    exit(1);
  }

  vector<unordered_map<string, string>> result;
  for (unsigned i = 1; i < lines.size(); ++i) {
    vector<string_view> line = SplitCSVLine(lines[i]);
    if (line.size() != headers.size()) {
      fprintf(stderr, "%s: Line had %zu columns, expected %zu\n", filename,
              line.size(), headers.size());
      break;
    }

    unordered_map<string, string> fields;
    for (unsigned j = 0; j < line.size(); ++j) {
      fields.emplace(headers[j], std::move(line[j]));
    }
    result.push_back(std::move(fields));
  }

  return result;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "USAGE: pinpoint_ci CSV_FILE [CONFIDENCE_LEVEL]\n");
    exit(1);
  }

  // The default 0.99 matches Pinpoint.
  double confidence_level = (argc > 2) ? atof(argv[2]) : 0.99;
  unordered_map<string, pair<vector<double>, vector<double>>> samples;
  bool any_is_speedometer = false;

  for (unordered_map<string, string>& line : ReadCSV(argv[1])) {
    if (line.count("name") == 0 || line.count("displayLabel") == 0 ||
        line.count("avg") == 0) {
      continue;
    }
    const string& name = line["name"];
    const string& display_label = line["displayLabel"];
    double avg = atof(line["avg"].c_str());
    bool is_motionmark =
        name == "motionmark" || line.count("motionmarkTag") != 0;
    bool is_speedometer =
        name.find("TodoMVC") != string::npos ||
        (line.count("stories") != 0 && line["stories"] == "Speedometer3");
    if (!is_motionmark && !is_speedometer) {
      // Not the core metrics we are looking for.
      continue;
    }
    any_is_speedometer |= is_speedometer;
    if (name.find("/") != string::npos || name.find("Lower") != string::npos ||
        name.find("Upper") != string::npos) {
      // More sub-metrics.
      continue;
    }
    string story;
    if (name == "motionmark") {
      if (line.count("stories") == 0) {
        fprintf(stderr, "WARNING: Could not find MotionMark story\n");
        continue;
      }
      story = line["stories"];
    } else {
      story = name;
    }

    if (display_label.find("base:") != string::npos) {
      samples[story].first.push_back(avg);
    } else if (display_label.find("exp:") != string::npos) {
      samples[story].second.push_back(avg);
    } else {
      fprintf(stderr, "WARNING: Unknown display_label %s\n",
              display_label.c_str());
    }
  }

  // Estimate the ratios for all of our data.
  vector<vector<RatioBootstrapEstimator::Sample>> data;
  for (const auto& [key, story_samples] : samples) {
    // These should always be the same in Pinpoint, but just to be sure.
    unsigned num_samples =
        std::min(story_samples.first.size(), story_samples.second.size());
    vector<RatioBootstrapEstimator::Sample> story_data;
    for (unsigned i = 0; i < num_samples; ++i) {
      story_data.push_back(RatioBootstrapEstimator::Sample{
          story_samples.first[i], story_samples.second[i]});
    }
    data.push_back(std::move(story_data));
  }
  RatioBootstrapEstimator estimator(base::RandUint64());
  constexpr int kNumRuns = 2000;
  vector<RatioBootstrapEstimator::Estimate> estimates =
      estimator.ComputeRatioEstimates(data, kNumRuns, confidence_level,
                                      /*compute_geometric_mean=*/false);

  // Sort by name, then print. (We assume all names are ASCII.)
  unsigned data_index = 0;
  int max_key_len = 0;
  vector<pair<string, RatioBootstrapEstimator::Estimate>> to_print;
  for (const auto& [key, story_samples] : samples) {
    to_print.emplace_back(key, std::move(estimates[data_index]));
    ++data_index;
    max_key_len = std::max<int>(max_key_len, key.length());
  }
  std::ranges::sort(
      to_print, [](const pair<string, RatioBootstrapEstimator::Estimate>& a,
                   const pair<string, RatioBootstrapEstimator::Estimate>& b) {
        return a.first < b.first;
      });
  for (const auto& [key, estimate] : to_print) {
    // Convert from ratios to percent change. For Pinpoint, higher-is-better,
    // so we also need to convert from before/after to after/before.
    double lower = 100.0 * (1.0 / estimate.upper - 1.0);
    double upper = 100.0 * (1.0 / estimate.lower - 1.0);

    // If our confidence interval doesn't touch 100%, we know (at the given
    // confidence level) that there is a real change. It might be a bit
    // confusing when an interval with -0.0% or +0.0% is shown as significant
    // (due to rounding), but this is probably confusing no matter what we do.
    const char* emoji = "  ";
    if (lower > 0.0 && upper > 0.0) {
      if (any_is_speedometer && key != "Score") {
        // For Speedometer, lower is better (except for Score),
        // so adjust the thumbs accordingly. We could flip the values, too,
        // for ease of understanding, but be consistent with Pinpoint.
        emoji = "👎";
      } else {
        emoji = "👍";
      }
    } else if (lower < -0.0 && upper < -0.0) {
      if (any_is_speedometer && key != "Score") {
        emoji = "👍";
      } else {
        emoji = "👎";
      }
    }

    printf("%s %-*s  [%+5.1f%%, %+5.1f%%]\n", emoji, max_key_len, key.c_str(),
           lower, upper);
  }
}
