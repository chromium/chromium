// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_issue_conversion.h"

#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/inspector/inspector_issue.h"
#include "third_party/blink/renderer/core/inspector/protocol/audits.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

std::unique_ptr<protocol::Audits::AffectedCookie> BuildAffectedCookie(
    const mojom::blink::AffectedCookiePtr& cookie) {
  auto protocol_cookie = std::move(protocol::Audits::AffectedCookie::create()
                                       .setName(cookie->name)
                                       .setPath(cookie->path)
                                       .setDomain(cookie->domain));
  return protocol_cookie.build();
}

std::unique_ptr<protocol::Audits::AffectedRequest> BuildAffectedRequest(
    const mojom::blink::AffectedRequestPtr& request) {
  auto protocol_request = protocol::Audits::AffectedRequest::create()
                              .setRequestId(request->request_id)
                              .build();
  if (!request->url.empty()) {
    protocol_request->setUrl(request->url);
  }
  return protocol_request;
}

std::unique_ptr<protocol::Audits::AffectedFrame> BuildAffectedFrame(
    const mojom::blink::AffectedFramePtr& frame) {
  return protocol::Audits::AffectedFrame::create()
      .setFrameId(frame->frame_id)
      .build();
}

blink::protocol::String InspectorIssueCodeValue(
    mojom::blink::InspectorIssueCode code) {
  switch (code) {
    case mojom::blink::InspectorIssueCode::kCookieIssue:
      return protocol::Audits::InspectorIssueCodeEnum::CookieIssue;
    case mojom::blink::InspectorIssueCode::kMixedContentIssue:
      return protocol::Audits::InspectorIssueCodeEnum::MixedContentIssue;
    case mojom::blink::InspectorIssueCode::kBlockedByResponseIssue:
      return protocol::Audits::InspectorIssueCodeEnum::BlockedByResponseIssue;
    case mojom::blink::InspectorIssueCode::kContentSecurityPolicyIssue:
      return protocol::Audits::InspectorIssueCodeEnum::
          ContentSecurityPolicyIssue;
    case mojom::blink::InspectorIssueCode::kSharedArrayBufferIssue:
      return protocol::Audits::InspectorIssueCodeEnum::SharedArrayBufferIssue;
    case mojom::blink::InspectorIssueCode::kLowTextContrastIssue:
      return protocol::Audits::InspectorIssueCodeEnum::LowTextContrastIssue;
    case mojom::blink::InspectorIssueCode::kHeavyAdIssue:
    case mojom::blink::InspectorIssueCode::kFederatedAuthRequestIssue:
    case mojom::blink::InspectorIssueCode::kFederatedAuthUserInfoRequestIssue:
    case mojom::blink::InspectorIssueCode::kBounceTrackingIssue:
    case mojom::blink::InspectorIssueCode::kCookieDeprecationMetadataIssue:
    case mojom::blink::InspectorIssueCode::kGenericIssue:
    case mojom::blink::InspectorIssueCode::kDeprecationIssue:
    case mojom::blink::InspectorIssueCode::kAttributionReportingIssue:
      NOTREACHED();
  }
}

protocol::String BuildCookieExclusionReason(
    mojom::blink::CookieExclusionReason exclusion_reason) {
  switch (exclusion_reason) {
    case blink::mojom::blink::CookieExclusionReason::
        kExcludeSameSiteUnspecifiedTreatedAsLax:
      return protocol::Audits::CookieExclusionReasonEnum::
          ExcludeSameSiteUnspecifiedTreatedAsLax;
    case blink::mojom::blink::CookieExclusionReason::
        kExcludeSameSiteNoneInsecure:
      return protocol::Audits::CookieExclusionReasonEnum::
          ExcludeSameSiteNoneInsecure;
    case blink::mojom::blink::CookieExclusionReason::kExcludeSameSiteLax:
      return protocol::Audits::CookieExclusionReasonEnum::ExcludeSameSiteLax;
    case blink::mojom::blink::CookieExclusionReason::kExcludeSameSiteStrict:
      return protocol::Audits::CookieExclusionReasonEnum::ExcludeSameSiteStrict;
    case blink::mojom::blink::CookieExclusionReason::kExcludeDomainNonASCII:
      return protocol::Audits::CookieExclusionReasonEnum::ExcludeDomainNonASCII;
    case blink::mojom::blink::CookieExclusionReason::kExcludeThirdPartyPhaseout:
      return protocol::Audits::CookieExclusionReasonEnum::
          ExcludeThirdPartyPhaseout;
  }
}

