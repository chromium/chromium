// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_return_to_recent_tab_item.h"

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_gesture_commands.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGSize regularCellSize = {/*width=*/343, /*height=*/72};
}

@implementation ContentSuggestionsReturnToRecentTabItem
@synthesize metricsRecorded;
@synthesize suggestionIdentifier;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [ContentSuggestionsReturnToRecentTabCell class];
  }
  return self;
}

- (void)configureCell:(ContentSuggestionsReturnToRecentTabCell*)cell {
  [super configureCell:cell];
  [cell setTitle:self.title];
  [cell setSubtitle:self.subtitle];
  cell.accessibilityLabel = self.title;
  if (self.icon) {
    [cell setIconImage:self.icon];
  }
  cell.accessibilityCustomActions = [self customActions];
}

- (CGFloat)cellHeightForWidth:(CGFloat)width {
  return [ContentSuggestionsReturnToRecentTabCell defaultSize].height;
}

// Custom action for a cell configured with this item.
- (NSArray<UIAccessibilityCustomAction*>*)customActions {
  UIAccessibilityCustomAction* openMostRecentTab =
      [[UIAccessibilityCustomAction alloc]
          initWithName:@"Open Most Recent Tab"
                target:self
              selector:@selector(openMostRecentTab)];

  return @[ openMostRecentTab ];
}

- (BOOL)openMostRecentTab {
  // TODO:(crbug.com/1173160) implement.
  return YES;
}

@end

#pragma mark - ContentSuggestionsReturnToRecentTabCell

@interface ContentSuggestionsReturnToRecentTabCell ()

// Favicon image.
@property(nonatomic, strong) UIImageView* iconImageView;

// Title of the most recent tab tile.
@property(nonatomic, strong, readonly) UILabel* titleLabel;

// Subtitle of the most recent tab tile.
@property(nonatomic, strong, readonly) UILabel* subtitleLabel;

@end

@implementation ContentSuggestionsReturnToRecentTabCell

- (void)setTitle:(NSString*)title {
  self.titleLabel.text = title;
}

- (void)setSubtitle:(NSString*)subtitle {
  self.subtitleLabel.text = subtitle;
}

+ (CGSize)defaultSize {
  return regularCellSize;
}

- (void)setIconImage:(UIImage*)image {
  _iconImageView.image = image;
  _iconImageView.hidden = image == nil;
}

@end
