// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRERENDER_MODEL_PRERENDER_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_PRERENDER_MODEL_PRERENDER_BROWSER_AGENT_H_

#include "ios/chrome/browser/shared/model/browser/browser_user_data.h"

// A BrowserAgent responsible for managing the pre-rendering of web pages.
class PrerenderBrowserAgent : public BrowserUserData<PrerenderBrowserAgent> {
 public:
  ~PrerenderBrowserAgent() override;

 private:
  friend class BrowserUserData<PrerenderBrowserAgent>;
  PrerenderBrowserAgent(Browser* browser);
};

#endif  // IOS_CHROME_BROWSER_PRERENDER_MODEL_PRERENDER_BROWSER_AGENT_H_
