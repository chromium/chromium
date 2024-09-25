// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/rlz/rlz_tracker_delegate_impl.h"

#import "base/check.h"
#import "base/command_line.h"
#import "base/functional/bind.h"
#import "base/notreached.h"
#import "components/omnibox/browser/omnibox_event_global_tracker.h"
#import "components/omnibox/browser/omnibox_log.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/google/model/google_brand.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/public/provider/chrome/browser/app_distribution/app_distribution_api.h"
#import "ios/web/public/thread/web_thread.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

RLZTrackerDelegateImpl::RLZTrackerDelegateImpl() {}

RLZTrackerDelegateImpl::~RLZTrackerDelegateImpl() {}

// static
bool RLZTrackerDelegateImpl::IsGoogleDefaultSearch(ProfileIOS* profile) {
  bool is_google_default_search = false;
  TemplateURLService* template_url_service =
      ios::TemplateURLServiceFactory::GetForProfile(profile);
  if (template_url_service) {
    const TemplateURL* url_template =
        template_url_service->GetDefaultSearchProvider();
    is_google_default_search =
        url_template && url_template->url_ref().HasGoogleBaseURLs(
                            template_url_service->search_terms_data());
  }
  return is_google_default_search;
}

// static
bool RLZTrackerDelegateImpl::IsGoogleHomepage(ProfileIOS* profile) {
  // iOS does not have a notion of home page.
  return false;
}

// static
bool RLZTrackerDelegateImpl::IsGoogleInStartpages(ProfileIOS* profile) {
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
  brand->assign(ios::provider::GetBrandCode());
  return true;
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

bool RLZTrackerDelegateImpl::GetLanguage(std::u16string* language) {
  // TODO(crbug.com/40816693): Implement.
  NOTIMPLEMENTED();
  return false;
}

bool RLZTrackerDelegateImpl::GetReferral(std::u16string* referral) {
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
    base::OnceClosure callback) {
  DCHECK(!callback.is_null());
  on_omnibox_search_callback_ = std::move(callback);
  on_omnibox_url_opened_subscription_ =
      OmniboxEventGlobalTracker::GetInstance()->RegisterCallback(
          base::BindRepeating(&RLZTrackerDelegateImpl::OnURLOpenedFromOmnibox,
                              base::Unretained(this)));
}

void RLZTrackerDelegateImpl::SetHomepageSearchCallback(
    base::OnceClosure callback) {
  NOTREACHED_IN_MIGRATION();
}

void RLZTrackerDelegateImpl::RunHomepageSearchCallback() {
  NOTREACHED_IN_MIGRATION();
}

bool RLZTrackerDelegateImpl::ShouldUpdateExistingAccessPointRlz() {
  NOTREACHED_IN_MIGRATION();
  return false;
}

void RLZTrackerDelegateImpl::OnURLOpenedFromOmnibox(OmniboxLog* log) {
  // In M-36, we made NOTIFICATION_OMNIBOX_OPENED_URL fire more often than
  // it did previously.  The RLZ folks want RLZ's "first search" detection
  // to remain as unaffected as possible by this change.  This test is
  // there to keep the old behavior.
  if (!log->is_popup_open)
    return;

  on_omnibox_url_opened_subscription_ = {};

  if (!on_omnibox_search_callback_.is_null())
    std::move(on_omnibox_search_callback_).Run();
}
