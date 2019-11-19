// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/cookie_jar.h"

#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

CookieJar::CookieJar(blink::Document* document) : document_(document) {}

CookieJar::~CookieJar() = default;

void CookieJar::SetCookie(const String& value) {
  KURL cookie_url = document_->CookieURL();
  if (cookie_url.IsEmpty())
    return;

  RequestRestrictedCookieManagerIfNeeded();
  SCOPED_BLINK_UMA_HISTOGRAM_TIMER("Blink.CookieJar.SyncCookiesSetTime");
  backend_->SetCookieFromString(cookie_url, document_->SiteForCookies(),
                                document_->TopFrameOrigin(), value);
}

String CookieJar::Cookies() {
  KURL cookie_url = document_->CookieURL();
  if (cookie_url.IsEmpty())
    return String();

  SCOPED_BLINK_UMA_HISTOGRAM_TIMER("Blink.CookieJar.SyncCookiesTime");
  RequestRestrictedCookieManagerIfNeeded();
  String value;
  backend_->GetCookiesString(cookie_url, document_->SiteForCookies(),
                             document_->TopFrameOrigin(), &value);
  return value;
}

bool CookieJar::CookiesEnabled() {
  KURL cookie_url = document_->CookieURL();
  if (cookie_url.IsEmpty())
    return false;

  RequestRestrictedCookieManagerIfNeeded();
  bool cookies_enabled = false;
  backend_->CookiesEnabledFor(cookie_url, document_->SiteForCookies(),
                              document_->TopFrameOrigin(), &cookies_enabled);
  return cookies_enabled;
}

void CookieJar::RequestRestrictedCookieManagerIfNeeded() {
  if (!backend_.is_bound() || !backend_.is_connected()) {
    backend_.reset();
    document_->GetInterfaceProvider()->GetInterface(
        backend_.BindNewPipeAndPassReceiver());
  }
}

}  // namespace blink
