// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/start_surface/start_surface_recent_tab_browser_agent.h"

#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - StartSurfaceBrowserAgent

BROWSER_USER_DATA_KEY_IMPL(StartSurfaceRecentTabBrowserAgent)

StartSurfaceRecentTabBrowserAgent::StartSurfaceRecentTabBrowserAgent(
    Browser* browser)
    : browser_(browser) {
  browser_->AddObserver(this);
  browser_->GetWebStateList()->AddObserver(this);
}

StartSurfaceRecentTabBrowserAgent::~StartSurfaceRecentTabBrowserAgent() =
    default;

#pragma mark - Public

void StartSurfaceRecentTabBrowserAgent::SaveMostRecentTab() {
  most_recent_tab_ = browser_->GetWebStateList()->GetActiveWebState();
}

void StartSurfaceRecentTabBrowserAgent::AddObserver(
    StartSurfaceRecentTabRemovalObserver* observer) {
  DCHECK(!observers_.HasObserver(observer));
  observers_.AddObserver(observer);
}

void StartSurfaceRecentTabBrowserAgent::RemoveObserver(
    StartSurfaceRecentTabRemovalObserver* observer) {
  observers_.RemoveObserver(observer);
}

#pragma mark - BrowserObserver

void StartSurfaceRecentTabBrowserAgent::BrowserDestroyed(Browser* browser) {
  browser_->GetWebStateList()->RemoveObserver(this);
  browser_->RemoveObserver(this);
}

#pragma mark - WebStateListObserver

void StartSurfaceRecentTabBrowserAgent::WebStateDetachedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index) {
  if (!most_recent_tab_) {
    return;
  }

  if (most_recent_tab_ == web_state) {
    for (auto& observer : observers_) {
      observer.MostRecentTabRemoved(most_recent_tab_);
    }
    most_recent_tab_ = nullptr;
    return;
  }
}
