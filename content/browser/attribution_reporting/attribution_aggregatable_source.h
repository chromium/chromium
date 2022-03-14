// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_AGGREGATABLE_SOURCE_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_AGGREGATABLE_SOURCE_H_

#include "content/browser/attribution_reporting/attribution_reporting.pb.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

// This is a wrapper of `proto::AttributionAggregatableSource`.
class CONTENT_EXPORT AttributionAggregatableSource {
 public:
  // Returns `absl::nullopt` if `proto` is invalid.
  static absl::optional<AttributionAggregatableSource> Create(
      proto::AttributionAggregatableSource proto);

  AttributionAggregatableSource();
  ~AttributionAggregatableSource();

  AttributionAggregatableSource(const AttributionAggregatableSource&);
  AttributionAggregatableSource(AttributionAggregatableSource&&);

  AttributionAggregatableSource& operator=(
      const AttributionAggregatableSource&);
  AttributionAggregatableSource& operator=(AttributionAggregatableSource&&);

  const proto::AttributionAggregatableSource& proto() const { return proto_; }

 private:
  explicit AttributionAggregatableSource(
      proto::AttributionAggregatableSource proto);

  proto::AttributionAggregatableSource proto_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_AGGREGATABLE_SOURCE_H_
