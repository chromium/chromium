// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_audits_agent.h"

#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/web/web_image.h"
#include "third_party/blink/renderer/core/inspector/inspector_issue.h"
#include "third_party/blink/renderer/core/inspector/inspector_issue_storage.h"
#include "third_party/blink/renderer/core/inspector/inspector_network_agent.h"
#include "third_party/blink/renderer/platform/graphics/image_data_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"

namespace blink {

using protocol::Maybe;
using protocol::Response;

namespace encoding_enum = protocol::Audits::GetEncodedResponse::EncodingEnum;

namespace {

static constexpr int kMaximumEncodeImageWidthInPixels = 10000;

static constexpr int kMaximumEncodeImageHeightInPixels = 10000;

static constexpr double kDefaultEncodeQuality = 1;

bool EncodeAsImage(char* body,
                   size_t size,
                   const String& encoding,
                   const double quality,
                   Vector<unsigned char>* output) {
  const WebSize maximum_size = WebSize(kMaximumEncodeImageWidthInPixels,
                                       kMaximumEncodeImageHeightInPixels);
  SkBitmap bitmap = WebImage::FromData(WebData(body, size), maximum_size);
  if (bitmap.isNull())
    return false;

  SkImageInfo info =
      SkImageInfo::Make(bitmap.width(), bitmap.height(), kRGBA_8888_SkColorType,
                        kUnpremul_SkAlphaType);
  uint32_t row_bytes = static_cast<uint32_t>(info.minRowBytes());
  Vector<unsigned char> pixel_storage(
      SafeCast<wtf_size_t>(info.computeByteSize(row_bytes)));
  SkPixmap pixmap(info, pixel_storage.data(), row_bytes);
  sk_sp<SkImage> image = SkImage::MakeFromBitmap(bitmap);

  if (!image || !image->readPixels(pixmap, 0, 0))
    return false;

  std::unique_ptr<ImageDataBuffer> image_to_encode =
      ImageDataBuffer::Create(pixmap);
  if (!image_to_encode)
    return false;

  String mime_type_name = StringView("image/") + encoding;
  ImageEncodingMimeType mime_type;
  bool valid_mime_type = ParseImageEncodingMimeType(mime_type_name, mime_type);
  DCHECK(valid_mime_type);
  return image_to_encode->EncodeImage(mime_type, quality, output);
}

}  // namespace

void InspectorAuditsAgent::Trace(Visitor* visitor) const {
  visitor->Trace(network_agent_);
  visitor->Trace(inspector_issue_storage_);
  InspectorBaseAgent::Trace(visitor);
}

InspectorAuditsAgent::InspectorAuditsAgent(InspectorNetworkAgent* network_agent,
                                           InspectorIssueStorage* storage)
    : inspector_issue_storage_(storage),
      enabled_(&agent_state_, false),
      network_agent_(network_agent) {}

InspectorAuditsAgent::~InspectorAuditsAgent() = default;

protocol::Response InspectorAuditsAgent::getEncodedResponse(
    const String& request_id,
    const String& encoding,
    Maybe<double> quality,
    Maybe<bool> size_only,
    Maybe<protocol::Binary>* out_body,
    int* out_original_size,
    int* out_encoded_size) {
  DCHECK(encoding == encoding_enum::Jpeg || encoding == encoding_enum::Png ||
         encoding == encoding_enum::Webp);

  String body;
  bool is_base64_encoded;
  Response response =
      network_agent_->GetResponseBody(request_id, &body, &is_base64_encoded);
  if (!response.IsSuccess())
    return response;

  Vector<char> base64_decoded_buffer;
  if (!is_base64_encoded || !Base64Decode(body, base64_decoded_buffer) ||
      base64_decoded_buffer.size() == 0) {
    return Response::ServerError("Failed to decode original image");
  }

  Vector<unsigned char> encoded_image;
  if (!EncodeAsImage(base64_decoded_buffer.data(), base64_decoded_buffer.size(),
                     encoding, quality.fromMaybe(kDefaultEncodeQuality),
                     &encoded_image)) {
    return Response::ServerError("Could not encode image with given settings");
  }

  *out_original_size = static_cast<int>(base64_decoded_buffer.size());
  *out_encoded_size = static_cast<int>(encoded_image.size());

  if (!size_only.fromMaybe(false)) {
    *out_body = protocol::Binary::fromVector(std::move(encoded_image));
  }
  return Response::Success();
}

Response InspectorAuditsAgent::enable() {
  if (enabled_.Get()) {
    return Response::Success();
  }

  enabled_.Set(true);
  InnerEnable();
  return Response::Success();
}

Response InspectorAuditsAgent::disable() {
  if (!enabled_.Get()) {
    return Response::Success();
  }

  enabled_.Clear();
  instrumenting_agents_->RemoveInspectorAuditsAgent(this);
  return Response::Success();
}

void InspectorAuditsAgent::Restore() {
  if (!enabled_.Get())
    return;
  InnerEnable();
}

void InspectorAuditsAgent::InnerEnable() {
  instrumenting_agents_->AddInspectorAuditsAgent(this);
  for (wtf_size_t i = 0; i < inspector_issue_storage_->size(); ++i)
    InspectorIssueAdded(inspector_issue_storage_->at(i));
}

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
  if (!request->url.IsEmpty()) {
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
    case mojom::blink::InspectorIssueCode::kSameSiteCookieIssue:
      return protocol::Audits::InspectorIssueCodeEnum::SameSiteCookieIssue;
    case mojom::blink::InspectorIssueCode::kMixedContentIssue:
      return protocol::Audits::InspectorIssueCodeEnum::MixedContentIssue;
    case mojom::blink::InspectorIssueCode::kBlockedByResponseIssue:
      return protocol::Audits::InspectorIssueCodeEnum::BlockedByResponseIssue;
    case mojom::blink::InspectorIssueCode::kContentSecurityPolicyIssue:
      return protocol::Audits::InspectorIssueCodeEnum::
          ContentSecurityPolicyIssue;
  }
}

