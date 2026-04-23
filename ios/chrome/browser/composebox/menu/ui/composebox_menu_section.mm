// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_section.h"

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_item.h"

@implementation ComposeboxMenuSection

- (instancetype)initWithTitle:(NSString*)title
                        items:(NSArray<ComposeboxMenuItem*>*)items {
  self = [super init];
  if (self) {
    _title = [title copy];
    _items = [items copy];
  }
  return self;
}

@end
