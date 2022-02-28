// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_AGGREGATABLE_SOURCES_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_AGGREGATABLE_SOURCES_H_

#include "content/browser/attribution_reporting/attribution_reporting.pb.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

// This is a wrapper of `proto::AttributionAggregatableSources`.
class CONTENT_EXPORT AttributionAggregatableSources {
 public:
  // Returns `absl::nullopt` if `proto` is invalid.
  static absl::optional<AttributionAggregatableSources> Create(
      proto::AttributionAggregatableSources proto);

  AttributionAggregatableSources();
  ~AttributionAggregatableSources();

  AttributionAggregatableSources(const AttributionAggregatableSources&);
  AttributionAggregatableSources(AttributionAggregatableSources&&);

  AttributionAggregatableSources& operator=(
      const AttributionAggregatableSources&);
  AttributionAggregatableSources& operator=(AttributionAggregatableSources&&);

  const proto::AttributionAggregatableSources& proto() const { return proto_; }

 private:
  explicit AttributionAggregatableSources(
      proto::AttributionAggregatableSources proto);

  proto::AttributionAggregatableSources proto_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_AGGREGATABLE_SOURCES_H_
