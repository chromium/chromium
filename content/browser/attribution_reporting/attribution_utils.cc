// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_utils.h"

#include "base/check.h"
#include "base/json/json_writer.h"
#include "base/time/time.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

constexpr base::TimeDelta kWindowTinyOffset = base::Milliseconds(1);

}  // namespace

base::Time LastTriggerTimeForReportTime(base::Time report_time) {
  // kWindowTinyOffset is needed as the window is not selected right at
  // report_time.
  return report_time - kWindowTinyOffset;
}

std::string SerializeAttributionJson(base::ValueView body, bool pretty_print) {
  int options = pretty_print ? base::JSONWriter::OPTIONS_PRETTY_PRINT : 0;

  std::string output_json;
  bool success =
      base::JSONWriter::WriteWithOptions(body, options, &output_json);
  DCHECK(success);
  return output_json;
}

base::Time ComputeReportWindowTime(
    absl::optional<base::Time> report_window_time,
    base::Time expiry_time) {
  return report_window_time.has_value() &&
                 report_window_time.value() <= expiry_time
             ? report_window_time.value()
             : expiry_time;
}

}  // namespace content
