// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/chrome_activity_item_thumbnail_generator.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/web/public/web_state.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ChromeActivityItemThumbnailGenerator () <CRWWebStateObserver> {
  // WebState to be used for generating the snapshot.
  web::WebState* _webState;
  // Bridges WebStateObserver methods to this object.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
}
@end

@implementation ChromeActivityItemThumbnailGenerator

#pragma mark - Initializers

- (instancetype)initWithWebState:(web::WebState*)webState {
  DCHECK(webState);
  self = [super init];
  if (self) {
    // Thumbnail shouldn't be generated for incognito tabs. So there is no need
    // to observe the webState.
    if (webState->GetBrowserState()->IsOffTheRecord())
      return self;
    _webState = webState;
    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    _webState->AddObserver(_webStateObserver.get());
  }
  return self;
}

- (void)dealloc {
  if (_webState)
    _webState->RemoveObserver(_webStateObserver.get());
}

#pragma mark - Public methods

- (UIImage*)thumbnailWithSize:(const CGSize&)size {
  if (!_webState)
    return nil;
  UIImage* snapshot = SnapshotTabHelper::FromWebState(_webState)
                          ->GenerateSnapshotWithoutOverlays();
  if (!snapshot)
    return nil;
  return ResizeImage(snapshot, size, ProjectionMode::kAspectFillAlignTop,
                     /*opaque=*/YES);
}

#pragma mark - Private methods
#pragma mark - CRWWebStateObserver protocol

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  _webState->RemoveObserver(_webStateObserver.get());
  _webStateObserver.reset();
  _webState = nullptr;
}

@end
