// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/test/fake_drag_session.h"

@implementation FakeDragSession

- (instancetype)initWithItems:(NSArray<UIDragItem*>*)items {
  self = [super init];
  if (self) {
    _items = items;
    _allowsMoveOperation = YES;
    _restrictedToDraggingApplication = NO;
    _localContext = nil;
  }
  return self;
}

- (CGPoint)locationInView:(UIView*)view {
  return CGPointZero;
}

- (BOOL)hasItemsConformingToTypeIdentifiers:
    (NSArray<NSString*>*)typeIdentifiers {
  return YES;
}

- (BOOL)canLoadObjectsOfClass:(Class<NSItemProviderReading>)aClass {
  return YES;
}

@end
