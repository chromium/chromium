// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/prerender/model/prerender_browser_agent.h"

PrerenderBrowserAgent::~PrerenderBrowserAgent() = default;

PrerenderBrowserAgent::PrerenderBrowserAgent(Browser* browser)
    : BrowserUserData(browser) {}
