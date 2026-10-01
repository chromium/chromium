// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_BROWSER_AGENT_H_

#import "base/scoped_observation.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_browser_agent.h"
#import "ios/chrome/browser/shared/model/browser/browser_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/web/public/web_state_observer.h"

class Browser;
class WebStateList;

namespace web {
class WebState;
}  // namespace web

namespace contextual_cueing {

// Browser agent that relays browser-level (per window) signals to the
// `ContextualCueingTabHelper` of the active tab, so tab helpers never need to
// reach back into their hosting `Browser`. Currently relays whether Gemini
// (Helios) is invoked in the window.
class ContextualCueingBrowserAgent
    : public BrowserUserData<ContextualCueingBrowserAgent>,
      public BrowserObserver,
      public GeminiBrowserAgent::Observer,
      public WebStateListObserver,
      public web::WebStateObserver {
 public:
  ContextualCueingBrowserAgent(const ContextualCueingBrowserAgent&) = delete;
  ContextualCueingBrowserAgent& operator=(const ContextualCueingBrowserAgent&) =
      delete;

  ~ContextualCueingBrowserAgent() override;

  // BrowserObserver:
  void BrowserDestroyed(Browser* browser) override;

  // GeminiBrowserAgent::Observer:
  void OnFloatyInvokedChanged(bool is_invoked) override;

  // WebStateListObserver:
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;

  // web::WebStateObserver:
  void WebStateRealized(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  friend class BrowserUserData<ContextualCueingBrowserAgent>;

  explicit ContextualCueingBrowserAgent(Browser* browser);

  // Syncs the newly active `web_state` (may be null). Pushes
  // `is_gemini_invoked_` immediately if `web_state` is realized; otherwise
  // observes it so the push happens from `WebStateRealized()`, since its tab
  // helper is only attached at realization and realization is never forced.
  void TrackActiveWebState(web::WebState* web_state);

  // Pushes `is_gemini_invoked_` to the `ContextualCueingTabHelper` of
  // `web_state`. No-op if `web_state` is null, unrealized, or has no tab
  // helper.
  void UpdateGeminiInvokedForWebState(web::WebState* web_state);

  // Whether Gemini (Helios) is currently invoked in the window, mirrored from
  // `GeminiBrowserAgent` via `OnFloatyInvokedChanged()`.
  bool is_gemini_invoked_ = false;

  base::ScopedObservation<Browser, BrowserObserver> browser_observation_{this};
  // The agent is `Browser` user data destroyed in unspecified order relative to
  // this one, so this observation is reset in `BrowserDestroyed()`.
  base::ScopedObservation<GeminiBrowserAgent, GeminiBrowserAgent::Observer>
      gemini_browser_agent_observation_{this};
  base::ScopedObservation<WebStateList, WebStateListObserver>
      web_state_list_observation_{this};
  // Only set while the active web state is unrealized; see
  // `TrackActiveWebState()`.
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      active_web_state_observation_{this};
};

}  // namespace contextual_cueing

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_BROWSER_AGENT_H_
