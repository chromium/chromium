// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/group_grid_cell.h"

#import <ostream>

#import "base/check.h"
#import "base/check_op.h"
#import "base/debug/dump_without_crashing.h"
#import "base/notreached.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/group_tab_view.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_group_snapshots_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/gfx/ios/uikit_util.h"

namespace {

// The size of symbol icons.
NSInteger kIconSymbolPointSize = 13;

// Offsets the top and bottom snapshot views.
const CGFloat kSnapshotViewLeadingOffset = 4;
const CGFloat kSnapshotViewTrailingOffset = 4;
const CGFloat kSnapShotViewBottomOffset = 4;
const CGFloat kGroupColorViewSize = 18;

}  // namespace

@implementation GroupGridCell {
  // The constraints enabled under accessibility font size.
  NSArray<NSLayoutConstraint*>* _accessibilityConstraints;
  // The constraints enabled under normal font size.
  NSArray<NSLayoutConstraint*>* _nonAccessibilityConstraints;
  // The constraints enabled while showing the close icon.
  NSArray<NSLayoutConstraint*>* _closeIconConstraints;
  // The constraints enabled while showing the selection icon.
  NSArray<NSLayoutConstraint*>* _selectIconConstraints;
  // Header height of the cell.
  NSLayoutConstraint* _topBarHeightConstraint;
  // Visual components of the cell.
  UIView* _topBar;
  UIView* _groupColorView;
  UILabel* _titleLabel;
  UIImageView* _closeIconView;
  UIImageView* _selectIconView;
  // Since the close icon dimensions are smaller than the recommended tap target
  // size, use an overlaid tap target button.
  UIButton* _closeTapTargetButton;
  UIView* _border;

  TabGroupSnapshotsView* _groupSnapshotsView;
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
    [self setupTopBar];
    _groupSnapshotsView = [[TabGroupSnapshotsView alloc]
        initWithTabGroupInfos:nil
                         size:0
                        light:self.theme == GridThemeLight
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

    [contentView addSubview:_topBar];
    [contentView addSubview:_groupSnapshotsView];
    [contentView addSubview:_closeTapTargetButton];
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
      [_topBar.topAnchor constraintEqualToAnchor:contentView.topAnchor],
      [_topBar.leadingAnchor constraintEqualToAnchor:contentView.leadingAnchor],
      [_topBar.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor],
      [_groupSnapshotsView.topAnchor
          constraintEqualToAnchor:_topBar.bottomAnchor],
      [_groupSnapshotsView.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kSnapshotViewLeadingOffset],
      [_groupSnapshotsView.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor
                         constant:-kSnapshotViewTrailingOffset],
      [_groupSnapshotsView.bottomAnchor
          constraintEqualToAnchor:contentView.bottomAnchor
                         constant:-kSnapShotViewBottomOffset],
      [_closeTapTargetButton.topAnchor
          constraintEqualToAnchor:contentView.topAnchor],
      [_closeTapTargetButton.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor],
      [_closeTapTargetButton.widthAnchor
          constraintEqualToConstant:kGridCellCloseTapTargetWidthHeight],
      [_closeTapTargetButton.heightAnchor
          constraintEqualToConstant:kGridCellCloseTapTargetWidthHeight],
    ];
    [NSLayoutConstraint activateConstraints:constraints];
  }

  if (@available(iOS 17, *)) {
    [self registerForTraitChanges:@[ UITraitPreferredContentSizeCategory.self ]
                       withAction:@selector(updateTopBarSize)];
  }
  return self;
}

#pragma mark - UIView

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];

  if (@available(iOS 17, *)) {
    return;
  }

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
  self.groupColor = nil;
  self.selected = NO;
  self.opacity = 1.0;
  self.hidden = NO;
}

#pragma mark - UIAccessibility

