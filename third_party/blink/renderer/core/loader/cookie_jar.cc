// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/cookie_jar.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {
namespace {

void LogCookieHistogram(const char* prefix,
                        bool cookie_manager_requested,
                        base::TimeDelta elapsed) {
  base::UmaHistogramTimes(
      base::StrCat({prefix, cookie_manager_requested ? "ManagerRequested"
                                                     : "ManagerAvailable"}),
      elapsed);
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
  backend_->SetCookieFromString(cookie_url, document_->SiteForCookies(),
                                document_->TopFrameOrigin(), value);
  LogCookieHistogram("Blink.SetCookieTime.", requested, timer.Elapsed());
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

}  // namespace blink
