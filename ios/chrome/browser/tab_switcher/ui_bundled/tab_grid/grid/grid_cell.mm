// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_cell.h"

#import <ostream>

#import "base/check.h"
#import "base/check_op.h"
#import "base/debug/dump_without_crashing.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/elements/top_aligned_image_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/grid_empty_thumbnail_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/gfx/ios/uikit_util.h"

namespace {

// The size of symbol icons.
constexpr NSInteger kIconSymbolPointSize = 13;

// Scale of activity indicator replacing fav icon when active.
const CGFloat kIndicatorScale = 0.75;

// Inset between the snapshot view and the cell.
const CGFloat kSnapshotInset = 4.0f;

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

// Positions `view` by setting its frame's origin to `point`.
void PositionView(UIView* view, CGPoint point) {
  if (!view) {
    return;
  }
  CGRect frame = view.frame;
  frame.origin = point;
  view.frame = frame;
}

// Returns the accessibility identifier to set on a GridCell when positioned at
// the given index.
NSString* GridCellAccessibilityIdentifier(NSUInteger index) {
  return [NSString stringWithFormat:@"%@%lu", kGridCellIdentifierPrefix, index];
}

// Returns the accessibility identifier to set on the snapshot view when
// positioned at the given index.
NSString* GridCellSnapshotAccessibilityIdentifier(NSUInteger index) {
  return [NSString
      stringWithFormat:@"%@%lu", kGridCellSnapshotIdentifierPrefix, index];
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
@property(nonatomic, weak) UIActivityIndicatorView* activityIndicator;
@property(nonatomic, weak) UIActivityIndicatorView* snapshotActivityIndicator;
// Since the close icon dimensions are smaller than the recommended tap target
// size, use an overlaid tap target button.
@property(nonatomic, weak) UIButton* closeTapTargetButton;
@property(nonatomic, weak) UIView* border;
// Whether or not the cell is currently displaying an editing state.
@property(nonatomic, readonly) BOOL isInSelectionMode;
@property(nonatomic, weak) GridEmptyThumbnailView* emptyView;
// UI elements for highlighted state.
// Container for the cell's contents to enable shrinking transform.
@property(nonatomic, strong) UIView* containerView;
// Horizontal constraints for `containerView`.
@property(nonatomic, strong) NSLayoutConstraint* containerLeadingConstraint;
@property(nonatomic, strong) NSLayoutConstraint* containerTrailingConstraint;
// Background view to show while cell is highlighted.
@property(nonatomic, strong) UIView* groupingBackgroundView;
// Dimming view over the cell contents while cell is highlighted.
@property(nonatomic, strong) UIView* dimmingView;

@end

@implementation GridCell {
  // YES if the cell is currently highlighted.
  BOOL _highlighted;
}

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
    self.contentView.layer.cornerRadius = kGridCellCornerRadius;
    self.contentView.layer.masksToBounds = YES;
    UIView* contentContainer = self.contentView;

    if (IsTabGridDragAndDropEnabled()) {
      UIView* containerView = [[UIView alloc] init];
      containerView.translatesAutoresizingMaskIntoConstraints = NO;
      containerView.backgroundColor = [UIColor colorNamed:kBackgroundColor];
      containerView.layer.cornerRadius = kGridCellCornerRadius;
      containerView.layer.masksToBounds = YES;
      [self.contentView addSubview:containerView];
      _containerView = containerView;
      AddSameConstraints(self.contentView, containerView);
      contentContainer = _containerView;
    }

    UIView* topBar = [self setupTopBar];
    TopAlignedImageView* snapshotView = [[TopAlignedImageView alloc] init];
    snapshotView.translatesAutoresizingMaskIntoConstraints = NO;
    if (IsTabGridEmptyThumbnailUIEnabled()) {
      snapshotView.layer.cornerRadius = kGridCellCornerRadius;
      snapshotView.layer.masksToBounds = YES;
    }

    UIButton* closeTapTargetButton =
        [ExtendedTouchTargetButton buttonWithType:UIButtonTypeCustom];
    closeTapTargetButton.translatesAutoresizingMaskIntoConstraints = NO;
    [closeTapTargetButton addTarget:self
                             action:@selector(closeButtonTapped:)
                   forControlEvents:UIControlEventTouchUpInside];
    closeTapTargetButton.accessibilityIdentifier =
        kGridCellCloseButtonIdentifier;
    [contentContainer addSubview:topBar];
    [contentContainer addSubview:snapshotView];
    if (IsTabGridEmptyThumbnailUIEnabled()) {
      GridEmptyThumbnailView* emptyView = [[GridEmptyThumbnailView alloc]
          initWithType:EmptyThumbnailTypeGridCell];
      emptyView.translatesAutoresizingMaskIntoConstraints = NO;
      [snapshotView addSubview:emptyView];
      AddSameConstraints(snapshotView, emptyView);
      _emptyView = emptyView;
    }
    PriceCardView* priceCardView = [[PriceCardView alloc] init];
    [snapshotView addSubview:priceCardView];

    UIActivityIndicatorView* snapshotActivityIndicator =
        [[UIActivityIndicatorView alloc] init];
    snapshotActivityIndicator.translatesAutoresizingMaskIntoConstraints = NO;
    [snapshotView addSubview:snapshotActivityIndicator];

    [contentContainer addSubview:closeTapTargetButton];
    _topBar = topBar;
    _snapshotView = snapshotView;
    _closeTapTargetButton = closeTapTargetButton;
    _priceCardView = priceCardView;
    _snapshotActivityIndicator = snapshotActivityIndicator;
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
    CGFloat margin = IsTabGridEmptyThumbnailUIEnabled() ? kSnapshotInset : 0;
    self.containerLeadingConstraint = [snapshotView.leadingAnchor
        constraintEqualToAnchor:contentContainer.leadingAnchor
                       constant:margin];
    self.containerTrailingConstraint = [snapshotView.trailingAnchor
        constraintEqualToAnchor:contentContainer.trailingAnchor
                       constant:-margin];
    NSArray* constraints = @[
      [topBar.topAnchor constraintEqualToAnchor:contentContainer.topAnchor],
      [topBar.leadingAnchor
          constraintEqualToAnchor:contentContainer.leadingAnchor],
      [topBar.trailingAnchor
          constraintEqualToAnchor:contentContainer.trailingAnchor],
      [snapshotView.topAnchor constraintEqualToAnchor:topBar.bottomAnchor],
      self.containerLeadingConstraint,
      self.containerTrailingConstraint,
      [snapshotView.bottomAnchor
          constraintEqualToAnchor:contentContainer.bottomAnchor
                         constant:-margin],
      [closeTapTargetButton.topAnchor
          constraintEqualToAnchor:contentContainer.topAnchor],
      [closeTapTargetButton.trailingAnchor
          constraintEqualToAnchor:contentContainer.trailingAnchor],
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
      [snapshotActivityIndicator.centerXAnchor
          constraintEqualToAnchor:snapshotView.centerXAnchor],
      [snapshotActivityIndicator.centerYAnchor
          constraintEqualToAnchor:snapshotView.centerYAnchor],
    ];
    [NSLayoutConstraint activateConstraints:constraints];

    if (IsTabGridDragAndDropEnabled()) {
      self.groupingBackgroundView = [[UIView alloc] initWithFrame:self.bounds];
      self.groupingBackgroundView.translatesAutoresizingMaskIntoConstraints =
          NO;
      self.groupingBackgroundView.backgroundColor =
          [UIColor colorNamed:kStaticBlue400Color];
      self.groupingBackgroundView.layer.cornerRadius = kGridCellCornerRadius;
      self.groupingBackgroundView.layer.masksToBounds = YES;
      self.groupingBackgroundView.alpha = 0;
      self.groupingBackgroundView.hidden = YES;
      // Insert it behind the cell's contentView
      [self.contentView insertSubview:self.groupingBackgroundView
                         belowSubview:self.containerView];
      AddSameConstraints(self.groupingBackgroundView, self.contentView);

      self.dimmingView = [[UIView alloc] initWithFrame:self.bounds];
      self.dimmingView.translatesAutoresizingMaskIntoConstraints = NO;
      self.dimmingView.backgroundColor =
          [[UIColor blackColor] colorWithAlphaComponent:0.5];
      self.dimmingView.layer.cornerRadius = kGridCellCornerRadius;
      self.dimmingView.hidden = YES;
      self.dimmingView.alpha = 0.0;
      [contentContainer addSubview:self.dimmingView];
      AddSameConstraints(self.dimmingView, contentContainer);
    }

    NSArray<UITrait>* traits = TraitCollectionSetForTraits(
        @[ UITraitPreferredContentSizeCategory.class ]);
    __weak __typeof(self) weakSelf = self;
    UITraitChangeHandler handler = ^(id<UITraitEnvironment> traitEnvironment,
                                     UITraitCollection* previousCollection) {
      [weakSelf updateUIOnTraitChange:previousCollection];
    };
    [self registerForTraitChanges:traits withHandler:handler];
  }
  return self;
}

