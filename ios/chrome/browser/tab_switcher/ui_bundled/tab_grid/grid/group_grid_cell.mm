// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/group_grid_cell.h"

#import <ostream>

#import "base/check.h"
#import "base/check_op.h"
#import "base/debug/dump_without_crashing.h"
#import "base/notreached.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/saved_tab_groups/ui/face_pile_providing.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/group_grid_cell_dot_view.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/group_tab_view.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_snapshots_view.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_snapshot_and_favicon.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/gfx/ios/uikit_util.h"

namespace {

// The size of symbol icons.
const CGFloat kIconSymbolPointSize = 13;

// Offsets the top and bottom snapshot views.
const CGFloat kSnapshotViewLeadingOffset = 4;
const CGFloat kSnapshotViewTrailingOffset = 4;
const CGFloat kSnapShotViewBottomOffset = 4;
// The top bar inset of the t under normal font size.
const CGFloat kTopBarInset = 10;
// The top bar inset under accessibility font size.
const CGFloat kTopBarLargeInset = 20;

}  // namespace

@interface GroupGridCell ()

// The face pile view.
@property(nonatomic, strong) UIView* facePile;
// Border for when the cell is selected.
@property(nonatomic, strong) UIView* border;
// UI elements for highlighted state.
// Container for the cell's contents to enable shrinking transform.
@property(nonatomic, strong) UIView* containerView;
// Background view to show while cell is highlighted.
@property(nonatomic, strong) UIView* groupingBackgroundView;
// Dimming view over the cell contents while cell is highlighted.
@property(nonatomic, strong) UIView* dimmingView;

@end

