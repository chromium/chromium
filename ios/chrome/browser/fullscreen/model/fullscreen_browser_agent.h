// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FULLSCREEN_MODEL_FULLSCREEN_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_FULLSCREEN_MODEL_FULLSCREEN_BROWSER_AGENT_H_

#include "base/observer_list.h"
#include "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent_observer.h"
#include "ios/chrome/browser/shared/model/browser/browser_user_data.h"

// A class that holds the fullscreen state for a browser.
class FullscreenBrowserAgent : public BrowserUserData<FullscreenBrowserAgent> {
 public:
  ~FullscreenBrowserAgent() override;

  FullscreenBrowserAgent(const FullscreenBrowserAgent&) = delete;
  FullscreenBrowserAgent& operator=(const FullscreenBrowserAgent&) = delete;

  // Adds `observer` to the list of observers.
  void AddObserver(FullscreenBrowserAgentObserver* observer);

  // Removes `observer` from the list of observers.
  void RemoveObserver(FullscreenBrowserAgentObserver* observer);

 private:
  friend class BrowserUserData<FullscreenBrowserAgent>;
  explicit FullscreenBrowserAgent(Browser* browser);

  base::ObserverList<FullscreenBrowserAgentObserver, true> observers_;
};

#endif  // IOS_CHROME_BROWSER_FULLSCREEN_MODEL_FULLSCREEN_BROWSER_AGENT_H_
