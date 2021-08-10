// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_report.h"

namespace content {

ConversionReport::ConversionReport(StorableImpression impression,
                                   uint64_t conversion_data,
                                   base::Time conversion_time,
                                   base::Time report_time,
                                   absl::optional<int64_t> conversion_id)
    : impression(std::move(impression)),
      conversion_data(conversion_data),
      conversion_time(conversion_time),
      report_time(report_time),
      original_report_time(report_time),
      conversion_id(conversion_id) {}

ConversionReport::ConversionReport(const ConversionReport& other) = default;

ConversionReport& ConversionReport::operator=(const ConversionReport& other) =
    default;

ConversionReport::ConversionReport(ConversionReport&& other) = default;

ConversionReport& ConversionReport::operator=(ConversionReport&& other) =
    default;

ConversionReport::~ConversionReport() = default;

}  // namespace content
