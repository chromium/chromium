// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_MODEL_INFOBAR_BADGE_TAB_HELPER_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_INFOBARS_MODEL_INFOBAR_BADGE_TAB_HELPER_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/infobars/model/infobar_badge_tab_helper_observer.h"

// Objective-C protocol with methods called through the bridge which are
// equivalent to `InfobarBadgeTabHelperObserver`.
@protocol InfobarBadgeTabHelperObserving <NSObject>

// The given InfobarBadgeTabHelper has an update to its infobar badges.
- (void)infobarBadgesUpdated:(InfobarBadgeTabHelper*)tabHelper;

@end

// Bridge to forward observation of InfobarBadgeTabHelper in Objective-C.
class InfobarBadgeTabHelperObserverBridge
    : public InfobarBadgeTabHelperObserver {
 public:
  explicit InfobarBadgeTabHelperObserverBridge(
      id<InfobarBadgeTabHelperObserving> observer);

  InfobarBadgeTabHelperObserverBridge(
      const InfobarBadgeTabHelperObserverBridge&) = delete;
  InfobarBadgeTabHelperObserverBridge& operator=(
      const InfobarBadgeTabHelperObserverBridge&) = delete;

  ~InfobarBadgeTabHelperObserverBridge() override;

  // InfobarBadgeTabHelperObserver:
  void InfobarBadgesUpdated(InfobarBadgeTabHelper* tab_helper) override;

 private:
  __weak id<InfobarBadgeTabHelperObserving> observer_ = nil;
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_MODEL_INFOBAR_BADGE_TAB_HELPER_OBSERVER_BRIDGE_H_
