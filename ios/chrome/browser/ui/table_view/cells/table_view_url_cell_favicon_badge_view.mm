// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/cells/table_view_url_cell_favicon_badge_view.h"

#import "ios/chrome/browser/ui/reading_list/reading_list_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TableViewURLCellFaviconBadgeView

- (instancetype)init {
  if (self = [super init]) {
    self.hidden = YES;
    self.accessibilityIdentifier = [[self class] accessibilityIdentifier];
  }
  return self;
}

#pragma mark - Public

+ (NSString*)accessibilityIdentifier {
  return kTableViewURLCellFaviconBadgeViewID;
}

#pragma mark - UIImageView

- (void)setImage:(UIImage*)image {
  [super setImage:image];
  self.hidden = !image;
}

@end
