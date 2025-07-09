// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/location_report_body.h"

#include "third_party/blink/public/common/scheme_registry.h"
#include "third_party/blink/renderer/bindings/core/v8/capture_source_location.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
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
    SourceLocation* location) {
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
  hash = HashInts(hash, line ? blink::GetHash(*line) : 0);
  hash = HashInts(hash, column ? blink::GetHash(*column) : 0);
  return hash;
}

bool LocationReportBody::IsExtensionSource() const {
  // TODO(crbug.com/356098278): Either remove this KURL instantiation completely
  // or store `source_file_` as a KURL and only convert to string when sending
  // reports.
  KURL source_file_url(source_file_);
  if (!source_file_url.IsValid()) {
    return false;
  }
  return CommonSchemeRegistry::IsExtensionScheme(
      source_file_url.Protocol().Utf8());
}

}  // namespace blink
