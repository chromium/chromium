// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_MODEL_DATA_PROTECTION_TAB_HELPER_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_MODEL_DATA_PROTECTION_TAB_HELPER_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "ios/chrome/browser/enterprise/data_protection/model/data_protection_tab_helper.h"
#import "ios/chrome/browser/enterprise/data_protection/model/data_protection_tab_helper_observer.h"

namespace web {
class WebState;
}

// Objective-C protocol for observing DataProtectionTabHelper changes.
@protocol DataProtectionTabHelperObserving <NSObject>
@optional
- (void)screenshotProtectionDidChangeForWebState:(web::WebState*)webState
                                     isProtected:(BOOL)isProtected;
@end

// Bridge class that observes DataProtectionTabHelper on behalf of
// Objective-C clients.
class DataProtectionTabHelperObserverBridge
    : public DataProtectionTabHelperObserver {
 public:
  DataProtectionTabHelperObserverBridge(
      id<DataProtectionTabHelperObserving> observer,
      DataProtectionTabHelper* tab_helper);
  DataProtectionTabHelperObserverBridge(
      const DataProtectionTabHelperObserverBridge&) = delete;
  DataProtectionTabHelperObserverBridge& operator=(
      const DataProtectionTabHelperObserverBridge&) = delete;
  ~DataProtectionTabHelperObserverBridge() override;

  // DataProtectionTabHelperObserver:
  void ScreenshotProtectionDidChange(
      web::WebState* web_state,
      bool screenshot_protection_enabled) override;
  void DataProtectionTabHelperDestroyed(
      DataProtectionTabHelper* helper) override;

 private:
  __weak id<DataProtectionTabHelperObserving> observer_;
  base::ScopedObservation<DataProtectionTabHelper,
                          DataProtectionTabHelperObserver>
      scoped_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_MODEL_DATA_PROTECTION_TAB_HELPER_OBSERVER_BRIDGE_H_