protocol::String BuildCookieExclusionReason(
    mojom::blink::SameSiteCookieExclusionReason exclusion_reason) {
  switch (exclusion_reason) {
    case blink::mojom::blink::SameSiteCookieExclusionReason::
        kExcludeSameSiteUnspecifiedTreatedAsLax:
      return protocol::Audits::SameSiteCookieExclusionReasonEnum::
          ExcludeSameSiteUnspecifiedTreatedAsLax;
    case blink::mojom::blink::SameSiteCookieExclusionReason::
        kExcludeSameSiteNoneInsecure:
      return protocol::Audits::SameSiteCookieExclusionReasonEnum::
          ExcludeSameSiteNoneInsecure;
    case blink::mojom::blink::SameSiteCookieExclusionReason::
        kExcludeSameSiteLax:
      return protocol::Audits::SameSiteCookieExclusionReasonEnum::
          ExcludeSameSiteLax;
    case blink::mojom::blink::SameSiteCookieExclusionReason::
        kExcludeSameSiteStrict:
      return protocol::Audits::SameSiteCookieExclusionReasonEnum::
          ExcludeSameSiteStrict;
  }
}

std::unique_ptr<std::vector<blink::protocol::String>>
BuildCookieExclusionReasons(
    const WTF::Vector<mojom::blink::SameSiteCookieExclusionReason>&
        exclusion_reasons) {
  auto protocol_exclusion_reasons =
      std::make_unique<std::vector<blink::protocol::String>>();
  for (const auto& reason : exclusion_reasons) {
    protocol_exclusion_reasons->push_back(BuildCookieExclusionReason(reason));
  }
  return protocol_exclusion_reasons;
}

protocol::String BuildCookieWarningReason(
    mojom::blink::SameSiteCookieWarningReason warning_reason) {
  switch (warning_reason) {
    case blink::mojom::blink::SameSiteCookieWarningReason::
        kWarnSameSiteUnspecifiedCrossSiteContext:
      return protocol::Audits::SameSiteCookieWarningReasonEnum::
          WarnSameSiteUnspecifiedCrossSiteContext;
    case blink::mojom::blink::SameSiteCookieWarningReason::
        kWarnSameSiteNoneInsecure:
      return protocol::Audits::SameSiteCookieWarningReasonEnum::
          WarnSameSiteNoneInsecure;
    case blink::mojom::blink::SameSiteCookieWarningReason::
        kWarnSameSiteUnspecifiedLaxAllowUnsafe:
      return protocol::Audits::SameSiteCookieWarningReasonEnum::
          WarnSameSiteUnspecifiedLaxAllowUnsafe;
    case blink::mojom::blink::SameSiteCookieWarningReason::
        kWarnSameSiteStrictLaxDowngradeStrict:
      return protocol::Audits::SameSiteCookieWarningReasonEnum::
          WarnSameSiteStrictLaxDowngradeStrict;
    case blink::mojom::blink::SameSiteCookieWarningReason::
        kWarnSameSiteStrictCrossDowngradeStrict:
      return protocol::Audits::SameSiteCookieWarningReasonEnum::
          WarnSameSiteStrictCrossDowngradeStrict;
    case blink::mojom::blink::SameSiteCookieWarningReason::
        kWarnSameSiteStrictCrossDowngradeLax:
      return protocol::Audits::SameSiteCookieWarningReasonEnum::
          WarnSameSiteStrictCrossDowngradeLax;
    case blink::mojom::blink::SameSiteCookieWarningReason::
        kWarnSameSiteLaxCrossDowngradeStrict:
      return protocol::Audits::SameSiteCookieWarningReasonEnum::
          WarnSameSiteLaxCrossDowngradeStrict;
    case blink::mojom::blink::SameSiteCookieWarningReason::
        kWarnSameSiteLaxCrossDowngradeLax:
      return protocol::Audits::SameSiteCookieWarningReasonEnum::
          WarnSameSiteLaxCrossDowngradeLax;
  }
}

