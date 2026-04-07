// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_MODEL_DATA_PROTECTION_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_MODEL_DATA_PROTECTION_TAB_HELPER_H_

#include <optional>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/observer_list.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

class DataProtectionTabHelperObserver;
class ProfileIOS;

namespace web {
class NavigationContext;
class WebState;
}  // namespace web

namespace data_controls {
class IOSRulesService;
}

namespace enterprise_data_protection {
class DataProtectionUrlLookupService;
}

namespace safe_browsing {
class RealTimeUrlLookupServiceBase;
class RTLookupResponse;
}  // namespace safe_browsing

// Tab helper that manages data protection enterprise policies for a WebState.
// It checks both Data Controls and WebProtect real-time URL lookups
// to determine if screenshot protection should be enabled.
//
// The helper monitors navigations and redirects. It performs local policy
// checks via Data Controls and, for non-incognito profiles, remote real-time
// URL lookups. If any check indicates that the content should be protected,
// screenshot protection is enabled for the WebState.
class DataProtectionTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<DataProtectionTabHelper> {
 public:
  DataProtectionTabHelper(const DataProtectionTabHelper&) = delete;
  DataProtectionTabHelper& operator=(const DataProtectionTabHelper&) = delete;

  ~DataProtectionTabHelper() override;

  // Adds/removes observers to listen for data protection state changes.
  void AddObserver(DataProtectionTabHelperObserver* observer);
  void RemoveObserver(DataProtectionTabHelperObserver* observer);

  // Returns the current screenshot protection state for the tab.
  // This state is based on the currently committed navigation.
  bool IsScreenshotProtectionEnabled() const {
    return committed_navigation_.screenshot_protection_enabled;
  }

  // web::WebStateObserver:
  // Notifies that a navigation has started.
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;

  // Notifies that a navigation has redirected. Re-evaluates policy checks
  // for the new URL if protection is not already latched.
  void DidRedirectNavigation(
      web::WebState* web_state,
      web::NavigationContext* navigation_context) override;

  // Notifies that a navigation has finished. Commits the protection state
  // if the navigation successfully committed.
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;

  // Called when the WebState is being destroyed.
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  friend class web::WebStateUserData<DataProtectionTabHelper>;

  explicit DataProtectionTabHelper(web::WebState* web_state);

  // State for a single navigation attempt.
  struct NavigationState {
    std::optional<int64_t> navigation_id;
    // True if screenshot protection is enabled for this navigation.
    bool screenshot_protection_enabled = false;
  };

  // Initiates policy checks for the initial URL (the one visible when the
  // helper is created).
  void CheckPolicyForInitialURL();

  // Initiates policy checks (Data Controls and Real-time lookup) for the
  // given navigation context.
  void PerformChecks(web::NavigationContext* navigation_context);

  // Returns true if the given `url` should have screenshot protection enabled
  // based on local Data Controls rules.
  bool IsScreenshotBlockedByDataControls(const GURL& url);

  // Returns true if a real-time URL lookup should be performed.
  bool ShouldPerformRealTimeLookup() const;

  // Initiates a real-time URL lookup for the given `url` if enterprise lookup
  // is enabled.
  void EvaluateRealTimePolicy(const GURL& url,
                              std::optional<int64_t> navigation_id);

  // Callback invoked when a real-time lookup result is received.
  // Updates the pending or committed state based on the navigation_id.
  void OnRealTimeLookupResult(
      std::optional<int64_t> navigation_id,
      std::unique_ptr<safe_browsing::RTLookupResponse> response);

  // Updates the tab's screenshot protection state and notifies observers
  // if the state actually changes.
  void UpdateScreenshotProtectionState(bool new_state);

  // Helper methods to access services from the Profile.
  ProfileIOS* GetProfile() const;
  data_controls::IOSRulesService* GetRulesService() const;
  enterprise_data_protection::DataProtectionUrlLookupService* GetLookupService()
      const;
  safe_browsing::RealTimeUrlLookupServiceBase* GetRealTimeLookupService() const;

  // Pointer to the WebState this helper is attached to.
  raw_ptr<web::WebState> web_state_ = nullptr;

  // --- State ---
  // State for the navigation that is currently in progress.
  NavigationState pending_navigation_;

  // State for the navigation that is currently committed and displayed.
  NavigationState committed_navigation_;

  // List of observers.
  base::ObserverList<DataProtectionTabHelperObserver, /*check_empty=*/true>
      observers_;

  base::WeakPtrFactory<DataProtectionTabHelper> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_MODEL_DATA_PROTECTION_TAB_HELPER_H_