@implementation GroupGridCell {
  // The dot/facepile container view constraints enabled under accessibility
  // font size.
  NSArray<NSLayoutConstraint*>* _dotContainerAccessibilityConstraints;
  // The dot/facepile container view constraints enabled under normal font size.
  NSArray<NSLayoutConstraint*>* _dotContainerNormalConstraints;
  // The constraints enabled while showing the close icon.
  NSArray<NSLayoutConstraint*>* _closeIconConstraints;
  // The constraints enabled while showing the selection icon.
  NSArray<NSLayoutConstraint*>* _selectIconConstraints;
  // Header height of the cell.
  NSLayoutConstraint* _topBarHeightConstraint;
  // Visual components of the cell.
  UIView* _topBar;
  GroupGridCellDotView* _dotContainer;
  UIView* _facePileContainerView;
  UILabel* _titleLabel;
  UIImageView* _closeIconView;
  UIImageView* _selectIconView;
  // Since the close icon dimensions are smaller than the recommended tap target
  // size, use an overlaid tap target button.
  UIButton* _closeTapTargetButton;
  TabGroupSnapshotsView* _groupSnapshotsView;
  // YES if the cell is currently highlighted.
  BOOL _highlighted;
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
      containerView.backgroundColor =
          [UIColor colorNamed:kSecondaryBackgroundColor];
      containerView.layer.cornerRadius = kGridCellCornerRadius;
      containerView.layer.masksToBounds = YES;
      [self.contentView addSubview:containerView];
      _containerView = containerView;
      AddSameConstraints(self.contentView, containerView);
      contentContainer = _containerView;
    }

    [self setupTopBar];
    _groupSnapshotsView = [[TabGroupSnapshotsView alloc]
        initWithLightInterface:self.theme == GridThemeLight
                          cell:YES];
    _groupSnapshotsView.translatesAutoresizingMaskIntoConstraints = NO;

    _closeTapTargetButton =
        [ExtendedTouchTargetButton buttonWithType:UIButtonTypeCustom];
    _closeTapTargetButton.translatesAutoresizingMaskIntoConstraints = NO;
    [_closeTapTargetButton addTarget:self
                              action:@selector(closeButtonTapped:)
                    forControlEvents:UIControlEventTouchUpInside];
    _closeTapTargetButton.accessibilityIdentifier =
        kGridCellCloseButtonIdentifier;

    [contentContainer addSubview:_topBar];
    [contentContainer addSubview:_groupSnapshotsView];
    [contentContainer addSubview:_closeTapTargetButton];
    _opacity = 1.0;

    self.contentView.backgroundColor =
        [UIColor colorNamed:kSecondaryBackgroundColor];

    _groupSnapshotsView.backgroundColor =
        [UIColor colorNamed:kSecondaryBackgroundColor];
    _topBar.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
    _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _closeIconView.tintColor = [UIColor colorNamed:kCloseButtonColor];

    self.layer.cornerRadius = kGridCellCornerRadius;
    self.layer.shadowColor = [UIColor blackColor].CGColor;
    self.layer.shadowOffset = CGSizeMake(0, 0);
    self.layer.shadowRadius = 4.0f;
    self.layer.shadowOpacity = 0.5f;
    self.layer.masksToBounds = NO;
    _groupSnapshotsView.layer.cornerRadius = kGridCellCornerRadius;
    _groupSnapshotsView.layer.masksToBounds = YES;

    NSArray* constraints = @[
      [_topBar.topAnchor constraintEqualToAnchor:contentContainer.topAnchor],
      [_topBar.leadingAnchor
          constraintEqualToAnchor:contentContainer.leadingAnchor],
      [_topBar.trailingAnchor
          constraintEqualToAnchor:contentContainer.trailingAnchor],
      [_groupSnapshotsView.topAnchor
          constraintEqualToAnchor:_topBar.bottomAnchor],
      [_groupSnapshotsView.leadingAnchor
          constraintEqualToAnchor:contentContainer.leadingAnchor
                         constant:kSnapshotViewLeadingOffset],
      [_groupSnapshotsView.trailingAnchor
          constraintEqualToAnchor:contentContainer.trailingAnchor
                         constant:-kSnapshotViewTrailingOffset],
      [_groupSnapshotsView.bottomAnchor
          constraintEqualToAnchor:contentContainer.bottomAnchor
                         constant:-kSnapShotViewBottomOffset],
      [_closeTapTargetButton.topAnchor
          constraintEqualToAnchor:contentContainer.topAnchor],
      [_closeTapTargetButton.trailingAnchor
          constraintEqualToAnchor:contentContainer.trailingAnchor],
      [_closeTapTargetButton.widthAnchor
          constraintEqualToConstant:kGridCellCloseTapTargetWidthHeight],
      [_closeTapTargetButton.heightAnchor
          constraintEqualToConstant:kGridCellCloseTapTargetWidthHeight],
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
      self.groupingBackgroundView.alpha = 0.0;
      self.groupingBackgroundView.hidden = YES;
      // Insert it behind the cell's contentView
      [self addSubview:self.groupingBackgroundView];
      [self.contentView insertSubview:self.groupingBackgroundView
                         belowSubview:self.containerView];
      AddSameConstraints(self.groupingBackgroundView, self);

      self.dimmingView = [[UIView alloc] initWithFrame:self.bounds];
      self.dimmingView.translatesAutoresizingMaskIntoConstraints = NO;
      self.dimmingView.backgroundColor =
          [[UIColor blackColor] colorWithAlphaComponent:0.5];
      self.dimmingView.hidden = YES;
      self.dimmingView.alpha = 0.0;
      [contentContainer addSubview:self.dimmingView];
      AddSameConstraints(self.dimmingView, contentContainer);
    }

    [self registerForTraitChanges:@[ UITraitPreferredContentSizeCategory.class ]
                       withAction:@selector(updateTopBarConstraints)];
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
  self.groupColor = nil;
  self.selected = NO;
  self.opacity = 1.0;
  self.hidden = NO;
  self.facePileProvider = nil;
  if (IsTabGridDragAndDropEnabled()) {
    [self setHighlightForGrouping:NO];
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
  if ([self isInSelectionMode]) {
    // If the cell is in tab grid selection mode, only allow toggling the
    // selection state.
    return nil;
  }

  // In normal cell mode, there are 2 actions, accessible through swiping. The
  // default is to select the group cell. Another is to close the group cell.
  return @[ [[UIAccessibilityCustomAction alloc]
      initWithName:l10n_util::GetNSString(IDS_IOS_TAB_SWITCHER_CLOSE_GROUP)
            target:self
          selector:@selector(closeButtonTapped:)] ];
}

