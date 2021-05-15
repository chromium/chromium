// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_report.h"

#include <tuple>

namespace content {

ConversionReport::ConversionReport(const StorableImpression& impression,
                                   const std::string& conversion_data,
                                   base::Time conversion_time,
                                   base::Time report_time,
                                   const absl::optional<int64_t>& conversion_id)
    : impression(impression),
      conversion_data(conversion_data),
      conversion_time(conversion_time),
      report_time(report_time),
      conversion_id(conversion_id) {}

ConversionReport::ConversionReport(const ConversionReport& other) = default;

ConversionReport::~ConversionReport() = default;

std::ostream& operator<<(std::ostream& out, const ConversionReport& report) {
  out << "impression_data: " << report.impression.impression_data()
      << ", impression_origin: " << report.impression.impression_origin()
      << ", conversion_origin: " << report.impression.conversion_origin()
      << ", reporting_origin: " << report.impression.reporting_origin()
      << ", conversion_data: " << report.conversion_data
      << ", conversion_time: " << report.conversion_time
      << ", report_time: " << report.report_time
      << ", extra_delay: " << report.extra_delay;
  return out;
}

}  // namespace content
