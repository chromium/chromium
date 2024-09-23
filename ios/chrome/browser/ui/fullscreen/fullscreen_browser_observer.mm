// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_browser_observer.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"

FullscreenBrowserObserver::FullscreenBrowserObserver(
    FullscreenWebStateListObserver* web_state_list_observer,
    Browser* browser)
    : web_state_list_observer_(web_state_list_observer) {
  DCHECK(web_state_list_observer_);
  // TODO(crbug.com/41358770): DCHECK `browser` once FullscreenController is
  // fully scoped to a Browser.
  if (browser) {
    web_state_list_observer_->SetWebStateList(browser->GetWebStateList());
    scoped_observation_.Observe(browser);
  }
}

FullscreenBrowserObserver::~FullscreenBrowserObserver() = default;

void FullscreenBrowserObserver::FullscreenBrowserObserver::BrowserDestroyed(
    Browser* browser) {
  web_state_list_observer_->SetWebStateList(nullptr);
  DCHECK(scoped_observation_.IsObservingSource(browser));
  scoped_observation_.Reset();
}
