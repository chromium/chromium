// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_SUSPICIOUS_SITE_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_SUSPICIOUS_SITE_TAB_HELPER_H_

#import "base/memory/raw_ptr.h"
#import "base/memory/scoped_refptr.h"
#import "base/scoped_observation.h"
#import "components/safe_browsing/core/browser/suspicious_site_warning_allowlist.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_query_manager.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"
#import "url/gurl.h"

class HostContentSettingsMap;
@protocol SuspiciousSiteTabHelperDelegate;

namespace security_interstitials {
struct UnsafeResource;
}  // namespace security_interstitials

// Tab helper that manages the lifecycle, deferral, local allowlist checks, and
// metrics for post-commit Suspicious Site Warnings on a `web::WebState`.
class SuspiciousSiteTabHelper
    : public web::WebStateObserver,
      public SafeBrowsingQueryManager::Observer,
      public web::WebStateUserData<SuspiciousSiteTabHelper> {
 public:
  ~SuspiciousSiteTabHelper() override;

  SuspiciousSiteTabHelper(const SuspiciousSiteTabHelper&) = delete;
  SuspiciousSiteTabHelper& operator=(const SuspiciousSiteTabHelper&) = delete;

  // Sets the UI delegate responsible for presenting and dismissing the warning
  // alert. Setting a non-nil delegate attempts to present any pending warning
  // that is ready to be shown.
  void SetDelegate(id<SuspiciousSiteTabHelperDelegate> delegate);

  // Called when a main-frame Safe Browsing check returns
  // `SB_THREAT_TYPE_WARNABLE_SUSPICIOUS_SITE` for `resource`.
  void OnSuspiciousSiteWarningTriggered(
      const security_interstitials::UnsafeResource& resource);

  // Called when the user selects the primary "Back to safety" action on the
  // warning alert. Navigates back to the most recent safe item whose host
  // differs from the committed suspicious host, or loads the New Tab Page if no
  // safe back item exists.
  void OnBackToSafetyClicked();

  // Called when the user selects the secondary "Mark as safe" action on the
  // warning alert. Adds the committed host to the 30-day local allowlist.
  void OnMarkAsSafeClicked();

  // Called when the warning alert is dismissed externally by the system without
  // the user selecting either action button.
  void OnWarningDismissedBySystem();

 private:
  friend class web::WebStateUserData<SuspiciousSiteTabHelper>;

  SuspiciousSiteTabHelper(
      web::WebState* web_state,
      scoped_refptr<HostContentSettingsMap> host_content_settings_map);

  // Evaluates whether all preconditions are satisfied (navigation committed,
  // no pending main-frame Safe Browsing checks, not superseded by a blocking
  // interstitial, host not allowlisted, tab visible, and delegate attached)
  // and instructs `delegate_` to present the warning if so.
  void MaybeShowWarning();

  // Resets all pending and active warning state.
  void ResetWarningState();

  // web::WebStateObserver implementation.
  void WasShown(web::WebState* web_state) override;
  void WasHidden(web::WebState* web_state) override;
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // SafeBrowsingQueryManager::Observer implementation.
  void SafeBrowsingSyncQueryFinished(
      const SafeBrowsingQueryManager::QueryData& query_data) override;
  void SafeBrowsingAsyncQueryFinished(
      const SafeBrowsingQueryManager::QueryData& query_data) override;
  void SafeBrowsingQueryManagerDestroyed(
      SafeBrowsingQueryManager* manager) override;

  raw_ptr<web::WebState> web_state_ = nullptr;
  safe_browsing::SuspiciousSiteWarningAllowlist allowlist_;
  __weak id<SuspiciousSiteTabHelperDelegate> delegate_ = nil;

  // The URL from `UnsafeResource` that triggered the pending or active warning.
  GURL pending_suspicious_url_;
  // Whether the current main-frame navigation has committed without error.
  bool has_committed_navigation_ = false;
  // Whether the warning alert has been presented at least once for the current
  // warning session.
  bool has_shown_warning_ = false;
  // Whether the warning alert is currently presented by `delegate_`.
  bool is_warning_visible_ = false;

  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};
  base::ScopedObservation<SafeBrowsingQueryManager,
                          SafeBrowsingQueryManager::Observer>
      query_manager_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_SUSPICIOUS_SITE_TAB_HELPER_H_