std::unique_ptr<std::vector<blink::protocol::String>>
BuildCookieExclusionReasons(
    const WTF::Vector<mojom::blink::CookieExclusionReason>& exclusion_reasons) {
  auto protocol_exclusion_reasons =
      std::make_unique<std::vector<blink::protocol::String>>();
  for (const auto& reason : exclusion_reasons) {
    protocol_exclusion_reasons->push_back(BuildCookieExclusionReason(reason));
  }
  return protocol_exclusion_reasons;
}

protocol::String BuildCookieWarningReason(
    mojom::blink::CookieWarningReason warning_reason) {
  switch (warning_reason) {
    case blink::mojom::blink::CookieWarningReason::
        kWarnSameSiteUnspecifiedCrossSiteContext:
      return protocol::Audits::CookieWarningReasonEnum::
          WarnSameSiteUnspecifiedCrossSiteContext;
    case blink::mojom::blink::CookieWarningReason::kWarnSameSiteNoneInsecure:
      return protocol::Audits::CookieWarningReasonEnum::
          WarnSameSiteNoneInsecure;
    case blink::mojom::blink::CookieWarningReason::
        kWarnSameSiteUnspecifiedLaxAllowUnsafe:
      return protocol::Audits::CookieWarningReasonEnum::
          WarnSameSiteUnspecifiedLaxAllowUnsafe;
    case blink::mojom::blink::CookieWarningReason::
        kWarnSameSiteStrictLaxDowngradeStrict:
      return protocol::Audits::CookieWarningReasonEnum::
          WarnSameSiteStrictLaxDowngradeStrict;
    case blink::mojom::blink::CookieWarningReason::
        kWarnSameSiteStrictCrossDowngradeStrict:
      return protocol::Audits::CookieWarningReasonEnum::
          WarnSameSiteStrictCrossDowngradeStrict;
    case blink::mojom::blink::CookieWarningReason::
        kWarnSameSiteStrictCrossDowngradeLax:
      return protocol::Audits::CookieWarningReasonEnum::
          WarnSameSiteStrictCrossDowngradeLax;
    case blink::mojom::blink::CookieWarningReason::
        kWarnSameSiteLaxCrossDowngradeStrict:
      return protocol::Audits::CookieWarningReasonEnum::
          WarnSameSiteLaxCrossDowngradeStrict;
    case blink::mojom::blink::CookieWarningReason::
        kWarnSameSiteLaxCrossDowngradeLax:
      return protocol::Audits::CookieWarningReasonEnum::
          WarnSameSiteLaxCrossDowngradeLax;
    case blink::mojom::blink::CookieWarningReason::
        kWarnAttributeValueExceedsMaxSize:
      return protocol::Audits::CookieWarningReasonEnum::
          WarnAttributeValueExceedsMaxSize;
    case blink::mojom::blink::CookieWarningReason::kWarnDomainNonASCII:
      return protocol::Audits::CookieWarningReasonEnum::WarnDomainNonASCII;
    case blink::mojom::blink::CookieWarningReason::kWarnThirdPartyPhaseout:
      return protocol::Audits::CookieWarningReasonEnum::WarnThirdPartyPhaseout;
    case blink::mojom::blink::CookieWarningReason::
        kWarnCrossSiteRedirectDowngradeChangesInclusion:
      return protocol::Audits::CookieWarningReasonEnum::
          WarnCrossSiteRedirectDowngradeChangesInclusion;
  }
}

