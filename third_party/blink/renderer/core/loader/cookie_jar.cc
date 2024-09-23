// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/cookie_jar.h"

#include <cstdint>

#include "base/debug/dump_without_crashing.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/kurl_hash.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace {

enum class CookieCacheLookupResult {
  kCacheMissFirstAccess = 0,
  kCacheHitAfterGet = 1,
  kCacheHitAfterSet = 2,
  kCacheMissAfterGet = 3,
  kCacheMissAfterSet = 4,
  kMaxValue = kCacheMissAfterSet,
};

// Histogram for tracking first cookie requests.
constexpr char kFirstCookieRequestHistogram[] =
    "Blink.Experimental.Cookies.FirstCookieRequest";

// TODO(crbug.com/1276520): Remove after truncating characters are fully
// deprecated.
bool ContainsTruncatingChar(UChar c) {
  // equivalent to '\x00', '\x0D', or '\x0A'
  return c == '\0' || c == '\r' || c == '\n';
}

}  // namespace

CookieJar::CookieJar(blink::Document* document)
    : backend_(document->GetExecutionContext()), document_(document) {}

CookieJar::~CookieJar() = default;

void CookieJar::Trace(Visitor* visitor) const {
  visitor->Trace(backend_);
  visitor->Trace(document_);
}

void CookieJar::SetCookie(const String& value) {
  KURL cookie_url = document_->CookieURL();
  if (cookie_url.IsEmpty())
    return;

  base::ElapsedTimer timer;
  RequestRestrictedCookieManagerIfNeeded();
  backend_->SetCookieFromString(
      cookie_url, document_->SiteForCookies(), document_->TopFrameOrigin(),
      document_->GetExecutionContext()->GetStorageAccessApiStatus(), value);
  last_operation_was_set_ = true;
  base::UmaHistogramTimes("Blink.SetCookieTime", timer.Elapsed());
  if (is_first_operation_) {
    LogFirstCookieRequest(FirstCookieRequest::kFirstOperationWasSet);
  }

  // TODO(crbug.com/1276520): Remove after truncating characters are fully
  // deprecated
  if (value.Find(ContainsTruncatingChar) != kNotFound) {
    document_->CountDeprecation(WebFeature::kCookieWithTruncatingChar);
  }
}

void CookieJar::OnBackendDisconnect() {
  shared_memory_version_client_.reset();
  InvalidateCache();
}

String CookieJar::Cookies() {
  KURL cookie_url = document_->CookieURL();
  if (cookie_url.IsEmpty())
    return String();

  base::ElapsedTimer timer;
  RequestRestrictedCookieManagerIfNeeded();

  String value = g_empty_string;
  base::ReadOnlySharedMemoryRegion new_mapped_region;
  const bool get_version_shared_memory =
      !shared_memory_version_client_.has_value();

  // Store the latest cookie version to update |last_version_| after attempting
  // to get the string. Will get updated once more by GetCookiesString() if an
  // ipc is required.
  uint64_t new_version = last_version_;
  if (IPCNeeded()) {
    bool is_ad_tagged =
        document_->GetFrame() ? document_->GetFrame()->IsAdFrame() : false;

    if (!backend_->GetCookiesString(
            cookie_url, document_->SiteForCookies(),
            document_->TopFrameOrigin(),
            document_->GetExecutionContext()->GetStorageAccessApiStatus(),
            get_version_shared_memory, is_ad_tagged,
            /*force_disable_third_party_cookies=*/false, &new_version,
            &new_mapped_region, &value)) {
      // On IPC failure invalidate cached values and return empty string since
      // there is no guarantee the client can still validly access cookies in
      // the current context. See crbug.com/1468909.
      InvalidateCache();
      return g_empty_string;
    }
    last_cookies_ = value;
  }
  if (new_mapped_region.IsValid()) {
    shared_memory_version_client_.emplace(std::move(new_mapped_region));
  }
  base::UmaHistogramTimes("Blink.CookiesTime", timer.Elapsed());
  UpdateCacheAfterGetRequest(cookie_url, value, new_version);

  last_operation_was_set_ = false;
  if (is_first_operation_) {
    LogFirstCookieRequest(FirstCookieRequest::kFirstOperationWasGet);
  }
  return last_cookies_;
}

