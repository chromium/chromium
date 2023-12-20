// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/group_grid_cell.h"

#import <MaterialComponents/MaterialActivityIndicator.h>
#import <ostream>

#import "base/check.h"
#import "base/check_op.h"
#import "base/debug/dump_without_crashing.h"
#import "base/notreached.h"
#import "ios/chrome/browser/shared/ui/elements/top_aligned_image_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/group_bottom_trailing_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/gfx/ios/uikit_util.h"

namespace {

// The size of symbol icons.
NSInteger kIconSymbolPointSize = 13;

// Size of activity indicator replacing fav icon when active.
const CGFloat kIndicatorSize = 16.0;

// Offsets and dimension ratio constraints of the top and bottom snapshot views.
const CGFloat kSnapshotDimensionRatio = 0.5;
const CGFloat kSnapshotViewWidthOffset = 6;
const CGFloat kSnapshotViewHeightOffset = 4;
const CGFloat kSnapshotViewLeadingOffset = 4;
const CGFloat kSnapshotViewTrailingOffset = 4;
const CGFloat kSnapShotViewBottomOffset = 4;

}  // namespace

@interface GroupGridCell ()
// The constraints enabled under accessibility font size.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* accessibilityConstraints;
// The constraints enabled under normal font size.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* nonAccessibilityConstraints;
// The constraints enabled while showing the close icon.
@property(nonatomic, strong) NSArray<NSLayoutConstraint*>* closeIconConstraints;
// The constraints enabled while showing the selection icon.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* selectIconConstraints;
// Header height of the cell.
@property(nonatomic, strong) NSLayoutConstraint* topBarHeightConstraint;
// Visual components of the cell.
@property(nonatomic, weak) UIView* topBar;
@property(nonatomic, weak) UIImageView* iconView;
@property(nonatomic, weak) UIView* groupSnapshotsView;
@property(nonatomic, weak) TopAlignedImageView* topLeadingSnapshotView;
@property(nonatomic, weak) TopAlignedImageView* topTrailingSnapshotView;
@property(nonatomic, weak) TopAlignedImageView* bottomLeadingSnapshotView;
@property(nonatomic, weak)
    GroupGridBottomTrailingView* bottomTrailingSnapshotView;
// TODO(crbug.com/1501837): Add the bottom right snapshot view.
// TODO(crbug.com/1501837): Add the favicon views.
@property(nonatomic, weak) UILabel* titleLabel;
@property(nonatomic, weak) UIImageView* closeIconView;
@property(nonatomic, weak) UIImageView* selectIconView;
@property(nonatomic, weak) MDCActivityIndicator* activityIndicator;
// Since the close icon dimensions are smaller than the recommended tap target
// size, use an overlaid tap target button.
@property(nonatomic, weak) UIButton* closeTapTargetButton;
@property(nonatomic, weak) UIView* border;
// Whether or not the cell is currently displaying an editing state.
@property(nonatomic, readonly) BOOL isInSelectionMode;
@end

@implementation GroupGridCell

