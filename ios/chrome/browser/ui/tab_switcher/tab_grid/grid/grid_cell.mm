// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_cell.h"

#import <MaterialComponents/MaterialActivityIndicator.h>
#import <ostream>

#import "base/check.h"
#import "base/check_op.h"
#import "base/debug/dump_without_crashing.h"
#import "base/notreached.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/elements/top_aligned_image_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
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

// Frame-based layout utilities for GridTransitionCell.
// Scales the size of `view`'s frame by `factor` in both height and width. This
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

// Positions `view` by setting its frame's origin to `point`.
void PositionView(UIView* view, CGPoint point) {
  if (!view)
    return;
  CGRect frame = view.frame;
  frame.origin = point;
  view.frame = frame;
}

}  // namespace

@interface GridCell ()
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
@property(nonatomic, weak) TopAlignedImageView* snapshotView;
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

@implementation GridCell

+ (instancetype)transitionSelectionCellFromCell:(GridCell*)cell {
  GridCell* transitionSelectionCell = [[self alloc] initWithFrame:cell.bounds];
  transitionSelectionCell.selected = YES;
  transitionSelectionCell.theme = cell.theme;
  transitionSelectionCell.contentView.hidden = YES;
  transitionSelectionCell.opacity = cell.opacity;
  return transitionSelectionCell;
}

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
    TopAlignedImageView* snapshotView = [[TopAlignedImageView alloc] init];
    snapshotView.translatesAutoresizingMaskIntoConstraints = NO;

    UIButton* closeTapTargetButton =
        [ExtendedTouchTargetButton buttonWithType:UIButtonTypeCustom];
    closeTapTargetButton.translatesAutoresizingMaskIntoConstraints = NO;
    [closeTapTargetButton addTarget:self
                             action:@selector(closeButtonTapped:)
                   forControlEvents:UIControlEventTouchUpInside];
    closeTapTargetButton.accessibilityIdentifier =
        kGridCellCloseButtonIdentifier;
    [contentView addSubview:topBar];
    [contentView addSubview:snapshotView];
    PriceCardView* priceCardView = [[PriceCardView alloc] init];
    [snapshotView addSubview:priceCardView];
    [contentView addSubview:closeTapTargetButton];
    _topBar = topBar;
    _snapshotView = snapshotView;
    _closeTapTargetButton = closeTapTargetButton;
    _priceCardView = priceCardView;
    _opacity = 1.0;

    self.contentView.backgroundColor = [UIColor colorNamed:kBackgroundColor];
    self.snapshotView.backgroundColor = [UIColor colorNamed:kBackgroundColor];
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
      [priceCardView.topAnchor
          constraintEqualToAnchor:snapshotView.topAnchor
                         constant:kGridCellPriceDropTopSpacing],
      [priceCardView.leadingAnchor
          constraintEqualToAnchor:snapshotView.leadingAnchor
                         constant:kGridCellPriceDropLeadingSpacing],
      [priceCardView.trailingAnchor
          constraintLessThanOrEqualToAnchor:snapshotView.trailingAnchor
                                   constant:-kGridCellPriceDropTrailingSpacing],
    ];
    [NSLayoutConstraint activateConstraints:constraints];

    if (@available(iOS 17, *)) {
      NSArray<UITrait>* traits = TraitCollectionSetForTraits(
          @[ UITraitPreferredContentSizeCategory.self ]);
      __weak __typeof(self) weakSelf = self;
      UITraitChangeHandler handler = ^(id<UITraitEnvironment> traitEnvironment,
                                       UITraitCollection* previousCollection) {
        [weakSelf updateUIOnTraitChange:previousCollection];
      };
      [self registerForTraitChanges:traits withHandler:handler];
    }
  }
  return self;
}

#pragma mark - UIView

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }
  [self updateUIOnTraitChange:previousTraitCollection];
}
#endif