#pragma mark - Public

- (void)configureTabSnapshotAndFavicon:
            (TabSnapshotAndFavicon*)tabSnapshotAndFavicon
                              tabIndex:(NSInteger)tabIndex {
  CHECK_LE(tabIndex, _tabsCount);
  [_groupSnapshotsView configureTabSnapshotAndFavicon:tabSnapshotAndFavicon
                                             tabIndex:tabIndex];
}

- (NSArray<UIView*>*)allGroupTabViews {
  return [_groupSnapshotsView allGroupTabViews];
}

- (void)setHighlightForGrouping:(BOOL)highlight {
  CHECK(IsTabGridDragAndDropEnabled());
  if (_highlighted == highlight) {
    return;
  }
  _highlighted = highlight;

  __weak __typeof(self) weakSelf = self;
  if (highlight) {
    // Shrink and dim contents of cell while revealing blue background covering
    // rest of the cell.
    [UIView animateWithDuration:kGridCellHighlightDuration
                     animations:^{
                       [weakSelf highlightCell];
                     }];

  } else {
    [UIView animateWithDuration:kGridCellHighlightDuration
        animations:^{
          [weakSelf resetHighlight];
        }
        completion:^(BOOL finished) {
          GroupGridCell* strongSelf = weakSelf;
          strongSelf.dimmingView.hidden = YES;
          strongSelf.groupingBackgroundView.hidden = YES;
        }];
  }
}

#pragma mark - Setters

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
      _border.layer.borderColor =
          [UIColor colorNamed:kStaticBlue400Color].CGColor;
      break;
    case GridThemeDark:
      self.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
      _border.layer.borderColor = UIColor.whiteColor.CGColor;
      break;
  }

  _theme = theme;
}

- (void)setGroupColor:(UIColor*)groupColor {
  _dotContainer.color = groupColor;
  _groupColor = groupColor;
}

