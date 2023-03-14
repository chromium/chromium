// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/layout_guide_scene_agent.h"

#import "ios/chrome/browser/shared/ui/util/util_swift.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation LayoutGuideSceneAgent

- (instancetype)init {
  self = [super init];
  if (self) {
    _layoutGuideCenter = [[LayoutGuideCenter alloc] init];
    _incognitoLayoutGuideCenter = [[LayoutGuideCenter alloc] init];
  }
  return self;
}

@end
