// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/content_widget_extension/content_widget_view.h"

#include "base/logging.h"
#include "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/favicon/favicon_view.h"
#import "ios/chrome/common/ntp_tile/ntp_tile.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#import "ios/chrome/content_widget_extension/most_visited_tile_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Spacing between tiles.
const CGFloat kTileSpacing = 16;
// Height of a tile row.
const CGFloat kTileHeight = 100;
// Number of rows in the widget. Note that modifying this value will not add
// extra rows and will break functionality unless additional changes are made.
const int kRows = 2;
}  // namespace

@interface ContentWidgetView ()

// The number of icons to show per row.
@property(nonatomic, assign) int iconsPerRow;
// The first row of sites.
@property(nonatomic, strong) UIView* firstRow;
// The second row of sites.
@property(nonatomic, strong) UIView* secondRow;
// The height used in the compact display mode.
@property(nonatomic) CGFloat compactHeight;
// The first row's height constraint. Set its constant to modify the first row's
// height.
@property(nonatomic, strong) NSLayoutConstraint* firstRowHeightConstraint;
// Whether the second row of sites should be shown. False if there are no sites
// to show in that row.
@property(nonatomic, readonly) BOOL shouldShowSecondRow;
// The number of sites to display.
@property(nonatomic, assign) int siteCount;
// The most visited tile views; tiles remain in this array even when hidden.
@property(nonatomic, strong) NSArray<MostVisitedTileView*>* mostVisitedTiles;
// The delegate for actions in the view.
@property(nonatomic, weak) id<ContentWidgetViewDelegate> delegate;

// Sets up the widget UI in compact mode.
- (void)createUI;

// Arranges |tiles| horizontally in a view and returns the view.
- (UIView*)createRowFromTiles:(NSArray<MostVisitedTileView*>*)tiles;

// Returns the height to use for the first row, depending on the display mode.
- (CGFloat)firstRowHeight:(BOOL)compact;

// Returns the height to use for the second row (can be 0 if the row should not
// be shown).
- (CGFloat)secondRowHeight;

// Opens the |mostVisitedTile|'s url, using the delegate.
- (void)openURLFromMostVisited:(MostVisitedTileView*)mostVisitedTile;

@end

@implementation ContentWidgetView

@synthesize iconsPerRow = _iconsPerRow;
@synthesize firstRow = _firstRow;
@synthesize secondRow = _secondRow;
@synthesize compactHeight = _compactHeight;
@synthesize firstRowHeightConstraint = _firstRowHeightConstraint;
@synthesize siteCount = _siteCount;
@synthesize mostVisitedTiles = _mostVisitedTiles;
@synthesize delegate = _delegate;

- (instancetype)initWithDelegate:(id<ContentWidgetViewDelegate>)delegate
                   compactHeight:(CGFloat)compactHeight
                           width:(CGFloat)width {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    DCHECK(delegate);
    _delegate = delegate;
    _compactHeight = compactHeight;

    // At least 1 tile and at most 4.
    _iconsPerRow =
        MIN(4, MAX(width / (MostVisitedTileView.tileWidth + kTileSpacing), 1));

    [self createUI];
  }
  return self;
}

#pragma mark - properties

- (CGFloat)widgetExpandedHeight {
  return [self firstRowHeight:NO] + [self secondRowHeight];
}

- (BOOL)shouldShowSecondRow {
  return self.siteCount > self.iconsPerRow;
}

#pragma mark - UI creation

- (void)createUI {
  NSMutableArray* tiles = [[NSMutableArray alloc] init];
  for (int i = 0; i < _iconsPerRow * kRows; i++) {
    [tiles addObject:[[MostVisitedTileView alloc] init]];
  }
  _mostVisitedTiles = tiles;

  _firstRow = [self
      createRowFromTiles:[tiles
                             subarrayWithRange:NSMakeRange(0, _iconsPerRow)]];
  _secondRow = [self
      createRowFromTiles:[tiles subarrayWithRange:NSMakeRange(_iconsPerRow,
                                                              _iconsPerRow)]];

  [self addSubview:_firstRow];
  [self addSubview:_secondRow];

  _firstRowHeightConstraint = [_firstRow.heightAnchor
      constraintEqualToConstant:[self firstRowHeight:YES]];

  [NSLayoutConstraint activateConstraints:@[
    [_firstRow.topAnchor constraintEqualToAnchor:self.topAnchor],
    [_secondRow.topAnchor constraintEqualToAnchor:_firstRow.bottomAnchor],
    [self.leadingAnchor constraintEqualToAnchor:_firstRow.leadingAnchor],
    [self.leadingAnchor constraintEqualToAnchor:_secondRow.leadingAnchor],
    [self.trailingAnchor constraintEqualToAnchor:_firstRow.trailingAnchor],
    [self.trailingAnchor constraintEqualToAnchor:_secondRow.trailingAnchor],
    _firstRowHeightConstraint,
  ]];
}

