// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/intervention_report_body.h"

namespace blink {

void InterventionReportBody::BuildJSONValue(V8ObjectBuilder& builder) const {
  LocationReportBody::BuildJSONValue(builder);
  builder.AddString("id", id());
  builder.AddString("message", message());
}

}  // namespace blink