- (BOOL)isAccessibilityElement {
  // This makes the whole cell tappable in VoiceOver rather than the individual
  // title and close button.
  return YES;
}
// TODO(crbug.com/41484563): Add the accessibility custom actions.

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
      if (@available(iOS 17, *)) {
        [self updateInterfaceStyleForWindow:self.window];
      } else {
        self.overrideUserInterfaceStyle = UIUserInterfaceStyleLight;
      }
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
  if (groupColor) {
    _groupColor = groupColor;
    _groupColorView.backgroundColor = groupColor;
  }
}

- (void)configureWithGroupTabInfos:(NSArray<GroupTabInfo*>*)groupTabInfos
                    totalTabsCount:(NSInteger)totalTabsCount {
  CHECK_LE((int)groupTabInfos.count, totalTabsCount);
  [_groupSnapshotsView
      configureTabGroupSnapshotsViewWithTabGroupInfos:groupTabInfos
                                                 size:totalTabsCount];
}

- (NSArray<UIView*>*)allGroupTabViews {
  return [_groupSnapshotsView allGroupTabViews];
}

- (void)setTabsCount:(NSInteger)tabsCount {
  _tabsCount = tabsCount;
}

- (void)setTitle:(NSString*)title {
  _titleLabel.text = title;
  self.accessibilityLabel = l10n_util::GetNSStringF(
      IDS_IOS_TAB_GROUP_CELL_ACCESSIBILITY_TITLE,
      base::SysNSStringToUTF16(title), base::NumberToString16(_tabsCount));
  _title = [title copy];
}

- (void)setTitleHidden:(BOOL)titleHidden {
  _titleLabel.hidden = titleHidden;
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
- (void)setupTopBar {
  _topBar = [[UIView alloc] init];
  _topBar.translatesAutoresizingMaskIntoConstraints = NO;

  _groupColorView = [[UIView alloc] init];
  _groupColorView.translatesAutoresizingMaskIntoConstraints = NO;
  _groupColorView.layer.cornerRadius = kGroupColorViewSize / 2;

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

  [_topBar addSubview:_groupColorView];
  [_topBar addSubview:_titleLabel];
  [_topBar addSubview:_closeIconView];

  _accessibilityConstraints = @[
    [_titleLabel.leadingAnchor
        constraintEqualToAnchor:_topBar.leadingAnchor
                       constant:kGridCellHeaderLeadingInset],
    [_groupColorView.widthAnchor constraintEqualToConstant:0],
    [_groupColorView.heightAnchor constraintEqualToConstant:0],
  ];

  _nonAccessibilityConstraints = @[
    [_groupColorView.heightAnchor
        constraintEqualToConstant:kGroupColorViewSize],
    [_groupColorView.widthAnchor constraintEqualToConstant:kGroupColorViewSize],
    [_groupColorView.leadingAnchor
        constraintEqualToAnchor:_topBar.leadingAnchor
                       constant:kGridCellHeaderLeadingInset],
    [_groupColorView.centerYAnchor
        constraintEqualToAnchor:_topBar.centerYAnchor],
    [_titleLabel.leadingAnchor
        constraintEqualToAnchor:_groupColorView.trailingAnchor
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

  [self updateTopBarSize];
  [self configureCloseOrSelectIconConstraints];

  NSArray* constraints = @[
    _topBarHeightConstraint,
    [_titleLabel.centerYAnchor constraintEqualToAnchor:_topBar.centerYAnchor],
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

// Update constraints of top bar when system font size changes. If accessibility
// font size is chosen, the favicon will be hidden, and the title text will be
// shown in two lines.
- (void)updateTopBarSize {
  _topBarHeightConstraint.constant = [self topBarHeight];

  if (UIContentSizeCategoryIsAccessibilityCategory(
          self.traitCollection.preferredContentSizeCategory)) {
    _titleLabel.numberOfLines = 2;
    [NSLayoutConstraint deactivateConstraints:_nonAccessibilityConstraints];
    [NSLayoutConstraint activateConstraints:_accessibilityConstraints];
  } else {
    _titleLabel.numberOfLines = 1;
    [NSLayoutConstraint deactivateConstraints:_accessibilityConstraints];
    [NSLayoutConstraint activateConstraints:_nonAccessibilityConstraints];
  }
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

@end
