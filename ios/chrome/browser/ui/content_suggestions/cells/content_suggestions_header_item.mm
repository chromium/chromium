// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_header_item.h"

#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ContentSuggestionsHeaderItem

@synthesize view;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [ContentSuggestionsHeaderCell class];
  }
  return self;
}

- (void)configureCell:(ContentSuggestionsHeaderCell*)cell {
  [super configureCell:cell];
  [cell setHeaderView:self.view];
  cell.accessibilityIdentifier = [[self class] accessibilityIdentifier];
}

+ (NSString*)accessibilityIdentifier {
  return @"CSHeaderIdentifier";
}

@end

@implementation ContentSuggestionsHeaderCell

@synthesize headerView = _headerView;

- (void)setHeaderView:(UIView*)header {
  [_headerView removeFromSuperview];
  _headerView = header;

  if (!header)
    return;

  header.translatesAutoresizingMaskIntoConstraints = NO;
  [self.contentView addSubview:header];
  AddSameConstraints(self.contentView, header);
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.headerView = nil;
}

@end
