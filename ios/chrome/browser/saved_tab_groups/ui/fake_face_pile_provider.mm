// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/saved_tab_groups/ui/fake_face_pile_provider.h"

#import <UIKit/UIKit.h>

@implementation FakeFacePileProvider

#pragma mark - FacePileProviding

- (CGFloat)facePileWidth {
  return 60;
}

- (UIView*)facePileView {
  return [[UIView alloc] init];
}

- (BOOL)isEqualFacePileProviding:(id<FacePileProviding>)otherProvider {
  return NO;
}

@end
