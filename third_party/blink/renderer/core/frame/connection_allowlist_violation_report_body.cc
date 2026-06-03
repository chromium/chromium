// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/connection_allowlist_violation_report_body.h"

#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/core/frame/policy_container.h"
#include "third_party/blink/renderer/core/frame/report.h"
#include "third_party/blink/renderer/core/frame/reporting_context.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"

namespace blink {

// static
void ConnectionAllowlistViolationReportBody::QueueWebRTCReport(
    V8ConnectionAllowlistDisposition::Enum disposition,
    const ExecutionContext& execution_context) {
  const PolicyContainer* policy_container =
      execution_context.GetPolicyContainer();
  if (!policy_container) {
    return;
  }

  const std::optional<network::ConnectionAllowlist> allowlist =
      disposition == V8ConnectionAllowlistDisposition::Enum::kEnforce
          ? policy_container->GetPolicies().connection_allowlists.enforced
          : policy_container->GetPolicies().connection_allowlists.report_only;
  CHECK(allowlist.has_value());

  Vector<String> blink_allowlist;
  for (const std::string& pattern : allowlist->allowlist) {
    blink_allowlist.push_back(String::FromUtf8(pattern));
  }

  auto disposition_obj =
      disposition == V8ConnectionAllowlistDisposition::Enum::kEnforce
          ? V8ConnectionAllowlistDisposition(
                V8ConnectionAllowlistDisposition::Enum::kEnforce)
          : V8ConnectionAllowlistDisposition(
                V8ConnectionAllowlistDisposition::Enum::kReport);

  ConnectionAllowlistViolationReportBody* body =
      MakeGarbageCollected<ConnectionAllowlistViolationReportBody>(
          execution_context.Url().GetString(), "webrtc", blink_allowlist,
          disposition_obj);

  Report* report =
      MakeGarbageCollected<Report>(ReportType::kConnectionAllowlistViolation,
                                   execution_context.Url().GetString(), body);
  const std::optional<std::string> endpoint = allowlist->reporting_endpoint;

  if (endpoint.has_value()) {
    ReportingContext::From(&execution_context)
        ->QueueReport(report, Vector<String>{endpoint->c_str()});
  }
}

void ConnectionAllowlistViolationReportBody::BuildJSONValue(
    V8ObjectBuilder& builder) const {
  builder.AddString("url", url());
  builder.AddString("connection", connection());
  builder.AddVector<IDLString>("allowlist", allowlist());
  builder.AddString("disposition", disposition().AsStringView());
}

unsigned ConnectionAllowlistViolationReportBody::MatchId() const {
  unsigned hash = url().Impl()->GetHash();
  hash = HashInts(hash, connection().Impl()->GetHash());
  for (const String& spec : allowlist()) {
    hash = HashInts(hash, spec.Impl()->GetHash());
  }
  hash = HashInts(hash, disposition().AsString().Impl()->GetHash());
  return hash;
}

ConnectionAllowlistViolationReportBody::ConnectionAllowlistViolationReportBody(
    const String& url,
    const String& connection,
    const Vector<String>& allowlist,
    const V8ConnectionAllowlistDisposition& disposition)
    : LocationReportBody(),
      url_(url),
      connection_(connection),
      allowlist_(allowlist),
      disposition_(disposition) {}

}  // namespace blink
