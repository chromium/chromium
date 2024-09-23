// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_TAB_HELPER_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_TAB_HELPER_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper_observer.h"

class ContextualPanelTabHelper;

// Observes ContextualPanelTabHelper events in Objective-C.
@protocol ContextualPanelTabHelperObserving <NSObject>
@optional

- (void)contextualPanel:(ContextualPanelTabHelper*)tabHelper
             hasNewData:
                 (std::vector<base::WeakPtr<ContextualPanelItemConfiguration>>)
                     item_configurations;

- (void)contextualPanelTabHelperDestroyed:(ContextualPanelTabHelper*)tabHelper;

- (void)contextualPanelOpened:(ContextualPanelTabHelper*)tabHelper;

- (void)contextualPanelClosed:(ContextualPanelTabHelper*)tabHelper;

@end

// Bridge to observe ContextualPanelTabHelper in Objective-C.
class ContextualPanelTabHelperObserverBridge
    : public ContextualPanelTabHelperObserver {
 public:
  // It it the responsibility of calling code to add/remove the instance
  // as a ContextualPanelTabHelperObserver.
  ContextualPanelTabHelperObserverBridge(
      id<ContextualPanelTabHelperObserving> observer);

  ContextualPanelTabHelperObserverBridge(
      const ContextualPanelTabHelperObserverBridge&) = delete;
  ContextualPanelTabHelperObserverBridge& operator=(
      const ContextualPanelTabHelperObserverBridge&) = delete;

  ~ContextualPanelTabHelperObserverBridge() override;

  // ContextualPanelTabHelperObserver:
  void ContextualPanelHasNewData(
      ContextualPanelTabHelper* tab_helper,
      std::vector<base::WeakPtr<ContextualPanelItemConfiguration>>
          item_configurations) override;
  void ContextualPanelTabHelperDestroyed(
      ContextualPanelTabHelper* tab_helper) override;
  void ContextualPanelOpened(ContextualPanelTabHelper* tab_helper) override;

  // The given ContextualPanelTabHelper has closed its panel UI.
  void ContextualPanelClosed(ContextualPanelTabHelper* tab_helper) override;

 private:
  __weak id<ContextualPanelTabHelperObserving> observer_ = nil;
};

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_TAB_HELPER_OBSERVER_BRIDGE_H_
