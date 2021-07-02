// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_AUDITS_ISSUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_AUDITS_ISSUE_H_

#include <memory>
#include "base/unguessable_token.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"

namespace WTF {
class String;
}

namespace blink {

class Element;
class ExecutionContext;

namespace protocol {
namespace Audits {
class InspectorIssue;
}
}  // namespace protocol

enum class RendererCorsIssueCode {
  kDisallowedByMode,
  kCorsDisabledScheme,
  kNoCorsRedirectModeNotFollow,
};

enum class AttributionReportingIssueType {
  kPermissionPolicyDisabled,
  kInvalidAttributionSourceEventId,
  kInvalidAttributionData,
  kAttributionSourceUntrustworthyOrigin,
  kAttributionUntrustworthyOrigin,
};

// |AuditsIssue| is a thin wrapper around the Audits::InspectorIssue
// protocol class.
//
// There are a few motiviations for this class:
//  1) Prevent leakage of auto-generated CDP resources into the
//     rest of blink.
//  2) Control who can assemble Audits::InspectorIssue's as this should
//     happen |inspector| land.
//  3) Prevent re-compilation of various blink classes when the protocol
//     changes. The protocol type can be forward declared in header files,
//     but for the std::unique_ptr, the generated |Audits.h| header
//     would have to be included in various cc files.
class CORE_EXPORT AuditsIssue {
 public:
  AuditsIssue() = delete;
  AuditsIssue(const AuditsIssue&) = delete;
  AuditsIssue& operator=(const AuditsIssue&) = delete;

  AuditsIssue(AuditsIssue&&);
  AuditsIssue& operator=(AuditsIssue&&);

  const protocol::Audits::InspectorIssue* issue() const { return issue_.get(); }
  std::unique_ptr<protocol::Audits::InspectorIssue> TakeIssue();

  ~AuditsIssue();

  static void ReportQuirksModeIssue(ExecutionContext* execution_context,
                                    bool isLimitedQuirksMode,
                                    DOMNodeId document_node_id,
                                    String url,
                                    String frame_id,
                                    String loader_id);

  static void ReportCorsIssue(ExecutionContext* execution_context,
                              int64_t identifier,
                              RendererCorsIssueCode code,
                              WTF::String url,
                              WTF::String initiator_origin,
                              WTF::String failedParameter,
                              absl::optional<base::UnguessableToken> issue_id);
  // Reports an Attribution Reporting API issue to DevTools.
  // |reporting_execution_context| is the current execution context in which the
  // issue happens and is reported in (the "target" in DevTools terms).
  // |offending_frame_token| is the offending frame that triggered the issue.
  // |offending_frame_token| does not necessarly correspond to
  // |reporting_execution_context|, e.g. when an impression click in an iframe
  // is blocked due to an insecure main frame.
  static void ReportAttributionIssue(
      ExecutionContext* reporting_execution_context,
      AttributionReportingIssueType type,
      const absl::optional<base::UnguessableToken>& offending_frame_token =
          absl::nullopt,
      Element* element = nullptr,
      const absl::optional<String>& request_id = absl::nullopt,
      const absl::optional<String>& invalid_parameter = absl::nullopt);

  static void ReportNavigatorUserAgentAccess(
      ExecutionContext* execution_context,
      WTF::String url);

  static void ReportCrossOriginWasmModuleSharingIssue(
      ExecutionContext* execution_context,
      const std::string& wasm_source_url,
      WTF::String source_origin,
      WTF::String target_origin,
      bool is_warning);

 private:
  explicit AuditsIssue(std::unique_ptr<protocol::Audits::InspectorIssue> issue);

  std::unique_ptr<protocol::Audits::InspectorIssue> issue_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_AUDITS_ISSUE_H_
