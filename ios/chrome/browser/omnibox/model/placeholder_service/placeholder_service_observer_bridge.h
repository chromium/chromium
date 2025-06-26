// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_PLACEHOLDER_SERVICE_PLACEHOLDER_SERVICE_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_PLACEHOLDER_SERVICE_PLACEHOLDER_SERVICE_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/omnibox/model/placeholder_service/placeholder_service.h"

@protocol PlaceholderServiceObserving <NSObject>
@optional
- (void)placeholderTextUpdated;
- (void)placeholderImageUpdated;
- (void)placeholderServiceShuttingDown:(PlaceholderService*)service;
@end

class PlaceholderServiceObserverBridge : public PlaceholderServiceObserver {
 public:
  PlaceholderServiceObserverBridge(id<PlaceholderServiceObserving> owner,
                                   PlaceholderService* service);

  ~PlaceholderServiceObserverBridge() override;

  void OnPlaceholderTextChanged() override;
  void OnPlaceholderImageChanged() override;
  void OnPlaceholderServiceShuttingDown() override;

 private:
  __weak id<PlaceholderServiceObserving> owner_;
  raw_ptr<PlaceholderService> placeholder_service_;  // weak
};

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_PLACEHOLDER_SERVICE_PLACEHOLDER_SERVICE_OBSERVER_BRIDGE_H_
