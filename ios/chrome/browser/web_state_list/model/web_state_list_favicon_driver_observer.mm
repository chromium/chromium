// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/model/web_state_list_favicon_driver_observer.h"

#import "components/favicon/ios/web_favicon_driver.h"

#import "base/check.h"

WebStateListFaviconDriverObserver::WebStateListFaviconDriverObserver(
    WebStateList* web_state_list,
    id<WebStateFaviconDriverObserver> observer)
    : favicon_observer_(observer) {
  web_state_list_observation_.Observe(web_state_list);
  for (int i = 0; i < web_state_list->count(); ++i) {
    AddNewWebState(web_state_list->GetWebStateAt(i));
  }
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

#pragma mark - WebStateListObserver

void WebStateListFaviconDriverObserver::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly:
      // Do nothing when a WebState is selected and its status is updated.
      break;
    case WebStateListChange::Type::kDetach: {
      const WebStateListChangeDetach& detach_change =
          change.As<WebStateListChangeDetach>();
      DetachWebState(detach_change.detached_web_state());
      break;
    }
    case WebStateListChange::Type::kMove:
      // Do nothing when a WebState is moved.
      break;
    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replace_change =
          change.As<WebStateListChangeReplace>();
      // Forward to DetachWebState as this is considered a WebState removal.
      DetachWebState(replace_change.replaced_web_state());
      AddNewWebState(replace_change.inserted_web_state());
      break;
    }
    case WebStateListChange::Type::kInsert: {
      const WebStateListChangeInsert& insert_change =
          change.As<WebStateListChangeInsert>();
      AddNewWebState(insert_change.inserted_web_state());
      break;
    }
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

void WebStateListFaviconDriverObserver::DetachWebState(
    web::WebState* web_state) {
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