std::unique_ptr<std::vector<blink::protocol::String>> BuildCookieWarningReasons(
    const WTF::Vector<mojom::blink::CookieWarningReason>& warning_reasons) {
  auto protocol_warning_reasons =
      std::make_unique<std::vector<blink::protocol::String>>();
  for (const auto& reason : warning_reasons) {
    protocol_warning_reasons->push_back(BuildCookieWarningReason(reason));
  }
  return protocol_warning_reasons;
}
protocol::String BuildCookieOperation(mojom::blink::CookieOperation operation) {
  switch (operation) {
    case blink::mojom::blink::CookieOperation::kSetCookie:
      return protocol::Audits::CookieOperationEnum::SetCookie;
    case blink::mojom::blink::CookieOperation::kReadCookie:
      return protocol::Audits::CookieOperationEnum::ReadCookie;
  }
}

protocol::String BuildMixedContentResolutionStatus(
    mojom::blink::MixedContentResolutionStatus resolution_type) {
  switch (resolution_type) {
    case blink::mojom::blink::MixedContentResolutionStatus::
        kMixedContentBlocked:
      return protocol::Audits::MixedContentResolutionStatusEnum::
          MixedContentBlocked;
    case blink::mojom::blink::MixedContentResolutionStatus::
        kMixedContentAutomaticallyUpgraded:
      return protocol::Audits::MixedContentResolutionStatusEnum::
          MixedContentAutomaticallyUpgraded;
    case blink::mojom::blink::MixedContentResolutionStatus::
        kMixedContentWarning:
      return protocol::Audits::MixedContentResolutionStatusEnum::
          MixedContentWarning;
  }
}

protocol::String BuildMixedContentResourceType(
    mojom::blink::RequestContextType request_context) {
  switch (request_context) {
    case mojom::blink::RequestContextType::ATTRIBUTION_SRC:
      return protocol::Audits::MixedContentResourceTypeEnum::AttributionSrc;
    case blink::mojom::blink::RequestContextType::AUDIO:
      return protocol::Audits::MixedContentResourceTypeEnum::Audio;
    case blink::mojom::blink::RequestContextType::BEACON:
      return protocol::Audits::MixedContentResourceTypeEnum::Beacon;
    case blink::mojom::blink::RequestContextType::CSP_REPORT:
      return protocol::Audits::MixedContentResourceTypeEnum::CSPReport;
    case blink::mojom::blink::RequestContextType::DOWNLOAD:
      return protocol::Audits::MixedContentResourceTypeEnum::Download;
    case blink::mojom::blink::RequestContextType::EMBED:
      return protocol::Audits::MixedContentResourceTypeEnum::PluginResource;
    case blink::mojom::blink::RequestContextType::EVENT_SOURCE:
      return protocol::Audits::MixedContentResourceTypeEnum::EventSource;
    case blink::mojom::blink::RequestContextType::FAVICON:
      return protocol::Audits::MixedContentResourceTypeEnum::Favicon;
    case blink::mojom::blink::RequestContextType::FETCH:
      return protocol::Audits::MixedContentResourceTypeEnum::Resource;
    case blink::mojom::blink::RequestContextType::FONT:
      return protocol::Audits::MixedContentResourceTypeEnum::Font;
    case blink::mojom::blink::RequestContextType::FORM:
      return protocol::Audits::MixedContentResourceTypeEnum::Form;
    case blink::mojom::blink::RequestContextType::FRAME:
      return protocol::Audits::MixedContentResourceTypeEnum::Frame;
    case blink::mojom::blink::RequestContextType::HYPERLINK:
      return protocol::Audits::MixedContentResourceTypeEnum::Resource;
    case blink::mojom::blink::RequestContextType::IFRAME:
      return protocol::Audits::MixedContentResourceTypeEnum::Frame;
    case blink::mojom::blink::RequestContextType::IMAGE:
      return protocol::Audits::MixedContentResourceTypeEnum::Image;
    case blink::mojom::blink::RequestContextType::IMAGE_SET:
      return protocol::Audits::MixedContentResourceTypeEnum::Image;
    case blink::mojom::blink::RequestContextType::INTERNAL:
      return protocol::Audits::MixedContentResourceTypeEnum::Resource;
    case blink::mojom::blink::RequestContextType::JSON:
      // TODO(crbug.com/1511738): Consider adding a type
      // specific to JSON modules requests
      return protocol::Audits::MixedContentResourceTypeEnum::Resource;
    case blink::mojom::blink::RequestContextType::LOCATION:
      return protocol::Audits::MixedContentResourceTypeEnum::Resource;
    case blink::mojom::blink::RequestContextType::MANIFEST:
      return protocol::Audits::MixedContentResourceTypeEnum::Manifest;
    case blink::mojom::blink::RequestContextType::OBJECT:
      return protocol::Audits::MixedContentResourceTypeEnum::PluginResource;
    case blink::mojom::blink::RequestContextType::PING:
      return protocol::Audits::MixedContentResourceTypeEnum::Ping;
    case blink::mojom::blink::RequestContextType::PLUGIN:
      return protocol::Audits::MixedContentResourceTypeEnum::PluginData;
    case blink::mojom::blink::RequestContextType::PREFETCH:
      return protocol::Audits::MixedContentResourceTypeEnum::Prefetch;
    case blink::mojom::blink::RequestContextType::SCRIPT:
      return protocol::Audits::MixedContentResourceTypeEnum::Script;
    case blink::mojom::blink::RequestContextType::SERVICE_WORKER:
      return protocol::Audits::MixedContentResourceTypeEnum::ServiceWorker;
    case blink::mojom::blink::RequestContextType::SHARED_WORKER:
      return protocol::Audits::MixedContentResourceTypeEnum::SharedWorker;
    case blink::mojom::blink::RequestContextType::SPECULATION_RULES:
      return protocol::Audits::MixedContentResourceTypeEnum::SpeculationRules;
    case blink::mojom::blink::RequestContextType::STYLE:
      return protocol::Audits::MixedContentResourceTypeEnum::Stylesheet;
    case blink::mojom::blink::RequestContextType::SUBRESOURCE:
      return protocol::Audits::MixedContentResourceTypeEnum::Resource;
    case blink::mojom::blink::RequestContextType::SUBRESOURCE_WEBBUNDLE:
      return protocol::Audits::MixedContentResourceTypeEnum::Resource;
    case blink::mojom::blink::RequestContextType::TRACK:
      return protocol::Audits::MixedContentResourceTypeEnum::Track;
    case blink::mojom::blink::RequestContextType::UNSPECIFIED:
      return protocol::Audits::MixedContentResourceTypeEnum::Resource;
    case blink::mojom::blink::RequestContextType::VIDEO:
      return protocol::Audits::MixedContentResourceTypeEnum::Video;
    case blink::mojom::blink::RequestContextType::WORKER:
      return protocol::Audits::MixedContentResourceTypeEnum::Worker;
    case blink::mojom::blink::RequestContextType::XML_HTTP_REQUEST:
      return protocol::Audits::MixedContentResourceTypeEnum::XMLHttpRequest;
    case blink::mojom::blink::RequestContextType::XSLT:
      return protocol::Audits::MixedContentResourceTypeEnum::XSLT;
  }
}

