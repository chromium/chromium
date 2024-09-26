// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/safe_browsing/url_checker_delegate_impl.h"

#import "base/containers/contains.h"
#import "components/safe_browsing/core/browser/db/database_manager.h"
#import "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#import "components/safe_browsing/ios/browser/safe_browsing_url_allow_list.h"
#import "components/security_interstitials/core/unsafe_resource.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_client.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_query_manager.h"
#import "ios/components/security_interstitials/safe_browsing/unsafe_resource_util.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"

#pragma mark - UrlCheckerDelegateImpl

UrlCheckerDelegateImpl::UrlCheckerDelegateImpl(
    scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager,
    base::WeakPtr<SafeBrowsingClient> client)
    : database_manager_(std::move(database_manager)),
      client_(client),
      threat_types_(safe_browsing::CreateSBThreatTypeSet(
          {safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_MALWARE,
           safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_PHISHING,
           safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_UNWANTED,
           safe_browsing::SBThreatType::SB_THREAT_TYPE_BILLING})) {}

UrlCheckerDelegateImpl::~UrlCheckerDelegateImpl() = default;

void UrlCheckerDelegateImpl::MaybeDestroyNoStatePrefetchContents(
    base::OnceCallback<content::WebContents*()> web_contents_getter) {}

void UrlCheckerDelegateImpl::StartDisplayingBlockingPageHelper(
    const security_interstitials::UnsafeResource& resource,
    const std::string& method,
    const net::HttpRequestHeaders& headers,
    bool has_user_gesture) {
  // Helper function for managing a blocking page request for `resource`.  For
  // the committed interstitial flow, this function does not actually display
  // the blocking page.  Instead, it updates the allow list and stores a copy of
  // the unsafe resource before calling `resource`'s callback.  The blocking
  // page is displayed later when the do-not-proceed signal triggers an error
  // page.  Must be called on the UI thread.

  DCHECK_CURRENTLY_ON(web::WebThread::UI);

  // Send do-not-proceed signal if the WebState has been destroyed.
  web::WebState* web_state = resource.weak_web_state.get();
  if (!web_state) {
    resource.DispatchCallback(FROM_HERE, /*proceed=*/false,
                              /*showed_interstitial=*/false,
                              /*has_post_commit_interstitial_skipped=*/false);
    return;
  }

  if (client_ && client_->ShouldBlockUnsafeResource(resource)) {
    resource.DispatchCallback(FROM_HERE, /*proceed=*/false,
                              /*showed_interstitial=*/false,
                              /*has_post_commit_interstitial_skipped=*/false);
    return;
  }

  // Check if navigations to `resource`'s URL have already been allowed for the
  // given threat type.
  std::set<safe_browsing::SBThreatType> allowed_threats;
  const GURL url = resource.url;
  safe_browsing::SBThreatType threat_type = resource.threat_type;
  SafeBrowsingUrlAllowList* allow_list =
      SafeBrowsingUrlAllowList::FromWebState(web_state);
  if (allow_list->AreUnsafeNavigationsAllowed(url, &allowed_threats)) {
    if (base::Contains(allowed_threats, threat_type)) {
      resource.DispatchCallback(FROM_HERE, /*proceed=*/true,
                                /*showed_interstitial=*/false,
                                /*has_post_commit_interstitial_skipped=*/false);

      return;
    }
  }

  // Store the unsafe resource in the query manager.
  SafeBrowsingQueryManager::FromWebState(web_state)->StoreUnsafeResource(
      resource);

  // Send the do-not-proceed signal to cancel the navigation.  This will cause
  // the error page to be displayed using the stored UnsafeResource copy.
  resource.DispatchCallback(FROM_HERE, /*proceed=*/false,
                            /*showed_interstitial=*/true,
                            /*has_post_commit_interstitial_skipped=*/false);
}

void UrlCheckerDelegateImpl::
    StartObservingInteractionsForDelayedBlockingPageHelper(
        const security_interstitials::UnsafeResource& resource) {}

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
    base::optional_ref<const base::UnguessableToken> render_frame_token,
    bool originated_from_service_worker) {
  return false;
}

void UrlCheckerDelegateImpl::NotifySuspiciousSiteDetected(
    const base::RepeatingCallback<content::WebContents*()>&
        web_contents_getter) {
  // TODO(crbug.com/40817491): Implement reporting for suspicious sites.
}

void UrlCheckerDelegateImpl::SendUrlRealTimeAndHashRealTimeDiscrepancyReport(
    std::unique_ptr<safe_browsing::ClientSafeBrowsingReportRequest> report,
    const base::RepeatingCallback<content::WebContents*()>&
        web_contents_getter) {}

bool UrlCheckerDelegateImpl::AreBackgroundHashRealTimeSampleLookupsAllowed(
    const base::RepeatingCallback<content::WebContents*()>&
        web_contents_getter) {
  return false;
}

const safe_browsing::SBThreatTypeSet& UrlCheckerDelegateImpl::GetThreatTypes() {
  return threat_types_;
}

safe_browsing::SafeBrowsingDatabaseManager*
UrlCheckerDelegateImpl::GetDatabaseManager() {
  return database_manager_.get();
}

safe_browsing::BaseUIManager* UrlCheckerDelegateImpl::GetUIManager() {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}