#pragma mark - UIView

- (void)didMoveToWindow {
  [super didMoveToWindow];
  if (self.theme == GridThemeLight) {
    [self updateInterfaceStyleForWindow:self.window];
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
  self.emptyView.hidden = NO;
  self.selected = NO;
  self.priceCardView.hidden = YES;
  self.opacity = 1.0;
  self.hidden = NO;
  [self hideFaviconActivityIndicator];
  [self hideSnapshotActivityIndicator];
  if (IsTabGridDragAndDropEnabled()) {
    [self setHighlightForGrouping:NO];
  }
  if (self.layoutGuideCenter) {
    [self.layoutGuideCenter referenceView:nil
                                underName:kSelectedRegularCellGuide];
  }
}

#pragma mark - UIAccessibility

- (BOOL)isAccessibilityElement {
  // This makes the whole cell tappable in VoiceOver rather than the individual
  // title and close button.
  return YES;
}

#pragma mark - UIAccessibilityAction

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
  if (_theme == theme) {
    return;
  }

  // The light and dark themes have different colored borders based on the
  // theme, regardless of dark mode, so `overrideUserInterfaceStyle` is not
  // enough here.
  switch (theme) {
    case GridThemeLight:
      [self updateInterfaceStyleForWindow:self.window];
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

- (void)showFaviconActivityIndicator {
  [self.activityIndicator startAnimating];
  [self.activityIndicator setHidden:NO];
  [self.iconView setHidden:YES];
}

- (void)hideFaviconActivityIndicator {
  [self.activityIndicator stopAnimating];
  [self.activityIndicator setHidden:YES];
  [self.iconView setHidden:NO];
}

- (void)showSnapshotActivityIndicator {
  [self.snapshotActivityIndicator startAnimating];
  [self.snapshotActivityIndicator setHidden:NO];
  [self.emptyView setHidden:YES];
}

- (void)hideSnapshotActivityIndicator {
  [self.snapshotActivityIndicator stopAnimating];
  [self.snapshotActivityIndicator setHidden:YES];
}

- (CGRect)snapshotFrame {
  return [self.snapshotView.superview convertRect:self.snapshotView.frame
                                           toView:nil];
}

- (void)setSnapshot:(UIImage*)snapshot {
  self.snapshotView.image = snapshot;
  _snapshot = snapshot;
  if (IsTabGridEmptyThumbnailUIEnabled()) {
    self.emptyView.hidden = snapshot != nil;
  }
}

- (void)setPriceDrop:(NSString*)price previousPrice:(NSString*)previousPrice {
  [self.priceCardView setPriceDrop:price previousPrice:previousPrice];
  [self updateAccessibilityLabel];
}

- (void)hidePriceDrop {
  self.priceCardView.hidden = YES;
}

- (void)setTitle:(NSString*)title {
  self.titleLabel.text = title;
  _title = title;
  [self updateAccessibilityLabel];
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

- (void)registerAsSelectedCellGuide {
  [self.layoutGuideCenter referenceView:self.border
                              underName:kSelectedRegularCellGuide];
}

- (void)setActivityLabelData:(ActivityLabelData*)activityLabelData {
  [super setActivityLabelData:activityLabelData];
  [self updateAccessibilityLabel];
}

- (void)setLayoutType:(EmptyThumbnailLayoutType)layoutType {
  CHECK(IsTabGridEmptyThumbnailUIEnabled());
  _layoutType = layoutType;
  _emptyView.layoutType = layoutType;
}

- (void)setAccessibilityIdentifiersWithIndex:(NSUInteger)index {
  self.accessibilityIdentifier = GridCellAccessibilityIdentifier(index);
  self.snapshotView.accessibilityIdentifier =
      GridCellSnapshotAccessibilityIdentifier(index);
}

- (void)setHighlightForGrouping:(BOOL)highlight {
  CHECK(IsTabGridDragAndDropEnabled());
  if (_highlighted == highlight) {
    return;
  }
  _highlighted = highlight;

  __weak __typeof(self) weakSelf = self;
  if (highlight) {
    [UIView animateWithDuration:kGridCellHighlightDuration
                          delay:0
                        options:UIViewAnimationOptionBeginFromCurrentState
                     animations:^{
                       [weakSelf highlightCell];
                     }
                     completion:nil];

  } else {
    [UIView animateWithDuration:kGridCellHighlightDuration
        delay:0
        options:UIViewAnimationOptionBeginFromCurrentState
        animations:^{
          [weakSelf resetHighlight];
        }
        completion:^(BOOL finished) {
          GridCell* strongSelf = weakSelf;
          strongSelf.dimmingView.hidden = YES;
          strongSelf.groupingBackgroundView.hidden = YES;
        }];
  }
}

#pragma mark - Private

// Updates the accessibility label.
- (void)updateAccessibilityLabel {
  NSString* accessibilityLabel;
  if (self.activityLabelData) {
    accessibilityLabel = l10n_util::GetNSStringF(
        IDS_IOS_TAB_GRID_CELL_UPDATED, base::SysNSStringToUTF16(self.title));
  } else {
    accessibilityLabel = self.title;
  }
  if (accessibilityLabel && self.priceCardView.accessibilityLabel) {
    accessibilityLabel =
        [@[ accessibilityLabel, self.priceCardView.accessibilityLabel ]
            componentsJoinedByString:@". "];
  }
  self.accessibilityLabel = accessibilityLabel;
}

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

  UIActivityIndicatorView* activityIndicator =
      [[UIActivityIndicatorView alloc] init];
  activityIndicator.translatesAutoresizingMaskIntoConstraints = NO;
  activityIndicator.color = [UIColor colorNamed:kBlueColor];
  activityIndicator.transform = CGAffineTransformScale(
      activityIndicator.transform, kIndicatorScale, kIndicatorScale);

  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  titleLabel.font =
      [UIFont preferredFontForTextStyle:IsTabGridEmptyThumbnailUIEnabled()
                                            ? UIFontTextStyleSubheadline
                                            : UIFontTextStyleFootnote];
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
  [self.window.windowScene
      registerForTraitChanges:@[ UITraitUserInterfaceStyle.class ]
                   withTarget:self
                       action:@selector(interfaceStyleChangedForWindow:
                                                       traitCollection:)];
  self.overrideUserInterfaceStyle =
      self.window.windowScene.traitCollection.userInterfaceStyle;
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

// Animations to highlight this cell.
- (void)highlightCell {
  // Shrink and dim contents of cell while revealing blue
  // background covering rest of the cell.
  self.groupingBackgroundView.alpha = 1.0;
  self.groupingBackgroundView.hidden = NO;
  self.dimmingView.hidden = NO;
  self.dimmingView.alpha = 1.0;
  [self.containerView bringSubviewToFront:self.dimmingView];
  self.containerView.transform = CGAffineTransformMakeScale(
      kGridCellHighlightScaleTransform, kGridCellHighlightScaleTransform);
  if (!self.border.hidden) {
    // If cell is selected, then fill in space between
    // border and the cell view to merge into one blue
    // background with _groupingBackgroundView.
    self.border.layer.borderWidth =
        kGridCellSelectionRingGapWidth + kGridCellSelectionRingTintWidth + 1;
  }
}

// Animations to reset the highlight of this cell.
- (void)resetHighlight {
  self.groupingBackgroundView.alpha = 0.0;
  self.dimmingView.alpha = 0.0;
  self.containerView.transform = CGAffineTransformIdentity;
  if (!self.border.hidden) {
    self.border.layer.borderWidth = kGridCellSelectionRingTintWidth;
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
  NOTREACHED();
}

- (UIView*)topCellView {
  return self.topBar;
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
  NOTREACHED();
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
  // Use the same animation set up for both directions.
  [self prepareForAnimation];
}

- (void)positionTabViews {
  if (!IsNewTabGridTransitionsEnabled()) {
    self.containerLeadingConstraint.constant = 0;
    self.containerTrailingConstraint.constant = 0;
    self.containerView.layer.cornerRadius = 0;
    self.snapshotView.layer.cornerRadius = 0;
  }
  [self scaleTabViews];
  self.topBarHeightConstraint.constant = self.topTabView.frame.size.height;
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
  if (!IsNewTabGridTransitionsEnabled()) {
    self.containerView.layer.cornerRadius = kGridCellCornerRadius;
    self.containerLeadingConstraint.constant =
        IsTabGridEmptyThumbnailUIEnabled() ? kSnapshotInset : 0;
    self.containerTrailingConstraint.constant =
        IsTabGridEmptyThumbnailUIEnabled() ? -kSnapshotInset : 0;
    self.snapshotView.layer.cornerRadius = kGridCellCornerRadius;
  }
  [self scaleTabViews];
  self.topBarHeightConstraint.constant = [self topBarHeight];
  [self setNeedsUpdateConstraints];
  [self layoutIfNeeded];
  CGFloat topYOffset =
      kGridCellHeaderHeight - self.topTabView.frame.size.height;
  PositionView(self.topTabView, CGPointMake(0, topYOffset));
  // Position the main view so it's top-aligned with the main cell view.
  PositionView(self.mainTabView, self.mainCellView.frame.origin);
  if (!self.bottomTabView) {
    return;
  }

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
