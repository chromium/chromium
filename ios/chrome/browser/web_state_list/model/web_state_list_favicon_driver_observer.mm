// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/model/web_state_list_favicon_driver_observer.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"

WebStateListFaviconDriverObserver::WebStateListFaviconDriverObserver(
    Browser* browser,
    id<WebStateFaviconDriverObserver> observer)
    : favicon_observer_(observer) {
  StartObserving(browser, Policy::kAccordingToFeature);
}

WebStateListFaviconDriverObserver::~WebStateListFaviconDriverObserver() {
  StopObserving();
}

#pragma mark - TabsDependencyInstaller

void WebStateListFaviconDriverObserver::OnWebStateInserted(
    web::WebState* web_state) {
  favicon_driver_observations_.AddObservation(
      favicon::WebFaviconDriver::FromWebState(web_state));
}

void WebStateListFaviconDriverObserver::OnWebStateRemoved(
    web::WebState* web_state) {
  favicon_driver_observations_.RemoveObservation(
      favicon::WebFaviconDriver::FromWebState(web_state));
}

void WebStateListFaviconDriverObserver::OnWebStateDeleted(
    web::WebState* web_state) {
  // Nothing to do.
}

void WebStateListFaviconDriverObserver::OnActiveWebStateChanged(
    web::WebState* old_active,
    web::WebState* new_active) {
  // Nothing to do.
}

#pragma mark - favicon::FaviconDriverObserver

void WebStateListFaviconDriverObserver::OnFaviconUpdated(
    favicon::FaviconDriver* driver,
    favicon::FaviconDriverObserver::NotificationIconType icon_type,
    const GURL& icon_url,
    bool icon_url_changed,
    const gfx::Image& image) {
  // Since WebStateListFaviconDriverObserver only observes WebFaviconDriver
  // (enforced by the base::ScopedMultiSourceObservation<...>), this downcast
  // from FaviconDriver* to WebFaviconDriver* is valid.
  favicon::WebFaviconDriver* web_favicon_driver =
      static_cast<favicon::WebFaviconDriver*>(driver);

  [favicon_observer_ faviconDriver:web_favicon_driver
       didUpdateFaviconForWebState:web_favicon_driver->web_state()];
}
