// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/grid/grid_cell.h"

#import "base/logging.h"
#import "ios/chrome/browser/ui/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_grid/grid/top_aligned_image_view.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Frame-based layout utilities for GridTransitionCell.
// Scales the size of |view|'s frame by |factor| in both height and width. This
// scaling is done by changing the frame size without changing its origin,
// unlike a scale transform which scales around the view's center.
void ScaleView(UIView* view, CGFloat factor) {
  if (!view)
    return;
  CGRect frame = view.frame;
  frame.size.width *= factor;
  frame.size.height *= factor;
  view.frame = frame;
}

// Positions |view| by setting its frame's origin to |point|.
void PositionView(UIView* view, CGPoint point) {
  if (!view)
    return;
  CGRect frame = view.frame;
  frame.origin = point;
  view.frame = frame;
}
}  // namespace

@interface GridCell ()
// Header height of the cell.
@property(nonatomic, strong) NSLayoutConstraint* topBarHeight;
// Visual components of the cell.
@property(nonatomic, weak) UIView* topBar;
@property(nonatomic, weak) UIImageView* iconView;
@property(nonatomic, weak) TopAlignedImageView* snapshotView;
@property(nonatomic, weak) UILabel* titleLabel;
@property(nonatomic, weak) UIImageView* closeIconView;
// Since the close icon dimensions are smaller than the recommended tap target
// size, use an overlaid tap target button.
@property(nonatomic, weak) UIButton* closeTapTargetButton;
@property(nonatomic, weak) UIView* border;
@end

@implementation GridCell
// Public properties.
@synthesize delegate = _delegate;
@synthesize theme = _theme;
@synthesize itemIdentifier = _itemIdentifier;
@synthesize icon = _icon;
@synthesize snapshot = _snapshot;
@synthesize title = _title;
@synthesize titleHidden = _titleHidden;
// Private properties.
@synthesize topBarHeight = _topBarHeight;
@synthesize topBar = _topBar;
@synthesize iconView = _iconView;
@synthesize snapshotView = _snapshotView;
@synthesize titleLabel = _titleLabel;
@synthesize closeIconView = _closeIconView;
@synthesize closeTapTargetButton = _closeTapTargetButton;
@synthesize border = _border;

// |-dequeueReusableCellWithReuseIdentifier:forIndexPath:| calls this method to
// initialize a cell.
- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    [self setupSelectedBackgroundView];
    UIView* contentView = self.contentView;
    contentView.layer.cornerRadius = kGridCellCornerRadius;
    contentView.layer.masksToBounds = YES;
    UIView* topBar = [self setupTopBar];
    TopAlignedImageView* snapshotView = [[TopAlignedImageView alloc] init];
    snapshotView.translatesAutoresizingMaskIntoConstraints = NO;

    UIButton* closeTapTargetButton =
        [UIButton buttonWithType:UIButtonTypeCustom];
    closeTapTargetButton.translatesAutoresizingMaskIntoConstraints = NO;
    [closeTapTargetButton addTarget:self
                             action:@selector(closeButtonTapped:)
                   forControlEvents:UIControlEventTouchUpInside];
    closeTapTargetButton.accessibilityIdentifier =
        kGridCellCloseButtonIdentifier;

    [contentView addSubview:topBar];
    [contentView addSubview:snapshotView];
    [contentView addSubview:closeTapTargetButton];
    _topBar = topBar;
    _snapshotView = snapshotView;
    _closeTapTargetButton = closeTapTargetButton;

    _topBarHeight =
        [topBar.heightAnchor constraintEqualToConstant:kGridCellHeaderHeight];

    NSArray* constraints = @[
      [topBar.topAnchor constraintEqualToAnchor:contentView.topAnchor],
      [topBar.leadingAnchor constraintEqualToAnchor:contentView.leadingAnchor],
      [topBar.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor],
      _topBarHeight,
      [snapshotView.topAnchor constraintEqualToAnchor:topBar.bottomAnchor],
      [snapshotView.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor],
      [snapshotView.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor],
      [snapshotView.bottomAnchor
          constraintEqualToAnchor:contentView.bottomAnchor],
      [closeTapTargetButton.topAnchor
          constraintEqualToAnchor:contentView.topAnchor],
      [closeTapTargetButton.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor],
      [closeTapTargetButton.widthAnchor
          constraintEqualToConstant:kGridCellCloseTapTargetWidthHeight],
      [closeTapTargetButton.heightAnchor
          constraintEqualToConstant:kGridCellCloseTapTargetWidthHeight],
    ];
    [NSLayoutConstraint activateConstraints:constraints];
  }
  return self;
}

