// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/safe_browsing/url_checker_delegate_impl.h"

#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#import "components/safe_browsing/ios/browser/safe_browsing_url_allow_list.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/prerender/prerender_service.h"
#import "ios/chrome/browser/prerender/prerender_service_factory.h"
#import "ios/chrome/browser/safe_browsing/safe_browsing_query_manager.h"
#import "ios/chrome/browser/safe_browsing/unsafe_resource_util.h"
#include "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Helper function for managing a blocking page request for |resource|.  For the
// committed interstitial flow, this function does not actually display the
// blocking page.  Instead, it updates the allow list and stores a copy of the
// unsafe resource before calling |resource|'s callback.  The blocking page is
// displayed later when the do-not-proceed signal triggers an error page.  Must
// be called on the UI thread.
void HandleBlockingPageRequestOnUIThread(
    const security_interstitials::UnsafeResource resource) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);

  // Send do-not-proceed signal if the WebState has been destroyed.
  web::WebState* web_state = resource.weak_web_state.get();
  if (!web_state) {
    RunUnsafeResourceCallback(resource, /*proceed=*/false,
                              /*showed_interstitial=*/false);
    return;
  }

  // Send do-not-proceed signal if the WebState is for a prerender tab.
  PrerenderService* prerender_service =
      PrerenderServiceFactory::GetForBrowserState(
          ChromeBrowserState::FromBrowserState(web_state->GetBrowserState()));
  if (prerender_service &&
      prerender_service->IsWebStatePrerendered(web_state)) {
    RunUnsafeResourceCallback(resource, /*proceed=*/false,
                              /*showed_interstitial=*/false);
    return;
  }

  // Check if navigations to |resource|'s URL have already been allowed for the
  // given threat type.
  std::set<safe_browsing::SBThreatType> allowed_threats;
  const GURL url = resource.url;
  safe_browsing::SBThreatType threat_type = resource.threat_type;
  SafeBrowsingUrlAllowList* allow_list =
      SafeBrowsingUrlAllowList::FromWebState(web_state);
  if (allow_list->AreUnsafeNavigationsAllowed(url, &allowed_threats)) {
    if (allowed_threats.find(threat_type) != allowed_threats.end()) {
      RunUnsafeResourceCallback(resource, /*proceed=*/true,
                                /*showed_interstitial=*/false);
      return;
    }
  }

  // Store the unsafe resource in the query manager.
  SafeBrowsingQueryManager::FromWebState(web_state)->StoreUnsafeResource(
      resource);

  // Send the do-not-proceed signal to cancel the navigation.  This will cause
  // the error page to be displayed using the stored UnsafeResource copy.
  RunUnsafeResourceCallback(resource, /*proceed=*/false,
                            /*showed_interstitial=*/true);
}
}  // namespace

#pragma mark - UrlCheckerDelegateImpl

UrlCheckerDelegateImpl::UrlCheckerDelegateImpl(
    scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager)
    : database_manager_(std::move(database_manager)),
      threat_types_(safe_browsing::CreateSBThreatTypeSet(
          {safe_browsing::SB_THREAT_TYPE_URL_MALWARE,
           safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
           safe_browsing::SB_THREAT_TYPE_URL_UNWANTED,
           safe_browsing::SB_THREAT_TYPE_BILLING})) {}

UrlCheckerDelegateImpl::~UrlCheckerDelegateImpl() = default;

void UrlCheckerDelegateImpl::MaybeDestroyNoStatePrefetchContents(
    base::OnceCallback<content::WebContents*()> web_contents_getter) {}

void UrlCheckerDelegateImpl::StartDisplayingBlockingPageHelper(
    const security_interstitials::UnsafeResource& resource,
    const std::string& method,
    const net::HttpRequestHeaders& headers,
    bool is_main_frame,
    bool has_user_gesture) {
  // Query the allow list on the UI thread to determine whether the navigation
  // can proceed.
  web::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&HandleBlockingPageRequestOnUIThread, resource));
}

void UrlCheckerDelegateImpl::
    StartObservingInteractionsForDelayedBlockingPageHelper(
        const security_interstitials::UnsafeResource& resource,
        bool is_main_frame) {}

bool UrlCheckerDelegateImpl::IsUrlAllowlisted(const GURL& url) {
  return false;
}

void UrlCheckerDelegateImpl::SetPolicyAllowlistDomains(
    const std::vector<std::string>& allowlist_domains) {
  // The SafeBrowsingAllowlistDomains policy is not supported on iOS.
}

bool UrlCheckerDelegateImpl::ShouldSkipRequestCheck(
    const GURL& original_url,
    int frame_tree_node_id,
    int render_process_id,
    int render_frame_id,
    bool originated_from_service_worker) {
  return false;
}

void UrlCheckerDelegateImpl::NotifySuspiciousSiteDetected(
    const base::RepeatingCallback<content::WebContents*()>&
        web_contents_getter) {
  NOTREACHED();
}

const safe_browsing::SBThreatTypeSet& UrlCheckerDelegateImpl::GetThreatTypes() {
  return threat_types_;
}

safe_browsing::SafeBrowsingDatabaseManager*
UrlCheckerDelegateImpl::GetDatabaseManager() {
  return database_manager_.get();
}

safe_browsing::BaseUIManager* UrlCheckerDelegateImpl::GetUIManager() {
  NOTREACHED();
  return nullptr;
}
