// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_AUDITS_ISSUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_AUDITS_ISSUE_H_

#include <memory>
#include <optional>
#include <string>

#include "services/network/public/mojom/blocked_by_response_reason.mojom-forward.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy_violation_type.h"
#include "third_party/blink/renderer/core/inspector/protocol/audits.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace base {
class UnguessableToken;
}  // namespace base

namespace blink {

class Document;
class DocumentLoader;
class Element;
class ExecutionContext;
class KURL;
class LocalDOMWindow;
class LocalFrame;
class ResourceError;
class SecurityPolicyViolationEventInit;
class SourceLocation;

namespace protocol::Audits {
class InspectorIssue;
}  // namespace protocol::Audits

enum class RendererCorsIssueCode {
  kDisallowedByMode,
  kCorsDisabledScheme,
  kNoCorsRedirectModeNotFollow,
};

enum class SharedArrayBufferIssueType {
  kTransferIssue,
  kCreationIssue,
};

enum class MixedContentResolutionStatus {
  kMixedContentBlocked,
  kMixedContentAutomaticallyUpgraded,
  kMixedContentWarning,
};

enum class ClientHintIssueReason {
  kMetaTagAllowListInvalidOrigin,
  kMetaTagModifiedHTML,
};

enum class ElementAccessibilityIssueReason {
  kDisallowedSelectChild,
  kDisallowedOptGroupChild,
  kNonPhrasingContentOptionChild,
  kInteractiveContentOptionChild,
  kInteractiveContentLegendChild,
  kInteractiveContentSummaryDescendant,
  kValidChild,
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
  explicit AuditsIssue(std::unique_ptr<protocol::Audits::InspectorIssue> issue);

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
                              RendererCorsIssueCode code,
                              String url,
                              String initiator_origin,
                              String failedParameter,
                              std::optional<base::UnguessableToken> issue_id);

  static void ReportAttributionIssue(
      ExecutionContext* execution_context,
      mojom::blink::AttributionReportingIssueType type,
      Element* element,
      const String& request_url,
      const String& request_id,
      const String& invalid_parameter);

  static void ReportSharedArrayBufferIssue(
      ExecutionContext* execution_context,
      bool shared_buffer_transfer_allowed,
      SharedArrayBufferIssueType issue_type);

  // Reports a Deprecation issue to DevTools.
  // `execution_context` is used to extract the affected frame and source.
  // `type` is the enum used to differentiate messages.
  static void ReportDeprecationIssue(ExecutionContext* execution_context,
                                     String type);

  static void ReportClientHintIssue(LocalDOMWindow* local_dom_window,
                                    ClientHintIssueReason reason);

  static AuditsIssue CreateBlockedByResponseIssue(
      network::mojom::BlockedByResponseReason reason,
      uint64_t identifier,
      DocumentLoader* loader,
      const ResourceError& error,
      const base::UnguessableToken& token);

  static void ReportMixedContentIssue(
      const KURL& main_resource_url,
      const KURL& insecure_url,
      const mojom::blink::RequestContextType request_context,
      LocalFrame* frame,
      const MixedContentResolutionStatus resolution_status,
      const String& devtools_id);

  static AuditsIssue CreateContentSecurityPolicyIssue(
      const blink::SecurityPolicyViolationEventInit& violation_data,
      bool is_report_only,
      ContentSecurityPolicyViolationType violation_type,
      LocalFrame* frame_ancestor,
      Element* element,
      SourceLocation* source_location,
      std::optional<base::UnguessableToken> issue_id);

  static protocol::Audits::GenericIssueErrorType
  GenericIssueErrorTypeToProtocol(
      mojom::blink::GenericIssueErrorType error_type);

  static void ReportGenericIssue(LocalFrame* frame,
                                 mojom::blink::GenericIssueErrorType error_type,
                                 int violating_node_id);
  static void ReportGenericIssue(LocalFrame* frame,
                                 mojom::blink::GenericIssueErrorType error_type,
                                 int violating_node_id,
                                 const String& violating_node_attribute);
  static void ReportPartitioningBlobURLIssue(
      LocalDOMWindow* window,
      String blob_url,
      mojom::blink::PartitioningBlobURLInfo info);
  static void ReportStylesheetLoadingLateImportIssue(Document* document,
                                                     const KURL& url,
                                                     OrdinalNumber line,
                                                     OrdinalNumber column);

  static void ReportPropertyRuleIssue(
      Document* document,
      const KURL& url,
      OrdinalNumber line,
      OrdinalNumber column,
      protocol::Audits::PropertyRuleIssueReason reason,
      const String& propertyValue);

  static void ReportStylesheetLoadingRequestFailedIssue(
      Document* document,
      const KURL& url,
      const String& request_id,
      const KURL& initiator_url,
      OrdinalNumber initiator_line,
      OrdinalNumber initiator_column,
      const String& failureMessage);

  static void ReportElementAccessibilityIssue(
      Document* document,
      DOMNodeId node_id,
      ElementAccessibilityIssueReason issue_reason,
      bool has_disallowed_attributes);

  static void ReportUserReidentificationResourceBlockedIssue(
      LocalFrame* frame,
      std::optional<std::string> devtools_request_id,
      const KURL& affected_request_url);

  static void ReportUserReidentificationCanvasNoisedIssue(
      SourceLocation* source_location,
      ExecutionContext* execution_context);

  static void ReportPermissionElementIssue(
      ExecutionContext* execution_context,
      DOMNodeId node_id,
      protocol::Audits::PermissionElementIssueType issue_type,
      const String& type,
      bool is_warning,
      const String& permissionName = String(),
      const String& occluderNodeInfo = String(),
      const String& occluderParentNodeInfo = String(),
      const String& disableReason = String());

 private:

  std::unique_ptr<protocol::Audits::InspectorIssue> issue_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_AUDITS_ISSUE_H_
