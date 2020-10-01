// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/coop_access_violation_report_body.h"
#include "services/network/public/cpp/cross_origin_opener_policy.h"

namespace blink {

CoopAccessViolationReportBody::CoopAccessViolationReportBody(
    std::unique_ptr<SourceLocation> source_location,
    network::mojom::blink::CoopAccessReportType type,
    const String& property)
    : LocationReportBody(std::move(source_location)),
      type_(type),
      property_(property) {}

String CoopAccessViolationReportBody::type() const {
  return network::CoopAccessReportTypeToString(type_);
}

void CoopAccessViolationReportBody::BuildJSONValue(
    V8ObjectBuilder& builder) const {
  LocationReportBody::BuildJSONValue(builder);
  builder.AddString("type", type());
  builder.AddString("property", property());
}

}  // namespace blink
