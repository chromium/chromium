// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/net/ios_chrome_network_delegate.h"

#include <stdlib.h>

#include "base/base_paths.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/debug/stack_trace.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/cookies/cookie_options.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request.h"

namespace {

const char kDNTHeader[] = "DNT";

void ReportInvalidReferrerSend(const GURL& target_url,
                               const GURL& referrer_url) {
  LOG(ERROR) << "Cancelling request to " << target_url
             << " with invalid referrer " << referrer_url;
  // Record information to help debug http://crbug.com/422871
  if (!target_url.SchemeIsHTTPOrHTTPS())
    return;
  base::debug::DumpWithoutCrashing();
  NOTREACHED();
}

}  // namespace

IOSChromeNetworkDelegate::IOSChromeNetworkDelegate()
    : enable_do_not_track_(nullptr) {}

IOSChromeNetworkDelegate::~IOSChromeNetworkDelegate() {}

// static
void IOSChromeNetworkDelegate::InitializePrefsOnUIThread(
    BooleanPrefMember* enable_do_not_track,
    PrefService* pref_service) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  if (enable_do_not_track) {
    enable_do_not_track->Init(prefs::kEnableDoNotTrack, pref_service);
    enable_do_not_track->MoveToSequence(
        base::CreateSingleThreadTaskRunner({web::WebThread::IO}));
  }
}

int IOSChromeNetworkDelegate::OnBeforeURLRequest(
    net::URLRequest* request,
    net::CompletionOnceCallback callback,
    GURL* new_url) {
  if (enable_do_not_track_ && enable_do_not_track_->GetValue())
    request->SetExtraRequestHeaderByName(kDNTHeader, "1", true /* override */);
  return net::OK;
}

bool IOSChromeNetworkDelegate::OnCanGetCookies(
    const net::URLRequest& request,
    const net::CookieList& cookie_list,
    bool allowed_from_caller) {
  // Null during tests, or when we're running in the system context.
  if (!cookie_settings_)
    return allowed_from_caller;

  return allowed_from_caller && cookie_settings_->IsCookieAccessAllowed(
                                    request.url(), request.site_for_cookies());
}

bool IOSChromeNetworkDelegate::OnCanSetCookie(
    const net::URLRequest& request,
    const net::CanonicalCookie& cookie,
    net::CookieOptions* options,
    bool allowed_from_caller) {
  // Null during tests, or when we're running in the system context.
  if (!cookie_settings_)
    return allowed_from_caller;

  return allowed_from_caller && cookie_settings_->IsCookieAccessAllowed(
                                    request.url(), request.site_for_cookies());
}

bool IOSChromeNetworkDelegate::OnForcePrivacyMode(
    const GURL& url,
    const GURL& site_for_cookies,
    const base::Optional<url::Origin>& top_frame_origin) const {
  // Null during tests, or when we're running in the system context.
  if (!cookie_settings_.get())
    return false;

  return !cookie_settings_->IsCookieAccessAllowed(url, site_for_cookies,
                                                  top_frame_origin);
}

bool IOSChromeNetworkDelegate::
    OnCancelURLRequestWithPolicyViolatingReferrerHeader(
        const net::URLRequest& request,
        const GURL& target_url,
        const GURL& referrer_url) const {
  ReportInvalidReferrerSend(target_url, referrer_url);
  return true;
}
