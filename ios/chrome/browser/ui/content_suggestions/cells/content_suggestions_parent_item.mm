// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_parent_item.h"

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_cells_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_action_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_tile_view.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_parent_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_return_to_recent_tab_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_return_to_recent_tab_view.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_shortcut_tile_view.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_tile_layout_util.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_whats_new_view.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ContentSuggestionsParentItem ()

// List of all UITapGestureRecognizers created for the Most Visisted tiles.
@property(nonatomic, strong)
    NSMutableArray<UITapGestureRecognizer*>* mostVisitedTapRecognizers;
// The UILongPressGestureRecognizer for the Return To Recent Tab tile.
@property(nonatomic, strong)
    UITapGestureRecognizer* returnToRecentTabTapRecognizer;
@property(nonatomic, strong)
    UILongPressGestureRecognizer* returnToRecentTabLongPressRecognizer;
// The UITapGestureRecognizer for the NTP promo view.
@property(nonatomic, strong) UITapGestureRecognizer* promoTapRecognizer;

@end

@implementation ContentSuggestionsParentItem
@synthesize metricsRecorded;
@synthesize suggestionIdentifier;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [ContentSuggestionsParentCell class];
  }
  return self;
}

- (void)configureCell:(ContentSuggestionsParentCell*)cell {
  [super configureCell:cell];

  // Remove subviews from StackView in case prepareForReuse was not called (e.g.
  // itemHasChanged: was called).
  [cell removeContentViews];

  CGFloat horizontalSpacing =
      ContentSuggestionsTilesHorizontalSpacing(cell.traitCollection);
  if (self.returnToRecentItem) {
    ContentSuggestionsReturnToRecentTabView* returnToRecentTabTile =
        [[ContentSuggestionsReturnToRecentTabView alloc]
            initWithConfiguration:self.returnToRecentItem];
    self.returnToRecentTabTapRecognizer = [[UITapGestureRecognizer alloc]
        initWithTarget:self.tapTarget
                action:@selector(contentSuggestionsElementTapped:)];
    [returnToRecentTabTile
        addGestureRecognizer:self.returnToRecentTabTapRecognizer];
    self.returnToRecentTabTapRecognizer.enabled = YES;
    // Add long press functionality for the Return to Recent Tab tile.
    self.returnToRecentTabLongPressRecognizer =
        [[UILongPressGestureRecognizer alloc]
            initWithTarget:self.tapTarget
                    action:@selector(contentSuggestionsElementTapped:)];
    self.returnToRecentTabLongPressRecognizer.minimumPressDuration =
        ios::material::kDuration8;
    self.returnToRecentTabLongPressRecognizer.enabled = YES;
    [returnToRecentTabTile
        addGestureRecognizer:self.returnToRecentTabLongPressRecognizer];
    [cell addUIElement:returnToRecentTabTile
        withCustomBottomSpacing:content_suggestions::
                                    kReturnToRecentTabSectionBottomMargin];
    CGFloat cardWidth = content_suggestions::searchFieldWidth(
        cell.bounds.size.width, cell.traitCollection);
    [NSLayoutConstraint activateConstraints:@[
      [returnToRecentTabTile.widthAnchor constraintEqualToConstant:cardWidth],
      [returnToRecentTabTile.heightAnchor
          constraintEqualToConstant:kReturnToRecentTabSize.height]
    ]];
  }
  if (self.whatsNewItem) {
    ContentSuggestionsWhatsNewView* whatsNewView =
        [[ContentSuggestionsWhatsNewView alloc]
            initWithConfiguration:self.whatsNewItem];
    self.promoTapRecognizer = [[UITapGestureRecognizer alloc]
        initWithTarget:self.tapTarget
                action:@selector(contentSuggestionsElementTapped:)];
    [whatsNewView addGestureRecognizer:self.promoTapRecognizer];
    self.promoTapRecognizer.enabled = YES;
    [cell addUIElement:whatsNewView withCustomBottomSpacing:0];
    CGFloat width =
        MostVisitedTilesContentHorizontalSpace(cell.traitCollection);
    CGSize size =
        MostVisitedCellSize(cell.traitCollection.preferredContentSizeCategory);
    [NSLayoutConstraint activateConstraints:@[
      [whatsNewView.widthAnchor constraintEqualToConstant:width],
      [whatsNewView.heightAnchor constraintEqualToConstant:size.height]
    ]];
  }
  if (self.mostVisitedItems) {
    UIStackView* stackView = [[UIStackView alloc] init];
    stackView.axis = UILayoutConstraintAxisHorizontal;
    stackView.alignment = UIStackViewAlignmentTop;
    stackView.distribution = UIStackViewDistributionFillEqually;
    stackView.spacing = horizontalSpacing;
    NSUInteger index = 0;
    for (ContentSuggestionsMostVisitedItem* item in self.mostVisitedItems) {
      ContentSuggestionsMostVisitedTileView* view =
          [[ContentSuggestionsMostVisitedTileView alloc]
              initWithConfiguration:item];
      view.accessibilityIdentifier = [NSString
          stringWithFormat:
              @"%@%li",
              kContentSuggestionsMostVisitedAccessibilityIdentifierPrefix,
              index];
      view.menuProvider = self.menuProvider;
      UITapGestureRecognizer* tapRecognizer = [[UITapGestureRecognizer alloc]
          initWithTarget:self.tapTarget
                  action:@selector(contentSuggestionsElementTapped:)];
      [view addGestureRecognizer:tapRecognizer];
      [self.mostVisitedTapRecognizers addObject:tapRecognizer];
      [stackView addArrangedSubview:view];
      index++;
    }
    [cell addUIElement:stackView
        withCustomBottomSpacing:kMostVisitedBottomMargin];
    CGFloat width =
        MostVisitedTilesContentHorizontalSpace(cell.traitCollection);
    CGSize size =
        MostVisitedCellSize(cell.traitCollection.preferredContentSizeCategory);
    [NSLayoutConstraint activateConstraints:@[
      [stackView.widthAnchor constraintEqualToConstant:width],
      [stackView.heightAnchor constraintEqualToConstant:size.height]
    ]];
  }
  if (self.shortcutsItems) {
    UIStackView* stackView = [[UIStackView alloc] init];
    stackView.axis = UILayoutConstraintAxisHorizontal;
    stackView.alignment = UIStackViewAlignmentTop;
    stackView.distribution = UIStackViewDistributionFillEqually;
    stackView.spacing = horizontalSpacing;
    NSUInteger index = 0;
    for (ContentSuggestionsMostVisitedActionItem* item in self.shortcutsItems) {
      ContentSuggestionsShortcutTileView* view =
          [[ContentSuggestionsShortcutTileView alloc]
              initWithConfiguration:item];
      view.accessibilityIdentifier = [NSString
          stringWithFormat:
              @"%@%li",
              kContentSuggestionsShortcutsAccessibilityIdentifierPrefix, index];
      UITapGestureRecognizer* tapRecognizer = [[UITapGestureRecognizer alloc]
          initWithTarget:self.tapTarget
                  action:@selector(contentSuggestionsElementTapped:)];
      [view addGestureRecognizer:tapRecognizer];
      [self.mostVisitedTapRecognizers addObject:tapRecognizer];
      [stackView addArrangedSubview:view];
      index++;
    }

    [cell addUIElement:stackView withCustomBottomSpacing:0];
    CGFloat width =
        MostVisitedTilesContentHorizontalSpace(cell.traitCollection);
    CGSize size =
        MostVisitedCellSize(cell.traitCollection.preferredContentSizeCategory);
    [NSLayoutConstraint activateConstraints:@[
      [stackView.widthAnchor constraintEqualToConstant:width],
      // The parent StackView is UIStackViewDistributionFill so there will be no
      // spacing below the last element. Add what would be bottom spacing below
      // the last row to the height of this StackView.
      // TODO(crbug.com/1285378): Move this spacing to between the Feed header
      // and the ContentSuggestions parent view when migrating to
      // UIViewController.
      [stackView.heightAnchor
          constraintEqualToConstant:size.height + kMostVisitedBottomMargin],
    ]];
  }
}

