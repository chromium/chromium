// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_STORABLE_SOURCE_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_STORABLE_SOURCE_H_

#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/common/content_export.h"

namespace content {

// Contains attributes specific to a source that hasn't been stored yet.
class CONTENT_EXPORT StorableSource {
 public:
  // Represents the potential outcomes from attempting to register a source.
  enum class Result {
    kSuccess,
    kInternalError,
    kInsufficientSourceCapacity,
    kInsufficientUniqueDestinationCapacity,
    kExcessiveReportingOrigins,
    kProhibitedByBrowserPolicy,
  };

  explicit StorableSource(CommonSourceInfo common_info);

  ~StorableSource();

  StorableSource(const StorableSource&);
  StorableSource(StorableSource&&);

  StorableSource& operator=(const StorableSource&);
  StorableSource& operator=(StorableSource&&);

  const CommonSourceInfo& common_info() const { return common_info_; }

  CommonSourceInfo& common_info() { return common_info_; }

 private:
  CommonSourceInfo common_info_;

  // When adding new members, the corresponding `operator==()` definition in
  // `attribution_test_utils.h` should also be updated.
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_STORABLE_SOURCE_H_
