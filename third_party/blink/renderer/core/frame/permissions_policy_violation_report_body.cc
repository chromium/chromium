// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/permissions_policy_violation_report_body.h"

namespace blink {

void PermissionsPolicyViolationReportBody::BuildJSONValue(
    V8ObjectBuilder& builder) const {
  LocationReportBody::BuildJSONValue(builder);
  builder.AddString("featureId", featureId());
  builder.AddString("disposition", disposition());
  builder.AddStringOrNull("message", message());
}

}  // namespace blink