// `-dequeueReusableCellWithReuseIdentifier:forIndexPath:` calls this method to
// initialize a cell.
- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _state = GridCellStateNotEditing;

    // The background color must be set to avoid the corners behind the rounded
    // layer from showing when dragging and dropping. Unfortunately, using
    // `UIColor.clearColor` here will not remain transparent, so a solid color
    // must be chosen. Using the grid color prevents the corners from showing
    // while it transitions to the presented context menu/dragging state.
    self.backgroundColor = [UIColor colorNamed:kGridBackgroundColor];

    [self setupSelectedBackgroundView];
    UIView* contentView = self.contentView;
    contentView.layer.cornerRadius = kGridCellCornerRadius;
    contentView.layer.masksToBounds = YES;
    UIView* topBar = [self setupTopBar];
    UIView* groupSnapshotsView = [[UIView alloc] init];
    TopAlignedImageView* topLeadingSnapshotView =
        [[TopAlignedImageView alloc] init];
    TopAlignedImageView* topTrailingSnapshotView =
        [[TopAlignedImageView alloc] init];
    TopAlignedImageView* bottomLeadingSnapshotView =
        [[TopAlignedImageView alloc] init];
    GroupGridBottomTrailingView* bottomTrailingSnapshotView =
        [[GroupGridBottomTrailingView alloc] init];
    groupSnapshotsView.translatesAutoresizingMaskIntoConstraints = NO;
    topLeadingSnapshotView.translatesAutoresizingMaskIntoConstraints = NO;
    topTrailingSnapshotView.translatesAutoresizingMaskIntoConstraints = NO;
    bottomLeadingSnapshotView.translatesAutoresizingMaskIntoConstraints = NO;
    bottomTrailingSnapshotView.translatesAutoresizingMaskIntoConstraints = NO;
    UIButton* closeTapTargetButton =
        [UIButton buttonWithType:UIButtonTypeCustom];
    closeTapTargetButton.translatesAutoresizingMaskIntoConstraints = NO;
    [closeTapTargetButton addTarget:self
                             action:@selector(closeButtonTapped:)
                   forControlEvents:UIControlEventTouchUpInside];
    closeTapTargetButton.accessibilityIdentifier =
        kGridCellCloseButtonIdentifier;
    [groupSnapshotsView addSubview:topLeadingSnapshotView];
    [groupSnapshotsView addSubview:topTrailingSnapshotView];
    [groupSnapshotsView addSubview:bottomLeadingSnapshotView];
    [groupSnapshotsView addSubview:bottomTrailingSnapshotView];

    [contentView addSubview:topBar];
    [contentView addSubview:groupSnapshotsView];
    [contentView addSubview:closeTapTargetButton];
    _topBar = topBar;
    _topLeadingSnapshotView = topLeadingSnapshotView;
    _topTrailingSnapshotView = topTrailingSnapshotView;
    _bottomLeadingSnapshotView = bottomLeadingSnapshotView;
    _bottomTrailingSnapshotView = bottomTrailingSnapshotView;
    _groupSnapshotsView = groupSnapshotsView;
    _closeTapTargetButton = closeTapTargetButton;
    _opacity = 1.0;

    self.contentView.backgroundColor = [UIColor colorNamed:kBackgroundColor];
    self.topLeadingSnapshotView.backgroundColor =
        [UIColor colorNamed:kBackgroundColor];
    self.topTrailingSnapshotView.backgroundColor =
        [UIColor colorNamed:kBackgroundColor];
    self.bottomLeadingSnapshotView.backgroundColor =
        [UIColor colorNamed:kBackgroundColor];
    self.bottomTrailingSnapshotView.backgroundColor =
        [UIColor colorNamed:kBackgroundColor];
    // TODO(crbug.com/1501837): Apply different corner radius to each view.
    self.bottomLeadingSnapshotView.layer.cornerRadius = kGridCellCornerRadius;
    self.topLeadingSnapshotView.layer.cornerRadius = kGridCellCornerRadius;
    self.topTrailingSnapshotView.layer.cornerRadius = kGridCellCornerRadius;
    self.bottomTrailingSnapshotView.layer.cornerRadius = kGridCellCornerRadius;
    self.topLeadingSnapshotView.hidden = YES;
    self.topTrailingSnapshotView.hidden = YES;
    self.bottomLeadingSnapshotView.hidden = YES;
    self.bottomTrailingSnapshotView.hidden = YES;

    self.groupSnapshotsView.backgroundColor =
        [UIColor colorNamed:kBackgroundColor];
    self.topBar.backgroundColor = [UIColor colorNamed:kBackgroundColor];
    self.titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    self.closeIconView.tintColor = [UIColor colorNamed:kCloseButtonColor];

    self.layer.cornerRadius = kGridCellCornerRadius;
    self.layer.shadowColor = [UIColor blackColor].CGColor;
    self.layer.shadowOffset = CGSizeMake(0, 0);
    self.layer.shadowRadius = 4.0f;
    self.layer.shadowOpacity = 0.5f;
    self.layer.masksToBounds = NO;
    NSArray* constraints = @[
      [topBar.topAnchor constraintEqualToAnchor:contentView.topAnchor],
      [topBar.leadingAnchor constraintEqualToAnchor:contentView.leadingAnchor],
      [topBar.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor],
      [groupSnapshotsView.topAnchor
          constraintEqualToAnchor:topBar.bottomAnchor],
      [groupSnapshotsView.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor],
      [groupSnapshotsView.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor],
      [groupSnapshotsView.bottomAnchor
          constraintEqualToAnchor:contentView.bottomAnchor],
      [closeTapTargetButton.topAnchor
          constraintEqualToAnchor:contentView.topAnchor],
      [closeTapTargetButton.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor],
      [closeTapTargetButton.widthAnchor
          constraintEqualToConstant:kGridCellCloseTapTargetWidthHeight],
      [closeTapTargetButton.heightAnchor
          constraintEqualToConstant:kGridCellCloseTapTargetWidthHeight],

      [topLeadingSnapshotView.heightAnchor
          constraintEqualToAnchor:groupSnapshotsView.heightAnchor
                       multiplier:kSnapshotDimensionRatio
                         constant:-kSnapshotViewHeightOffset],
      [topLeadingSnapshotView.widthAnchor
          constraintEqualToAnchor:groupSnapshotsView.widthAnchor
                       multiplier:kSnapshotDimensionRatio
                         constant:-kSnapshotViewWidthOffset],
      [topLeadingSnapshotView.leadingAnchor
          constraintEqualToAnchor:groupSnapshotsView.leadingAnchor
                         constant:kSnapshotViewLeadingOffset],
      [topLeadingSnapshotView.topAnchor
          constraintEqualToAnchor:groupSnapshotsView.topAnchor],

      [topTrailingSnapshotView.heightAnchor
          constraintEqualToAnchor:groupSnapshotsView.heightAnchor
                       multiplier:kSnapshotDimensionRatio
                         constant:-kSnapshotViewHeightOffset],
      [topTrailingSnapshotView.widthAnchor
          constraintEqualToAnchor:groupSnapshotsView.widthAnchor
                       multiplier:kSnapshotDimensionRatio
                         constant:-kSnapshotViewWidthOffset],
      [topTrailingSnapshotView.trailingAnchor
          constraintEqualToAnchor:groupSnapshotsView.trailingAnchor
                         constant:-kSnapshotViewTrailingOffset],
      [topTrailingSnapshotView.topAnchor
          constraintEqualToAnchor:groupSnapshotsView.topAnchor],
      [bottomLeadingSnapshotView.heightAnchor
          constraintEqualToAnchor:groupSnapshotsView.heightAnchor
                       multiplier:kSnapshotDimensionRatio
                         constant:-kSnapshotViewHeightOffset],
      [bottomLeadingSnapshotView.widthAnchor
          constraintEqualToAnchor:groupSnapshotsView.widthAnchor
                       multiplier:kSnapshotDimensionRatio
                         constant:-kSnapshotViewWidthOffset],
      [bottomLeadingSnapshotView.leadingAnchor
          constraintEqualToAnchor:groupSnapshotsView.leadingAnchor
                         constant:kSnapshotViewLeadingOffset],
      [bottomLeadingSnapshotView.bottomAnchor
          constraintEqualToAnchor:groupSnapshotsView.bottomAnchor
                         constant:-kSnapShotViewBottomOffset],

      [bottomTrailingSnapshotView.heightAnchor
          constraintEqualToAnchor:groupSnapshotsView.heightAnchor
                       multiplier:kSnapshotDimensionRatio
                         constant:-kSnapshotViewHeightOffset],
      [bottomTrailingSnapshotView.widthAnchor
          constraintEqualToAnchor:groupSnapshotsView.widthAnchor
                       multiplier:kSnapshotDimensionRatio
                         constant:-kSnapshotViewWidthOffset],
      [bottomTrailingSnapshotView.trailingAnchor
          constraintEqualToAnchor:groupSnapshotsView.trailingAnchor
                         constant:-kSnapshotViewTrailingOffset],
      [bottomTrailingSnapshotView.bottomAnchor
          constraintEqualToAnchor:groupSnapshotsView.bottomAnchor
                         constant:-kSnapShotViewBottomOffset],

      [topLeadingSnapshotView.trailingAnchor
          constraintLessThanOrEqualToAnchor:topTrailingSnapshotView
                                                .leadingAnchor],
      [bottomLeadingSnapshotView.trailingAnchor
          constraintLessThanOrEqualToAnchor:bottomTrailingSnapshotView
                                                .leadingAnchor],
      [topLeadingSnapshotView.bottomAnchor
          constraintLessThanOrEqualToAnchor:bottomLeadingSnapshotView
                                                .topAnchor],
      [topTrailingSnapshotView.bottomAnchor
          constraintLessThanOrEqualToAnchor:bottomTrailingSnapshotView
                                                .topAnchor],
    ];
    [NSLayoutConstraint activateConstraints:constraints];
  }
  return self;
}

