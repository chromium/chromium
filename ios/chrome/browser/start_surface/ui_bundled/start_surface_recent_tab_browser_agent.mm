// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_recent_tab_browser_agent.h"

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_constants.h"
#import "ios/chrome/browser/ntp/shared/metrics/home_metrics.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_util.h"

#pragma mark - StartSurfaceBrowserAgent

StartSurfaceRecentTabBrowserAgent::StartSurfaceRecentTabBrowserAgent(
    Browser* browser)
    : BrowserUserData(browser) {
  StartObserving(browser_,
                 TabsDependencyInstaller::Policy::kAccordingToFeature);
}

StartSurfaceRecentTabBrowserAgent::~StartSurfaceRecentTabBrowserAgent() {
  StopObserving();
  favicon_driver_observation_.Reset();
}

#pragma mark - Public

void StartSurfaceRecentTabBrowserAgent::SaveMostRecentTab() {
  web::WebState* active_web_state =
      browser_->GetWebStateList()->GetActiveWebState();
  CHECK(active_web_state);

  if (most_recent_tab_ == active_web_state) {
    return;
  }

  SetMostRecentTab(active_web_state);
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

void StartSurfaceRecentTabBrowserAgent::OnWebStateInserted(
    web::WebState* web_state) {
  // Nothing to do.
}

void StartSurfaceRecentTabBrowserAgent::OnWebStateRemoved(
    web::WebState* web_state) {
  if (web_state != most_recent_tab_) {
    return;
  }

  SetMostRecentTab(nullptr);
}

void StartSurfaceRecentTabBrowserAgent::OnWebStateDeleted(
    web::WebState* web_state) {
  // Nothing to do.
}

void StartSurfaceRecentTabBrowserAgent::OnActiveWebStateChanged(
    web::WebState* old_active,
    web::WebState* new_active) {
  // Nothing to do.
}

#pragma mark - WebStateObserver

void StartSurfaceRecentTabBrowserAgent::WebStateDestroyed(
    web::WebState* web_state) {
  NOTREACHED();
}

void StartSurfaceRecentTabBrowserAgent::TitleWasSet(web::WebState* web_state) {
  for (auto& observer : observers_) {
    observer.MostRecentTabTitleUpdated(web_state, web_state->GetTitle());
  }
}

#pragma mark - favicon::FaviconDriverObserver

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
        observer.MostRecentTabFaviconUpdated(most_recent_tab_,
                                             favicon.ToUIImage());
      }
    }
  }
}

#pragma mark - Private methods

void StartSurfaceRecentTabBrowserAgent::SetMostRecentTab(
    web::WebState* web_state) {
  CHECK_NE(web_state, most_recent_tab_);

  // Stop observing the old tab.
  most_recent_tab_observation_.Reset();
  favicon_driver_observation_.Reset();

  if (web_state) {
    // If the tab is cleared, then notify the observers.
    for (auto& observer : observers_) {
      observer.MostRecentTabRemoved(most_recent_tab_);
    }
  }

  most_recent_tab_ = web_state;
  if (most_recent_tab_) {
    RecordModuleFreshnessSignal(ContentSuggestionsModuleType::kTabResumption,
                                browser_->GetProfile()->GetPrefs());

    // Start observing the new tab.
    most_recent_tab_observation_.Observe(most_recent_tab_);
    favicon_driver_observation_.Observe(
        favicon::WebFaviconDriver::FromWebState(most_recent_tab_));
  }
}
