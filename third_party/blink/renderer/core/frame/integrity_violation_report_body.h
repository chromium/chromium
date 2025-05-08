// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_INTEGRITY_VIOLATION_REPORT_BODY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_INTEGRITY_VIOLATION_REPORT_BODY_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/report_body.h"

namespace blink {

class CORE_EXPORT IntegrityViolationReportBody : public ReportBody {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit IntegrityViolationReportBody(const String& document_url,
                                        const String& blocked_url,
                                        const String& destination,
                                        bool report_only)
      : document_url_(document_url),
        blocked_url_(blocked_url),
        destination_(destination),
        report_only_(report_only) {}

  ~IntegrityViolationReportBody() override = default;

  const String& documentURL() const { return document_url_; }
  const String& blockedURL() const { return blocked_url_; }
  const String& destination() const { return destination_; }
  bool reportOnly() const { return report_only_; }

  void BuildJSONValue(V8ObjectBuilder& builder) const override;

 private:
  const String document_url_;
  const String blocked_url_;
  const String destination_;
  bool report_only_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_INTEGRITY_VIOLATION_REPORT_BODY_H_