protocol::String BuildBlockedByResponseReason(
    network::mojom::blink::BlockedByResponseReason reason) {
  switch (reason) {
    case network::mojom::blink::BlockedByResponseReason::
        kCoepFrameResourceNeedsCoepHeader:
      return protocol::Audits::BlockedByResponseReasonEnum::
          CoepFrameResourceNeedsCoepHeader;
    case network::mojom::blink::BlockedByResponseReason::
        kCoopSandboxedIFrameCannotNavigateToCoopPage:
      return protocol::Audits::BlockedByResponseReasonEnum::
          CoopSandboxedIFrameCannotNavigateToCoopPage;
    case network::mojom::blink::BlockedByResponseReason::kCorpNotSameOrigin:
      return protocol::Audits::BlockedByResponseReasonEnum::CorpNotSameOrigin;
    case network::mojom::blink::BlockedByResponseReason::
        kCorpNotSameOriginAfterDefaultedToSameOriginByCoep:
      return protocol::Audits::BlockedByResponseReasonEnum::
          CorpNotSameOriginAfterDefaultedToSameOriginByCoep;
    case network::mojom::blink::BlockedByResponseReason::
        kCorpNotSameOriginAfterDefaultedToSameOriginByDip:
      return protocol::Audits::BlockedByResponseReasonEnum::
          CorpNotSameOriginAfterDefaultedToSameOriginByDip;
    case network::mojom::blink::BlockedByResponseReason::
        kCorpNotSameOriginAfterDefaultedToSameOriginByCoepAndDip:
      return protocol::Audits::BlockedByResponseReasonEnum::
          CorpNotSameOriginAfterDefaultedToSameOriginByCoepAndDip;
    case network::mojom::blink::BlockedByResponseReason::kCorpNotSameSite:
      return protocol::Audits::BlockedByResponseReasonEnum::CorpNotSameSite;
  }
}

