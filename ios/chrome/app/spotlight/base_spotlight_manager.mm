// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/spotlight/base_spotlight_manager.h"

#import "ios/chrome/app/spotlight/searchable_item_factory.h"
#import "ios/chrome/app/spotlight/spotlight_interface.h"

@implementation BaseSpotlightManager

- (instancetype)
    initWithSpotlightInterface:(SpotlightInterface*)spotlightInterface
         searchableItemFactory:(SearchableItemFactory*)searchableItemFactory {
  self = [super init];
  if (self) {
    _isShuttingDown = NO;
    _spotlightInterface = spotlightInterface;
    _searchableItemFactory = searchableItemFactory;
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(appDidEnterBackground)
               name:UIApplicationDidEnterBackgroundNotification
             object:nil];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(appWillEnterForeground)
               name:UIApplicationWillEnterForegroundNotification
             object:nil];
  }
  return self;
}

- (void)shutdown {
  [self.searchableItemFactory cancelItemsGeneration];
  _isShuttingDown = YES;
}

- (void)appDidEnterBackground {
  self.isAppInBackground = YES;
  [self.searchableItemFactory cancelItemsGeneration];
}

- (void)appWillEnterForeground {
  self.isAppInBackground = NO;
}

@end
