// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/location_report_body.h"

namespace blink {

void LocationReportBody::BuildJSONValue(V8ObjectBuilder& builder) const {
  builder.AddStringOrNull("sourceFile", sourceFile());
  bool is_null = false;
  uint32_t line_number = lineNumber(is_null);
  if (is_null) {
    builder.AddNull("lineNumber");
  } else {
    builder.AddNumber("lineNumber", line_number);
  }
  is_null = true;
  uint32_t column_number = columnNumber(is_null);
  if (is_null) {
    builder.AddNull("columnNumber");
  } else {
    builder.AddNumber("columnNumber", column_number);
  }
}

}  // namespace blink
