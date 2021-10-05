// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_STORABLE_SOURCE_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_STORABLE_SOURCE_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace net {
class SchemefulSite;
}  // namespace net

namespace content {

// Struct which represents all stored attributes of an impression. All values
// should be sanitized before creating this object.
class CONTENT_EXPORT StorableSource {
 public:
  using Id = base::StrongAlias<StorableSource, int64_t>;

  // Denotes the type of source for this impression. This allows different types
  // of impressions to be processed differently by storage and attribution
  // logic.
  enum class SourceType {
    // An impression which was associated with a top-level navigation.
    kNavigation = 0,
    // An impression which was not associated with a navigation, such as an
    // impression for an anchor element with the registerattributionsource
    // attribute set.
    kEvent = 1,
    kMaxValue = kEvent,
  };

  // Denotes the attribution logic for an impression.
  enum class AttributionLogic {
    // Never send a report for this impression even if it gets attributed.
    kNever = 0,
    // Attribute the impression truthfully.
    kTruthfully = 1,
    // The browser generates a fake conversion for the source, causing a report
    // to always be sent for it.
    kFalsely = 2,
    kMaxValue = kFalsely,
  };

  StorableSource(uint64_t impression_data,
                 url::Origin impression_origin,
                 url::Origin conversion_origin,
                 url::Origin reporting_origin,
                 base::Time impression_time,
                 base::Time expiry_time,
                 SourceType source_type,
                 int64_t priority,
                 AttributionLogic attribution_logic,
                 absl::optional<Id> impression_id);
  StorableSource(const StorableSource& other);
  StorableSource& operator=(const StorableSource& other);
  StorableSource(StorableSource&& other);
  StorableSource& operator=(StorableSource&& other);
  ~StorableSource();

  uint64_t impression_data() const WARN_UNUSED_RESULT {
    return impression_data_;
  }

  const url::Origin& impression_origin() const WARN_UNUSED_RESULT {
    return impression_origin_;
  }

  const url::Origin& conversion_origin() const WARN_UNUSED_RESULT {
    return conversion_origin_;
  }

  const url::Origin& reporting_origin() const WARN_UNUSED_RESULT {
    return reporting_origin_;
  }

  base::Time impression_time() const WARN_UNUSED_RESULT {
    return impression_time_;
  }

  base::Time expiry_time() const WARN_UNUSED_RESULT { return expiry_time_; }

  absl::optional<Id> impression_id() const WARN_UNUSED_RESULT {
    return impression_id_;
  }

  SourceType source_type() const WARN_UNUSED_RESULT { return source_type_; }

  int64_t priority() const WARN_UNUSED_RESULT { return priority_; }

  AttributionLogic attribution_logic() const WARN_UNUSED_RESULT {
    return attribution_logic_;
  }

  const std::vector<int64_t>& dedup_keys() const WARN_UNUSED_RESULT {
    return dedup_keys_;
  }

  void SetDedupKeys(std::vector<int64_t> dedup_keys) {
    dedup_keys_ = std::move(dedup_keys);
  }

  // Returns the schemeful site of |conversion_origin|.
  //
  // TODO(johnidel): Consider storing the SchemefulSite as a separate member so
  // that we avoid unnecessary copies of |conversion_origin_|.
  net::SchemefulSite ConversionDestination() const WARN_UNUSED_RESULT;

  // Returns the schemeful site of |impression_origin|.
  //
  // TODO(johnidel): Consider storing the SchemefulSite as a separate member so
  // that we avoid unnecessary copies of |impression_origin_|.
  net::SchemefulSite ImpressionSite() const WARN_UNUSED_RESULT;

 private:
  uint64_t impression_data_;
  url::Origin impression_origin_;
  url::Origin conversion_origin_;
  url::Origin reporting_origin_;
  base::Time impression_time_;
  base::Time expiry_time_;
  SourceType source_type_;
  int64_t priority_;
  AttributionLogic attribution_logic_;

  // If null, an ID has not been assigned yet.
  absl::optional<Id> impression_id_;

  // Dedup keys associated with the impression. Only set in values returned from
  // `AttributionStorage::GetActiveImpressions()`.
  std::vector<int64_t> dedup_keys_;

  // When adding new members, the corresponding `operator==()` definition in
  // `conversion_test_utils.h` should also be updated.
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_STORABLE_SOURCE_H_