bool CookieJar::CookiesEnabled() {
  KURL cookie_url = document_->CookieURL();
  if (cookie_url.IsEmpty())
    return false;

  base::ElapsedTimer timer;
  RequestRestrictedCookieManagerIfNeeded();
  bool cookies_enabled = false;
  backend_->CookiesEnabledFor(
      cookie_url, document_->SiteForCookies(), document_->TopFrameOrigin(),
      document_->GetExecutionContext()->GetStorageAccessApiStatus(),
      &cookies_enabled);
  base::UmaHistogramTimes("Blink.CookiesEnabledTime", timer.Elapsed());
  if (is_first_operation_) {
    LogFirstCookieRequest(FirstCookieRequest::kFirstOperationWasCookiesEnabled);
  }
  return cookies_enabled;
}

void CookieJar::SetCookieManager(
    mojo::PendingRemote<network::mojom::blink::RestrictedCookieManager>
        cookie_manager) {
  backend_.reset();
  backend_.Bind(std::move(cookie_manager),
                document_->GetTaskRunner(TaskType::kInternalDefault));
}

void CookieJar::InvalidateCache() {
  last_cookies_hash_.reset();
  last_cookies_ = String();
  last_version_ = mojo::shared_memory_version::kInvalidVersion;
}

bool CookieJar::IPCNeeded() {
  // Not under the experiment, always use IPCs.
  if (!RuntimeEnabledFeatures::ReduceCookieIPCsEnabled()) {
    return true;
  }

  // |last_cookies_| can be null when converting the raw mojo payload failed.
  // (See ConvertUTF8ToUTF16() for details.) In that case use an IPC to request
  // another string to be safe.
  if (last_cookies_.IsNull()) {
    return true;
  }

  // No shared memory communication so IPC needed.
  if (!shared_memory_version_client_.has_value()) {
    return true;
  }

  // Cookie string has changed.
  if (shared_memory_version_client_->SharedVersionIsGreaterThan(
          last_version_)) {
    return true;
  }

  // No IPC needed!
  return false;
}

void CookieJar::RequestRestrictedCookieManagerIfNeeded() {
  if (!backend_.is_bound() || !backend_.is_connected()) {
    backend_.reset();

    // Either the backend was never bound or it became unbound. In case we're in
    // the unbound case perform the appropriate cleanup.
    OnBackendDisconnect();

    document_->GetFrame()->GetBrowserInterfaceBroker().GetInterface(
        backend_.BindNewPipeAndPassReceiver(
            document_->GetTaskRunner(TaskType::kInternalDefault)));
  }
}

void CookieJar::UpdateCacheAfterGetRequest(const KURL& cookie_url,
                                           const String& cookie_string,
                                           uint64_t new_version) {
  std::optional<unsigned> new_hash =
      WTF::HashInts(WTF::GetHash(cookie_url),
                    cookie_string.IsNull() ? 0 : WTF::GetHash(cookie_string));

  CookieCacheLookupResult result =
      CookieCacheLookupResult::kCacheMissFirstAccess;

  // An invalid version means no shared memory communication so assume changes
  // happened.
  const bool cookie_is_unchanged =
      new_version != mojo::shared_memory_version::kInvalidVersion &&
      last_version_ == new_version;

  if (last_cookies_hash_.has_value() && cookie_is_unchanged) {
    if (last_cookies_hash_ == new_hash) {
      result = last_operation_was_set_
                   ? CookieCacheLookupResult::kCacheHitAfterSet
                   : CookieCacheLookupResult::kCacheHitAfterGet;
    } else {
      result = last_operation_was_set_
                   ? CookieCacheLookupResult::kCacheMissAfterSet
                   : CookieCacheLookupResult::kCacheMissAfterGet;
    }
  }

  UMA_HISTOGRAM_ENUMERATION("Blink.Experimental.Cookies.CacheLookupResult2",
                            result);

  // Update the version to what it was before getting the string, ignoring any
  // changes that could have happened since then. This ensures as "stale" a
  // version as possible is used. This is the desired effect to avoid inhibiting
  // IPCs when not desired.
  last_version_ = new_version;
  last_cookies_hash_ = new_hash;
}

void CookieJar::LogFirstCookieRequest(FirstCookieRequest first_cookie_request) {
  is_first_operation_ = false;
  base::UmaHistogramEnumeration(kFirstCookieRequestHistogram,
                                first_cookie_request);
}

}  // namespace blink
