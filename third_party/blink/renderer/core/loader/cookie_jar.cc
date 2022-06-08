// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/cookie_jar.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "net/base/features.h"
#include "net/cookies/parsed_cookie.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/origin_trials/trial_token.h"
#include "third_party/blink/public/common/origin_trials/trial_token_result.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/kurl_hash.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

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

void LogCookieHistogram(const char* prefix,
                        bool cookie_manager_requested,
                        base::TimeDelta elapsed) {
  base::UmaHistogramTimes(
      base::StrCat({prefix, cookie_manager_requested ? "ManagerRequested"
                                                     : "ManagerAvailable"}),
      elapsed);
}

// TODO(crbug.com/1276520): Remove after truncating characters are fully
// deprecated.
bool ContainsTruncatingChar(UChar c) {
  // equivalent to '\x00', '\x0D', or '\x0A'
  return c == '\0' || c == '\r' || c == '\n';
}

bool ValidPartitionedCookiesOriginTrial(const ResourceResponse& response) {
  // This should never be called if partitioned cookies are disabled.
  DCHECK(base::FeatureList::IsEnabled(net::features::kPartitionedCookies));

  if (!response.HttpHeaderFields().Contains("origin-trial"))
    return false;

  blink::TrialTokenValidator validator;
  base::Time now(base::Time::Now());

  GURL url(response.ResponseUrl());
  if (!validator.IsTrialPossibleOnOrigin(url))
    return false;

  url::Origin origin = url::Origin::Create(url);
  url::Origin third_party_origins[] = {origin};
  StringUTF8Adaptor token_adaptor(response.HttpHeaderField("origin-trial"));
  TrialTokenResult result = validator.ValidateToken(
      token_adaptor.AsStringPiece(), origin, third_party_origins, now);

  return result.Status() == blink::OriginTrialTokenStatus::kSuccess &&
         result.ParsedToken()->feature_name() == "PartitionedCookies";
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
  bool requested = RequestRestrictedCookieManagerIfNeeded();
  backend_->SetCookieFromString(
      cookie_url, document_->SiteForCookies(), document_->TopFrameOrigin(),
      value,
      RuntimeEnabledFeatures::PartitionedCookiesEnabled(
          document_->GetExecutionContext()));
  last_operation_was_set_ = true;
  LogCookieHistogram("Blink.SetCookieTime.", requested, timer.Elapsed());

  // TODO(crbug.com/1276520): Remove after truncating characters are fully
  // deprecated
  if (value.Find(ContainsTruncatingChar) != kNotFound) {
    document_->CountDeprecation(WebFeature::kCookieWithTruncatingChar);
  }
}

String CookieJar::Cookies() {
  KURL cookie_url = document_->CookieURL();
  if (cookie_url.IsEmpty())
    return String();

  base::ElapsedTimer timer;
  bool requested = RequestRestrictedCookieManagerIfNeeded();
  String value;
  backend_->GetCookiesString(cookie_url, document_->SiteForCookies(),
                             document_->TopFrameOrigin(),
                             RuntimeEnabledFeatures::PartitionedCookiesEnabled(
                                 document_->GetExecutionContext()),
                             &value);
  LogCookieHistogram("Blink.CookiesTime.", requested, timer.Elapsed());
  UpdateCacheAfterGetRequest(cookie_url, value);

  last_operation_was_set_ = false;
  return value;
}

bool CookieJar::CookiesEnabled() {
  KURL cookie_url = document_->CookieURL();
  if (cookie_url.IsEmpty())
    return false;

  base::ElapsedTimer timer;
  bool requested = RequestRestrictedCookieManagerIfNeeded();
  bool cookies_enabled = false;
  backend_->CookiesEnabledFor(cookie_url, document_->SiteForCookies(),
                              document_->TopFrameOrigin(), &cookies_enabled);
  LogCookieHistogram("Blink.CookiesEnabledTime.", requested, timer.Elapsed());
  return cookies_enabled;
}

void CookieJar::SetCookieManager(
    mojo::PendingRemote<network::mojom::blink::RestrictedCookieManager>
        cookie_manager) {
  backend_.reset();
  backend_.Bind(std::move(cookie_manager),
                document_->GetTaskRunner(TaskType::kInternalDefault));
}

bool CookieJar::RequestRestrictedCookieManagerIfNeeded() {
  if (!backend_.is_bound() || !backend_.is_connected()) {
    backend_.reset();
    document_->GetFrame()->GetBrowserInterfaceBroker().GetInterface(
        backend_.BindNewPipeAndPassReceiver(
            document_->GetTaskRunner(TaskType::kInternalDefault)));
    return true;
  }
  return false;
}

void CookieJar::CheckPartitionedCookiesOriginTrial(
    const ResourceResponse& response) {
  if (!response.HasPartitionedCookie() ||
      !base::FeatureList::IsEnabled(net::features::kPartitionedCookies)) {
    return;
  }
  if (!ValidPartitionedCookiesOriginTrial(response)) {
    base::ElapsedTimer timer;
    bool requested = RequestRestrictedCookieManagerIfNeeded();
    LogCookieHistogram("Blink.CookiesEnabledTime.", requested,
                        timer.Elapsed());
    backend_->ConvertPartitionedCookiesToUnpartitioned(response.ResponseUrl());
  }
}

void CookieJar::UpdateCacheAfterGetRequest(const KURL& cookie_url,
                                           const String& cookie_string) {
  absl::optional<unsigned> new_hash = WTF::HashInts(
      KURLHash::GetHash(cookie_url),
      cookie_string.IsNull() ? 0 : StringHash::GetHash(cookie_string));

  CookieCacheLookupResult result =
      CookieCacheLookupResult::kCacheMissFirstAccess;

  if (last_cookies_hash_.has_value()) {
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

  last_cookies_hash_ = new_hash;
}

}  // namespace blink
