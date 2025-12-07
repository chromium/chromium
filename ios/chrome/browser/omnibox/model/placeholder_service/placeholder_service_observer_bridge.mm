// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/placeholder_service/placeholder_service_observer_bridge.h"

PlaceholderServiceObserverBridge::PlaceholderServiceObserverBridge(
    id<PlaceholderServiceObserving> owner,
    PlaceholderService* service)
    : owner_(owner), placeholder_service_(service) {
  service->AddObserver(this);
}

PlaceholderServiceObserverBridge::~PlaceholderServiceObserverBridge() {
  placeholder_service_->RemoveObserver(this);
}

void PlaceholderServiceObserverBridge::OnPlaceholderTextChanged() {
  if ([owner_ respondsToSelector:@selector(placeholderTextUpdated)]) {
    [owner_ placeholderTextUpdated];
  }
}

void PlaceholderServiceObserverBridge::OnPlaceholderImageChanged() {
  if ([owner_ respondsToSelector:@selector(placeholderImageUpdated)]) {
    [owner_ placeholderImageUpdated];
  }
}
void PlaceholderServiceObserverBridge::OnPlaceholderServiceShuttingDown() {
  if ([owner_ respondsToSelector:@selector(placeholderServiceShuttingDown:)]) {
    [owner_ placeholderServiceShuttingDown:placeholder_service_];
  }
}
