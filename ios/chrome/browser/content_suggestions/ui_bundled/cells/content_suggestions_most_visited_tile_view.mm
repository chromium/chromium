// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/content_suggestions_most_visited_tile_view.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "components/favicon_base/fallback_icon_style.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/content_suggestions_cells_constants.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/most_visited_tiles_commands.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_constants.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_menu_elements_provider.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/magic_stack/magic_stack_module_content_view_delegate.h"
#import "ios/chrome/browser/favicon/ui_bundled/favicon_attributes_with_payload.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_trait.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "skia/ext/skia_utils_ios.h"
#import "ui/base/l10n/l10n_util.h"

@interface ContentSuggestionsMostVisitedTileView ()

// Command handler for actions.
@property(nonatomic, weak) id<MostVisitedTilesCommands> commandHandler;

// Whether the incognito action should be available.
@property(nonatomic, assign) BOOL incognitoAvailable;

@end

@implementation ContentSuggestionsMostVisitedTileView

@synthesize configuration = _configuration;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame
                     tileType:ContentSuggestionsTileType::kMostVisited];
  if (self) {
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
    [self registerViewForTraitChanges];
  }
  return self;
}

- (instancetype)initWithConfiguration:
    (ContentSuggestionsMostVisitedItem*)config {
  self = [self initWithFrame:CGRectZero];
  if (self) {
    [self setConfiguration:config];
  }
  return self;
}

#pragma mark - UIContentView

- (void)setConfiguration:(id<UIContentConfiguration>)config {
  if (![config isKindOfClass:ContentSuggestionsMostVisitedItem.class]) {
    return;
  }
  ContentSuggestionsMostVisitedItem* item =
      base::apple::ObjCCastStrict<ContentSuggestionsMostVisitedItem>(config);
  BOOL hasPreviousItem = _configuration;
  _configuration = [item copy];
  // Update the layout according to `item`.
  if (IsNTPBackgroundCustomizationEnabled()) {
    [self applyBackgroundColors];
  } else {
    self.imageContainerView.backgroundColor =
        [UIColor colorNamed:kGrey100Color];
  }
  self.titleLabel.text = item.title;
  self.accessibilityLabel = item.title;
  _incognitoAvailable = item.incognitoAvailable;
  _commandHandler = item.commandHandler;
  self.menuElementsProvider = item.menuElementsProvider;
  self.isAccessibilityElement = item;
  self.accessibilityTraits =
      item ? UIAccessibilityTraitButton : UIAccessibilityTraitNone;
  self.accessibilityCustomActions = item ? [self customActions] : @[];
  if (item) {
    [_faviconView configureWithAttributes:item.attributes];
    if (!hasPreviousItem) {
      [self addInteraction:[[UIContextMenuInteraction alloc]
                               initWithDelegate:self]];
    }
  } else {
    // If there is no config, then this is a placeholder tile.
    self.titleLabel.backgroundColor = [UIColor colorNamed:kGrey100Color];
    if (hasPreviousItem) {
      for (id<UIInteraction> interaction in self.interactions) {
        [self removeInteraction:interaction];
      }
    }
  }
  // Update gesture recognizer.
  if (self.tapRecognizer) {
    [self removeGestureRecognizer:self.tapRecognizer];
  }
  UITapGestureRecognizer* tapRecognizer = [[UITapGestureRecognizer alloc]
      initWithTarget:item.commandHandler
              action:@selector(mostVisitedTileTapped:)];
  self.tapRecognizer = tapRecognizer;
  [self addGestureRecognizer:tapRecognizer];
  tapRecognizer.enabled = YES;
}

#pragma mark - UIContextMenuInteractionDelegate

- (UIContextMenuConfiguration*)contextMenuInteraction:
                                   (UIContextMenuInteraction*)interaction
                       configurationForMenuAtLocation:(CGPoint)location {
  NSArray<UIMenuElement*>* elements = [self.menuElementsProvider
      defaultContextMenuElementsForItem:[self mostVisitedItem]
                               fromView:self];
  UIContextMenuActionProvider actionProvider =
      ^(NSArray<UIMenuElement*>* suggestedActions) {
        return [UIMenu menuWithTitle:@"" children:elements];
      };
  return
      [UIContextMenuConfiguration configurationWithIdentifier:nil
                                              previewProvider:nil
                                               actionProvider:actionProvider];
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
                           IsContentSuggestionsCustomizable()
                               ? IDS_IOS_CONTENT_SUGGESTIONS_NEVER_SHOW_SITE
                               : IDS_IOS_CONTENT_SUGGESTIONS_REMOVE)
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
  [self.commandHandler openNewTabWithMostVisitedItem:[self mostVisitedItem]
                                           incognito:NO];
  return YES;
}

// Target for custom action.
- (BOOL)openInNewIncognitoTab {
  DCHECK(self.commandHandler);
  [self.commandHandler openNewTabWithMostVisitedItem:[self mostVisitedItem]
                                           incognito:YES];
  return YES;
}

// Target for custom action.
- (BOOL)removeMostVisited {
  DCHECK(self.commandHandler);
  [self.commandHandler removeMostVisited:[self mostVisitedItem]];
  return YES;
}

#pragma mark - NewTabPageColorUpdating

- (void)applyBackgroundColors {
  NewTabPageColorPalette* colorPalette =
      [self.traitCollection objectForNewTabPageTrait];
  // Favicon monogram will only be applied if defaultBackgroundColor is set.
  ContentSuggestionsMostVisitedItem* configuration = [self mostVisitedItem];
  if (configuration.attributes.defaultBackgroundColor) {
    if (colorPalette) {
      // If a color palette is available, apply its tint and background
      // colors to the attributes while preserving the other attributes.
      configuration.attributes = [FaviconAttributesWithPayload
          attributesWithMonogram:configuration.attributes.monogramString
                       textColor:colorPalette.secondaryCellColor
                 backgroundColor:colorPalette.monogramColor
          defaultBackgroundColor:configuration.attributes
                                     .defaultBackgroundColor];
    } else {
      // If no color palette is available, fall back to default icon style
      // colors.
      std::unique_ptr<favicon_base::FallbackIconStyle> default_icon_style =
          std::make_unique<favicon_base::FallbackIconStyle>();

      configuration.attributes = [FaviconAttributesWithPayload
          attributesWithMonogram:configuration.attributes.monogramString
                       textColor:skia::UIColorFromSkColor(
                                     default_icon_style->text_color)
                 backgroundColor:skia::UIColorFromSkColor(
                                     default_icon_style->background_color)
          defaultBackgroundColor:default_icon_style->
                                 is_default_background_color];
    }
  }

  // Update the favicon view with the new attributes.
  [self.faviconView configureWithAttributes:configuration.attributes];

  if (colorPalette) {
    self.imageContainerView.backgroundColor = colorPalette.tertiaryColor;
  } else {
    self.imageContainerView.backgroundColor =
        [UIColor colorNamed:kGrey100Color];
  }
}

#pragma mark - Private

// Returns the `ContentSuggestionsMostVisitedItem` casted `self.configuration`.
- (ContentSuggestionsMostVisitedItem*)mostVisitedItem {
  return base::apple::ObjCCastStrict<ContentSuggestionsMostVisitedItem>(
      self.configuration);
}

// Registers a list of UITraits to observe and invokes the
// `applyBackgroundColors` function whenever one of the observed trait's values
// change.
- (void)registerViewForTraitChanges {
  if (IsNTPBackgroundCustomizationEnabled()) {
    [self registerForTraitChanges:@[ NewTabPageTrait.class ]
                       withAction:@selector(applyBackgroundColors)];
  }
}

@end
