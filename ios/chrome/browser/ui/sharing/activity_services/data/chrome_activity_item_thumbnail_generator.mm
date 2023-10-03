// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/activity_services/data/chrome_activity_item_thumbnail_generator.h"

#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/web/public/web_state.h"

@implementation ChromeActivityItemThumbnailGenerator {
  // WebState used for generating the snapshot.
  base::WeakPtr<web::WebState> _weakWebState;
}

#pragma mark - Initializers

- (instancetype)initWithWebState:(web::WebState*)webState {
  if ((self = [super init])) {
    _weakWebState = webState->GetWeakPtr();
  }
  return self;
}

#pragma mark - Public methods

- (UIImage*)thumbnailWithSize:(const CGSize&)size {
  web::WebState* webState = _weakWebState.get();
  if (!webState) {
    return nil;
  }
  UIImage* snapshot = SnapshotTabHelper::FromWebState(webState)
                          ->GenerateSnapshotWithoutOverlays();
  if (!snapshot) {
    return nil;
  }
  return ResizeImage(snapshot, size, ProjectionMode::kAspectFillAlignTop,
                     /*opaque=*/YES);
}

@end
