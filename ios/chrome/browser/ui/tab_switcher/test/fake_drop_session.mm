// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/test/fake_drop_session.h"

#import "ios/chrome/browser/ui/tab_switcher/test/fake_drag_session.h"

@implementation FakeDropSession

- (instancetype)initWithItems:(NSArray<UIDragItem*>*)items {
  self = [super init];
  if (self) {
    _items = items;
    _progressIndicatorStyle = UIDropSessionProgressIndicatorStyleNone;
    _allowsMoveOperation = NO;
    _localDragSession = [[FakeDragSession alloc] initWithItems:items];
    _progress = nil;
    _restrictedToDraggingApplication = YES;
  }
  return self;
}

- (BOOL)canLoadObjectsOfClass:(Class<NSItemProviderReading>)aClass {
  return YES;
}

- (BOOL)hasItemsConformingToTypeIdentifiers:
    (NSArray<NSString*>*)typeIdentifiers {
  return YES;
}

- (UIDropOperation)dropOperation {
  return UIDropOperationCopy;
}

- (NSProgress*)
    loadObjectsOfClass:(Class<NSItemProviderReading>)aClass
            completion:
                (void (^)(NSArray<__kindof id<NSItemProviderReading>>* objects))
                    completion {
  return nil;
}

- (CGPoint)locationInView:(UIView*)view {
  return CGPointZero;
}

@end