#pragma mark - UICollectionViewCell

- (void)setHighlighted:(BOOL)highlighted {
  // NO-OP to disable highlighting and only allow selection.
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.itemIdentifier = nil;
  self.title = nil;
  self.titleHidden = NO;
  self.icon = nil;
  self.snapshot = nil;
  self.selected = NO;
}

#pragma mark - Accessibility

- (BOOL)isAccessibilityElement {
  // This makes the whole cell tappable in VoiceOver rather than the individual
  // title and close button.
  return YES;
}

- (NSArray*)accessibilityCustomActions {
  // Each cell has 2 custom actions, which is accessible through swiping. The
  // default is to select the cell. Another is to close the cell.
  return @[ [[UIAccessibilityCustomAction alloc]
      initWithName:l10n_util::GetNSString(IDS_IOS_TAB_SWITCHER_CLOSE_TAB)
            target:self
          selector:@selector(closeButtonTapped:)] ];
}

#pragma mark - Public

// Updates the theme to either dark or light. Updating is only done if the
// current theme is not the desired theme.
- (void)setTheme:(GridTheme)theme {
  if (_theme == theme)
    return;
  self.iconView.backgroundColor = UIColor.clearColor;
  self.snapshotView.backgroundColor =
      UIColorFromRGB(kGridCellSnapshotBackgroundColor);
  switch (theme) {
    case GridThemeLight:
      self.contentView.backgroundColor =
          UIColorFromRGB(kGridLightThemeCellHeaderColor);
      self.topBar.backgroundColor =
          UIColorFromRGB(kGridLightThemeCellHeaderColor);
      self.titleLabel.textColor = UIColorFromRGB(kGridLightThemeCellTitleColor);
      self.closeIconView.tintColor =
          UIColorFromRGB(kGridLightThemeCellCloseButtonTintColor);
      self.border.layer.borderColor =
          UIColorFromRGB(kGridLightThemeCellSelectionColor).CGColor;
      break;
    case GridThemeDark:
      self.contentView.backgroundColor =
          UIColorFromRGB(kGridDarkThemeCellHeaderColor);
      self.topBar.backgroundColor =
          UIColorFromRGB(kGridDarkThemeCellHeaderColor);
      self.titleLabel.textColor = UIColorFromRGB(kGridDarkThemeCellTitleColor);
      self.closeIconView.tintColor =
          UIColorFromRGB(kGridDarkThemeCellCloseButtonTintColor);
      self.border.layer.borderColor =
          UIColorFromRGB(kGridDarkThemeCellSelectionColor).CGColor;
      break;
  }
  _theme = theme;
}

- (void)setIcon:(UIImage*)icon {
  self.iconView.image = icon;
  _icon = icon;
}

- (void)setSnapshot:(UIImage*)snapshot {
  self.snapshotView.image = snapshot;
  _snapshot = snapshot;
}

- (void)setTitle:(NSString*)title {
  self.titleLabel.text = title;
  self.accessibilityLabel = title;
  _title = title;
}

- (void)setTitleHidden:(BOOL)titleHidden {
  self.titleLabel.hidden = titleHidden;
  _titleHidden = titleHidden;
}

#pragma mark - Private