#pragma mark - UIView

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  BOOL isPreviousAccessibilityCategory =
      UIContentSizeCategoryIsAccessibilityCategory(
          previousTraitCollection.preferredContentSizeCategory);
  BOOL isCurrentAccessibilityCategory =
      UIContentSizeCategoryIsAccessibilityCategory(
          self.traitCollection.preferredContentSizeCategory);
  if (isPreviousAccessibilityCategory ^ isCurrentAccessibilityCategory) {
    [self updateTopBarSize];
  }
}

#pragma mark - UICollectionViewCell

- (void)setHighlighted:(BOOL)highlighted {
  // NO-OP to disable highlighting and only allow selection.
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.title = nil;
  self.titleHidden = NO;
  self.icon = nil;
  [self configureWithGroupTabInfos:nil];
  self.bottomTrailingSnapshotView.mainSubviewImageAndFavicon.snapshot = nil;
  self.bottomTrailingSnapshotView.favicons = nil;
  self.selected = NO;
  self.opacity = 1.0;
  [self hideActivityIndicator];
}

#pragma mark - UIAccessibility

- (BOOL)isAccessibilityElement {
  // This makes the whole cell tappable in VoiceOver rather than the individual
  // title and close button.
  return YES;
}
// TODO(crbug.com/1511982): Add the accessibility custom actions.

