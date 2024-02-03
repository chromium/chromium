// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/location_report_body.h"
#include "third_party/blink/renderer/bindings/core/v8/capture_source_location.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"

namespace blink {

// static
LocationReportBody::ReportLocation LocationReportBody::CreateReportLocation(
    const String& file,
    std::optional<uint32_t> line_number,
    std::optional<uint32_t> column_number) {
  return file.empty() ? CreateReportLocation(CaptureSourceLocation())
                      : ReportLocation{file, line_number, column_number};
}

// static
LocationReportBody::ReportLocation LocationReportBody::CreateReportLocation(
    std::unique_ptr<SourceLocation> location) {
  return location->IsUnknown()
             ? ReportLocation{}
             : ReportLocation{location->Url(), location->LineNumber(),
                              location->ColumnNumber()};
}

void LocationReportBody::BuildJSONValue(V8ObjectBuilder& builder) const {
  builder.AddStringOrNull("sourceFile", sourceFile());
  if (lineNumber()) {
    builder.AddNumber("lineNumber", lineNumber().value());
  } else {
    builder.AddNull("lineNumber");
  }
  if (columnNumber()) {
    builder.AddNumber("columnNumber", columnNumber().value());
  } else {
    builder.AddNull("columnNumber");
  }
}

unsigned LocationReportBody::MatchId() const {
  const std::optional<uint32_t> line = lineNumber(), column = columnNumber();

  unsigned hash = sourceFile().IsNull() ? 0 : sourceFile().Impl()->GetHash();
  hash = WTF::HashInts(hash, line ? WTF::GetHash(*line) : 0);
  hash = WTF::HashInts(hash, column ? WTF::GetHash(*column) : 0);
  return hash;
}

}  // namespace blink
