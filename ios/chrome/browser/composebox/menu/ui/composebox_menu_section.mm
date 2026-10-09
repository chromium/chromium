// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_section.h"

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_item.h"

@implementation ComposeboxMenuSection

- (instancetype)initWithTitle:(NSString*)title
                        items:(NSArray<ComposeboxMenuItem*>*)items
                   identifier:(ComposeboxMenuSectionIdentifier)identifier {
  return [self initWithTitle:title
                         items:items
                    identifier:identifier
      showAsHorizontalCarousel:NO];
}

- (instancetype)initWithTitle:(NSString*)title
                        items:(NSArray<ComposeboxMenuItem*>*)items
                   identifier:(ComposeboxMenuSectionIdentifier)identifier
     showAsHorizontalCarousel:(BOOL)showAsHorizontalCarousel {
  self = [super init];
  if (self) {
    _title = [title copy];
    _items = [items copy];
    _identifier = identifier;
    _showAsHorizontalCarousel = showAsHorizontalCarousel;
  }
  return self;
}

@end
