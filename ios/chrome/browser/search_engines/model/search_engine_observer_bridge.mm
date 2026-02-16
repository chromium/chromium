// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engines/model/search_engine_observer_bridge.h"

SearchEngineObserverBridge::SearchEngineObserverBridge(
    id<SearchEngineObserving> owner,
    TemplateURLService* urlService)
    : owner_(owner), template_url_service_(urlService) {
  template_url_service_->AddObserver(this);
}

SearchEngineObserverBridge::~SearchEngineObserverBridge() {
  if (template_url_service_) {
    template_url_service_->RemoveObserver(this);
    template_url_service_ = nullptr;
  }
}

void SearchEngineObserverBridge::OnTemplateURLServiceChanged() {
  [owner_ searchEngineChanged];
}

void SearchEngineObserverBridge::OnTemplateURLServiceShuttingDown() {
  if ([owner_ respondsToSelector:@selector(templateURLServiceShuttingDown:)]) {
    [owner_ templateURLServiceShuttingDown:template_url_service_];
  }
  if (template_url_service_) {
    template_url_service_->RemoveObserver(this);
    template_url_service_ = nullptr;
  }
}
