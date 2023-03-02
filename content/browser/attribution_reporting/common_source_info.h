// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_COMMON_SOURCE_INFO_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_COMMON_SOURCE_INFO_H_

#include "base/time/time.h"
#include "components/attribution_reporting/source_type.mojom-forward.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {
class SchemefulSite;
}  // namespace net

namespace content {

constexpr base::TimeDelta kDefaultAttributionSourceExpiry = base::Days(30);

// Contains common attributes of `StorableSource` and `StoredSource`.
class CONTENT_EXPORT CommonSourceInfo {
 public:
  static base::Time GetExpiryTime(
      absl::optional<base::TimeDelta> declared_expiry,
      base::Time source_time,
      attribution_reporting::mojom::SourceType);

  static absl::optional<base::Time> GetReportWindowTime(
      absl::optional<base::TimeDelta> declared_window,
      base::Time source_time);

  CommonSourceInfo(attribution_reporting::SuitableOrigin source_origin,
                   attribution_reporting::SuitableOrigin reporting_origin,
                   base::Time source_time,
                   base::Time expiry_time,
                   absl::optional<base::Time> event_report_window_time,
                   absl::optional<base::Time> aggregatable_report_window_time,
                   attribution_reporting::mojom::SourceType);

  ~CommonSourceInfo();

  CommonSourceInfo(const CommonSourceInfo&);
  CommonSourceInfo(CommonSourceInfo&&);

  CommonSourceInfo& operator=(const CommonSourceInfo&);
  CommonSourceInfo& operator=(CommonSourceInfo&&);

  const attribution_reporting::SuitableOrigin& source_origin() const {
    return source_origin_;
  }

  const attribution_reporting::SuitableOrigin& reporting_origin() const {
    return reporting_origin_;
  }

  base::Time source_time() const { return source_time_; }

  base::Time expiry_time() const { return expiry_time_; }

  base::Time event_report_window_time() const {
    return event_report_window_time_;
  }

  base::Time aggregatable_report_window_time() const {
    return aggregatable_report_window_time_;
  }

  attribution_reporting::mojom::SourceType source_type() const {
    return source_type_;
  }

  // Returns the schemeful site of |source_origin|.
  //
  // TODO(johnidel): Consider storing the SchemefulSite as a separate member so
  // that we avoid unnecessary copies of |source_origin_|.
  net::SchemefulSite SourceSite() const;

 private:
  attribution_reporting::SuitableOrigin source_origin_;
  attribution_reporting::SuitableOrigin reporting_origin_;
  base::Time source_time_;
  base::Time expiry_time_;
  base::Time event_report_window_time_;
  base::Time aggregatable_report_window_time_;
  attribution_reporting::mojom::SourceType source_type_;

  // When adding new members, the corresponding `operator==()` definition in
  // `attribution_test_utils.h` should also be updated.
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_COMMON_SOURCE_INFO_H_
