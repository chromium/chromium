// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_SQL_UTILS_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_SQL_UTILS_H_

#include <string>

#include "components/attribution_reporting/source_type.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {
class EventReportWindows;
}  // namespace attribution_reporting

namespace url {
class Origin;
}  // namespace url

namespace content {

url::Origin DeserializeOrigin(const std::string& origin);

absl::optional<attribution_reporting::mojom::SourceType> DeserializeSourceType(
    int val);

std::string SerializeReadOnlySourceData(
    const attribution_reporting::EventReportWindows&,
    int max_event_level_reports);

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_SQL_UTILS_H_
