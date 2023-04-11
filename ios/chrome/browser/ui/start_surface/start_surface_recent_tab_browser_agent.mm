// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/start_surface/start_surface_recent_tab_browser_agent.h"

#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - StartSurfaceBrowserAgent

BROWSER_USER_DATA_KEY_IMPL(StartSurfaceRecentTabBrowserAgent)

StartSurfaceRecentTabBrowserAgent::StartSurfaceRecentTabBrowserAgent(
    Browser* browser)
    : favicon_driver_observer_(this), browser_(browser) {
  browser_->AddObserver(this);
  browser_->GetWebStateList()->AddObserver(this);
}

StartSurfaceRecentTabBrowserAgent::~StartSurfaceRecentTabBrowserAgent() =
    default;

#pragma mark - Public

void StartSurfaceRecentTabBrowserAgent::SaveMostRecentTab() {
  web::WebState* active_web_state =
      browser_->GetWebStateList()->GetActiveWebState();
  if (most_recent_tab_ != active_web_state) {
    most_recent_tab_ = active_web_state;
    DCHECK(favicon::WebFaviconDriver::FromWebState(most_recent_tab_));
    if (favicon_driver_observer_.IsObserving()) {
      favicon_driver_observer_.Reset();
    }
    favicon_driver_observer_.Observe(
        favicon::WebFaviconDriver::FromWebState(most_recent_tab_));
    if (web_state_observation_.IsObserving()) {
      web_state_observation_.Reset();
    }
    web_state_observation_.Observe(most_recent_tab_);
  }
}

void StartSurfaceRecentTabBrowserAgent::AddObserver(
    StartSurfaceRecentTabObserver* observer) {
  DCHECK(!observers_.HasObserver(observer));
  observers_.AddObserver(observer);
}

void StartSurfaceRecentTabBrowserAgent::RemoveObserver(
    StartSurfaceRecentTabObserver* observer) {
  observers_.RemoveObserver(observer);
}

#pragma mark - BrowserObserver

void StartSurfaceRecentTabBrowserAgent::BrowserDestroyed(Browser* browser) {
  browser_->GetWebStateList()->RemoveObserver(this);
  browser_->RemoveObserver(this);
  favicon_driver_observer_.Reset();
  web_state_observation_.Reset();
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
    favicon_driver_observer_.Reset();
    web_state_observation_.Reset();
    most_recent_tab_ = nullptr;
    return;
  }
}

#pragma mark - WebStateObserver

void StartSurfaceRecentTabBrowserAgent::WebStateDestroyed(
    web::WebState* web_state) {
  favicon_driver_observer_.Reset();
  web_state_observation_.Reset();
  most_recent_tab_ = nullptr;
}

void StartSurfaceRecentTabBrowserAgent::OnFaviconUpdated(
    favicon::FaviconDriver* driver,
    NotificationIconType notification_icon_type,
    const GURL& icon_url,
    bool icon_url_changed,
    const gfx::Image& image) {
  if (driver->FaviconIsValid()) {
    gfx::Image favicon = driver->GetFavicon();
    if (!favicon.IsEmpty()) {
      for (auto& observer : observers_) {
        observer.MostRecentTabFaviconUpdated(favicon.ToUIImage());
      }
    }
  }
}

void StartSurfaceRecentTabBrowserAgent::TitleWasSet(web::WebState* web_state) {
  for (auto& observer : observers_) {
    observer.MostRecentTabTitleUpdated(web_state->GetTitle());
  }
}
