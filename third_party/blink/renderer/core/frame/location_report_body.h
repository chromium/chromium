// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCATION_REPORT_BODY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCATION_REPORT_BODY_H_

#include <memory>
#include <utility>

#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/frame/report_body.h"

namespace blink {

class CORE_EXPORT LocationReportBody : public ReportBody {
 public:
  explicit LocationReportBody(std::unique_ptr<SourceLocation> location)
      : source_file_(location->Url()),
        line_number_(location->IsUnknown()
                         ? base::nullopt
                         : base::Optional<uint32_t>{location->LineNumber()}),
        column_number_(location->IsUnknown() ? base::nullopt
                                             : base::Optional<uint32_t>{
                                                   location->ColumnNumber()}) {}

  LocationReportBody() : LocationReportBody(SourceLocation::Capture()) {}

  LocationReportBody(const String& source_file,
                     base::Optional<uint32_t> line_number = base::nullopt,
                     base::Optional<uint32_t> column_number = base::nullopt)
      : source_file_(source_file),
        line_number_(line_number),
        column_number_(column_number) {}

  ~LocationReportBody() override = default;

  String sourceFile() const { return source_file_; }

  uint32_t lineNumber(bool& is_null) const {
    is_null = !line_number_.has_value();
    return line_number_.value_or(0);
  }

  uint32_t columnNumber(bool& is_null) const {
    is_null = !column_number_.has_value();
    return column_number_.value_or(0);
  }

  void BuildJSONValue(V8ObjectBuilder& builder) const override;

 protected:
  const String source_file_;
  base::Optional<uint32_t> line_number_;
  base::Optional<uint32_t> column_number_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCATION_REPORT_BODY_H_
