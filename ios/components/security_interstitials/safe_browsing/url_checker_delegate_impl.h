// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_URL_CHECKER_DELEGATE_IMPL_H_
#define IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_URL_CHECKER_DELEGATE_IMPL_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/safe_browsing/core/browser/url_checker_delegate.h"

class SafeBrowsingClient;

namespace safe_browsing {
class SafeBrowsingDatabaseManager;
}  // namespace safe_browsing

// This is an iOS implementation of the delegate interface for
// SafeBrowsingUrlCheckerImpl.
class UrlCheckerDelegateImpl : public safe_browsing::UrlCheckerDelegate {
 public:
  UrlCheckerDelegateImpl(
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
          database_manager,
      base::WeakPtr<SafeBrowsingClient> client);

  UrlCheckerDelegateImpl(const UrlCheckerDelegateImpl&) = delete;
  UrlCheckerDelegateImpl& operator=(const UrlCheckerDelegateImpl&) = delete;

 private:
  ~UrlCheckerDelegateImpl() override;

  // safe_browsing::UrlCheckerDelegate implementation
  void MaybeDestroyNoStatePrefetchContents(
      base::OnceCallback<content::WebContents*()> web_contents_getter) override;
  void StartDisplayingBlockingPageHelper(
      const security_interstitials::UnsafeResource& resource,
      const std::string& method,
      const net::HttpRequestHeaders& headers,
      bool has_user_gesture) override;
  void StartObservingInteractionsForDelayedBlockingPageHelper(
      const security_interstitials::UnsafeResource& resource) override;
  bool IsUrlAllowlisted(const GURL& url) override;
  void SetPolicyAllowlistDomains(
      const std::vector<std::string>& allowlist_domains) override;
  bool ShouldSkipRequestCheck(
      const GURL& original_url,
      int frame_tree_node_id,
      int render_process_id,
      base::optional_ref<const base::UnguessableToken> render_frame_token,
      bool originated_from_service_worker) override;

#pragma mark - Unused on iOS
  // This function is unused on iOS, since iOS cannot use content/.
  // TODO(crbug.com/40683815): Refactor SafeBrowsingUrlCheckerImpl and
  // UrlCheckerDelegate to extract the functionality that can be shared across
  // platforms, and move methods used only by content/ to classes used only by
  // content/.
  void NotifySuspiciousSiteDetected(
      const base::RepeatingCallback<content::WebContents*()>&
          web_contents_getter) override;
  void SendUrlRealTimeAndHashRealTimeDiscrepancyReport(
      std::unique_ptr<safe_browsing::ClientSafeBrowsingReportRequest> report,
      const base::RepeatingCallback<content::WebContents*()>&
          web_contents_getter) override;
#pragma mark - UrlCheckerDelegate
  const safe_browsing::SBThreatTypeSet& GetThreatTypes() override;
  safe_browsing::SafeBrowsingDatabaseManager* GetDatabaseManager() override;
  safe_browsing::BaseUIManager* GetUIManager() override;
  bool AreBackgroundHashRealTimeSampleLookupsAllowed(
      const base::RepeatingCallback<content::WebContents*()>&
          web_contents_getter) override;

  scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager_;
  base::WeakPtr<SafeBrowsingClient> client_;
  safe_browsing::SBThreatTypeSet threat_types_;
};

#endif  // IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_URL_CHECKER_DELEGATE_IMPL_H_
