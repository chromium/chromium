// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_CONVERSION_REPORT_H_
#define CONTENT_BROWSER_CONVERSIONS_CONVERSION_REPORT_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "content/browser/conversions/storable_impression.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

// Struct that contains all the data needed to serialize and send a conversion
// report. This represents the report for a conversion event and its associated
// impression.
struct CONTENT_EXPORT ConversionReport {
  // The conversion_id may not be set for a conversion report.
  ConversionReport(const StorableImpression& impression,
                   const std::string& conversion_data,
                   base::Time conversion_time,
                   base::Time report_time,
                   const absl::optional<int64_t>& conversion_id);
  ConversionReport(const ConversionReport& other);
  ~ConversionReport();

  // Impression associated with this conversion report.
  const StorableImpression impression;

  // Data provided at reporting time by the reporting origin. String
  // representing a valid hexadecimal number.
  const std::string conversion_data;

  // The time the conversion occurred.
  const base::Time conversion_time;

  // The time this conversion report should be sent.
  base::Time report_time;

  // Tracks ephemeral increases to |report_time| for this conversion report, for
  // the purposes of logging metrics.
  base::TimeDelta extra_delay;

  // Id assigned by storage to uniquely identify a completed conversion. If
  // null, an ID has not been assigned yet.
  const absl::optional<int64_t> conversion_id;
};

// Only used for logging.
CONTENT_EXPORT std::ostream& operator<<(
    std::ostream& out,
    const ConversionReport& ConversionReport);

}  // namespace content

#endif  // CONTENT_BROWSER_CONVERSIONS_CONVERSION_REPORT_H_
