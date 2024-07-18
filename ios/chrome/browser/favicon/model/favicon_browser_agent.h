// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FAVICON_MODEL_FAVICON_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_FAVICON_MODEL_FAVICON_BROWSER_AGENT_H_

#import "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ios/chrome/browser/sessions/model/session_restoration_observer.h"
#include "ios/chrome/browser/shared/model/browser/browser_user_data.h"

class SessionRestorationService;

// A BrowserAgent that prepares WebStates to fetch their favicon after
// a session restoration.
class FaviconBrowserAgent final : public BrowserUserData<FaviconBrowserAgent>,
                                  public SessionRestorationObserver {
 public:
  ~FaviconBrowserAgent() final;

  // SessionRestorationObserver implementation.
  void WillStartSessionRestoration(Browser* browser) final;
  void SessionRestorationFinished(
      Browser* browser,
      const std::vector<web::WebState*>& restored_web_states) final;

 private:
  friend class BrowserUserData<FaviconBrowserAgent>;

  explicit FaviconBrowserAgent(Browser* browser);

  // The Browser this object is attached to.
  raw_ptr<Browser> browser_;

  // Observation for SessionRestorationService events.
  base::ScopedObservation<SessionRestorationService, SessionRestorationObserver>
      session_restoration_service_observation_{this};

  BROWSER_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_FAVICON_MODEL_FAVICON_BROWSER_AGENT_H_