// Returns the default height of the content subviews and the spacing in between
// them.
- (CGFloat)cellHeightForWidth:(CGFloat)width {
  CGFloat height = 0;
  if (self.mostVisitedItems) {
    height += MostVisitedCellSize(
                  UIApplication.sharedApplication.preferredContentSizeCategory)
                  .height +
              kMostVisitedBottomMargin;
  }
  if (self.shortcutsItems) {
    height += MostVisitedCellSize(
                  UIApplication.sharedApplication.preferredContentSizeCategory)
                  .height +
              kMostVisitedBottomMargin;
  }
  if (self.returnToRecentItem) {
    height += (kReturnToRecentTabSize.height +
               content_suggestions::kReturnToRecentTabSectionBottomMargin);
  }
  if (self.whatsNewItem) {
    height += MostVisitedCellSize(
                  UIApplication.sharedApplication.preferredContentSizeCategory)
                  .height;
  }
  return height;
}

@end

#pragma mark - ContentSuggestionsParentCell

@interface ContentSuggestionsParentCell ()

// StackView holding all subviews.
@property(nonatomic, strong) UIStackView* verticalStackView;

@end

@implementation ContentSuggestionsParentCell

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _verticalStackView = [[UIStackView alloc] init];
    _verticalStackView.translatesAutoresizingMaskIntoConstraints = NO;
    _verticalStackView.axis = UILayoutConstraintAxisVertical;
    // A centered alignment will ensure the views are centered.
    _verticalStackView.alignment = UIStackViewAlignmentCenter;
    // A fill distribution allows for the custom spacing between elements and
    // height/width configurations for each row.
    _verticalStackView.distribution = UIStackViewDistributionFill;
    [self.contentView addSubview:_verticalStackView];
    AddSameConstraints(self.contentView, _verticalStackView);
  }
  return self;
}

- (void)addUIElement:(UIView*)view withCustomBottomSpacing:(CGFloat)spacing {
  [_verticalStackView addArrangedSubview:view];
  if (spacing > 0) {
    [_verticalStackView setCustomSpacing:spacing afterView:view];
  }
}
- (void)removeContentViews {
  for (UIView* view in [self.verticalStackView arrangedSubviews]) {
    [view removeFromSuperview];
  }
}

- (void)prepareForReuse {
  [super prepareForReuse];
  [self removeContentViews];
}

@end
