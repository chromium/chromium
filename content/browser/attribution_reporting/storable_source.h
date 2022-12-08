// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_STORABLE_SOURCE_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_STORABLE_SOURCE_H_

#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/common/content_export.h"

namespace attribution_reporting {
class SuitableOrigin;

struct SourceRegistration;
}  // namespace attribution_reporting

namespace base {
class Time;
}  // namespace base

namespace content {

// Contains attributes specific to a source that hasn't been stored yet.
class CONTENT_EXPORT StorableSource {
 public:
  // Represents the potential outcomes from attempting to register a source.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Result {
    kSuccess = 0,
    kInternalError = 1,
    kInsufficientSourceCapacity = 2,
    kInsufficientUniqueDestinationCapacity = 3,
    kExcessiveReportingOrigins = 4,
    kProhibitedByBrowserPolicy = 5,
    kSuccessNoised = 6,
    kMaxValue = kSuccessNoised,
  };

  // TODO(apaseltiner): Make this constructor test-only.
  StorableSource(CommonSourceInfo common_info,
                 bool is_within_fenced_frame,
                 bool debug_reporting);

  StorableSource(attribution_reporting::SuitableOrigin reporting_origin,
                 attribution_reporting::SourceRegistration,
                 base::Time source_time,
                 attribution_reporting::SuitableOrigin source_origin,
                 AttributionSourceType,
                 bool is_within_fenced_frame);

  ~StorableSource();

  StorableSource(const StorableSource&);
  StorableSource(StorableSource&&);

  StorableSource& operator=(const StorableSource&);
  StorableSource& operator=(StorableSource&&);

  const CommonSourceInfo& common_info() const { return common_info_; }

  CommonSourceInfo& common_info() { return common_info_; }

  bool is_within_fenced_frame() const { return is_within_fenced_frame_; }

  bool debug_reporting() const { return debug_reporting_; }

 private:
  CommonSourceInfo common_info_;

  // Whether the source is registered within a fenced frame tree.
  bool is_within_fenced_frame_;

  // Whether debug reporting is enabled.
  bool debug_reporting_;

  // When adding new members, the corresponding `operator==()` definition in
  // `attribution_test_utils.h` should also be updated.
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_STORABLE_SOURCE_H_