- (void)setTitle:(NSString*)title {
  _titleLabel.text = title;
  _title = [title copy];

  [self updateAccessibilityLabel];
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

- (void)setFacePileProvider:(id<FacePileProviding>)facePileProvider {
  if ([_facePileProvider isEqualFacePileProviding:facePileProvider]) {
    return;
  }
  _facePileProvider = facePileProvider;

  self.facePile = [_facePileProvider facePileView];
}

- (void)setFacePile:(UIView*)facePile {
  _dotContainer.facePile = facePile;
  _facePile = facePile;
  [self updateTopBarConstraints];
}

- (void)setTabsCount:(NSInteger)tabsCount {
  _tabsCount = tabsCount;
  _groupSnapshotsView.tabsCount = tabsCount;
}

- (void)setActivityLabelData:(ActivityLabelData*)activityLabelData {
  [super setActivityLabelData:activityLabelData];
  [self updateAccessibilityLabel];
}

- (void)setLayoutType:(EmptyThumbnailLayoutType)layoutType {
  _layoutType = layoutType;
  for (GroupTabView* view in [self allGroupTabViews]) {
    view.layoutType = layoutType;
  }
}

#pragma mark - Private

// Sets up the top bar with icon, title, and close button.
- (void)setupTopBar {
  _topBar = [[UIView alloc] init];
  _topBar.translatesAutoresizingMaskIntoConstraints = NO;

  _dotContainer = [[GroupGridCellDotView alloc] init];
  _dotContainer.translatesAutoresizingMaskIntoConstraints = NO;

  NSLayoutConstraint* facePileSmallWidth =
      [_dotContainer.widthAnchor constraintEqualToConstant:0];
  facePileSmallWidth.priority = UILayoutPriorityDefaultLow;
  facePileSmallWidth.active = YES;

  _titleLabel = [[UILabel alloc] init];
  _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  _titleLabel.adjustsFontForContentSizeCategory = YES;

  _closeIconView = [[UIImageView alloc] init];
  _closeIconView.translatesAutoresizingMaskIntoConstraints = NO;
  _closeIconView.contentMode = UIViewContentModeCenter;
  _closeIconView.hidden = [self isInSelectionMode];
  _closeIconView.image =
      DefaultSymbolTemplateWithPointSize(kXMarkSymbol, kIconSymbolPointSize);

  _selectIconView = [[UIImageView alloc] init];
  _selectIconView.translatesAutoresizingMaskIntoConstraints = NO;
  _selectIconView.contentMode = UIViewContentModeScaleAspectFit;
  _selectIconView.hidden = ![self isInSelectionMode];

  _selectIconView.image = [self selectIconImageForCurrentState];

  [_topBar addSubview:_selectIconView];

  [_topBar addSubview:_dotContainer];
  [_topBar addSubview:_titleLabel];
  [_topBar addSubview:_closeIconView];

  _dotContainerAccessibilityConstraints = @[
    [_dotContainer.leadingAnchor constraintEqualToAnchor:_topBar.leadingAnchor
                                                constant:kTopBarLargeInset],
    [_titleLabel.leadingAnchor
        constraintEqualToAnchor:_dotContainer.trailingAnchor
                       constant:kGridCellHeaderLeadingInset],
  ];

  _dotContainerNormalConstraints = @[
    [_dotContainer.leadingAnchor constraintEqualToAnchor:_topBar.leadingAnchor
                                                constant:kTopBarInset],
    [_titleLabel.leadingAnchor
        constraintEqualToAnchor:_dotContainer.trailingAnchor
                       constant:kGridCellHeaderLeadingInset],
  ];

  _topBarHeightConstraint =
      [_topBar.heightAnchor constraintEqualToConstant:kGridCellHeaderHeight];

  _closeIconConstraints = @[
    [_titleLabel.trailingAnchor
        constraintEqualToAnchor:_closeIconView.leadingAnchor
                       constant:-kGridCellTitleLabelContentInset],
    [_topBar.topAnchor constraintEqualToAnchor:_closeIconView.centerYAnchor
                                      constant:-kGridCellCloseButtonTopSpacing],
    [_closeIconView.trailingAnchor
        constraintEqualToAnchor:_topBar.trailingAnchor
                       constant:-kGridCellCloseButtonContentInset],
  ];

  _selectIconConstraints = @[
    [_selectIconView.heightAnchor
        constraintEqualToConstant:kGridCellSelectIconSize],
    [_selectIconView.widthAnchor
        constraintEqualToConstant:kGridCellSelectIconSize],
    [_titleLabel.trailingAnchor
        constraintEqualToAnchor:_selectIconView.leadingAnchor
                       constant:-kGridCellTitleLabelContentInset],
    [_topBar.topAnchor constraintEqualToAnchor:_selectIconView.topAnchor
                                      constant:-kGridCellSelectIconTopSpacing],
    [_selectIconView.trailingAnchor
        constraintEqualToAnchor:_topBar.trailingAnchor
                       constant:-kGridCellSelectIconContentInset],
  ];

  [self updateTopBarConstraints];
  [self configureCloseOrSelectIconConstraints];

  NSArray* constraints = @[
    _topBarHeightConstraint,
    [_titleLabel.centerYAnchor constraintEqualToAnchor:_topBar.centerYAnchor],
    [_dotContainer.centerYAnchor constraintEqualToAnchor:_topBar.centerYAnchor],
  ];

  [NSLayoutConstraint activateConstraints:constraints];
  [_titleLabel
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [_closeIconView
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [_closeIconView setContentHuggingPriority:UILayoutPriorityRequired
                                    forAxis:UILayoutConstraintAxisHorizontal];
  [_selectIconView
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [_selectIconView setContentHuggingPriority:UILayoutPriorityRequired
                                     forAxis:UILayoutConstraintAxisHorizontal];
}

- (UIImage*)selectIconImageForCurrentState {
  if (_state == GridCellStateEditingUnselected) {
    return DefaultSymbolTemplateWithPointSize(kCircleSymbol,
                                              kIconSymbolPointSize);
  }
  return DefaultSymbolTemplateWithPointSize(kCheckmarkCircleFillSymbol,
                                            kIconSymbolPointSize);
}

- (void)configureCloseOrSelectIconConstraints {
  BOOL showSelectionMode = [self isInSelectionMode] && _selectIconView;

  _closeIconView.hidden = showSelectionMode;
  _selectIconView.hidden = !showSelectionMode;

  if (showSelectionMode) {
    [NSLayoutConstraint deactivateConstraints:_closeIconConstraints];
    [NSLayoutConstraint activateConstraints:_selectIconConstraints];
  } else {
    [NSLayoutConstraint deactivateConstraints:_selectIconConstraints];
    [NSLayoutConstraint activateConstraints:_closeIconConstraints];
  }
}

// Informs whether or not the cell is currently displaying an editing state.
- (BOOL)isInSelectionMode {
  return self.state != GridCellStateNotEditing;
}

- (void)setState:(GridCellState)state {
  if (state == _state) {
    return;
  }

  _state = state;
  // TODO(crbug.com/40942154): Add the accessibility value for selected and
  // unselected states.
  self.accessibilityValue = nil;
  _closeTapTargetButton.enabled = ![self isInSelectionMode];
  _selectIconView.image = [self selectIconImageForCurrentState];

  [self configureCloseOrSelectIconConstraints];
  _border.hidden = [self isInSelectionMode];
}

// Sets up the selection border. The tint color is set when the theme is
// selected.
- (void)setupSelectedBackgroundView {
  self.selectedBackgroundView = [[UIView alloc] init];
  self.selectedBackgroundView.backgroundColor =
      [UIColor colorNamed:kGridBackgroundColor];
  _border = [[UIView alloc] init];
  _border.hidden = [self isInSelectionMode];
  _border.translatesAutoresizingMaskIntoConstraints = NO;
  _border.backgroundColor = [UIColor colorNamed:kGridBackgroundColor];
  _border.layer.cornerRadius = kGridCellCornerRadius +
                               kGridCellSelectionRingGapWidth +
                               kGridCellSelectionRingTintWidth;
  _border.layer.borderWidth = kGridCellSelectionRingTintWidth;
  [self.selectedBackgroundView addSubview:_border];
  [NSLayoutConstraint activateConstraints:@[
    [_border.topAnchor
        constraintEqualToAnchor:self.selectedBackgroundView.topAnchor
                       constant:-kGridCellSelectionRingTintWidth -
                                kGridCellSelectionRingGapWidth],
    [_border.leadingAnchor
        constraintEqualToAnchor:self.selectedBackgroundView.leadingAnchor
                       constant:-kGridCellSelectionRingTintWidth -
                                kGridCellSelectionRingGapWidth],
    [_border.trailingAnchor
        constraintEqualToAnchor:self.selectedBackgroundView.trailingAnchor
                       constant:kGridCellSelectionRingTintWidth +
                                kGridCellSelectionRingGapWidth],
    [_border.bottomAnchor
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
             ? kGroupGridCellHeaderAccessibilityHeight
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

// Updates the top bar constraints accoring to the availability of
// `facePileViewController` and the accessibility font size.
- (void)updateTopBarConstraints {
  _topBarHeightConstraint.constant = [self topBarHeight];
  if (UIContentSizeCategoryIsAccessibilityCategory(
          self.traitCollection.preferredContentSizeCategory)) {
    [NSLayoutConstraint deactivateConstraints:_dotContainerNormalConstraints];
    [NSLayoutConstraint
        activateConstraints:_dotContainerAccessibilityConstraints];
  } else {
    [NSLayoutConstraint
        deactivateConstraints:_dotContainerAccessibilityConstraints];
    [NSLayoutConstraint activateConstraints:_dotContainerNormalConstraints];
  }
}

// Updates the accessibility label.
- (void)updateAccessibilityLabel {
  if (self.activityLabelData) {
    self.accessibilityLabel = l10n_util::GetNSStringF(
        IDS_IOS_TAB_GROUP_CELL_UPDATED_ACCESSIBILITY_TITLE,
        base::SysNSStringToUTF16(self.title),
        base::NumberToString16(_tabsCount));
  } else {
    self.accessibilityLabel =
        l10n_util::GetNSStringF(IDS_IOS_TAB_GROUP_CELL_ACCESSIBILITY_TITLE,
                                base::SysNSStringToUTF16(self.title),
                                base::NumberToString16(_tabsCount));
  }
}

// Animations to highlight this cell.
- (void)highlightCell {
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
