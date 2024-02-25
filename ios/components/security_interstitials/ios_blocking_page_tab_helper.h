// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_SECURITY_INTERSTITIALS_IOS_BLOCKING_PAGE_TAB_HELPER_H_
#define IOS_COMPONENTS_SECURITY_INTERSTITIALS_IOS_BLOCKING_PAGE_TAB_HELPER_H_

#import "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ios/components/security_interstitials/ios_security_interstitial_page.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

namespace web {
class WebState;
}  // namespace web

namespace security_interstitials {

// Helps manage IOSSecurityInterstitialPage lifetime independent from
// interstitial code. Stores an IOSSecurityInterstitialPage while a committed
// error page is currently being shown, then destroyes it when the user
// navigates away.
class IOSBlockingPageTabHelper
    : public web::WebStateUserData<IOSBlockingPageTabHelper> {
 public:
  IOSBlockingPageTabHelper(const IOSBlockingPageTabHelper&) = delete;
  IOSBlockingPageTabHelper& operator=(const IOSBlockingPageTabHelper&) = delete;

  ~IOSBlockingPageTabHelper() override;

  // Associates `blocking_page` with `navigation_id`.  When the last committed
  // navigation ID matches `navigation_id`, JavaScript commands are handled by
  // `blocking_page`.
  void AssociateBlockingPage(
      int64_t navigation_id,
      std::unique_ptr<IOSSecurityInterstitialPage> blocking_page);

  // Determines whether a URL should be shown on the current navigation page.
  bool ShouldDisplayURL() const;

  // Returns the blocking page for the currently-visible interstitial, if any.
  IOSSecurityInterstitialPage* GetCurrentBlockingPage() const;

  // Handler for `BlockingPageMessage` JavaScript command. Dispatch to more
  // specific handler.
  //  void OnBlockingPageMessageReceived(const base::Value& message);
  void OnBlockingPageCommandReceived(SecurityInterstitialCommand command);

 private:
  WEB_STATE_USER_DATA_KEY_DECL();
  explicit IOSBlockingPageTabHelper(web::WebState* web_state);
  friend class web::WebStateUserData<IOSBlockingPageTabHelper>;

  // Updates the tab helper state for a finished navigation with `navigation_id`
  // that was optionally committed.
  void UpdateForFinishedNavigation(int64_t navigation_id, bool committed);

  // Helper object that listens for the navigation ID of the last committed
  // item.
  class CommittedNavigationIDListener : public web::WebStateObserver {
   public:
    // Constructor for a listener that notifies `tab_helper` of committed
    // navigation ID updates.
    explicit CommittedNavigationIDListener(
        web::WebState* web_state,
        IOSBlockingPageTabHelper* tab_helper);
    ~CommittedNavigationIDListener() override;

   private:
    // web::WebStateObserver:
    void DidFinishNavigation(
        web::WebState* web_state,
        web::NavigationContext* navigation_context) override;
    void WebStateDestroyed(web::WebState* web_state) override;

    raw_ptr<IOSBlockingPageTabHelper> tab_helper_ = nullptr;
    base::ScopedObservation<web::WebState, web::WebStateObserver>
        scoped_observation_{this};
  };

  // The navigation ID of the last committed navigation.  Used to associate
  // blocking pages with the last committed navigation when the navigation is
  // committed before the blocking page is provided to AssociateBlockingPage().
  int64_t last_committed_navigation_id_ = 0;
  // Keeps track of blocking pages for navigations that have encountered
  // certificate errors in this WebState. When a navigation commits, the
  // corresponding blocking page is moved out and stored in
  // `blocking_page_for_currently_committed_navigation_`.
  std::map<int64_t, std::unique_ptr<IOSSecurityInterstitialPage>>
      blocking_pages_for_navigations_;
  // Keeps track of the blocking page for the currently committed navigation, if
  // there is one. The value is replaced (if the new committed navigation has a
  // blocking page) or reset on every committed navigation.
  std::unique_ptr<IOSSecurityInterstitialPage>
      blocking_page_for_currently_committed_navigation_;

  // Helper object that notifies the tab helper of committed navigation IDs.
  CommittedNavigationIDListener navigation_id_listener_;

  base::WeakPtrFactory<IOSBlockingPageTabHelper> weak_factory_{this};
};

}  // namespace security_interstitials

#endif  // IOS_COMPONENTS_SECURITY_INTERSTITIALS_IOS_BLOCKING_PAGE_TAB_HELPER_H_
