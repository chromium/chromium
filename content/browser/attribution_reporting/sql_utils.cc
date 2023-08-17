// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/sql_utils.h"

#include <string>

#include "base/check.h"
#include "base/check_op.h"
#include "base/time/time.h"
#include "components/attribution_reporting/event_report_windows.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "content/browser/attribution_reporting/attribution_reporting.pb.h"
#include "sql/statement.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {
using ::attribution_reporting::mojom::SourceType;
}  // namespace

url::Origin DeserializeOrigin(const std::string& origin) {
  return url::Origin::Create(GURL(origin));
}

absl::optional<SourceType> DeserializeSourceType(int val) {
  switch (val) {
    case static_cast<int>(SourceType::kNavigation):
      return SourceType::kNavigation;
    case static_cast<int>(SourceType::kEvent):
      return SourceType::kEvent;
    default:
      return absl::nullopt;
  }
}

std::string SerializeReadOnlySourceData(
    const attribution_reporting::EventReportWindows& event_report_windows,
    int max_event_level_reports,
    double randomized_response_rate) {
  DCHECK_GE(max_event_level_reports, 0);
  proto::AttributionReadOnlySourceData msg;

  msg.set_max_event_level_reports(max_event_level_reports);
  msg.set_event_level_report_window_start_time(
      event_report_windows.start_time().InMicroseconds());

  for (base::TimeDelta time : event_report_windows.end_times()) {
    msg.add_event_level_report_window_end_times(time.InMicroseconds());
  }

  if (randomized_response_rate >= 0 && randomized_response_rate <= 1) {
    msg.set_randomized_response_rate(randomized_response_rate);
  }

  std::string str;
  bool success = msg.SerializeToString(&str);
  DCHECK(success);
  return str;
}

absl::optional<proto::AttributionReadOnlySourceData>
DeserializeReadOnlySourceDataAsProto(sql::Statement& stmt, int col) {
  std::string str;
  if (!stmt.ColumnBlobAsString(col, &str)) {
    return absl::nullopt;
  }

  proto::AttributionReadOnlySourceData msg;
  if (!msg.ParseFromString(str)) {
    return absl::nullopt;
  }
  return msg;
}

}  // namespace content
