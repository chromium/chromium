// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/csp_violation_report_body.h"

namespace blink {

void CSPViolationReportBody::BuildJSONValue(V8ObjectBuilder& builder) const {
  LocationReportBody::BuildJSONValue(builder);
  builder.AddString("documentURL", documentURL());
  builder.AddStringOrNull("referrer", referrer());
  builder.AddStringOrNull("blockedURL", blockedURL());
  builder.AddString("effectiveDirective", effectiveDirective());
  builder.AddString("originalPolicy", originalPolicy());
  builder.AddStringOrNull("sample", sample());
  builder.AddString("disposition", disposition());
  builder.AddNumber("statusCode", statusCode());
}

}  // namespace blink
