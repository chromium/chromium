// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_IMPRESSION_LIMITS_MODEL_IMPRESSION_LIMIT_SERVICE_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_IMPRESSION_LIMITS_MODEL_IMPRESSION_LIMIT_SERVICE_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "ios/chrome/browser/content_suggestions/impression_limits/model/impression_limit_service.h"
#import "url/gurl.h"

// Delegate to receive events from `ImpressionLimitService::Observer`
@protocol ImpressionLimitServiceObserverBridgeDelegate <NSObject>
- (void)impressionLimitService:(ImpressionLimitService*)impressionLimitService
                 didUntrackURL:(GURL)url;
@end

// Observer class for events related to ShopCard impressions.
class ImpressionLimitServiceObserverBridge
    : public ImpressionLimitService::Observer {
 public:
  ImpressionLimitServiceObserverBridge(
      id<ImpressionLimitServiceObserverBridgeDelegate> delegate,
      ImpressionLimitService* service);

  ImpressionLimitServiceObserverBridge(
      const ImpressionLimitServiceObserverBridge&) = delete;
  ImpressionLimitServiceObserverBridge& operator=(
      const ImpressionLimitServiceObserverBridge&) = delete;

  ~ImpressionLimitServiceObserverBridge() override;

  // `ImpressionLimitService::Observer` implementation:
  void OnUntracked(const GURL& url) override;

 private:
  __weak id<ImpressionLimitServiceObserverBridgeDelegate> delegate_ = nil;
  raw_ptr<ImpressionLimitService> service_ = nullptr;
  base::ScopedObservation<ImpressionLimitService,
                          ImpressionLimitService::Observer>
      scoped_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_IMPRESSION_LIMITS_MODEL_IMPRESSION_LIMIT_SERVICE_OBSERVER_BRIDGE_H_
