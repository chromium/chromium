// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/table_view/table_view_url_cell_favicon_badge_view.h"

#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"

@implementation TableViewURLCellFaviconBadgeView

- (instancetype)init {
  if ((self = [super init])) {
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
