// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_recent_tab_browser_agent.h"

#import "ios/chrome/browser/ntp/shared/metrics/home_metrics.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_util.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"

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
    RecordModuleFreshnessSignal(ContentSuggestionsModuleType::kTabResumption);
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
    web_state_observation_.Observe(most_recent_tab_.get());
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

void StartSurfaceRecentTabBrowserAgent::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly:
      // Do nothing when a WebState is selected and its status is updated.
      break;
    case WebStateListChange::Type::kDetach: {
      if (!most_recent_tab_) {
        return;
      }

      const WebStateListChangeDetach& detach_change =
          change.As<WebStateListChangeDetach>();
      if (most_recent_tab_.get() == detach_change.detached_web_state()) {
        for (auto& observer : observers_) {
          observer.MostRecentTabRemoved(most_recent_tab_);
        }
        favicon_driver_observer_.Reset();
        web_state_observation_.Reset();
        most_recent_tab_ = nullptr;
        return;
      }
      break;
    }
    case WebStateListChange::Type::kMove:
      // Do nothing when a WebState is moved.
      break;
    case WebStateListChange::Type::kReplace:
      // Do nothing when a WebState is replaced.
      break;
    case WebStateListChange::Type::kInsert:
      // Do nothing when a WebState is inserted.
      break;
    case WebStateListChange::Type::kGroupCreate:
      // Do nothing when a group is created.
      break;
    case WebStateListChange::Type::kGroupVisualDataUpdate:
      // Do nothing when a tab group's visual data are updated.
      break;
    case WebStateListChange::Type::kGroupMove:
      // Do nothing when a tab group is moved.
      break;
    case WebStateListChange::Type::kGroupDelete:
      // Do nothing when a group is deleted.
      break;
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
        observer.MostRecentTabFaviconUpdated(most_recent_tab_,
                                             favicon.ToUIImage());
      }
    }
  }
}

void StartSurfaceRecentTabBrowserAgent::TitleWasSet(web::WebState* web_state) {
  for (auto& observer : observers_) {
    observer.MostRecentTabTitleUpdated(web_state, web_state->GetTitle());
  }
}
