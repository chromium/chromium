// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_cell.h"

#import <MaterialComponents/MaterialActivityIndicator.h>
#import <ostream>

#import "base/check.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/ui/elements/top_aligned_image_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_item_identifier.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_tabs_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/legacy_grid_transition_animation.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/gradient_view.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state_id.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/gfx/ios/uikit_util.h"

namespace {
// TODO(crbug.com/40890700): Refactor this method.
// Frame-based layout utilities for GridTransitionCell.
// Scales the size of `view`'s frame by `factor` in both height and width. This
// scaling is done by changing the frame size without changing its origin,
// unlike a scale transform which scales around the view's center.
void ScaleView(UIView* view, CGFloat factor) {
  if (!view) {
    return;
  }
  CGRect frame = view.frame;
  frame.size.width *= factor;
  frame.size.height *= factor;
  view.frame = frame;
}

// TODO(crbug.com/40890700): Refactor this method.
// Positions `view` by setting its frame's origin to `point`.
void PositionView(UIView* view, CGPoint point) {
  if (!view) {
    return;
  }
  CGRect frame = view.frame;
  frame.origin = point;
  view.frame = frame;
}

// Returns "dark" non-dynamic color from provided `dynamicColor`.
//
// Note: Pinned cell doesn't change it's colors in the Light/Dark modes.
// When dynamic colors are used it might create discreptancies during the
// transtion animation. The cell could be copied with system interface traits
// applied. Therefore, the Light colors could show up during the animation.
//
// In order to match the appearance of the other UI-elements on the tab grid
// it is worth using the same color names, but using "dark" non-dynamic part
// of them.
UIColor* GetInterfaceStyleDarkColor(UIColor* dynamicColor) {
  UITraitCollection* interfaceStyleDarkTraitCollection = [UITraitCollection
      traitCollectionWithUserInterfaceStyle:UIUserInterfaceStyleDark];

  return [dynamicColor
      resolvedColorWithTraitCollection:interfaceStyleDarkTraitCollection];
}
}  // namespace

@interface PinnedCell ()

// Title is displayed by this label.
@property(nonatomic, weak) UILabel* titleLabel;
// View that holds the WebState's snapshot image.
@property(nonatomic, weak) TopAlignedImageView* snapshotView;
// Container view that holds favicon and title.
//
// Note: This view is a counterpart of the GridCell's `topBar`. It is named as
// a `header` since there are no top and bottom parts (bars) in the pinned
// cell.
@property(nonatomic, weak) UIView* headerView;
// Gradient view that fades out the trailing part of the title label.
@property(nonatomic, weak) UIView* titleLabelFader;
// Snapshot view's top constraint.
@property(nonatomic, strong) NSLayoutConstraint* snapshotViewTopConstraint;
// Header view's height constraint.
@property(nonatomic, strong) NSLayoutConstraint* headerViewHeightConstraint;
// Favicon container view's center Y constraint.
@property(nonatomic, strong)
    NSLayoutConstraint* faviconContainerViewCenterYConstraint;
@end

@implementation PinnedCell {
  // Container for the title label.
  UIView* _titleLabelContainer;
  // Container for the `_faviconView`.
  UIView* _faviconContainerView;
  // View for displaying the favicon.
  UIImageView* _faviconView;
  // Activity Indicator view that animates while WebState is loading.
  MDCActivityIndicator* _activityIndicator;
  // Title label's leading constraint.
  NSLayoutConstraint* _titleLabelLeadingConstraint;
  // Title label's trailing constraint.
  NSLayoutConstraint* _titleLabelTrailingConstraint;
  // Title label's fader leading constraint.
  NSLayoutConstraint* _titleLabelFaderLeadingConstraint;
  // Title label's fader trailing constraint.
  NSLayoutConstraint* _titleLabelFaderTrailingConstraint;
}

+ (instancetype)transitionSelectionCellFromCell:(PinnedCell*)cell {
  PinnedCell* transitionSelectionCell =
      [[self alloc] initWithFrame:cell.bounds];
  transitionSelectionCell.selected = YES;
  transitionSelectionCell.contentView.hidden = YES;
  return transitionSelectionCell;
}

