// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_return_to_recent_tab_item.h"

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_cells_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_gesture_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_return_to_recent_tab_view.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
}

- (CGFloat)cellHeightForWidth:(CGFloat)width {
  return kReturnToRecentTabSize.height;
}

@end

#pragma mark - ContentSuggestionsReturnToRecentTabCell

@interface ContentSuggestionsReturnToRecentTabCell ()

// Container view holding Return to Recent Tab tile.
@property(nonatomic, strong)
    ContentSuggestionsReturnToRecentTabView* recentTabView;

@end

@implementation ContentSuggestionsReturnToRecentTabCell

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _recentTabView =
        [[ContentSuggestionsReturnToRecentTabView alloc] initWithFrame:frame];
    [self.contentView addSubview:_recentTabView];
    _recentTabView.translatesAutoresizingMaskIntoConstraints = NO;
    AddSameConstraints(self.contentView, _recentTabView);
    self.isAccessibilityElement = YES;
  }
  return self;
}

- (void)setHighlighted:(BOOL)highlighted {
  [super setHighlighted:highlighted];
  __weak ContentSuggestionsReturnToRecentTabCell* weakSelf = self;
  [UIView transitionWithView:self
                    duration:ios::material::kDuration8
                     options:UIViewAnimationOptionCurveEaseInOut
                  animations:^{
                    weakSelf.recentTabView.backgroundColor =
                        highlighted ? [UIColor colorNamed:kGrey100Color]
                                    : [UIColor clearColor];
                  }
                  completion:nil];
}

- (void)setTitle:(NSString*)title {
  self.recentTabView.titleLabel.text = title;
}

- (void)setSubtitle:(NSString*)subtitle {
  self.recentTabView.subtitleLabel.text = subtitle;
}

+ (CGSize)defaultSize {
  return kReturnToRecentTabSize;
}

- (void)setIconImage:(UIImage*)image {
  self.recentTabView.iconImageView.image = image;
  self.recentTabView.iconImageView.hidden = image == nil;
}

@end
