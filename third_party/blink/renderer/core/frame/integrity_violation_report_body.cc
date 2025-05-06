// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/integrity_violation_report_body.h"

namespace blink {

void IntegrityViolationReportBody::BuildJSONValue(
    V8ObjectBuilder& builder) const {
  builder.AddString("documentURL", documentURL());
  builder.AddStringOrNull("blockedURL", blockedURL());
  builder.AddStringOrNull("destination", destination());
  builder.AddBoolean("reportOnly", reportOnly());
}

}  // namespace blink
