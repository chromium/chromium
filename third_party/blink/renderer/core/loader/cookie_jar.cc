// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/cookie_jar.h"

#include "base/debug/dump_without_crashing.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
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
  bool site_for_cookies_ok = true;
  bool top_frame_origin_ok = true;
  backend_->SetCookieFromString(cookie_url, document_->SiteForCookies(),
                                document_->TopFrameOrigin(), value,
                                &site_for_cookies_ok, &top_frame_origin_ok);
  last_operation_was_set_ = true;
  LogCookieHistogram("Blink.SetCookieTime.", requested, timer.Elapsed());

  // TODO(crbug.com/1276520): Remove after truncating characters are fully
  // deprecated
  if (value.Find(ContainsTruncatingChar) != kNotFound) {
    document_->CountDeprecation(WebFeature::kCookieWithTruncatingChar);
  }

  static bool reported = false;
  if (!site_for_cookies_ok) {
    if (!reported) {
      reported = true;
      SCOPED_CRASH_KEY_STRING256("RCM", "document-site_for_cookies",
                                 document_->SiteForCookies().ToDebugString());
      SCOPED_CRASH_KEY_STRING256(
          "RCM", "document-top_frame_origin",
          document_->TopFrameOrigin()->ToUrlOrigin().GetDebugString());
      // Only origin here, since url is probably way too sensitive.
      SCOPED_CRASH_KEY_STRING256(
          "RCM", "document-origin",
          url::Origin::Create(GURL(cookie_url)).GetDebugString());
      base::debug::DumpWithoutCrashing();
    }
  }
  if (!top_frame_origin_ok) {
    if (!reported) {
      reported = true;
      SCOPED_CRASH_KEY_STRING256("RCM", "document-site_for_cookies",
                                 document_->SiteForCookies().ToDebugString());
      SCOPED_CRASH_KEY_STRING256(
          "RCM", "document-top_frame_origin",
          document_->TopFrameOrigin()->ToUrlOrigin().GetDebugString());
      // Only origin here, since url is probably way too sensitive.
      SCOPED_CRASH_KEY_STRING256(
          "RCM", "document-origin",
          url::Origin::Create(GURL(cookie_url)).GetDebugString());
      base::debug::DumpWithoutCrashing();
    }
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
                             document_->TopFrameOrigin(), &value);
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

void CookieJar::UpdateCacheAfterGetRequest(const KURL& cookie_url,
                                           const String& cookie_string) {
  absl::optional<unsigned> new_hash =
      WTF::HashInts(WTF::GetHash(cookie_url),
                    cookie_string.IsNull() ? 0 : WTF::GetHash(cookie_string));

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
