/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"

#include "base/strings/string_number_conversions.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/trust_tokens.mojom-blink-forward.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/resource_request_blocked_reason.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/trust_token_params_conversion.h"

namespace blink {

namespace {
constexpr char kThrottledErrorDescription[] =
    "Request throttled. Visit https://dev.chromium.org/throttling for more "
    "information.";
}  // namespace

ResourceError ResourceError::CancelledError(const KURL& url) {
  return ResourceError(net::ERR_ABORTED, url, std::nullopt);
}

ResourceError ResourceError::CancelledDueToAccessCheckError(
    const KURL& url,
    ResourceRequestBlockedReason blocked_reason) {
  ResourceError error = CancelledError(url);
  error.is_access_check_ = true;
  error.should_collapse_inititator_ =
      blocked_reason == ResourceRequestBlockedReason::kSubresourceFilter;
  return error;
}

ResourceError ResourceError::CancelledDueToAccessCheckError(
    const KURL& url,
    ResourceRequestBlockedReason blocked_reason,
    const String& localized_description) {
  ResourceError error = CancelledDueToAccessCheckError(url, blocked_reason);
  error.localized_description_ = localized_description;
  return error;
}

ResourceError ResourceError::BlockedByResponse(
    const KURL& url,
    network::mojom::BlockedByResponseReason blocked_by_response_reason) {
  ResourceError error(net::ERR_BLOCKED_BY_RESPONSE, url, std::nullopt);
  error.blocked_by_response_reason_ = blocked_by_response_reason;
  return error;
}

ResourceError ResourceError::CacheMissError(const KURL& url) {
  return ResourceError(net::ERR_CACHE_MISS, url, std::nullopt);
}

ResourceError ResourceError::TimeoutError(const KURL& url) {
  return ResourceError(net::ERR_TIMED_OUT, url, std::nullopt);
}

ResourceError ResourceError::Failure(const KURL& url) {
  return ResourceError(net::ERR_FAILED, url, std::nullopt);
}

ResourceError ResourceError::HttpError(const KURL& url) {
  ResourceError error = CancelledError(url);
  error.is_cancelled_from_http_error_ = true;
  return error;
}

ResourceError::ResourceError(
    int error_code,
    const KURL& url,
    std::optional<network::CorsErrorStatus> cors_error_status)
    : error_code_(error_code),
      failing_url_(url),
      is_access_check_(cors_error_status.has_value()),
      cors_error_status_(cors_error_status) {
  DCHECK_NE(error_code_, 0);
  InitializeDescription();
}

ResourceError::ResourceError(const KURL& url,
                             const network::CorsErrorStatus& cors_error_status)
    : ResourceError(net::ERR_FAILED, url, cors_error_status) {}

ResourceError::ResourceError(const WebURLError& error)
    : error_code_(error.reason()),
      extended_error_code_(error.extended_reason()),
      resolve_error_info_(error.resolve_error_info()),
      failing_url_(error.url()),
      is_access_check_(error.is_web_security_violation()),
      has_copy_in_cache_(error.has_copy_in_cache()),
      cors_error_status_(error.cors_error_status()),
      should_collapse_inititator_(error.should_collapse_initiator()),
      blocked_by_response_reason_(error.blocked_by_response_reason()),
      trust_token_operation_error_(error.trust_token_operation_error()) {
  DCHECK_NE(error_code_, 0);
  InitializeDescription();
}

ResourceError::operator WebURLError() const {
  WebURLError::HasCopyInCache has_copy_in_cache =
      has_copy_in_cache_ ? WebURLError::HasCopyInCache::kTrue
                         : WebURLError::HasCopyInCache::kFalse;

  if (cors_error_status_) {
    DCHECK_EQ(net::ERR_FAILED, error_code_);
    return WebURLError(*cors_error_status_, has_copy_in_cache, failing_url_);
  }

  if (trust_token_operation_error_ !=
      network::mojom::blink::TrustTokenOperationStatus::kOk) {
    return WebURLError(error_code_, trust_token_operation_error_, failing_url_);
  }

  return WebURLError(
      error_code_, extended_error_code_, resolve_error_info_, has_copy_in_cache,
      is_access_check_ ? WebURLError::IsWebSecurityViolation::kTrue
                       : WebURLError::IsWebSecurityViolation::kFalse,
      failing_url_,
      should_collapse_inititator_
          ? WebURLError::ShouldCollapseInitiator::kTrue
          : WebURLError::ShouldCollapseInitiator::kFalse);
}

bool ResourceError::Compare(const ResourceError& a, const ResourceError& b) {
  if (a.ErrorCode() != b.ErrorCode())
    return false;

  if (a.FailingURL() != b.FailingURL())
    return false;

  if (a.LocalizedDescription() != b.LocalizedDescription())
    return false;

  if (a.IsAccessCheck() != b.IsAccessCheck())
    return false;

  if (a.HasCopyInCache() != b.HasCopyInCache())
    return false;

  if (a.CorsErrorStatus() != b.CorsErrorStatus())
    return false;

  if (a.extended_error_code_ != b.extended_error_code_)
    return false;

  if (a.resolve_error_info_ != b.resolve_error_info_)
    return false;

  if (a.trust_token_operation_error_ != b.trust_token_operation_error_)
    return false;

  if (a.should_collapse_inititator_ != b.should_collapse_inititator_)
    return false;

  return true;
}

bool ResourceError::IsTimeout() const {
  return error_code_ == net::ERR_TIMED_OUT;
}

bool ResourceError::IsCancellation() const {
  return error_code_ == net::ERR_ABORTED;
}

bool ResourceError::IsTrustTokenCacheHit() const {
  return error_code_ ==
         net::ERR_TRUST_TOKEN_OPERATION_SUCCESS_WITHOUT_SENDING_REQUEST;
}

bool ResourceError::IsUnactionableTrustTokensStatus() const {
  return IsTrustTokenCacheHit() ||
         (error_code_ == net::ERR_TRUST_TOKEN_OPERATION_FAILED &&
          trust_token_operation_error_ ==
              network::mojom::TrustTokenOperationStatus::kUnauthorized);
}

bool ResourceError::IsCacheMiss() const {
  return error_code_ == net::ERR_CACHE_MISS;
}

bool ResourceError::WasBlockedByResponse() const {
  return error_code_ == net::ERR_BLOCKED_BY_RESPONSE;
}

bool ResourceError::WasBlockedByORB() const {
  return error_code_ == net::ERR_BLOCKED_BY_ORB;
}

namespace {
blink::ResourceRequestBlockedReason
BlockedByResponseReasonToResourceRequestBlockedReason(
    network::mojom::BlockedByResponseReason reason) {
  switch (reason) {
    case network::mojom::BlockedByResponseReason::
        kCoepFrameResourceNeedsCoepHeader:
      return blink::ResourceRequestBlockedReason::
          kCoepFrameResourceNeedsCoepHeader;
    case network::mojom::BlockedByResponseReason::
        kCoopSandboxedIFrameCannotNavigateToCoopPage:
      return blink::ResourceRequestBlockedReason::
          kCoopSandboxedIFrameCannotNavigateToCoopPage;
    case network::mojom::BlockedByResponseReason::kCorpNotSameOrigin:
      return blink::ResourceRequestBlockedReason::kCorpNotSameOrigin;
    case network::mojom::BlockedByResponseReason::
        kCorpNotSameOriginAfterDefaultedToSameOriginByCoep:
      return blink::ResourceRequestBlockedReason::
          kCorpNotSameOriginAfterDefaultedToSameOriginByCoep;
    case network::mojom::BlockedByResponseReason::
        kCorpNotSameOriginAfterDefaultedToSameOriginByDip:
      return blink::ResourceRequestBlockedReason::
          kCorpNotSameOriginAfterDefaultedToSameOriginByDip;
    case network::mojom::BlockedByResponseReason::
        kCorpNotSameOriginAfterDefaultedToSameOriginByCoepAndDip:
      return blink::ResourceRequestBlockedReason::
          kCorpNotSameOriginAfterDefaultedToSameOriginByCoepAndDip;
    case network::mojom::BlockedByResponseReason::kCorpNotSameSite:
      return blink::ResourceRequestBlockedReason::kCorpNotSameSite;
  }
  NOTREACHED_IN_MIGRATION();
  return blink::ResourceRequestBlockedReason::kOther;
}
}  // namespace

std::optional<ResourceRequestBlockedReason>
ResourceError::GetResourceRequestBlockedReason() const {
  if (error_code_ != net::ERR_BLOCKED_BY_CLIENT &&
      error_code_ != net::ERR_BLOCKED_BY_RESPONSE) {
    return std::nullopt;
  }
  if (blocked_by_response_reason_) {
    return BlockedByResponseReasonToResourceRequestBlockedReason(
        *blocked_by_response_reason_);
  }

  if (extended_error_code_ <=
      static_cast<int>(ResourceRequestBlockedReason::kMax)) {
    return static_cast<ResourceRequestBlockedReason>(extended_error_code_);
  }

  return std::nullopt;
}

std::optional<network::mojom::BlockedByResponseReason>
ResourceError::GetBlockedByResponseReason() const {
  if (error_code_ != net::ERR_BLOCKED_BY_CLIENT &&
      error_code_ != net::ERR_BLOCKED_BY_RESPONSE) {
    return std::nullopt;
  }
  return blocked_by_response_reason_;
}

namespace {
String DescriptionForBlockedByClientOrResponse(
    int error,
    const std::optional<blink::ResourceRequestBlockedReason>& reason) {
  if (!reason || *reason == ResourceRequestBlockedReason::kOther)
    return WebString::FromASCII(net::ErrorToString(error));
  std::string detail;
  switch (*reason) {
    case ResourceRequestBlockedReason::kOther:
      NOTREACHED_IN_MIGRATION();  // handled above
      break;
    case ResourceRequestBlockedReason::kCSP:
      detail = "CSP";
      break;
    case ResourceRequestBlockedReason::kMixedContent:
      detail = "MixedContent";
      break;
    case ResourceRequestBlockedReason::kOrigin:
      detail = "Origin";
      break;
    case ResourceRequestBlockedReason::kInspector:
      detail = "Inspector";
      break;
    case ResourceRequestBlockedReason::kSubresourceFilter:
      detail = "SubresourceFilter";
      break;
    case ResourceRequestBlockedReason::kContentType:
      detail = "ContentType";
      break;
    case ResourceRequestBlockedReason::kCoepFrameResourceNeedsCoepHeader:
      detail = "ResponseNeedsCrossOriginEmbedderPolicy";
      break;
    case ResourceRequestBlockedReason::
        kCoopSandboxedIFrameCannotNavigateToCoopPage:
      detail = "SandboxedIFrameCannotNavigateToOriginIsolatedPage";
      break;
    case ResourceRequestBlockedReason::kCorpNotSameOrigin:
      detail = "NotSameOrigin";
      break;
    case ResourceRequestBlockedReason::
        kCorpNotSameOriginAfterDefaultedToSameOriginByCoep:
      detail = "NotSameOriginAfterDefaultedToSameOriginByCoep";
      break;
    case ResourceRequestBlockedReason::
        kCorpNotSameOriginAfterDefaultedToSameOriginByDip:
      detail = "NotSameOriginAfterDefaultedToSameOriginByDip";
      break;
    case ResourceRequestBlockedReason::
        kCorpNotSameOriginAfterDefaultedToSameOriginByCoepAndDip:
      detail = "NotSameOriginAfterDefaultedToSameOriginByCoepAndDip";
      break;
    case ResourceRequestBlockedReason::kCorpNotSameSite:
      detail = "NotSameSite";
      break;
    case ResourceRequestBlockedReason::kConversionRequest:
      detail = "ConversionRequest";
      break;
  }
  return WebString::FromASCII(net::ErrorToString(error) + "." + detail);
}
}  // namespace

void ResourceError::InitializeDescription() {
  if (error_code_ == net::ERR_TEMPORARILY_THROTTLED) {
    localized_description_ = WebString::FromASCII(kThrottledErrorDescription);
  } else if (error_code_ == net::ERR_BLOCKED_BY_CLIENT ||
             error_code_ == net::ERR_BLOCKED_BY_RESPONSE) {
    std::optional<ResourceRequestBlockedReason> reason =
        GetResourceRequestBlockedReason();
    localized_description_ =
        DescriptionForBlockedByClientOrResponse(error_code_, reason);
  } else {
    localized_description_ = WebString::FromASCII(
        net::ExtendedErrorToString(error_code_, extended_error_code_));
  }
}

std::ostream& operator<<(std::ostream& os, const ResourceError& error) {
  return os << ", ErrorCode = " << error.ErrorCode()
            << ", FailingURL = " << error.FailingURL()
            << ", LocalizedDescription = " << error.LocalizedDescription()
            << ", IsCancellation = " << error.IsCancellation()
            << ", IsAccessCheck = " << error.IsAccessCheck()
            << ", IsTimeout = " << error.IsTimeout()
            << ", HasCopyInCache = " << error.HasCopyInCache()
            << ", IsCacheMiss = " << error.IsCacheMiss()
            << ", TrustTokenOperationError = "
            << String::FromUTF8(base::NumberToString(
                   static_cast<int32_t>(error.TrustTokenOperationError())));
}

}  // namespace blink
