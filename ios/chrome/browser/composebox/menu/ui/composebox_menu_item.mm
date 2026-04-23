// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_item.h"

@implementation ComposeboxMenuItem

- (instancetype)initWithTitle:(NSString*)title
                        image:(UIImage*)image
                         type:(ComposeboxMenuItemType)type {
  self = [super init];
  if (self) {
    _title = [title copy];
    _image = image;
    _type = type;
  }
  return self;
}

@end
