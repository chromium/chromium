// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/test_report_body.h"

namespace blink {

void TestReportBody::BuildJSONValue(V8ObjectBuilder& builder) const {
  builder.AddString("message", message());
}

}  // namespace blink
