// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent.h"

FullscreenBrowserAgent::FullscreenBrowserAgent(Browser* browser)
    : BrowserUserData(browser) {}

FullscreenBrowserAgent::~FullscreenBrowserAgent() {}

void FullscreenBrowserAgent::AddObserver(
    FullscreenBrowserAgentObserver* observer) {
  observers_.AddObserver(observer);
}

void FullscreenBrowserAgent::RemoveObserver(
    FullscreenBrowserAgentObserver* observer) {
  observers_.RemoveObserver(observer);
}