- (void)didMoveToWindow {
  if (self.theme == GridThemeLight) {
    if (@available(iOS 17, *)) {
      [self updateInterfaceStyleForWindow:self.window];
    }
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
  self.snapshot = nil;
  self.snapshotView.image = nil;
  self.selected = NO;
  self.priceCardView.hidden = YES;
  self.opacity = 1.0;
  self.hidden = NO;
  [self hideActivityIndicator];
}

#pragma mark - UIAccessibility

- (BOOL)isAccessibilityElement {
  // This makes the whole cell tappable in VoiceOver rather than the individual
  // title and close button.
  return YES;
}

- (NSArray*)accessibilityCustomActions {
  if (self.isInSelectionMode) {
    // If the cell is in tab grid selection mode, only allow toggling the
    // selection state.
    return nil;
  }

  // In normal cell mode, there are 2 actions, accessible through swiping. The
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

  // The light and dark themes have different colored borders based on the
  // theme, regardless of dark mode, so `overrideUserInterfaceStyle` is not
  // enough here.
  switch (theme) {
    case GridThemeLight:
      if (@available(iOS 17, *)) {
        [self updateInterfaceStyleForWindow:self.window];
      } else {
        self.overrideUserInterfaceStyle = UIUserInterfaceStyleLight;
      }
      self.border.layer.borderColor =
          [UIColor colorNamed:kStaticBlue400Color].CGColor;
      break;
    case GridThemeDark:
      self.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
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

- (void)setSnapshot:(UIImage*)snapshot {
  self.snapshotView.image = snapshot;
  _snapshot = snapshot;
}

- (void)fadeInSnapshot:(UIImage*)snapshot {
  // Do not fade in the same snapshot
  if ([_snapshot isEqual:snapshot]) {
    return;
  }
  // Do not fade in if there is no previous snapshot
  if (_snapshot != nil) {
    [UIView transitionWithView:self.snapshotView
                      duration:0.2f
                       options:UIViewAnimationOptionTransitionCrossDissolve
                    animations:^{
                      self.snapshotView.image = snapshot;
                    }
                    completion:nil];
  } else {
    self.snapshotView.image = snapshot;
  }
  _snapshot = snapshot;
}

- (void)setPriceDrop:(NSString*)price previousPrice:(NSString*)previousPrice {
  [self.priceCardView setPriceDrop:price previousPrice:previousPrice];
  // Only append PriceCardView accessibility text if it doesn't already exist in
  // the accessibility label.
  if ([self.accessibilityLabel
          rangeOfString:self.priceCardView.accessibilityLabel]
          .location == NSNotFound) {
    self.accessibilityLabel =
        [@[ self.accessibilityLabel, self.priceCardView.accessibilityLabel ]
            componentsJoinedByString:@". "];
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
  _opacity = alpha;
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
  if (_state == GridCellStateEditingSelected) {
    self.accessibilityValue =
        l10n_util::GetNSString(IDS_IOS_TAB_GRID_CELL_SELECTED);
  } else if (_state == GridCellStateEditingUnselected) {
    self.accessibilityValue =
        l10n_util::GetNSString(IDS_IOS_TAB_GRID_CELL_DESELECTED);
  } else {
    self.accessibilityValue = nil;
  }
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
  [self.delegate closeButtonTappedForCell:self];
}

// Returns the height of top bar in grid cell. The value depends on whether
// accessibility font size is chosen.
- (CGFloat)topBarHeight {
  return UIContentSizeCategoryIsAccessibilityCategory(
             self.traitCollection.preferredContentSizeCategory)
             ? kGridCellHeaderAccessibilityHeight
             : kGridCellHeaderHeight;
}

// If window is not nil, register for updates to its interface style updates and
// set the user interface style to be the same as the window.
- (void)updateInterfaceStyleForWindow:(UIWindow*)window {
  if (!window) {
    return;
  }
  if (@available(iOS 17, *)) {
    [self.window.windowScene
        registerForTraitChanges:@[ UITraitUserInterfaceStyle.self ]
                     withTarget:self
                         action:@selector(interfaceStyleChangedForWindow:
                                                         traitCollection:)];
    self.overrideUserInterfaceStyle =
        self.window.windowScene.traitCollection.userInterfaceStyle;
  }
}

// Callback for the observation of the user interface style trait of the window
// scene.
- (void)interfaceStyleChangedForWindow:(UIView*)window
                       traitCollection:(UITraitCollection*)traitCollection {
  self.overrideUserInterfaceStyle =
      self.window.windowScene.traitCollection.userInterfaceStyle;
}

// Updates the size of the 'top bar' UI when the view's UITraits change.
- (void)updateUIOnTraitChange:(UITraitCollection*)previousTraitCollection {
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
  proxy.priceCardView = cell.priceCardView;
  proxy.opacity = cell.opacity;
  return proxy;
}
#pragma mark - GridToTabTransitionView properties.

- (void)setTopCellView:(UIView*)topCellView {
  // The top cell view is `topBar` and can't be changed.
  NOTREACHED_IN_MIGRATION();
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
  NOTREACHED_IN_MIGRATION();
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

- (void)prepareForTransitionWithAnimationDirection:
    (GridAnimationDirection)animationDirection {
  // Use the same animation set up for both directions.
  [self prepareForAnimation];
}

- (void)positionTabViews {
  [self scaleTabViews];
  self.topBarHeightConstraint.constant = self.topTabView.frame.size.height;
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
  self.topBarHeightConstraint.constant = [self topBarHeight];
  [self setNeedsUpdateConstraints];
  [self layoutIfNeeded];
  CGFloat topYOffset =
      kGridCellHeaderHeight - self.topTabView.frame.size.height;
  PositionView(self.topTabView, CGPointMake(0, topYOffset));
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
    CGFloat bottomYOffset = CGRectGetMaxY(self.mainCellView.frame);
    PositionView(self.bottomTabView, CGPointMake(0, bottomYOffset));
  }
}

#pragma mark - Private helper methods

// Common logic for the cell animation preparation.
- (void)prepareForAnimation {
  // Remove dark corners from the transition animtation cell.
  self.backgroundColor = [UIColor clearColor];
}

// Scales the tab views relative to the current width of the cell.
- (void)scaleTabViews {
  CGFloat scale = self.bounds.size.width / _previousTabViewWidth;
  ScaleView(self.topTabView, scale);
  ScaleView(self.mainTabView, scale);
  ScaleView(self.bottomTabView, scale);
  _previousTabViewWidth = self.mainTabView.frame.size.width;
}

@end
