// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRERENDER_MODEL_PRERENDER_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_PRERENDER_MODEL_PRERENDER_BROWSER_AGENT_H_

#import <Foundation/Foundation.h>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "components/prefs/pref_change_registrar.h"
#include "ios/chrome/browser/prerender/model/prerender_pref.h"
#include "ios/chrome/browser/prerender/model/prerender_tab_helper_delegate.h"
#include "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#include "ios/web/public/navigation/referrer.h"
#include "net/base/network_change_notifier.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

class ManageAccountsDelegate;
@protocol PrerenderBrowserAgentDelegate;

namespace web {
class WebState;
}  // namespace web

// A BrowserAgent responsible for managing the pre-rendering of web pages.
class PrerenderBrowserAgent final
    : public BrowserUserData<PrerenderBrowserAgent>,
      public net::NetworkChangeNotifier::NetworkChangeObserver,
      public PrerenderTabHelperDelegate {
 public:
  // Policy for starting pre-rendering.
  enum PrerenderPolicy {
    kNoDelay,
    kDefaultDelay,
  };

  ~PrerenderBrowserAgent() final;

  // Sets the delegate that will provide information to this agent.
  void SetDelegate(id<PrerenderBrowserAgentDelegate> delegate);

  // Schedule prerendering for `url` with the given `transition` and `referrer`
  // with delay based on `policy`.
  //
  // If there is already an existing request for `url`, this method does
  // nothing and does not reset the delay. If there is an existing request
  // for a different URL, it is cancelled before the new request is queued.
  //
  // Unless `policy` is `kNoDelay` the pre-rendering is only started after
  // a short delay, to prevent unnecessary pre-rendering while the user is
  // typing.
  void StartPrerender(const GURL& url,
                      const web::Referrer& referrer,
                      ui::PageTransition transition,
                      PrerenderPolicy policy);

  // Promotes the pre-rendered tab to a real tab, replacing the Browser's
  // active WebState if it is used to pre-render `url`. Otherwise cancels
  // the pre-rendering.
  //
  // Returns whether the active WebState was replaced or not.
  bool ValidatePrerender(const GURL& url, ui::PageTransition transition);

  // Returns whether a pre-rendered WebState is being inserted into the
  // Browser by this agent.
  bool IsInsertingPrerender() const;

  // PrerenderTabHelperDelegate implementation.
  void CancelPrerender() final;

 private:
  // Helper classes used to store information about prerender requests.
  template <typename WebStatePtr>
  class Request;
  class RequestInfos;

  // Helper classes that will implement the WebStateDelegate, WebStateObserver
  // and WebStatePolicyDecider API respectively for PrerenderBrowserAgent.
  class Delegate;
  class Observer;
  class PolicyDecider;
  class ManageAccountsDelegate;

  // Enumeration for pre-render termination reason.
  enum class PrerenderFinalStatus;

  friend class Delegate;
  friend class Observer;
  friend class PolicyDecider;
  friend class ManageAccountsDelegate;
  friend class BrowserUserData<PrerenderBrowserAgent>;

  PrerenderBrowserAgent(Browser* browser);

  // Returns whether pre-render is enabled or not.
  bool Enabled() const;

  // Returns whether `url` is already pre-rendered or there is a scheduled
  // request to pre-render it.
  bool IsPrerenderdOrScheduled(const GURL& url) const;

  // Starts the pending request.
  void StartPendingRequest();

  // Schedule a call to cancel the pre-rendering on the next run loop.
  void ScheduleCancelPrerender();

  // Cancels any scheduled pre-render request. Does nothing if there is no
  // requests scheduled.
  void CancelScheduledRequest();

  // Cancels the pre-render with `reason`.
  void CancelPrerenderInternal(PrerenderFinalStatus reason);

  // Destroys the pre-render with `reason`.
  void DestroyPrerender(PrerenderFinalStatus reason);

  // Releases the pre-render WebState with `reason`.
  [[nodiscard]] std::unique_ptr<web::WebState> ReleasePrerender(
      PrerenderFinalStatus reason);

  // Called when the NetworkPredictionSetting value may have changed.
  void OnNetworkPredictionSettingChanged();

  // net::NetworkChangeNotifier::NetworkChangeObserver implementation.
  void OnNetworkChanged(net::NetworkChangeNotifier::ConnectionType type) final;

  SEQUENCE_CHECKER(sequence_checker_);

  __weak id<PrerenderBrowserAgentDelegate> delegate_;

  // Pending and in-progress pre-render requests.
  std::unique_ptr<Request<base::WeakPtr<web::WebState>>> scheduled_request_;
  std::unique_ptr<Request<std::unique_ptr<web::WebState>>> prerender_request_;

  std::unique_ptr<Delegate> web_state_delegate_;
  std::unique_ptr<Observer> web_state_observer_;
  std::unique_ptr<PolicyDecider> policy_decider_;

  // Used for all pre-renders.
  std::unique_ptr<ManageAccountsDelegate> manage_accounts_delegate_;

  // Used to schedule pre-render requests.
  base::OneShotTimer timer_;

  // Registration with the NSNotificationCenter.
  __strong id<NSObject> nsnotification_registration_;

  // Used to track the preferences and the network status.
  PrefChangeRegistrar registrar_;

  // Is the pre-render tab being converted to a real tab?
  bool loading_prerender_ = false;

  base::WeakPtrFactory<PrerenderBrowserAgent> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_PRERENDER_MODEL_PRERENDER_BROWSER_AGENT_H_
