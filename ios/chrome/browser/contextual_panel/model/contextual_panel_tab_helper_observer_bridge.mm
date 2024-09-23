// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper_observer_bridge.h"

ContextualPanelTabHelperObserverBridge::ContextualPanelTabHelperObserverBridge(
    id<ContextualPanelTabHelperObserving> observer)
    : observer_(observer) {}

ContextualPanelTabHelperObserverBridge::
    ~ContextualPanelTabHelperObserverBridge() = default;

void ContextualPanelTabHelperObserverBridge::ContextualPanelHasNewData(
    ContextualPanelTabHelper* tab_helper,
    std::vector<base::WeakPtr<ContextualPanelItemConfiguration>>
        item_configurations) {
  if ([observer_ respondsToSelector:@selector(contextualPanel:hasNewData:)]) {
    [observer_ contextualPanel:tab_helper hasNewData:item_configurations];
  }
}

void ContextualPanelTabHelperObserverBridge::ContextualPanelTabHelperDestroyed(
    ContextualPanelTabHelper* tab_helper) {
  if ([observer_
          respondsToSelector:@selector(contextualPanelTabHelperDestroyed:)]) {
    [observer_ contextualPanelTabHelperDestroyed:tab_helper];
  }
}

void ContextualPanelTabHelperObserverBridge::ContextualPanelOpened(
    ContextualPanelTabHelper* tab_helper) {
  if ([observer_ respondsToSelector:@selector(contextualPanelOpened:)]) {
    [observer_ contextualPanelOpened:tab_helper];
  }
}

void ContextualPanelTabHelperObserverBridge::ContextualPanelClosed(
    ContextualPanelTabHelper* tab_helper) {
  if ([observer_ respondsToSelector:@selector(contextualPanelClosed:)]) {
    [observer_ contextualPanelClosed:tab_helper];
  }
}
