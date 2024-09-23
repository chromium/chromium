// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_tile_view.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_cells_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/most_visited_tiles_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_menu_provider.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface ContentSuggestionsMostVisitedTileView ()

// Command handler for actions.
@property(nonatomic, weak) id<MostVisitedTilesCommands> commandHandler;

// Whether the incognito action should be available.
@property(nonatomic, assign) BOOL incognitoAvailable;

@end

@implementation ContentSuggestionsMostVisitedTileView

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame
                     tileType:ContentSuggestionsTileType::kMostVisited];
  if (self) {
      self.imageContainerView.backgroundColor =
          [UIColor colorNamed:kGrey100Color];
      self.imageContainerView.layer.cornerRadius =
          kMagicStackImageContainerWidth / 2;
      self.imageContainerView.layer.masksToBounds = NO;
      self.imageContainerView.clipsToBounds = YES;

      UIStackView* stackView = [[UIStackView alloc] init];
      stackView.translatesAutoresizingMaskIntoConstraints = NO;
      stackView.axis = UILayoutConstraintAxisVertical;
      stackView.spacing = 10;
      stackView.alignment = UIStackViewAlignmentCenter;
      stackView.distribution = UIStackViewDistributionFill;

      [stackView addArrangedSubview:self.imageContainerView];
      [stackView addArrangedSubview:self.titleLabel];

      [NSLayoutConstraint activateConstraints:@[
        [self.imageContainerView.widthAnchor
            constraintEqualToConstant:kMagicStackImageContainerWidth],
        [self.imageContainerView.heightAnchor
            constraintEqualToAnchor:self.imageContainerView.widthAnchor],
      ]];

      [self addSubview:stackView];
      AddSameConstraints(stackView, self);

    _faviconView = [[FaviconView alloc] init];
    _faviconView.font = [UIFont systemFontOfSize:22];
    _faviconView.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[
      [_faviconView.heightAnchor
          constraintEqualToConstant:kMagicStackFaviconWidth],
      [_faviconView.widthAnchor
          constraintEqualToAnchor:_faviconView.heightAnchor],
    ]];

    [self addSubview:_faviconView];
    AddSameCenterConstraints(_faviconView, self.imageContainerView);
  }
  return self;
}

- (instancetype)initWithConfiguration:
    (ContentSuggestionsMostVisitedItem*)config {
  self = [self initWithFrame:CGRectZero];
  if (self) {
    if (!config) {
      // If there is no config, then this is a placeholder tile.
      self.titleLabel.backgroundColor = [UIColor colorNamed:kGrey100Color];
    } else {
      _config = config;
      self.titleLabel.text = config.title;
      self.accessibilityLabel = config.title;
      _incognitoAvailable = config.incognitoAvailable;
      [_faviconView configureWithAttributes:config.attributes];
      _commandHandler = config.commandHandler;
      self.isAccessibilityElement = YES;
      self.accessibilityTraits = UIAccessibilityTraitButton;
      self.accessibilityCustomActions = [self customActions];
      [self addInteraction:[[UIContextMenuInteraction alloc]
                               initWithDelegate:self]];
    }
  }
  return self;
}

#pragma mark - UIContextMenuInteractionDelegate

- (UIContextMenuConfiguration*)contextMenuInteraction:
                                   (UIContextMenuInteraction*)interaction
                       configurationForMenuAtLocation:(CGPoint)location {
  return [self.menuProvider contextMenuConfigurationForItem:self.config
                                                   fromView:self];
}

- (UITargetedPreview*)contextMenuInteraction:
                          (UIContextMenuInteraction*)interaction
    previewForHighlightingMenuWithConfiguration:
        (UIContextMenuConfiguration*)configuration {
  // This ensures that the background of the context menu matches the background
  // behind the tile.
  UIPreviewParameters* previewParameters = [[UIPreviewParameters alloc] init];
  previewParameters.backgroundColor =
      [UIColor colorNamed:kGroupedSecondaryBackgroundColor];
  CGRect previewPath = CGRectInset(interaction.view.bounds, -2, -12);
  previewParameters.visiblePath =
      [UIBezierPath bezierPathWithRoundedRect:previewPath cornerRadius:12];
  return [[UITargetedPreview alloc] initWithView:self
                                      parameters:previewParameters];
}

#pragma mark - AccessibilityCustomAction

// Custom action for a cell configured with this item.
- (NSArray<UIAccessibilityCustomAction*>*)customActions {
  UIAccessibilityCustomAction* openInNewTab =
      [[UIAccessibilityCustomAction alloc]
          initWithName:l10n_util::GetNSString(
                           IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)
                target:self
              selector:@selector(openInNewTab)];
  UIAccessibilityCustomAction* removeMostVisited =
      [[UIAccessibilityCustomAction alloc]
          initWithName:l10n_util::GetNSString(
                           IDS_IOS_CONTENT_SUGGESTIONS_REMOVE)
                target:self
              selector:@selector(removeMostVisited)];

  NSMutableArray* actions =
      [NSMutableArray arrayWithObjects:openInNewTab, removeMostVisited, nil];
  if (self.incognitoAvailable) {
    UIAccessibilityCustomAction* openInNewIncognitoTab =
        [[UIAccessibilityCustomAction alloc]
            initWithName:l10n_util::GetNSString(
                             IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB)
                  target:self
                selector:@selector(openInNewIncognitoTab)];
    [actions addObject:openInNewIncognitoTab];
  }
  return actions;
}

// Target for custom action.
- (BOOL)openInNewTab {
  DCHECK(self.commandHandler);
  [self.commandHandler openNewTabWithMostVisitedItem:self.config incognito:NO];
  return YES;
}

// Target for custom action.
- (BOOL)openInNewIncognitoTab {
  DCHECK(self.commandHandler);
  [self.commandHandler openNewTabWithMostVisitedItem:self.config incognito:YES];
  return YES;
}

// Target for custom action.
- (BOOL)removeMostVisited {
  DCHECK(self.commandHandler);
  [self.commandHandler removeMostVisited:self.config];
  return YES;
}

@end