std::unique_ptr<std::vector<blink::protocol::String>> BuildCookieWarningReasons(
    const WTF::Vector<mojom::blink::SameSiteCookieWarningReason>&
        warning_reasons) {
  auto protocol_warning_reasons =
      std::make_unique<std::vector<blink::protocol::String>>();
  for (const auto& reason : warning_reasons) {
    protocol_warning_reasons->push_back(BuildCookieWarningReason(reason));
  }
  return protocol_warning_reasons;
}
protocol::String BuildCookieOperation(
    mojom::blink::SameSiteCookieOperation operation) {
  switch (operation) {
    case blink::mojom::blink::SameSiteCookieOperation::kSetCookie:
      return protocol::Audits::SameSiteCookieOperationEnum::SetCookie;
    case blink::mojom::blink::SameSiteCookieOperation::kReadCookie:
      return protocol::Audits::SameSiteCookieOperationEnum::ReadCookie;
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
    case blink::mojom::blink::RequestContextType::IMPORT:
      return protocol::Audits::MixedContentResourceTypeEnum::Import;
    case blink::mojom::blink::RequestContextType::INTERNAL:
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
    case blink::mojom::blink::RequestContextType::STYLE:
      return protocol::Audits::MixedContentResourceTypeEnum::Stylesheet;
    case blink::mojom::blink::RequestContextType::SUBRESOURCE:
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

}  // namespace

void InspectorAuditsAgent::InspectorIssueAdded(InspectorIssue* issue) {
  auto issueDetails = protocol::Audits::InspectorIssueDetails::create();

  if (issue->Details()->samesite_cookie_issue_details) {
    const auto* d = issue->Details()->samesite_cookie_issue_details.get();
    auto sameSiteCookieDetails =
        std::move(protocol::Audits::SameSiteCookieIssueDetails::create()
                      .setCookie(BuildAffectedCookie(d->cookie))
                      .setCookieExclusionReasons(
                          BuildCookieExclusionReasons(d->exclusion_reason))
                      .setCookieWarningReasons(
                          BuildCookieWarningReasons(d->warning_reason))
                      .setOperation(BuildCookieOperation(d->operation)));

    if (d->site_for_cookies) {
      sameSiteCookieDetails.setSiteForCookies(*d->site_for_cookies);
    }
    if (d->cookie_url) {
      sameSiteCookieDetails.setCookieUrl(*d->cookie_url);
    }
    if (d->request) {
      sameSiteCookieDetails.setRequest(BuildAffectedRequest(d->request));
    }
    issueDetails.setSameSiteCookieIssueDetails(sameSiteCookieDetails.build());
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
    if (d->source_location) {
      auto source_location = protocol::Audits::SourceCodeLocation::create()
                                 .setUrl(d->source_location->url)
                                 .setColumnNumber(d->source_location->column)
                                 .setLineNumber(d->source_location->line)
                                 .build();
      cspDetails.setSourceCodeLocation(std::move(source_location));
    }
    if (d->violating_node_id)
      cspDetails.setViolatingNodeId(d->violating_node_id);
    issueDetails.setContentSecurityPolicyIssueDetails(cspDetails.build());
  }

  auto inspector_issue = protocol::Audits::InspectorIssue::create()
                             .setCode(InspectorIssueCodeValue(issue->Code()))
                             .setDetails(issueDetails.build())
                             .build();

  GetFrontend()->issueAdded(std::move(inspector_issue));
  GetFrontend()->flush();
}

}  // namespace blink
