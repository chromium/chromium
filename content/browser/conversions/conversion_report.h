// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_CONVERSION_REPORT_H_
#define CONTENT_BROWSER_CONVERSIONS_CONVERSION_REPORT_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/optional.h"
#include "base/time/time.h"
#include "content/browser/conversions/storable_impression.h"
#include "content/common/content_export.h"

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
                   const base::Optional<int64_t>& conversion_id);
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

  // The attribution credit assigned to this conversion report. This is derived
  // from the set of all impressions that matched a singular conversion event.
  // This should be in the range 0-100. A set of ConversionReports for one
  // conversion event should have their |attribution_credit| sum equal to 100.
  int attribution_credit = 0;

  // Id assigned by storage to uniquely identify a completed conversion. If
  // null, an ID has not been assigned yet.
  const base::Optional<int64_t> conversion_id;
};

// Only used for logging.
CONTENT_EXPORT std::ostream& operator<<(
    std::ostream& out,
    const ConversionReport& ConversionReport);

}  // namespace content

#endif  // CONTENT_BROWSER_CONVERSIONS_CONVERSION_REPORT_H_
