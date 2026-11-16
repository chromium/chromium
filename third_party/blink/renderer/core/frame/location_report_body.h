// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCATION_REPORT_BODY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCATION_REPORT_BODY_H_

#include <memory>
#include <utility>

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/report_body.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"

namespace blink {

class ExecutionContext;

class CORE_EXPORT LocationReportBody : public ReportBody {
 private:
  struct ReportLocation {
    String file;
    std::optional<uint32_t> line_number;
    std::optional<uint32_t> column_number;
  };

  static ReportLocation CreateReportLocation(
      const String& file,
      std::optional<uint32_t> line_number,
      std::optional<uint32_t> column_number);

  static ReportLocation CreateReportLocation(SourceLocation* location);

  static ReportLocation CreateReportLocation(
      const ExecutionContext* execution_context);

  explicit LocationReportBody(const ReportLocation& location)
      : source_file_(location.file),
        line_number_(location.line_number),
        column_number_(location.column_number) {}

 protected:
  explicit LocationReportBody(SourceLocation* location)
      : LocationReportBody(CreateReportLocation(location)) {}

  // Captures the source location from `execution_context`.
  explicit LocationReportBody(const ExecutionContext* execution_context)
      : LocationReportBody(CreateReportLocation(execution_context)) {}

  LocationReportBody(const String& source_file,
                     std::optional<uint32_t> line_number,
                     std::optional<uint32_t> column_number)
      : LocationReportBody(
            CreateReportLocation(source_file, line_number, column_number)) {}

 public:
  ~LocationReportBody() override = default;

  const String& sourceFile() const { return source_file_; }

  std::optional<uint32_t> lineNumber() const { return line_number_; }
  std::optional<uint32_t> columnNumber() const { return column_number_; }

  void BuildJSONValue(V8ObjectBuilder& builder) const override;

  unsigned MatchId() const override;

  bool IsExtensionSource() const override;

 protected:
  const String source_file_;
  const std::optional<uint32_t> line_number_;
  const std::optional<uint32_t> column_number_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCATION_REPORT_BODY_H_
