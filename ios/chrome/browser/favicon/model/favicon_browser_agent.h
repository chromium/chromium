// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FAVICON_MODEL_FAVICON_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_FAVICON_MODEL_FAVICON_BROWSER_AGENT_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "ios/chrome/browser/sessions/model/session_restoration_observer.h"
#include "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#include "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#include "ios/web/public/web_state_observer.h"

class SessionRestorationService;

// A BrowserAgent that prepares WebStates to fetch their favicon after
// a session restoration.
class FaviconBrowserAgent final : public BrowserUserData<FaviconBrowserAgent>,
                                  public SessionRestorationObserver,
                                  public web::WebStateObserver,
                                  public WebStateListObserver {
 public:
  ~FaviconBrowserAgent() final;

  // SessionRestorationObserver implementation.
  void WillStartSessionRestoration(Browser* browser) final;
  void SessionRestorationFinished(
      Browser* browser,
      const std::vector<web::WebState*>& restored_web_states) final;

  // WebStateListObserver implementation.
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;

  // web::WebStateObserver implementation.
  void WebStateRealized(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  friend class BrowserUserData<FaviconBrowserAgent>;

  explicit FaviconBrowserAgent(Browser* browser);

  // Helper called when starting to observe an unrealized WebState.
  void StartObservingWebState(web::WebState* web_state);

  // Helper called when stopping to observe WebState (either because
  // it became realized, was detached or destroyed).
  void StopObservingWebState(web::WebState* web_state);

  // Starts fetching the favicon for a WebState.
  void FetchFaviconForWebState(web::WebState* web_state);

  // Observation for SessionRestorationService events.
  base::ScopedObservation<SessionRestorationService, SessionRestorationObserver>
      session_restoration_service_observation_{this};

  // Observation of the WebStateList.
  base::ScopedObservation<WebStateList, WebStateListObserver>
      web_state_list_observation_{this};

  // Observation for unrealized WebStates.
  base::ScopedMultiSourceObservation<web::WebState, web::WebStateObserver>
      web_state_observations_{this};
};

#endif  // IOS_CHROME_BROWSER_FAVICON_MODEL_FAVICON_BROWSER_AGENT_H_
