// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/saved_tab_groups/ui/fake_face_pile_consumer.h"

@implementation FakeFacePileConsumer

- (void)setSharedButtonWhenEmpty:(BOOL)showsShareButtonWhenEmpty {
  _lastShowsShareButtonWhenEmpty = showsShareButtonWhenEmpty;
}

- (void)setFacePileBackgroundColor:(UIColor*)backgroundColor {
  _lastFacePileBackgroundColor = backgroundColor;
}

- (void)setAvatarSize:(CGFloat)avatarSize {
  _lastAvatarSize = avatarSize;
}

- (void)updateWithFaces:(NSArray<id<ShareKitAvatarPrimitive>>*)faces
            totalNumber:(NSInteger)totalNumber {
  _lastFaces = faces;
  _lastTotalNumber = totalNumber;
  _updateWithViewsCallCount++;
}

@end
