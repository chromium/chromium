// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_item.h"

@implementation ComposeboxMenuItem

- (instancetype)initWithTitle:(NSString*)title
                        image:(UIImage*)image
                         type:(ComposeboxMenuItemType)type
                     disabled:(BOOL)disabled {
  self = [super init];
  if (self) {
    _title = [title copy];
    _image = image;
    _type = type;
    _disabled = disabled;
  }
  return self;
}

- (instancetype)initWithTitle:(NSString*)title
                        image:(UIImage*)image
                         type:(ComposeboxMenuItemType)type {
  return [self initWithTitle:title image:image type:type disabled:NO];
}

@end