#pragma mark - Public

// Updates the theme to either dark or light. Updating is only done if the
// current theme is not the desired theme.
- (void)setTheme:(GridTheme)theme {
  if (_theme == theme) {
    return;
  }

  self.overrideUserInterfaceStyle = (theme == GridThemeDark)
                                        ? UIUserInterfaceStyleDark
                                        : UIUserInterfaceStyleUnspecified;

  // The light and dark themes have different colored borders based on the
  // theme, regardless of dark mode, so `overrideUserInterfaceStyle` is not
  // enough here.
  switch (theme) {
    case GridThemeLight:
      self.border.layer.borderColor =
          [UIColor colorNamed:kStaticBlue400Color].CGColor;
      break;
    case GridThemeDark:
      self.border.layer.borderColor = UIColor.whiteColor.CGColor;
      break;
  }

  _theme = theme;
}

- (void)setIcon:(UIImage*)icon {
  self.iconView.image = icon;
  _icon = icon;
}

- (void)showActivityIndicator {
  [self.activityIndicator startAnimating];
  [self.activityIndicator setHidden:NO];
  [self.iconView setHidden:YES];
}

- (void)hideActivityIndicator {
  [self.activityIndicator stopAnimating];
  [self.activityIndicator setHidden:YES];
  [self.iconView setHidden:NO];
}

