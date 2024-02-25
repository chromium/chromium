// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HTTPS_UPGRADES_MODEL_HTTPS_ONLY_MODE_UPGRADE_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_HTTPS_UPGRADES_MODEL_HTTPS_ONLY_MODE_UPGRADE_TAB_HELPER_H_

#import "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"
#include "url/gurl.h"

class HttpsUpgradeService;
class PrefService;
class PrerenderService;

// This tab helper handles HTTP main frame navigation upgrades to HTTPS.
// When it encounters an eligible HTTP navigation, it cancels the navigation,
// starts a new navigation to the HTTPS version of the URL and observes the
// response.
// If the response is error free, it considers the upgrade successful.
// Otherwise, it shows the HTTPS-Only Mode interstitial which asks the user to
// proceed to the HTTP URL or go back to the previous page.
class HttpsOnlyModeUpgradeTabHelper
    : public web::WebStateObserver,
      public web::WebStatePolicyDecider,
      public web::WebStateUserData<HttpsOnlyModeUpgradeTabHelper> {
 public:
  ~HttpsOnlyModeUpgradeTabHelper() override;
  HttpsOnlyModeUpgradeTabHelper(const HttpsOnlyModeUpgradeTabHelper&) = delete;
  HttpsOnlyModeUpgradeTabHelper& operator=(
      const HttpsOnlyModeUpgradeTabHelper&) = delete;

  // Returns true if the upgrade timer is running.
  bool IsTimerRunningForTesting() const;
  // Clears the allowlist that contains domains allowed over HTTP.
  void ClearAllowlistForTesting();

 private:
  friend class web::WebStateUserData<HttpsOnlyModeUpgradeTabHelper>;

  enum class State {
    // Initial state. The navigation hasn't started yet, or started but hasn't
    // been upgraded because it's already HTTPS or a non-HTTP scheme.
    kNone,
    // The navigation is stopped to start an upgraded navigation.
    kStoppedToUpgrade,
    // The upgraded navigation is started.
    kUpgraded,
    // The upgraded navigation timed out.
    kStoppedWithTimeout,
    // The upgraded navigation is stopped to start a fallback navigation (e.g.
    // due to the upgraded navigation redirecting to HTTP).
    kStoppedToFallback,
    // A fallback navigation is started.
    kFallbackStarted,
    // Final state. Either the interstitial is shown or the upgrade completed
    // successfully.
    kDone,
  };

  HttpsOnlyModeUpgradeTabHelper(web::WebState* web_state,
                                PrefService* prefs,
                                PrerenderService* prerender_service,
                                HttpsUpgradeService* service);

  // Returns true if url can be loaded over HTTP (e.g. it was previously
  // allowlisted).
  bool IsHttpAllowedForUrl(const GURL& url) const;
  // Called when the upgrade timer times out.
  void OnHttpsLoadTimeout(base::WeakPtr<web::WebState> weak_web_state);
  // Stops the current navigation and sets the state so that an upgrade will be
  // started.
  void StopToUpgrade(
      const GURL& url,
      const web::Referrer& referrer,
      base::OnceCallback<void(web::WebStatePolicyDecider::PolicyDecision)>
          callback);
  // Initiates a fallback navigation to the original HTTP URL. This will be
  // cancelled in ShouldAllowResponse() with an HTTP interstitial, unless the
  // HTTP URL was previously allowlisted.
  void FallbackToHttp();
  // Sets the initial state and clears the timer.
  void ResetState();

  // web::WebStatePolicyDecider implementation:
  void ShouldAllowResponse(
      NSURLResponse* response,
      WebStatePolicyDecider::ResponseInfo response_info,
      web::WebStatePolicyDecider::PolicyDecisionCallback callback) override;
  void WebStateDestroyed() override;

  // web::WebStateObserver implementation:
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* context) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // Internal state of the navigation.
  State state_ = State::kNone;

  // The original HTTP URL that was navigated to.
  GURL http_url_;

  // Parameters for the upgraded navigation. These are stored upon a new
  // navigation and used when starting a fallback or upgraded navigation.
  GURL upgraded_https_url_;
  ui::PageTransition navigation_transition_type_ = ui::PAGE_TRANSITION_FIRST;
  bool navigation_is_renderer_initiated_ = false;
  web::Referrer referrer_;
  // Set to true when a new navigation with a POST method is started.
  // Used to check if the navigation should be upgraded when a response is
  // received. Cleared when the current navigation finishes.
  bool navigation_is_post_ = false;

  base::OneShotTimer timer_;

  raw_ptr<PrefService> prefs_;
  raw_ptr<PrerenderService> prerender_service_;
  raw_ptr<HttpsUpgradeService> service_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_HTTPS_UPGRADES_MODEL_HTTPS_ONLY_MODE_UPGRADE_TAB_HELPER_H_
