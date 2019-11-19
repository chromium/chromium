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

#include "net/base/net_errors.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/resource_request_blocked_reason.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"

namespace blink {

namespace {
constexpr char kThrottledErrorDescription[] =
    "Request throttled. Visit https://dev.chromium.org/throttling for more "
    "information.";
}  // namespace

ResourceError ResourceError::CancelledError(const KURL& url) {
  return ResourceError(net::ERR_ABORTED, url, base::nullopt);
}

ResourceError ResourceError::CancelledDueToAccessCheckError(
    const KURL& url,
    ResourceRequestBlockedReason blocked_reason) {
  ResourceError error = CancelledError(url);
  error.is_access_check_ = true;
  error.blocked_by_subresource_filter_ =
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

ResourceError ResourceError::CacheMissError(const KURL& url) {
  return ResourceError(net::ERR_CACHE_MISS, url, base::nullopt);
}

ResourceError ResourceError::TimeoutError(const KURL& url) {
  return ResourceError(net::ERR_TIMED_OUT, url, base::nullopt);
}

ResourceError ResourceError::Failure(const KURL& url) {
  return ResourceError(net::ERR_FAILED, url, base::nullopt);
}

ResourceError::ResourceError(
    int error_code,
    const KURL& url,
    base::Optional<network::CorsErrorStatus> cors_error_status)
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
      failing_url_(error.url()),
      is_access_check_(error.is_web_security_violation()),
      has_copy_in_cache_(error.has_copy_in_cache()),
      cors_error_status_(error.cors_error_status()) {
  DCHECK_NE(error_code_, 0);
  InitializeDescription();
}

ResourceError ResourceError::Copy() const {
  ResourceError error_copy(error_code_, failing_url_.Copy(),
                           cors_error_status_);
  error_copy.extended_error_code_ = extended_error_code_;
  error_copy.has_copy_in_cache_ = has_copy_in_cache_;
  error_copy.localized_description_ = localized_description_.IsolatedCopy();
  error_copy.is_access_check_ = is_access_check_;
  return error_copy;
}

ResourceError::operator WebURLError() const {
  WebURLError::HasCopyInCache has_copy_in_cache =
      has_copy_in_cache_ ? WebURLError::HasCopyInCache::kTrue
                         : WebURLError::HasCopyInCache::kFalse;

  if (cors_error_status_) {
    DCHECK_EQ(net::ERR_FAILED, error_code_);
    return WebURLError(*cors_error_status_, has_copy_in_cache, failing_url_);
  }

  return WebURLError(error_code_, extended_error_code_, has_copy_in_cache,
                     is_access_check_
                         ? WebURLError::IsWebSecurityViolation::kTrue
                         : WebURLError::IsWebSecurityViolation::kFalse,
                     failing_url_);
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

  return true;
}

bool ResourceError::IsTimeout() const {
  return error_code_ == net::ERR_TIMED_OUT;
}

bool ResourceError::IsCancellation() const {
  return error_code_ == net::ERR_ABORTED;
}

bool ResourceError::IsCacheMiss() const {
  return error_code_ == net::ERR_CACHE_MISS;
}

bool ResourceError::WasBlockedByResponse() const {
  return error_code_ == net::ERR_BLOCKED_BY_RESPONSE;
}

bool ResourceError::ShouldCollapseInitiator() const {
  return blocked_by_subresource_filter_ ||
         GetResourceRequestBlockedReason() ==
             ResourceRequestBlockedReason::kCollapsedByClient;
}

base::Optional<ResourceRequestBlockedReason>
ResourceError::GetResourceRequestBlockedReason() const {
  if (error_code_ != net::ERR_BLOCKED_BY_CLIENT &&
      error_code_ != net::ERR_BLOCKED_BY_RESPONSE) {
    return base::nullopt;
  }
  return static_cast<ResourceRequestBlockedReason>(extended_error_code_);
}

namespace {
String DescriptionForBlockedByClientOrResponse(int error, int extended_error) {
  if (extended_error == 0)
    return WebString::FromASCII(net::ErrorToString(error));
  std::string detail;
  switch (static_cast<ResourceRequestBlockedReason>(extended_error)) {
    case ResourceRequestBlockedReason::kOther:
      NOTREACHED();  // extended_error == 0, handled above
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
    case ResourceRequestBlockedReason::kCollapsedByClient:
      detail = "Collapsed";
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
    localized_description_ = DescriptionForBlockedByClientOrResponse(
        error_code_, extended_error_code_);
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
            << ", IsCacheMiss = " << error.IsCacheMiss();
}

}  // namespace blink