// Sets up the top bar with icon, title, and close button.
- (UIView*)setupTopBar {
  UIView* topBar = [[UIView alloc] init];
  topBar.translatesAutoresizingMaskIntoConstraints = NO;

  UIImageView* iconView = [[UIImageView alloc] init];
  iconView.translatesAutoresizingMaskIntoConstraints = NO;
  iconView.contentMode = UIViewContentModeScaleAspectFill;
  iconView.layer.cornerRadius = kGridCellIconCornerRadius;
  iconView.layer.masksToBounds = YES;

  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  titleLabel.adjustsFontForContentSizeCategory = YES;

  UIImageView* closeIconView = [[UIImageView alloc] init];
  closeIconView.translatesAutoresizingMaskIntoConstraints = NO;
  closeIconView.contentMode = UIViewContentModeCenter;
  closeIconView.image = [[UIImage imageNamed:@"grid_cell_close_button"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];

  [topBar addSubview:iconView];
  [topBar addSubview:titleLabel];
  [topBar addSubview:closeIconView];
  _iconView = iconView;
  _titleLabel = titleLabel;
  _closeIconView = closeIconView;

  NSArray* constraints = @[
    [iconView.leadingAnchor
        constraintEqualToAnchor:topBar.leadingAnchor
                       constant:kGridCellHeaderLeadingInset],
    [iconView.centerYAnchor constraintEqualToAnchor:topBar.centerYAnchor],
    [iconView.widthAnchor constraintEqualToConstant:kGridCellIconDiameter],
    [iconView.heightAnchor constraintEqualToConstant:kGridCellIconDiameter],
    [titleLabel.leadingAnchor
        constraintEqualToAnchor:iconView.trailingAnchor
                       constant:kGridCellHeaderLeadingInset],
    [titleLabel.centerYAnchor constraintEqualToAnchor:topBar.centerYAnchor],
    [titleLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:closeIconView.leadingAnchor
                                 constant:-kGridCellTitleLabelContentInset],
    [closeIconView.topAnchor constraintEqualToAnchor:topBar.topAnchor],
    [closeIconView.bottomAnchor constraintEqualToAnchor:topBar.bottomAnchor],
    [closeIconView.trailingAnchor
        constraintEqualToAnchor:topBar.trailingAnchor
                       constant:-kGridCellCloseButtonContentInset],
  ];
  [NSLayoutConstraint activateConstraints:constraints];
  [titleLabel
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [closeIconView
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [closeIconView setContentHuggingPriority:UILayoutPriorityRequired
                                   forAxis:UILayoutConstraintAxisHorizontal];
  return topBar;
}

// Sets up the selection border. The tint color is set when the theme is
// selected.
- (void)setupSelectedBackgroundView {
  self.selectedBackgroundView = [[UIView alloc] init];
  self.selectedBackgroundView.backgroundColor =
      UIColorFromRGB(kGridBackgroundColor);
  UIView* border = [[UIView alloc] init];
  border.translatesAutoresizingMaskIntoConstraints = NO;
  border.backgroundColor = UIColorFromRGB(kGridBackgroundColor);
  border.layer.cornerRadius = kGridCellCornerRadius +
                              kGridCellSelectionRingGapWidth +
                              kGridCellSelectionRingTintWidth;
  border.layer.borderWidth = kGridCellSelectionRingTintWidth;
  [self.selectedBackgroundView addSubview:border];
  _border = border;
  [NSLayoutConstraint activateConstraints:@[
    [border.topAnchor
        constraintEqualToAnchor:self.selectedBackgroundView.topAnchor
                       constant:-kGridCellSelectionRingTintWidth -
                                kGridCellSelectionRingGapWidth],
    [border.leadingAnchor
        constraintEqualToAnchor:self.selectedBackgroundView.leadingAnchor
                       constant:-kGridCellSelectionRingTintWidth -
                                kGridCellSelectionRingGapWidth],
    [border.trailingAnchor
        constraintEqualToAnchor:self.selectedBackgroundView.trailingAnchor
                       constant:kGridCellSelectionRingTintWidth +
                                kGridCellSelectionRingGapWidth],
    [border.bottomAnchor
        constraintEqualToAnchor:self.selectedBackgroundView.bottomAnchor
                       constant:kGridCellSelectionRingTintWidth +
                                kGridCellSelectionRingGapWidth]
  ]];
}

// Selector registered to the close button.
- (void)closeButtonTapped:(id)sender {
  [self.delegate closeButtonTappedForCell:self];
}

@end

@implementation GridTransitionSelectionCell

+ (instancetype)transitionCellFromCell:(GridCell*)cell {
  GridTransitionSelectionCell* proxy = [[self alloc] initWithFrame:cell.bounds];
  proxy.selected = YES;
  proxy.theme = cell.theme;
  proxy.contentView.hidden = YES;
  return proxy;
}

@end

@implementation GridTransitionCell {
  // Previous tab view width, used to scale the tab views.
  CGFloat _previousTabViewWidth;
}

// Synthesis of GridToTabTransitionView properties.
@synthesize topTabView = _topTabView;
@synthesize mainTabView = _mainTabView;
@synthesize bottomTabView = _bottomTabView;

+ (instancetype)transitionCellFromCell:(GridCell*)cell {
  GridTransitionCell* proxy = [[self alloc] initWithFrame:cell.bounds];
  proxy.selected = NO;
  proxy.theme = cell.theme;
  proxy.icon = cell.icon;
  proxy.snapshot = cell.snapshot;
  proxy.title = cell.title;
  proxy.titleHidden = cell.titleHidden;
  return proxy;
}
#pragma mark - GridToTabTransitionView properties.

- (void)setTopCellView:(UIView*)topCellView {
  // The top cell view is |topBar| and can't be changed.
  NOTREACHED();
}

- (UIView*)topCellView {
  return self.topBar;
}

- (void)setTopTabView:(UIView*)topTabView {
  DCHECK(!_topTabView) << "topTabView should only be set once.";
  if (!topTabView.superview)
    [self.contentView addSubview:topTabView];
  _topTabView = topTabView;
}

- (void)setMainCellView:(UIView*)mainCellView {
  // The main cell view is the snapshot view and can't be changed.
  NOTREACHED();
}

- (UIView*)mainCellView {
  return self.snapshotView;
}

- (void)setMainTabView:(UIView*)mainTabView {
  DCHECK(!_mainTabView) << "mainTabView should only be set once.";
  if (!mainTabView.superview)
    [self.contentView addSubview:mainTabView];
  _previousTabViewWidth = mainTabView.frame.size.width;
  _mainTabView = mainTabView;
}

- (void)setBottomTabView:(UIView*)bottomTabView {
  DCHECK(!_bottomTabView) << "bottomTabView should only be set once.";
  if (!bottomTabView.superview)
    [self.contentView addSubview:bottomTabView];
  _bottomTabView = bottomTabView;
}

- (CGFloat)cornerRadius {
  return self.contentView.layer.cornerRadius;
}

- (void)setCornerRadius:(CGFloat)radius {
  self.contentView.layer.cornerRadius = radius;
}

#pragma mark - GridToTabTransitionView methods

- (void)positionTabViews {
  [self scaleTabViews];
  self.topBarHeight.constant = self.topTabView.frame.size.height;
  [self setNeedsUpdateConstraints];
  [self layoutIfNeeded];
  PositionView(self.topTabView, CGPointMake(0, 0));
  // Position the main view so it's top-aligned with the main cell view.
  PositionView(self.mainTabView, self.mainCellView.frame.origin);
  if (!self.bottomTabView)
    return;

  // Position the bottom tab view at the bottom.
  CGFloat yPosition = CGRectGetMaxY(self.contentView.bounds) -
                      self.bottomTabView.frame.size.height;
  PositionView(self.bottomTabView, CGPointMake(0, yPosition));
}

- (void)positionCellViews {
  [self scaleTabViews];
  self.topBarHeight.constant = kGridCellHeaderHeight;
  [self setNeedsUpdateConstraints];
  [self layoutIfNeeded];
  CGFloat yOffset = kGridCellHeaderHeight - self.topTabView.frame.size.height;
  PositionView(self.topTabView, CGPointMake(0, yOffset));
  // Position the main view so it's top-aligned with the main cell view.
  PositionView(self.mainTabView, self.mainCellView.frame.origin);
  if (!self.bottomTabView)
    return;

  if (self.bottomTabView.frame.origin.y > 0) {
    // Position the bottom tab so it's equivalently located.
    CGFloat scale = self.bounds.size.width / _previousTabViewWidth;
    PositionView(self.bottomTabView,
                 CGPointMake(0, self.bottomTabView.frame.origin.y * scale));
  } else {
    // Position the bottom tab view below the main content view.
    CGFloat yOffset = CGRectGetMaxY(self.mainCellView.frame);
    PositionView(self.bottomTabView, CGPointMake(0, yOffset));
  }
}

#pragma mark - Private helper methods

// Scales the tab views relative to the current width of the cell.
- (void)scaleTabViews {
  CGFloat scale = self.bounds.size.width / _previousTabViewWidth;
  ScaleView(self.topTabView, scale);
  ScaleView(self.mainTabView, scale);
  ScaleView(self.bottomTabView, scale);
  _previousTabViewWidth = self.mainTabView.frame.size.width;
}

@end
