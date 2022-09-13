// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zoom/zoom_event_manager.h"

#include <memory>

#include "base/observer_list.h"
#include "components/zoom/zoom_event_manager_observer.h"
#include "content/public/browser/browser_context.h"

namespace {
static const char kBrowserZoomEventManager[] = "browser_zoom_event_manager";
}

namespace zoom {

ZoomEventManager* ZoomEventManager::GetForBrowserContext(
    content::BrowserContext* context) {
  if (!context->GetUserData(kBrowserZoomEventManager)) {
    context->SetUserData(kBrowserZoomEventManager,
                         std::make_unique<ZoomEventManager>());
  }
  return static_cast<ZoomEventManager*>(
      context->GetUserData(kBrowserZoomEventManager));
}

ZoomEventManager::ZoomEventManager() {}

ZoomEventManager::~ZoomEventManager() {}

void ZoomEventManager::OnZoomLevelChanged(
    const content::HostZoomMap::ZoomLevelChange& change) {
  zoom_level_changed_callbacks_.Notify(change);
}

base::CallbackListSubscription ZoomEventManager::AddZoomLevelChangedCallback(
    content::HostZoomMap::ZoomLevelChangedCallback callback) {
  return zoom_level_changed_callbacks_.Add(std::move(callback));
}

void ZoomEventManager::OnDefaultZoomLevelChanged() {
  for (auto& observer : observers_)
    observer.OnDefaultZoomLevelChanged();
}

void ZoomEventManager::AddObserver(ZoomEventManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void ZoomEventManager::RemoveObserver(ZoomEventManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace zoom