- (UIView*)createRowFromTiles:(NSArray<MostVisitedTileView*>*)tiles {
  UIStackView* stack = [[UIStackView alloc] initWithArrangedSubviews:tiles];
  stack.translatesAutoresizingMaskIntoConstraints = NO;
  stack.axis = UILayoutConstraintAxisHorizontal;
  stack.alignment = UIStackViewAlignmentTop;
  stack.distribution = UIStackViewDistributionEqualSpacing;
  stack.layoutMargins = UIEdgeInsetsZero;
  stack.spacing = kTileSpacing;
  stack.layoutMarginsRelativeArrangement = YES;

  UIView* container = [[UIView alloc] initWithFrame:CGRectZero];
  container.translatesAutoresizingMaskIntoConstraints = NO;
  [container addSubview:stack];

  [NSLayoutConstraint activateConstraints:@[
    [stack.centerYAnchor constraintEqualToAnchor:container.centerYAnchor],
    [stack.centerXAnchor constraintEqualToAnchor:container.centerXAnchor],
    [container.heightAnchor constraintGreaterThanOrEqualToConstant:kTileHeight],
  ]];

  return container;
}

- (void)updateSites:(NSDictionary<NSURL*, NTPTile*>*)sites {
  for (NTPTile* site in sites.objectEnumerator) {
    // If the site's position is > the # of tiles shown, there is no tile to
    // update. Remember that sites is a dictionary and is not ordered by
    // position.
    if (static_cast<NSUInteger>(site.position) >= self.mostVisitedTiles.count) {
      continue;
    }
    MostVisitedTileView* tileView = self.mostVisitedTiles[site.position];
    tileView.titleLabel.text = site.title;
    tileView.URL = site.URL;

    FaviconAttributes* attributes = nil;

    if (site.faviconFileName) {
      NSURL* filePath = [app_group::ContentWidgetFaviconsFolder()
          URLByAppendingPathComponent:site.faviconFileName];
      UIImage* faviconImage = [UIImage imageWithContentsOfFile:filePath.path];
      if (faviconImage) {
        attributes = [FaviconAttributes attributesWithImage:faviconImage];
      }
    }
    if (!attributes) {
      if ([site.fallbackMonogram length] == 0) {
        // Something bad happened when saving the icon. Switch to best effort to
        // show something to the user.
        site.fallbackMonogram = @"";
      }
      if (!site.fallbackTextColor || !site.fallbackBackgroundColor) {
        // Something bad happened when saving the icon. Switch to best effort to
        // show something to the user.
        // kDefaultTextColor = SK_ColorWHITE;
        site.fallbackTextColor = UIColor.whiteColor;
        // kDefaultBackgroundColor = SkColorSetRGB(0x78, 0x78, 0x78);
        site.fallbackBackgroundColor = [UIColor colorWithRed:0x78 / 255.0f
                                                       green:0x78 / 255.0f
                                                        blue:0x78 / 255.0f
                                                       alpha:1];
        site.fallbackIsDefaultColor = YES;
      }
      attributes = [FaviconAttributes
          attributesWithMonogram:site.fallbackMonogram
                       textColor:site.fallbackTextColor
                 backgroundColor:site.fallbackBackgroundColor
          defaultBackgroundColor:site.fallbackIsDefaultColor];
    }
    [tileView.faviconView configureWithAttributes:attributes];
    tileView.alpha = 1;
    tileView.userInteractionEnabled = YES;
    [tileView addTarget:self
                  action:@selector(openURLFromMostVisited:)
        forControlEvents:UIControlEventTouchUpInside];
    tileView.accessibilityLabel = site.title;
  }

  self.siteCount = sites.count;
  [self hideEmptyTiles];
}

- (void)openURLFromMostVisited:(MostVisitedTileView*)mostVisitedTile {
  [self.delegate openURL:mostVisitedTile.URL];
}

- (void)hideEmptyTiles {
  for (int i = self.siteCount; i < kRows * self.iconsPerRow; i++) {
    self.mostVisitedTiles[i].alpha = 0;
    self.mostVisitedTiles[i].userInteractionEnabled = NO;
  }
}

- (CGFloat)firstRowHeight:(BOOL)compact {
  if (compact) {
    return self.compactHeight;
  }

  CGFloat firstRowHeight = kTileHeight + kRows * kTileSpacing;
  CGFloat secondRowHeight = [self secondRowHeight];
  CGFloat totalHeight = firstRowHeight + secondRowHeight;
  if (totalHeight > self.compactHeight) {
    return firstRowHeight;
  }

  // The expanded height should be strictly greater than compactHeight,
  // otherwise iOS does not update the UI correctly.
  return self.compactHeight - secondRowHeight + 1;
}

- (CGFloat)secondRowHeight {
  return self.shouldShowSecondRow ? kTileHeight + kTileSpacing : 0;
}

- (BOOL)sitesFitSingleRow {
  return self.iconsPerRow >= self.siteCount;
}

#pragma mark - ContentWidgetView

- (void)showMode:(BOOL)compact {
  self.firstRowHeightConstraint.constant = [self firstRowHeight:compact];
}

@end
