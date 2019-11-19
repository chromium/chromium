// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/rlz/rlz_tracker_delegate_impl.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "components/omnibox/browser/omnibox_event_global_tracker.h"
#include "components/omnibox/browser/omnibox_log.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/google/google_brand.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#include "ios/web/public/thread/web_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

RLZTrackerDelegateImpl::RLZTrackerDelegateImpl() {}

RLZTrackerDelegateImpl::~RLZTrackerDelegateImpl() {}

// static
bool RLZTrackerDelegateImpl::IsGoogleDefaultSearch(
    ios::ChromeBrowserState* browser_state) {
  bool is_google_default_search = false;
  TemplateURLService* template_url_service =
      ios::TemplateURLServiceFactory::GetForBrowserState(browser_state);
  if (template_url_service) {
    const TemplateURL* url_template =
        template_url_service->GetDefaultSearchProvider();
    is_google_default_search = url_template &&
                               url_template->url_ref().HasGoogleBaseURLs(
                                   template_url_service->search_terms_data());
  }
  return is_google_default_search;
}

// static
bool RLZTrackerDelegateImpl::IsGoogleHomepage(
    ios::ChromeBrowserState* browser_state) {
  // iOS does not have a notion of home page.
  return false;
}

// static
bool RLZTrackerDelegateImpl::IsGoogleInStartpages(
    ios::ChromeBrowserState* browser_state) {
  // iOS does not have a notion of start pages.
  return false;
}

void RLZTrackerDelegateImpl::Cleanup() {
  on_omnibox_search_callback_.Reset();
}

bool RLZTrackerDelegateImpl::IsOnUIThread() {
  return web::WebThread::CurrentlyOn(web::WebThread::UI);
}

scoped_refptr<network::SharedURLLoaderFactory>
RLZTrackerDelegateImpl::GetURLLoaderFactory() {
  return GetApplicationContext()->GetSharedURLLoaderFactory();
}

bool RLZTrackerDelegateImpl::GetBrand(std::string* brand) {
  return ios::google_brand::GetBrand(brand);
}

bool RLZTrackerDelegateImpl::IsBrandOrganic(const std::string& brand) {
  return brand.empty() || ios::google_brand::IsOrganic(brand);
}

bool RLZTrackerDelegateImpl::GetReactivationBrand(std::string* brand) {
  // iOS does not have reactivation brand.
  return false;
}

bool RLZTrackerDelegateImpl::ShouldEnableZeroDelayForTesting() {
  return false;
}

bool RLZTrackerDelegateImpl::GetLanguage(base::string16* language) {
  // TODO(thakis): Implement.
  NOTIMPLEMENTED();
  return false;
}

bool RLZTrackerDelegateImpl::GetReferral(base::string16* referral) {
  // The referral program is defunct and not used. No need to implement this
  // function on non-Win platforms.
  return true;
}

bool RLZTrackerDelegateImpl::ClearReferral() {
  // The referral program is defunct and not used. No need to implement this
  // function on non-Win platforms.
  return true;
}

void RLZTrackerDelegateImpl::SetOmniboxSearchCallback(
    const base::Closure& callback) {
  DCHECK(!callback.is_null());
  on_omnibox_search_callback_ = callback;
  on_omnibox_url_opened_subscription_ =
      OmniboxEventGlobalTracker::GetInstance()->RegisterCallback(
          base::Bind(&RLZTrackerDelegateImpl::OnURLOpenedFromOmnibox,
                     base::Unretained(this)));
}

void RLZTrackerDelegateImpl::SetHomepageSearchCallback(
    const base::Closure& callback) {
  NOTREACHED();
}

bool RLZTrackerDelegateImpl::ShouldUpdateExistingAccessPointRlz() {
  NOTREACHED();
  return false;
}

void RLZTrackerDelegateImpl::OnURLOpenedFromOmnibox(OmniboxLog* log) {
  // In M-36, we made NOTIFICATION_OMNIBOX_OPENED_URL fire more often than
  // it did previously.  The RLZ folks want RLZ's "first search" detection
  // to remain as unaffected as possible by this change.  This test is
  // there to keep the old behavior.
  if (!log->is_popup_open)
    return;

  on_omnibox_url_opened_subscription_.reset();

  using std::swap;
  base::Closure callback_to_run;
  swap(callback_to_run, on_omnibox_search_callback_);
  if (!callback_to_run.is_null())
    callback_to_run.Run();
}