- (void)configureWithGroupTabInfos:(NSArray<GroupTabInfo*>*)groupTabInfos {
  // Hide all the views when the cell is reconfigured and clear their images.
  self.topLeadingSnapshotView.image = nil;
  self.topTrailingSnapshotView.image = nil;
  self.bottomLeadingSnapshotView.image = nil;
  self.topLeadingSnapshotView.hidden = YES;
  self.topTrailingSnapshotView.hidden = YES;
  self.bottomLeadingSnapshotView.hidden = YES;

  int groupTabInfosLength = [groupTabInfos count];
  if (groupTabInfosLength > 0) {
    self.topLeadingSnapshotView.image = groupTabInfos[0].snapshot;
    self.topLeadingSnapshotView.hidden = NO;
  }
  if (groupTabInfosLength > 1) {
    self.topTrailingSnapshotView.image = groupTabInfos[1].snapshot;
    self.topTrailingSnapshotView.hidden = NO;
  }
  if (groupTabInfosLength > 2) {
    self.bottomLeadingSnapshotView.image = groupTabInfos[2].snapshot;
    self.bottomLeadingSnapshotView.hidden = NO;
  }
  if (groupTabInfosLength == 4) {
    self.bottomTrailingSnapshotView.mainSubviewImageAndFavicon =
        groupTabInfos[3];
    self.bottomTrailingSnapshotView.hidden = NO;
  }
  if (groupTabInfosLength > 4) {
    NSMutableArray<UIImage*>* favicons = [[NSMutableArray alloc] init];
    NSRange range;
    range.location = 3;
    range.length = groupTabInfosLength - 3;
    for (GroupTabInfo* snapshotFavicon in
         [groupTabInfos subarrayWithRange:range]) {
      if (snapshotFavicon.favicon != nil) {
        [favicons addObject:snapshotFavicon.favicon];
      }
    }
    self.bottomTrailingSnapshotView.favicons = favicons;
    self.bottomTrailingSnapshotView.hidden = NO;
  }
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

- (UIDragPreviewParameters*)dragPreviewParameters {
  UIBezierPath* visiblePath = [UIBezierPath
      bezierPathWithRoundedRect:self.bounds
                   cornerRadius:self.contentView.layer.cornerRadius];
  UIDragPreviewParameters* params = [[UIDragPreviewParameters alloc] init];
  params.visiblePath = visiblePath;
  return params;
}

- (void)setOpacity:(CGFloat)opacity {
  _opacity = opacity;
  self.alpha = opacity;
}

- (void)setAlpha:(CGFloat)alpha {
  // Make sure alpha is synchronized with opacity.
  super.alpha = _opacity;
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
  iconView.backgroundColor = UIColor.clearColor;
  iconView.tintColor = [UIColor colorNamed:kGrey400Color];

  CGRect indicatorFrame = CGRectMake(0, 0, kIndicatorSize, kIndicatorSize);
  MDCActivityIndicator* activityIndicator =
      [[MDCActivityIndicator alloc] initWithFrame:indicatorFrame];
  activityIndicator.translatesAutoresizingMaskIntoConstraints = NO;
  activityIndicator.cycleColors = @[ [UIColor colorNamed:kBlueColor] ];
  activityIndicator.radius = ui::AlignValueToUpperPixel(kIndicatorSize / 2);

  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  titleLabel.adjustsFontForContentSizeCategory = YES;

  UIImageView* closeIconView = [[UIImageView alloc] init];
  closeIconView.translatesAutoresizingMaskIntoConstraints = NO;
  closeIconView.contentMode = UIViewContentModeCenter;
  closeIconView.hidden = self.isInSelectionMode;
  closeIconView.image =
      DefaultSymbolTemplateWithPointSize(kXMarkSymbol, kIconSymbolPointSize);

  UIImageView* selectIconView = [[UIImageView alloc] init];
  selectIconView.translatesAutoresizingMaskIntoConstraints = NO;
  selectIconView.contentMode = UIViewContentModeScaleAspectFit;
  selectIconView.hidden = !self.isInSelectionMode;

  selectIconView.image = [self selectIconImageForCurrentState];

  [topBar addSubview:selectIconView];
  _selectIconView = selectIconView;

  [topBar addSubview:iconView];
  [topBar addSubview:activityIndicator];
  [topBar addSubview:titleLabel];
  [topBar addSubview:closeIconView];

  _iconView = iconView;
  _activityIndicator = activityIndicator;
  _titleLabel = titleLabel;
  _closeIconView = closeIconView;

  _accessibilityConstraints = @[
    [titleLabel.leadingAnchor
        constraintEqualToAnchor:topBar.leadingAnchor
                       constant:kGridCellHeaderLeadingInset],
    [iconView.widthAnchor constraintEqualToConstant:0],
    [iconView.heightAnchor constraintEqualToConstant:0],
  ];

  _nonAccessibilityConstraints = @[
    [iconView.leadingAnchor
        constraintEqualToAnchor:topBar.leadingAnchor
                       constant:kGridCellHeaderLeadingInset],
    [iconView.centerYAnchor constraintEqualToAnchor:topBar.centerYAnchor],
    [iconView.widthAnchor constraintEqualToConstant:kGridCellIconDiameter],
    [iconView.heightAnchor constraintEqualToConstant:kGridCellIconDiameter],
    [titleLabel.leadingAnchor
        constraintEqualToAnchor:iconView.trailingAnchor
                       constant:kGridCellHeaderLeadingInset],
  ];

  _topBarHeightConstraint =
      [topBar.heightAnchor constraintEqualToConstant:kGridCellHeaderHeight];

  _closeIconConstraints = @[
    [titleLabel.trailingAnchor
        constraintEqualToAnchor:closeIconView.leadingAnchor
                       constant:-kGridCellTitleLabelContentInset],
    [topBar.topAnchor constraintEqualToAnchor:closeIconView.centerYAnchor
                                     constant:-kGridCellCloseButtonTopSpacing],
    [closeIconView.trailingAnchor
        constraintEqualToAnchor:topBar.trailingAnchor
                       constant:-kGridCellCloseButtonContentInset],
  ];

  if (_selectIconView) {
    _selectIconConstraints = @[
      [_selectIconView.heightAnchor
          constraintEqualToConstant:kGridCellSelectIconSize],
      [_selectIconView.widthAnchor
          constraintEqualToConstant:kGridCellSelectIconSize],
      [titleLabel.trailingAnchor
          constraintEqualToAnchor:_selectIconView.leadingAnchor
                         constant:-kGridCellTitleLabelContentInset],
      [topBar.topAnchor constraintEqualToAnchor:_selectIconView.topAnchor
                                       constant:-kGridCellSelectIconTopSpacing],
      [_selectIconView.trailingAnchor
          constraintEqualToAnchor:topBar.trailingAnchor
                         constant:-kGridCellSelectIconContentInset],

    ];
  }

  [self updateTopBarSize];
  [self configureCloseOrSelectIconConstraints];

  NSArray* constraints = @[
    _topBarHeightConstraint,
    [titleLabel.centerYAnchor constraintEqualToAnchor:topBar.centerYAnchor],
  ];

  // Center indicator over favicon.
  AddSameCenterXConstraint(self, iconView, activityIndicator);
  AddSameCenterYConstraint(self, iconView, activityIndicator);

  [NSLayoutConstraint activateConstraints:constraints];
  [titleLabel
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [closeIconView
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [closeIconView setContentHuggingPriority:UILayoutPriorityRequired
                                   forAxis:UILayoutConstraintAxisHorizontal];
  if (_selectIconView) {
    [_selectIconView
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    [_selectIconView
        setContentHuggingPriority:UILayoutPriorityRequired
                          forAxis:UILayoutConstraintAxisHorizontal];
  }
  return topBar;
}

- (UIImage*)selectIconImageForCurrentState {
  if (_state == GridCellStateEditingUnselected) {
    return DefaultSymbolTemplateWithPointSize(kCircleSymbol,
                                              kIconSymbolPointSize);
  }
  return DefaultSymbolTemplateWithPointSize(kCheckmarkCircleFillSymbol,
                                            kIconSymbolPointSize);
}

// Update constraints of top bar when system font size changes. If accessibility
// font size is chosen, the favicon will be hidden, and the title text will be
// shown in two lines.
- (void)updateTopBarSize {
  self.topBarHeightConstraint.constant = [self topBarHeight];

  if (UIContentSizeCategoryIsAccessibilityCategory(
          self.traitCollection.preferredContentSizeCategory)) {
    self.titleLabel.numberOfLines = 2;
    [NSLayoutConstraint deactivateConstraints:_nonAccessibilityConstraints];
    [NSLayoutConstraint activateConstraints:_accessibilityConstraints];
  } else {
    self.titleLabel.numberOfLines = 1;
    [NSLayoutConstraint deactivateConstraints:_accessibilityConstraints];
    [NSLayoutConstraint activateConstraints:_nonAccessibilityConstraints];
  }
}

- (void)configureCloseOrSelectIconConstraints {
  BOOL showSelectionMode = self.isInSelectionMode && _selectIconView;

  self.closeIconView.hidden = showSelectionMode;
  self.selectIconView.hidden = !showSelectionMode;

  if (showSelectionMode) {
    [NSLayoutConstraint deactivateConstraints:_closeIconConstraints];
    [NSLayoutConstraint activateConstraints:_selectIconConstraints];
  } else {
    [NSLayoutConstraint deactivateConstraints:_selectIconConstraints];
    [NSLayoutConstraint activateConstraints:_closeIconConstraints];
  }
}

- (BOOL)isInSelectionMode {
  return self.state != GridCellStateNotEditing;
}

- (void)setState:(GridCellState)state {
  if (state == _state) {
    return;
  }

  _state = state;
  // TODO(crbug.com/1501837): Add the accessibility value for selected and
  // unselected states.
  self.accessibilityValue = nil;
  _closeTapTargetButton.enabled = !self.isInSelectionMode;
  self.selectIconView.image = [self selectIconImageForCurrentState];

  [self configureCloseOrSelectIconConstraints];
  self.border.hidden = self.isInSelectionMode;
}

// Sets up the selection border. The tint color is set when the theme is
// selected.
- (void)setupSelectedBackgroundView {
  self.selectedBackgroundView = [[UIView alloc] init];
  self.selectedBackgroundView.backgroundColor =
      [UIColor colorNamed:kGridBackgroundColor];
  UIView* border = [[UIView alloc] init];
  border.hidden = self.isInSelectionMode;
  border.translatesAutoresizingMaskIntoConstraints = NO;
  border.backgroundColor = [UIColor colorNamed:kGridBackgroundColor];
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
  [self.delegate closeButtonTappedForGroupCell:self];
}

// Returns the height of top bar in grid cell. The value depends on whether
// accessibility font size is chosen.
- (CGFloat)topBarHeight {
  return UIContentSizeCategoryIsAccessibilityCategory(
             self.traitCollection.preferredContentSizeCategory)
             ? kGridCellHeaderAccessibilityHeight
             : kGridCellHeaderHeight;
}

@end