protocol::String BuildViolationType(
    mojom::blink::ContentSecurityPolicyViolationType violation_type) {
  switch (violation_type) {
    case blink::mojom::blink::ContentSecurityPolicyViolationType::
        kInlineViolation:
      return protocol::Audits::ContentSecurityPolicyViolationTypeEnum::
          KInlineViolation;
    case blink::mojom::blink::ContentSecurityPolicyViolationType::
        kEvalViolation:
      return protocol::Audits::ContentSecurityPolicyViolationTypeEnum::
          KEvalViolation;
    case blink::mojom::blink::ContentSecurityPolicyViolationType::
        kWasmEvalViolation:
      return protocol::Audits::ContentSecurityPolicyViolationTypeEnum::
          KWasmEvalViolation;
    case blink::mojom::blink::ContentSecurityPolicyViolationType::kURLViolation:
      return protocol::Audits::ContentSecurityPolicyViolationTypeEnum::
          KURLViolation;
    case blink::mojom::blink::ContentSecurityPolicyViolationType::
        kTrustedTypesSinkViolation:
      return protocol::Audits::ContentSecurityPolicyViolationTypeEnum::
          KTrustedTypesSinkViolation;
    case blink::mojom::blink::ContentSecurityPolicyViolationType::
        kTrustedTypesPolicyViolation:
      return protocol::Audits::ContentSecurityPolicyViolationTypeEnum::
          KTrustedTypesPolicyViolation;
  }
}

protocol::String BuildSABIssueType(
    blink::mojom::blink::SharedArrayBufferIssueType type) {
  switch (type) {
    case blink::mojom::blink::SharedArrayBufferIssueType::kTransferIssue:
      return protocol::Audits::SharedArrayBufferIssueTypeEnum::TransferIssue;
    case blink::mojom::blink::SharedArrayBufferIssueType::kCreationIssue:
      return protocol::Audits::SharedArrayBufferIssueTypeEnum::CreationIssue;
  }
}

std::unique_ptr<protocol::Audits::SourceCodeLocation> BuildAffectedLocation(
    const blink::mojom::blink::AffectedLocationPtr& affected_location) {
  auto protocol_affected_location =
      protocol::Audits::SourceCodeLocation::create()
          .setUrl(affected_location->url)
          .setColumnNumber(affected_location->column)
          .setLineNumber(affected_location->line)
          .build();
  if (!affected_location->script_id.empty())
    protocol_affected_location->setScriptId(affected_location->script_id);
  return protocol_affected_location;
}

}  // namespace

