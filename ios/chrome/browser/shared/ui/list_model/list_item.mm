// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/list_model/list_item.h"

#import "ios/chrome/browser/shared/ui/list_model/list_item+Controller.h"

@implementation ListItem

@synthesize type = _type;
@synthesize cellClass = _cellClass;
@synthesize accessibilityIdentifier = _accessibilityIdentifier;

- (instancetype)initWithType:(NSInteger)type {
  if ((self = [super init])) {
    _type = type;
  }
  return self;
}

- (instancetype)init {
  return [self initWithType:0];
}

- (void)setCellClass:(Class)cellClass {
  _cellClass = cellClass;
}

@end

@implementation ListItem (Controller)

- (void)setType:(NSInteger)type {
  _type = type;
}

@end
