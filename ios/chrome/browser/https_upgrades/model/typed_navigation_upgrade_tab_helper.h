// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HTTPS_UPGRADES_MODEL_TYPED_NAVIGATION_UPGRADE_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_HTTPS_UPGRADES_MODEL_TYPED_NAVIGATION_UPGRADE_TAB_HELPER_H_

#import "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"
#include "url/gurl.h"

class HttpsUpgradeService;
class PrerenderService;

// A tab helper that handles the fallback and timeout logic for Omnibox HTTPS
// upgrades. Omnibox HTTPS upgrades feature automatically upgrades navigations
// typed without a scheme to https. For example, if the user types "example.com"
// in the omnibox, this feature first attempts to load https://example.com, then
// falls back to http://example.com if the https load fails. This tab helper
// observes the navigations and initiates fallback http:// navigations when
// necessary.
class TypedNavigationUpgradeTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<TypedNavigationUpgradeTabHelper> {
 public:
  ~TypedNavigationUpgradeTabHelper() override;

  TypedNavigationUpgradeTabHelper(const TypedNavigationUpgradeTabHelper&) =
      delete;
  TypedNavigationUpgradeTabHelper& operator=(
      const TypedNavigationUpgradeTabHelper&) = delete;

  // Returns true if the upgrade timer is running.
  bool IsTimerRunningForTesting() const;

  // web::WebStateObserver implementation:
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  static const char kHistogramName[];

 private:
  friend class web::WebStateUserData<TypedNavigationUpgradeTabHelper>;

  enum class State {
    // No omnibox upgrade or fallback navigation happening at the moment. Could
    // be due to one of the following cases:
    // - The navigation was not upgraded in the first place.
    // - The navigation upgrade completed successfully
    // - The navigation upgrade failed and the fallback navigation completed.
    kNone,
    // The upgraded navigation is started.
    kUpgraded,
    // A fallback navigation to http:// is started.
    kFallbackStarted,
    // The upgraded navigation timed out.
    kStoppedWithTimeout,
  };

  TypedNavigationUpgradeTabHelper(web::WebState* web_state,
                                  PrerenderService* prerender_service,
                                  HttpsUpgradeService* service);

  // Called when the upgrade timer times out.
  void OnHttpsLoadTimeout(base::WeakPtr<web::WebState> weak_web_state);
  // Starts a fallback navigation to the http:// version of https_url.
  void FallbackToHttp(web::WebState* web_state, const GURL& https_url);

  // Internal state of the navigation.
  State state_ = State::kNone;

  // Parameters for the upgraded navigation.
  GURL upgraded_https_url_;
  ui::PageTransition navigation_transition_type_ = ui::PAGE_TRANSITION_FIRST;
  bool navigation_is_renderer_initiated_ = false;
  web::Referrer referrer_;

  base::OneShotTimer timer_;

  raw_ptr<PrerenderService> prerender_service_;
  raw_ptr<HttpsUpgradeService> service_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_HTTPS_UPGRADES_MODEL_TYPED_NAVIGATION_UPGRADE_TAB_HELPER_H_
