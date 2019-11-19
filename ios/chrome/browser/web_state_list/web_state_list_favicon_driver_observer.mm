// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/web_state_list_favicon_driver_observer.h"

#include "components/favicon/ios/web_favicon_driver.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

WebStateListFaviconDriverObserver::WebStateListFaviconDriverObserver(
    WebStateList* web_state_list,
    id<WebStateFaviconDriverObserver> observer)
    : favicon_observer_(observer), web_state_list_observer_(this) {
  web_state_list_observer_.Add(web_state_list);
  for (int i = 0; i < web_state_list->count(); ++i)
    AddNewWebState(web_state_list->GetWebStateAt(i));
}

WebStateListFaviconDriverObserver::~WebStateListFaviconDriverObserver() {
  if (!driver_to_web_state_map_.empty()) {
    for (const auto& pair : driver_to_web_state_map_) {
      favicon::FaviconDriver* driver = pair.first;
      driver->RemoveObserver(this);
    }
    driver_to_web_state_map_.clear();
  }
}

void WebStateListFaviconDriverObserver::WebStateInsertedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index,
    bool activating) {
  AddNewWebState(web_state);
}

void WebStateListFaviconDriverObserver::WebStateReplacedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int index) {
  if (old_web_state) {
    // Forward to WebStateDetachedAt as this is considered a webState removal.
    WebStateDetachedAt(web_state_list, old_web_state, index);
  }

  if (new_web_state) {
    AddNewWebState(new_web_state);
  }
}

void WebStateListFaviconDriverObserver::WebStateDetachedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index) {
  favicon::WebFaviconDriver* driver =
      favicon::WebFaviconDriver::FromWebState(web_state);
  if (driver) {
    auto iterator = driver_to_web_state_map_.find(driver);
    DCHECK(iterator != driver_to_web_state_map_.end());
    DCHECK(iterator->second == web_state);
    driver_to_web_state_map_.erase(iterator);
    driver->RemoveObserver(this);
  }
}

void WebStateListFaviconDriverObserver::OnFaviconUpdated(
    favicon::FaviconDriver* driver,
    favicon::FaviconDriverObserver::NotificationIconType icon_type,
    const GURL& icon_url,
    bool icon_url_changed,
    const gfx::Image& image) {
  auto iterator = driver_to_web_state_map_.find(driver);
  DCHECK(iterator != driver_to_web_state_map_.end());
  DCHECK(iterator->second);
  [favicon_observer_ faviconDriver:driver
       didUpdateFaviconForWebState:iterator->second];
}

void WebStateListFaviconDriverObserver::AddNewWebState(
    web::WebState* web_state) {
  favicon::WebFaviconDriver* driver =
      favicon::WebFaviconDriver::FromWebState(web_state);
  if (driver) {
    auto iterator = driver_to_web_state_map_.find(driver);
    DCHECK(iterator == driver_to_web_state_map_.end());
    driver_to_web_state_map_[driver] = web_state;
    driver->AddObserver(this);
  }
}