- (instancetype)initWithFrame:(CGRect)frame {
  if ((self = [super initWithFrame:frame])) {
    self.backgroundColor = GetInterfaceStyleDarkColor(
        [UIColor colorNamed:kSecondaryBackgroundColor]);
    self.layer.cornerRadius = kPinnedCellCornerRadius;
    self.layer.masksToBounds = NO;

    self.contentView.layer.cornerRadius = kPinnedCellCornerRadius;
    self.contentView.layer.masksToBounds = YES;
    self.contentView.backgroundColor = GetInterfaceStyleDarkColor(
        [UIColor colorNamed:kSecondaryBackgroundColor]);

    [self setupSelectedBackgroundView];
    [self setupSnapshotView];
    [self setupHeaderView];
    [self setupFaviconContainerView];
    [self setupFaviconView];
    [self setupActivityIndicator];
    [self setupTitleLabel];
    [self setupTitleLabelFader];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.icon = nil;
  self.title = nil;
  self.snapshot = nil;
  self.pinnedItemIdentifier = web::WebStateID();
}

#pragma mark - Public

- (void)setHighlighted:(BOOL)highlighted {
  // NO-OP to disable highlighting and only allow selection.
}

- (UIImage*)icon {
  return _faviconView.image;
}

- (void)setIcon:(UIImage*)icon {
  _faviconView.image = icon;
}

- (UIImage*)snapshot {
  return self.snapshotView.image;
}

- (void)setSnapshot:(UIImage*)snapshot {
  self.snapshotView.image = snapshot;
}

- (NSString*)title {
  return _titleLabel.text;
}

- (void)setTitle:(NSString*)title {
  NSTextAlignment titleTextAligment = DetermineBestAlignmentForText(title);

  _titleLabel.text = [title copy];
  _titleLabel.textAlignment = titleTextAligment;
  self.accessibilityLabel = [self accessibilityLabelWithTitle:title];

  [self updateTitleLabelAppearance];
  [self updateTitleLabelFaderAppearance];
}

- (UIDragPreviewParameters*)dragPreviewParameters {
  UIBezierPath* visiblePath = [UIBezierPath
      bezierPathWithRoundedRect:self.bounds
                   cornerRadius:self.contentView.layer.cornerRadius];
  UIDragPreviewParameters* params = [[UIDragPreviewParameters alloc] init];
  params.visiblePath = visiblePath;
  return params;
}

- (void)showActivityIndicator {
  [_activityIndicator startAnimating];
  [_activityIndicator setHidden:NO];
  [_faviconContainerView setHidden:YES];
}

- (void)hideActivityIndicator {
  [_activityIndicator stopAnimating];
  [_activityIndicator setHidden:YES];
  [_faviconContainerView setHidden:NO];
}

- (void)setPinnedItemIdentifier:(web::WebStateID)pinnedItemIdentifier {
  _pinnedItemIdentifier = pinnedItemIdentifier;
  if (pinnedItemIdentifier.valid()) {
    TabSwitcherItem* item =
        [[TabSwitcherItem alloc] initWithIdentifier:pinnedItemIdentifier];
    self.itemIdentifier = [[GridItemIdentifier alloc] initWithTabItem:item];
  } else {
    self.itemIdentifier = nil;
  }
}

#pragma mark - UIAccessibility

- (BOOL)isAccessibilityElement {
  // This makes the whole cell tappable in VoiceOver.
  return YES;
}

#pragma mark - Private

// Sets up the selection border.
- (void)setupSelectedBackgroundView {
  UIView* selectedBackgroundBorderView = [[UIView alloc] init];
  selectedBackgroundBorderView.translatesAutoresizingMaskIntoConstraints = NO;
  selectedBackgroundBorderView.layer.cornerRadius =
      kPinnedCellCornerRadius + kPinnedCellSelectionRingGapWidth +
      kPinnedCellSelectionRingTintWidth;
  selectedBackgroundBorderView.layer.borderWidth =
      kPinnedCellSelectionRingTintWidth;
  selectedBackgroundBorderView.layer.borderColor =
      [UIColor colorNamed:kStaticBlue400Color].CGColor;

  UIView* selectedBackgroundView = [[UIView alloc] init];
  [selectedBackgroundView addSubview:selectedBackgroundBorderView];

  [NSLayoutConstraint activateConstraints:@[
    [selectedBackgroundBorderView.topAnchor
        constraintEqualToAnchor:selectedBackgroundView.topAnchor
                       constant:-kPinnedCellSelectionRingPadding],
    [selectedBackgroundBorderView.leadingAnchor
        constraintEqualToAnchor:selectedBackgroundView.leadingAnchor
                       constant:-kPinnedCellSelectionRingPadding],
    [selectedBackgroundBorderView.trailingAnchor
        constraintEqualToAnchor:selectedBackgroundView.trailingAnchor
                       constant:kPinnedCellSelectionRingPadding],
    [selectedBackgroundBorderView.bottomAnchor
        constraintEqualToAnchor:selectedBackgroundView.bottomAnchor
                       constant:kPinnedCellSelectionRingPadding]
  ]];

  self.selectedBackgroundView = selectedBackgroundView;
}

- (void)setupSnapshotView {
  TopAlignedImageView* snapshotView = [[TopAlignedImageView alloc] init];
  snapshotView.translatesAutoresizingMaskIntoConstraints = NO;
  // Snapshot view is shown only during the animation transition to the Tab
  // view. The Tab view uses not static, but dynamic colors. Therefore, it is
  // safe to apply dynamic color here.
  snapshotView.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
  snapshotView.hidden = YES;
  _snapshotView = snapshotView;

  UIView* contentView = self.contentView;
  [contentView addSubview:snapshotView];

  _snapshotViewTopConstraint = [snapshotView.topAnchor
      constraintEqualToAnchor:contentView.topAnchor
                     constant:kPinnedCellSnapshotTopPadding];

  NSArray* constraints = @[
    _snapshotViewTopConstraint,
    [snapshotView.leadingAnchor
        constraintEqualToAnchor:contentView.leadingAnchor],
    [snapshotView.trailingAnchor
        constraintEqualToAnchor:contentView.trailingAnchor],
    [snapshotView.bottomAnchor
        constraintGreaterThanOrEqualToAnchor:contentView.bottomAnchor],
  ];
  [NSLayoutConstraint activateConstraints:constraints];
}

- (void)setupHeaderView {
  UIView* headerView = [[UIView alloc] init];
  headerView.translatesAutoresizingMaskIntoConstraints = NO;
  headerView.clipsToBounds = YES;
  headerView.backgroundColor = GetInterfaceStyleDarkColor(
      [UIColor colorNamed:kSecondaryBackgroundColor]);
  headerView.layer.cornerRadius = kPinnedCellCornerRadius;
  _headerView = headerView;

  UIView* contentView = self.contentView;
  [contentView addSubview:headerView];

  _headerViewHeightConstraint =
      [headerView.heightAnchor constraintEqualToConstant:kPinnedCellHeight];

  NSArray* constraints = @[
    _headerViewHeightConstraint,
    [headerView.topAnchor constraintEqualToAnchor:contentView.topAnchor],
    [headerView.leadingAnchor
        constraintEqualToAnchor:contentView.leadingAnchor],
    [headerView.trailingAnchor
        constraintEqualToAnchor:contentView.trailingAnchor],
  ];
  [NSLayoutConstraint activateConstraints:constraints];
}

// Sets up the `_faviconContainerView` view.
- (void)setupFaviconContainerView {
  UIView* faviconContainerView = [[UIView alloc] init];
  faviconContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  [_headerView addSubview:faviconContainerView];

  _faviconContainerViewCenterYConstraint = [faviconContainerView.centerYAnchor
      constraintEqualToAnchor:_headerView.topAnchor
                     constant:kPinnedCellHeight / 2];

  [NSLayoutConstraint activateConstraints:@[
    _faviconContainerViewCenterYConstraint,
    [faviconContainerView.leadingAnchor
        constraintEqualToAnchor:_headerView.leadingAnchor
                       constant:kPinnedCellHorizontalPadding],
    [faviconContainerView.widthAnchor
        constraintEqualToConstant:kPinnedCellFaviconContainerWidth],
    [faviconContainerView.heightAnchor
        constraintEqualToAnchor:faviconContainerView.widthAnchor],
  ]];

  _faviconContainerView = faviconContainerView;
}

// Sets up the `_faviconView` view.
- (void)setupFaviconView {
  UIImageView* faviconView = [[UIImageView alloc] init];
  [_faviconContainerView addSubview:faviconView];

  faviconView.translatesAutoresizingMaskIntoConstraints = NO;
  faviconView.contentMode = UIViewContentModeScaleAspectFill;
  faviconView.clipsToBounds = YES;
  faviconView.layer.cornerRadius = kPinnedCellFaviconCornerRadius;
  faviconView.layer.masksToBounds = YES;
  faviconView.tintColor =
      GetInterfaceStyleDarkColor([UIColor colorNamed:kTextPrimaryColor]);

  [NSLayoutConstraint activateConstraints:@[
    [faviconView.widthAnchor constraintEqualToConstant:kPinnedCellFaviconWidth],
    [faviconView.heightAnchor constraintEqualToAnchor:faviconView.widthAnchor],
    [faviconView.centerYAnchor
        constraintEqualToAnchor:_faviconContainerView.centerYAnchor],
    [faviconView.centerXAnchor
        constraintEqualToAnchor:_faviconContainerView.centerXAnchor],
  ]];

  _faviconView = faviconView;
  // Set the default icon.
  self.icon = nil;
}

- (void)setupActivityIndicator {
  CGRect indicatorFrame =
      CGRectMake(0, 0, kPinnedCellFaviconWidth, kPinnedCellFaviconWidth);
  MDCActivityIndicator* activityIndicator =
      [[MDCActivityIndicator alloc] initWithFrame:indicatorFrame];
  activityIndicator.translatesAutoresizingMaskIntoConstraints = NO;
  activityIndicator.cycleColors = @[ [UIColor colorNamed:kBlueColor] ];
  activityIndicator.radius =
      ui::AlignValueToUpperPixel(kPinnedCellFaviconWidth / 2);
  [_headerView addSubview:activityIndicator];

  [NSLayoutConstraint activateConstraints:@[
    [activityIndicator.centerYAnchor
        constraintEqualToAnchor:_faviconView.centerYAnchor],
    [activityIndicator.centerXAnchor
        constraintEqualToAnchor:_faviconView.centerXAnchor],
  ]];

  _activityIndicator = activityIndicator;
}

// Sets up the title label.
- (void)setupTitleLabel {
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  titleLabel.adjustsFontForContentSizeCategory = YES;
  titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  titleLabel.textColor =
      GetInterfaceStyleDarkColor([UIColor colorNamed:kTextPrimaryColor]);

  UIView* titleLabelContainer = [[UIView alloc] init];
  titleLabelContainer.translatesAutoresizingMaskIntoConstraints = NO;
  titleLabelContainer.clipsToBounds = YES;
  [titleLabelContainer addSubview:titleLabel];

  [_headerView addSubview:titleLabelContainer];

  [NSLayoutConstraint activateConstraints:@[
    [titleLabelContainer.heightAnchor
        constraintLessThanOrEqualToAnchor:_headerView.heightAnchor],
    [titleLabelContainer.centerYAnchor
        constraintEqualToAnchor:_faviconContainerView.centerYAnchor],
    [titleLabelContainer.leadingAnchor
        constraintEqualToAnchor:_faviconContainerView.trailingAnchor
                       constant:kPinnedCellTitleLeadingPadding],
    [titleLabelContainer.trailingAnchor
        constraintEqualToAnchor:_headerView.trailingAnchor
                       constant:-kPinnedCellHorizontalPadding],
    [titleLabelContainer.heightAnchor
        constraintEqualToAnchor:titleLabel.heightAnchor],
    [titleLabelContainer.centerYAnchor
        constraintEqualToAnchor:titleLabel.centerYAnchor]
  ]];

  _titleLabelLeadingConstraint = [titleLabelContainer.leadingAnchor
      constraintEqualToAnchor:titleLabel.leadingAnchor];
  _titleLabelTrailingConstraint = [titleLabelContainer.trailingAnchor
      constraintEqualToAnchor:titleLabel.trailingAnchor];

  _titleLabelContainer = titleLabelContainer;
  _titleLabel = titleLabel;

  [self updateTitleLabelAppearance];
}

// Sets up the gradient view that fades out the title label.
- (void)setupTitleLabelFader {
  UIColor* backgroundColor = GetInterfaceStyleDarkColor(
      [UIColor colorNamed:kSecondaryBackgroundColor]);

  UIColor* transparentColor = [backgroundColor colorWithAlphaComponent:0.0f];
  UIColor* opaqueColor = [backgroundColor colorWithAlphaComponent:1.0f];

  GradientView* gradientView =
      [[GradientView alloc] initWithStartColor:transparentColor
                                      endColor:opaqueColor
                                    startPoint:CGPointMake(0.0f, 0.5f)
                                      endPoint:CGPointMake(1.0f, 0.5f)];
  gradientView.translatesAutoresizingMaskIntoConstraints = NO;
  [_headerView addSubview:gradientView];

  [NSLayoutConstraint activateConstraints:@[
    [gradientView.widthAnchor
        constraintEqualToConstant:kPinnedCellFaderGradientWidth],
    [gradientView.heightAnchor
        constraintEqualToAnchor:_titleLabelContainer.heightAnchor],
    [gradientView.centerYAnchor
        constraintEqualToAnchor:_titleLabelContainer.centerYAnchor],
  ]];

  _titleLabelFaderLeadingConstraint = [gradientView.leadingAnchor
      constraintEqualToAnchor:_titleLabelContainer.leadingAnchor];
  _titleLabelFaderTrailingConstraint = [gradientView.trailingAnchor
      constraintEqualToAnchor:_titleLabelContainer.trailingAnchor];

  _titleLabelFader = gradientView;

  [self updateTitleLabelFaderAppearance];
}

- (void)updateTitleLabelAppearance {
  NSTextAlignment titleTextAligment = _titleLabel.textAlignment;

  if (UseRTLLayout()) {
    if (titleTextAligment == NSTextAlignmentLeft) {
      _titleLabelLeadingConstraint.active = NO;
      _titleLabelTrailingConstraint.active = YES;
    } else {
      _titleLabelTrailingConstraint.active = NO;
      _titleLabelLeadingConstraint.active = YES;
    }
  } else {
    if (titleTextAligment == NSTextAlignmentLeft) {
      _titleLabelTrailingConstraint.active = NO;
      _titleLabelLeadingConstraint.active = YES;
    } else {
      _titleLabelLeadingConstraint.active = NO;
      _titleLabelTrailingConstraint.active = YES;
    }
  }
}

// Updates the position and direction of the gradient view that fades out the
// title label.
- (void)updateTitleLabelFaderAppearance {
  NSTextAlignment titleTextAligment = _titleLabel.textAlignment;

  if (UseRTLLayout()) {
    if (titleTextAligment == NSTextAlignmentLeft) {
      [_titleLabelFader setTransform:CGAffineTransformMakeScale(1, 1)];
      _titleLabelFaderTrailingConstraint.active = NO;
      _titleLabelFaderLeadingConstraint.active = YES;
    } else {
      [_titleLabelFader setTransform:CGAffineTransformMakeScale(-1, 1)];
      _titleLabelFaderLeadingConstraint.active = NO;
      _titleLabelFaderTrailingConstraint.active = YES;
    }
  } else {
    if (titleTextAligment == NSTextAlignmentLeft) {
      [_titleLabelFader setTransform:CGAffineTransformMakeScale(1, 1)];
      _titleLabelFaderLeadingConstraint.active = NO;
      _titleLabelFaderTrailingConstraint.active = YES;
    } else {
      [_titleLabelFader setTransform:CGAffineTransformMakeScale(-1, 1)];
      _titleLabelFaderTrailingConstraint.active = NO;
      _titleLabelFaderLeadingConstraint.active = YES;
    }
  }
}

- (NSString*)accessibilityLabelWithTitle:(NSString*)title {
  return l10n_util::GetNSStringF(IDS_IOS_PINNED_TAB_ACCESSIBILITY_LABEL,
                                 base::SysNSStringToUTF16(title));
}

@end

// TODO(crbug.com/40890700): Refacor PinnedTransitionCell.
@implementation PinnedTransitionCell {
  // Previous tab view width, used to scale the tab views.
  CGFloat _previousTabViewWidth;
}

// Synthesis of GridToTabTransitionView properties.
@synthesize topTabView = _topTabView;
@synthesize mainTabView = _mainTabView;
@synthesize bottomTabView = _bottomTabView;

+ (instancetype)transitionCellFromCell:(PinnedCell*)cell {
  PinnedTransitionCell* proxy = [[self alloc] initWithFrame:cell.bounds];
  proxy.selected = NO;
  proxy.icon = cell.icon;
  proxy.title = cell.title;
  proxy.snapshot = cell.snapshot;

  return proxy;
}

#pragma mark - GridToTabTransitionView properties.

- (void)setTopCellView:(UIView*)topCellView {
  // The top cell view is `topBar` and can't be changed.
  NOTREACHED_IN_MIGRATION();
}

- (UIView*)topCellView {
  return self.headerView;
}

- (void)setTopTabView:(UIView*)topTabView {
  DCHECK(!_topTabView) << "topTabView should only be set once.";
  if (!topTabView.superview) {
    [self.contentView addSubview:topTabView];
  }
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
  if (!mainTabView.superview) {
    [self.contentView addSubview:mainTabView];
  }
  _previousTabViewWidth = mainTabView.frame.size.width;
  _mainTabView = mainTabView;
}

- (void)setBottomTabView:(UIView*)bottomTabView {
  DCHECK(!_bottomTabView) << "bottomTabView should only be set once.";
  if (!bottomTabView.superview) {
    [self.contentView addSubview:bottomTabView];
  }
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
  switch (animationDirection) {
    case GridAnimationDirectionContracting:
      [self prepareForContractingAnimation];
      break;
    case GridAnimationDirectionExpanding:
      [self prepareForExpandingAnimation];
      break;
  }
}

- (void)positionTabViews {
  [self scaleTabViews];

  self.snapshotViewTopConstraint.constant = self.topTabView.frame.size.height;
  self.headerViewHeightConstraint.constant = self.topTabView.frame.size.height;
  self.faviconContainerViewCenterYConstraint.constant =
      self.topTabView.frame.size.height / 2;

  [self setNeedsUpdateConstraints];
  [self layoutIfNeeded];

  PositionView(self.topTabView, CGPointMake(0, 0));
  // Position the main view so it's top-aligned with the main cell view.
  PositionView(self.mainTabView, self.mainCellView.frame.origin);

  if (!self.bottomTabView) {
    return;
  }

  // Position the bottom tab view at the bottom.
  CGFloat yPosition = CGRectGetMaxY(self.contentView.bounds) -
                      self.bottomTabView.frame.size.height;
  PositionView(self.bottomTabView, CGPointMake(0, yPosition));
}

- (void)positionCellViews {
  [self scaleTabViews];

  self.snapshotViewTopConstraint.constant = kPinnedCellSnapshotTopPadding;
  self.faviconContainerViewCenterYConstraint.constant = kPinnedCellHeight / 2;

  [self setNeedsUpdateConstraints];
  [self layoutIfNeeded];

  PositionView(self.topTabView, CGPointMake(0, 0));
  // Position the main view so it's top-aligned with the main cell view.
  PositionView(self.mainTabView, self.mainCellView.frame.origin);

  if (!self.bottomTabView) {
    return;
  }

  if (self.bottomTabView.frame.origin.y > 0) {
    // Position the bottom tab so it's equivalently located.
    PositionView(self.bottomTabView,
                 CGPointMake(0, CGRectGetMaxY(self.mainCellView.frame) +
                                    CGRectGetHeight(self.mainCellView.frame)));
  } else {
    // Position the bottom tab view below the main content view.
    CGFloat bottomYOffset = CGRectGetMaxY(self.mainCellView.frame);
    PositionView(self.bottomTabView, CGPointMake(0, bottomYOffset));
  }
}

#pragma mark - Private helper methods

// Prepares the cell for contracting animation.
- (void)prepareForContractingAnimation {
  [self preapreForAnimation];
}

// Prepares the cell for expanding animation.
- (void)prepareForExpandingAnimation {
  [self preapreForAnimation];

  self.headerView.backgroundColor =
      [UIColor colorNamed:kSecondaryBackgroundColor];
  self.titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];

  self.titleLabelFader.hidden = YES;
}

// Common logic for the cell animation preparation.
- (void)preapreForAnimation {
  // Remove dark corners from the transition animtation cell.
  self.backgroundColor = [UIColor clearColor];

  // Pinned transition cell does the transition to the Tab view, which uses
  // not static but dynamic colors. Therefore, in order to match the background
  // colors of the Tab view, all pinned transtion cell colors must be dynamic.
  self.contentView.backgroundColor =
      [UIColor colorNamed:kSecondaryBackgroundColor];

  self.snapshotView.hidden = NO;
}

// Scales the tab views relative to the current width of the cell.
- (void)scaleTabViews {
  DUMP_WILL_BE_CHECK_NE(_previousTabViewWidth, 0);
  CGFloat scale = self.bounds.size.width / _previousTabViewWidth;
  ScaleView(self.topTabView, scale);
  ScaleView(self.mainTabView, scale);
  ScaleView(self.bottomTabView, scale);
  _previousTabViewWidth = self.mainTabView.frame.size.width;
}

@end
