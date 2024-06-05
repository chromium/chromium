// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engines/model/search_engine_observer_bridge.h"

SearchEngineObserverBridge::SearchEngineObserverBridge(
    id<SearchEngineObserving> owner,
    TemplateURLService* urlService)
    : owner_(owner), templateURLService_(urlService) {
  templateURLService_->AddObserver(this);
}

SearchEngineObserverBridge::~SearchEngineObserverBridge() {
  templateURLService_->RemoveObserver(this);
}

void SearchEngineObserverBridge::OnTemplateURLServiceChanged() {
  [owner_ searchEngineChanged];
}

void SearchEngineObserverBridge::OnTemplateURLServiceShuttingDown() {
  if ([owner_ respondsToSelector:@selector(templateURLServiceShuttingDown:)]) {
    [owner_ templateURLServiceShuttingDown:templateURLService_];
  }
}
