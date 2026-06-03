// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CONNECTION_ALLOWLIST_VIOLATION_REPORT_BODY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CONNECTION_ALLOWLIST_VIOLATION_REPORT_BODY_H_

#include "services/network/public/cpp/connection_allowlist.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_connection_allowlist_disposition.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/location_report_body.h"
#include "third_party/blink/renderer/core/frame/report_body.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// Represents the body of a Connection Allowlist violation report. Also
// encapsulates the logic to queue reports for specific violation cases.
// To queue a report for a new violation case, add a new public static method of
// the format Queue<violation_type>Report.
class CORE_EXPORT ConnectionAllowlistViolationReportBody
    : public LocationReportBody {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static void QueueWebRTCReport(
      V8ConnectionAllowlistDisposition::Enum disposition,
      const ExecutionContext& execution_context);

  ~ConnectionAllowlistViolationReportBody() override = default;

  const String& url() const { return url_; }
  const String& connection() const { return connection_; }
  const Vector<String>& allowlist() const { return allowlist_; }
  const V8ConnectionAllowlistDisposition& disposition() const {
    return disposition_;
  }

  void BuildJSONValue(V8ObjectBuilder& builder) const override;

  unsigned MatchId() const override;

  // Builds the Connection Allowlist report body provided to QueueReport.
  // The entry point for constructing reports should be a static Queue*Report
  // method defined above, but this needs to be public for MakeGarbageCollected.
  // - `url` is the URL of the context which generated the report.
  // - `connection` is a string representation of the destination of the
  //   offending connection. For URL-based requests, this should be full URL
  //   spec. For requests that are cancelled before the destination is known,
  //   this should be a string constant descriptive of the violation (like
  //   "webrtc").
  // - `allowlist` is the collection of allowlisted URL patterns, represented
  //    as strings.
  // - `disposition` is either kEnforce or kReport, depending on if the
  //    violation should result in the request being blocked.
  ConnectionAllowlistViolationReportBody(
      const String& url,
      const String& connection,
      const Vector<String>& allowlist,
      const V8ConnectionAllowlistDisposition& disposition);

 private:
  const String url_;
  const String connection_;
  const Vector<String> allowlist_;
  const V8ConnectionAllowlistDisposition disposition_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CONNECTION_ALLOWLIST_VIOLATION_REPORT_BODY_H_
