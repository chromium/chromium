// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INFO_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INFO_H_

#include <stdint.h>

#include "base/time/time.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

// Contains information available at the time a trigger of any type is
// associated with a `StoredSource`.
struct CONTENT_EXPORT AttributionInfo {
  AttributionInfo(StoredSource source,
                  base::Time time,
                  absl::optional<uint64_t> debug_key);
  ~AttributionInfo();

  AttributionInfo(const AttributionInfo&);
  AttributionInfo(AttributionInfo&&);

  AttributionInfo& operator=(const AttributionInfo&);
  AttributionInfo& operator=(AttributionInfo&&);

  // Source associated with this attribution.
  StoredSource source;

  // The time the trigger occurred.
  base::Time time;

  absl::optional<uint64_t> debug_key;

  // When adding new members, the corresponding `operator==()` definition in
  // `attribution_test_utils.h` should also be updated.
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INFO_H_