std::unique_ptr<protocol::Audits::InspectorIssue>
ConvertInspectorIssueToProtocolFormat(InspectorIssue* issue) {
  auto issueDetails = protocol::Audits::InspectorIssueDetails::create();

  if (issue->Details()->cookie_issue_details) {
    const auto* d = issue->Details()->cookie_issue_details.get();
    auto cookieDetails =
        std::move(protocol::Audits::CookieIssueDetails::create()
                      .setCookie(BuildAffectedCookie(d->cookie))
                      .setCookieExclusionReasons(
                          BuildCookieExclusionReasons(d->exclusion_reason))
                      .setCookieWarningReasons(
                          BuildCookieWarningReasons(d->warning_reason))
                      .setOperation(BuildCookieOperation(d->operation)));

    if (d->site_for_cookies) {
      cookieDetails.setSiteForCookies(*d->site_for_cookies);
    }
    if (d->cookie_url) {
      cookieDetails.setCookieUrl(*d->cookie_url);
    }
    if (d->request) {
      cookieDetails.setRequest(BuildAffectedRequest(d->request));
    }
    issueDetails.setCookieIssueDetails(cookieDetails.build());
  }

  if (issue->Details()->mixed_content_issue_details) {
    const auto* d = issue->Details()->mixed_content_issue_details.get();
    auto mixedContentDetails =
        protocol::Audits::MixedContentIssueDetails::create()
            .setResourceType(BuildMixedContentResourceType(d->request_context))
            .setResolutionStatus(
                BuildMixedContentResolutionStatus(d->resolution_status))
            .setInsecureURL(d->insecure_url)
            .setMainResourceURL(d->main_resource_url)
            .build();
    if (d->request) {
      mixedContentDetails->setRequest(BuildAffectedRequest(d->request));
    }
    if (d->frame) {
      mixedContentDetails->setFrame(BuildAffectedFrame(d->frame));
    }
    issueDetails.setMixedContentIssueDetails(std::move(mixedContentDetails));
  }

  if (issue->Details()->blocked_by_response_issue_details) {
    const auto* d = issue->Details()->blocked_by_response_issue_details.get();
    auto blockedByResponseDetails =
        protocol::Audits::BlockedByResponseIssueDetails::create()
            .setRequest(BuildAffectedRequest(d->request))
            .setReason(BuildBlockedByResponseReason(d->reason))
            .build();
    if (d->parentFrame) {
      blockedByResponseDetails->setParentFrame(
          BuildAffectedFrame(d->parentFrame));
    }
    if (d->blockedFrame) {
      blockedByResponseDetails->setBlockedFrame(
          BuildAffectedFrame(d->blockedFrame));
    }
    issueDetails.setBlockedByResponseIssueDetails(
        std::move(blockedByResponseDetails));
  }

  if (issue->Details()->csp_issue_details) {
    const auto* d = issue->Details()->csp_issue_details.get();
    auto cspDetails =
        std::move(protocol::Audits::ContentSecurityPolicyIssueDetails::create()
                      .setViolatedDirective(d->violated_directive)
                      .setIsReportOnly(d->is_report_only)
                      .setContentSecurityPolicyViolationType(BuildViolationType(
                          d->content_security_policy_violation_type)));
    if (d->blocked_url) {
      cspDetails.setBlockedURL(*d->blocked_url);
    }
    if (d->frame_ancestor)
      cspDetails.setFrameAncestor(BuildAffectedFrame(d->frame_ancestor));
    if (d->affected_location) {
      cspDetails.setSourceCodeLocation(
          BuildAffectedLocation(d->affected_location));
    }
    if (d->violating_node_id)
      cspDetails.setViolatingNodeId(d->violating_node_id);
    issueDetails.setContentSecurityPolicyIssueDetails(cspDetails.build());
  }

  if (issue->Details()->sab_issue_details) {
    const auto* d = issue->Details()->sab_issue_details.get();
    auto details =
        protocol::Audits::SharedArrayBufferIssueDetails::create()
            .setIsWarning(d->is_warning)
            .setType(BuildSABIssueType(d->type))
            .setSourceCodeLocation(BuildAffectedLocation(d->affected_location))
            .build();
    issueDetails.setSharedArrayBufferIssueDetails(std::move(details));
  }

  if (issue->Details()->low_text_contrast_details) {
    const auto* d = issue->Details()->low_text_contrast_details.get();
    auto lowContrastDetails =
        protocol::Audits::LowTextContrastIssueDetails::create()
            .setThresholdAA(d->threshold_aa)
            .setThresholdAAA(d->threshold_aaa)
            .setFontSize(d->font_size)
            .setFontWeight(d->font_weight)
            .setContrastRatio(d->contrast_ratio)
            .setViolatingNodeSelector(d->violating_node_selector)
            .setViolatingNodeId(d->violating_node_id)
            .build();
    issueDetails.setLowTextContrastIssueDetails(std::move(lowContrastDetails));
  }

  auto final_issue = protocol::Audits::InspectorIssue::create()
                         .setCode(InspectorIssueCodeValue(issue->Code()))
                         .setDetails(issueDetails.build())
                         .build();
  if (issue->Details()->issue_id) {
    String issue_id = String::FromUTF8(issue->Details()->issue_id->ToString());
    final_issue->setIssueId(issue_id);
  }
  return final_issue;
}

}  // namespace blink
