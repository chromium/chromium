// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_CONVERSION_REPORT_H_
#define CONTENT_BROWSER_CONVERSIONS_CONVERSION_REPORT_H_

#include <stdint.h>

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
  ConversionReport(StorableImpression impression,
                   uint64_t conversion_data,
                   base::Time conversion_time,
                   base::Time report_time,
                   absl::optional<int64_t> conversion_id);
  ConversionReport(const ConversionReport& other);
  ConversionReport& operator=(const ConversionReport& other);
  ConversionReport(ConversionReport&& other);
  ConversionReport& operator=(ConversionReport&& other);
  ~ConversionReport();

  // Impression associated with this conversion report.
  StorableImpression impression;

  // Data provided at reporting time by the reporting origin. Depending on the
  // source type, this contains the associated data in the trigger redirect.
  uint64_t conversion_data;

  // The time the conversion occurred.
  base::Time conversion_time;

  // The time this conversion report should be sent.
  base::Time report_time;

  // The original report time assigned to this report when it was created,
  // ignoring any ephemeral increases to |report_time| for this conversion
  // report.
  base::Time original_report_time;

  // Id assigned by storage to uniquely identify a completed conversion. If
  // null, an ID has not been assigned yet.
  absl::optional<int64_t> conversion_id;

  // When adding new members, the corresponding `operator==()` definition in
  // `conversion_test_utils.h` should also be updated.
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONVERSIONS_CONVERSION_REPORT_H_
