// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/perf/perf_test.h"

#include <stdio.h>

#include <string_view>

#include "base/strings/string_number_conversions.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

namespace {

std::string ResultsToString(std::string_view measurement,
                            std::string_view modifier,
                            std::string_view trace,
                            std::string_view values,
                            std::string_view prefix,
                            std::string_view suffix,
                            std::string_view units,
                            bool important) {
  // <*>RESULT <graph_name>: <trace_name>= <value> <units>
  // <*>RESULT <graph_name>: <trace_name>= {<mean>, <std deviation>} <units>
  // <*>RESULT <graph_name>: <trace_name>= [<value>,value,value,...,] <units>
  return absl::StrFormat("%sRESULT %s%s: %s= %s%s%s %s\n", important ? "*" : "",
                         measurement, modifier, trace, prefix, values, suffix,
                         units);
}

void PrintResultsImpl(std::string_view measurement,
                      std::string_view modifier,
                      std::string_view trace,
                      std::string_view values,
                      std::string_view prefix,
                      std::string_view suffix,
                      std::string_view units,
                      bool important) {
  fflush(stdout);
  absl::PrintF("%s", ResultsToString(measurement, modifier, trace, values,
                                     prefix, suffix, units, important));
  fflush(stdout);
}

}  // namespace

namespace perf_test {

void PrintResult(std::string_view measurement,
                 std::string_view modifier,
                 std::string_view trace,
                 size_t value,
                 std::string_view units,
                 bool important) {
  PrintResultsImpl(measurement, modifier, trace, base::NumberToString(value),
                   "", "", units, important);
}

void PrintResult(std::string_view measurement,
                 std::string_view modifier,
                 std::string_view trace,
                 double value,
                 std::string_view units,
                 bool important) {
  PrintResultsImpl(measurement, modifier, trace, base::NumberToString(value),
                   "", "", units, important);
}

void AppendResult(std::string& output,
                  std::string_view measurement,
                  std::string_view modifier,
                  std::string_view trace,
                  size_t value,
                  std::string_view units,
                  bool important) {
  output +=
      ResultsToString(measurement, modifier, trace, base::NumberToString(value),
                      "", "", units, important);
}

void PrintResult(std::string_view measurement,
                 std::string_view modifier,
                 std::string_view trace,
                 std::string_view value,
                 std::string_view units,
                 bool important) {
  PrintResultsImpl(measurement, modifier, trace, value, "", "", units,
                   important);
}

void AppendResult(std::string& output,
                  std::string_view measurement,
                  std::string_view modifier,
                  std::string_view trace,
                  std::string_view value,
                  std::string_view units,
                  bool important) {
  output += ResultsToString(measurement, modifier, trace, value, "", "", units,
                            important);
}

void PrintResultMeanAndError(std::string_view measurement,
                             std::string_view modifier,
                             std::string_view trace,
                             std::string_view mean_and_error,
                             std::string_view units,
                             bool important) {
  PrintResultsImpl(measurement, modifier, trace, mean_and_error, "{", "}",
                   units, important);
}

void AppendResultMeanAndError(std::string& output,
                              std::string_view measurement,
                              std::string_view modifier,
                              std::string_view trace,
                              std::string_view mean_and_error,
                              std::string_view units,
                              bool important) {
  output += ResultsToString(measurement, modifier, trace, mean_and_error, "{",
                            "}", units, important);
}

void PrintResultList(std::string_view measurement,
                     std::string_view modifier,
                     std::string_view trace,
                     std::string_view values,
                     std::string_view units,
                     bool important) {
  PrintResultsImpl(measurement, modifier, trace, values, "[", "]", units,
                   important);
}

void AppendResultList(std::string& output,
                      std::string_view measurement,
                      std::string_view modifier,
                      std::string_view trace,
                      std::string_view values,
                      std::string_view units,
                      bool important) {
  output += ResultsToString(measurement, modifier, trace, values, "[", "]",
                            units, important);
}

void PrintSystemCommitCharge(std::string_view test_name,
                             size_t charge,
                             bool important) {
  PrintSystemCommitCharge(stdout, test_name, charge, important);
}

void PrintSystemCommitCharge(FILE* target,
                             std::string_view test_name,
                             size_t charge,
                             bool important) {
  absl::FPrintF(target, "%s",
                SystemCommitChargeToString(test_name, charge, important));
}

std::string SystemCommitChargeToString(std::string_view test_name,
                                       size_t charge,
                                       bool important) {
  std::string output;
  AppendResult(output, "commit_charge", "", "cc" + std::string(test_name),
               charge, "kb", important);
  return output;
}

}  // namespace perf_test
