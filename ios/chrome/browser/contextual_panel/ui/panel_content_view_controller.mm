// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/ui/panel_content_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/contextual_panel/ui/contextual_sheet_display_controller.h"
#import "ios/chrome/browser/contextual_panel/ui/panel_block_data.h"
#import "ios/chrome/browser/contextual_panel/ui/panel_block_metrics_data.h"
#import "ios/chrome/browser/contextual_panel/ui/panel_item_collection_view_cell.h"
#import "ios/chrome/browser/contextual_panel/ui/trait_collection_change_delegate.h"
#import "ios/chrome/browser/contextual_panel/utils/contextual_panel_metrics.h"
#import "ios/chrome/browser/shared/public/commands/contextual_sheet_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/public/provider/chrome/browser/font/font_api.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
// Top margin between the header logo and the top of the panel.
const CGFloat kLogoTopMargin = 24;

// Bottom margin between the header logo and the top of the collection view
const CGFloat kLogoBottomMargin = 18;

// Size of the close button.
const CGFloat kCloseButtonIconSize = 30;

// Margin between the close button and the trailing edge of the screen.
const CGFloat kCloseButtonTrailingMargin = 16;

// Height of the drag handle view.
const CGFloat kDragHandleHeight = 5;

// Width of the drag handle view.
const CGFloat kDragHandleWidth = 36;

// Top margin between the drag handle view and the panel.
const CGFloat kDragHandleTopMargin = 5;

// The size of the logo image.
constexpr CGFloat kLogoSize = 22;

// The top logo has a specific font size for branding reasons.
const CGFloat kLogoLabelFontSize = 18;

// The margin between the bottom of the content and the collection view.
const CGFloat kContentBottomMargin = 16;

// Threshold for how long a view is onscreen to count as visible.
const base::TimeDelta kVisibleTimeThreshold = base::Milliseconds(10);

// The time range's expected min, max values and bucket count for custom
// histograms.
constexpr base::TimeDelta kVisibleTimeHistogramMin = base::Milliseconds(1);
constexpr base::TimeDelta kVisibleTimeHistogramMax = base::Minutes(10);
constexpr int kVisibleTimeHistogramBucketCount = 100;

// Identifier for the one section in this collection view.
NSString* const kSectionIdentifier = @"section1";

NSString* const kViewAccessibilityIdentifier = @"PanelContentViewAXID";

NSString* const kCloseButtonAccessibilityIdentifier = @"PanelCloseButtonAXID";

UIImage* CloseButtonImage(BOOL highlighted) {
  NSArray<UIColor*>* palette = @[
    [UIColor colorNamed:kGrey600Color],
    [UIColor colorNamed:kBackgroundColor],
  ];

  if (highlighted) {
    NSMutableArray<UIColor*>* transparentPalette =
        [[NSMutableArray alloc] init];
    [palette enumerateObjectsUsingBlock:^(UIColor* color, NSUInteger idx,
                                          BOOL* stop) {
      [transparentPalette addObject:[color colorWithAlphaComponent:0.6]];
    }];
    palette = [transparentPalette copy];
  }

  return SymbolWithPalette(
      DefaultSymbolWithPointSize(kXMarkCircleFillSymbol, kCloseButtonIconSize),
      palette);
}

}  // namespace

@interface PanelContentViewController () <UICollectionViewDelegate,
                                          UIPointerInteractionDelegate>

@end

@implementation PanelContentViewController {
  // The background visual effect view behind all the content.
  UIVisualEffectView* _backgroundVisualEffectView;

  // The header view at the top of the panel.
  UIVisualEffectView* _headerView;

  // Background for the header when the Reduce Transparency accessibility
  // setting is on.
  UIView* _headerViewAccessibilityBackground;

  // The button to close the view.
  UIButton* _closeButton;

  // The view for the small drag handle at the top of the panel.
  UIView* _dragHandleView;

  // The collection view managed by this view controller
  UICollectionView* _collectionView;

  // The data source for this collection view.
  UICollectionViewDiffableDataSource<NSString*, NSString*>* _diffableDataSource;

  // The blocks currently being displayed.
  NSArray<PanelBlockData*>* _panelBlocks;

  NSMutableDictionary<NSString*, PanelBlockMetricsData*>*
      _panelBlocksMetricsDataDict;

  // The stored height of the expanded bottom toolbar.
  CGFloat _bottomToolbarHeight;

  // Stored time the panel appeared.
  base::Time _appearanceTime;
}

#pragma mark - UIViewController

- (instancetype)init {
  self = [super init];
  if (self) {
    _panelBlocksMetricsDataDict = [[NSMutableDictionary alloc] init];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.accessibilityIdentifier = kViewAccessibilityIdentifier;

  [self createBackground];
  [self.view addSubview:_backgroundVisualEffectView];
  AddSameConstraints(self.view, _backgroundVisualEffectView);

  [self createCollectionView];
  [self.view addSubview:_collectionView];
  AddSameConstraints(self.view, _collectionView);

  // Create and set up the header view. This should be added after the
  // collection view because the header should go above the collection view.
  UIBlurEffect* headerBlurEffect =
      [UIBlurEffect effectWithStyle:UIBlurEffectStyleRegular];
  _headerView = [[UIVisualEffectView alloc] initWithEffect:headerBlurEffect];
  _headerView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_headerView];

  [NSLayoutConstraint activateConstraints:@[
    [self.view.leadingAnchor constraintEqualToAnchor:_headerView.leadingAnchor],
    [self.view.trailingAnchor
        constraintEqualToAnchor:_headerView.trailingAnchor],
    [self.view.topAnchor constraintEqualToAnchor:_headerView.topAnchor],
  ]];

  _headerViewAccessibilityBackground = [[UIView alloc] init];
  _headerViewAccessibilityBackground.translatesAutoresizingMaskIntoConstraints =
      NO;
  _headerViewAccessibilityBackground.backgroundColor =
      [UIColor colorNamed:kGrey100Color];
  [_headerView.contentView addSubview:_headerViewAccessibilityBackground];
  AddSameConstraints(_headerView, _headerViewAccessibilityBackground);
  _headerViewAccessibilityBackground.hidden =
      !UIAccessibilityIsReduceTransparencyEnabled();

  [self createDragHandleView];
  [_headerView.contentView addSubview:_dragHandleView];
  [NSLayoutConstraint activateConstraints:@[
    [_headerView.centerXAnchor
        constraintEqualToAnchor:_dragHandleView.centerXAnchor],
    [_dragHandleView.topAnchor
        constraintEqualToAnchor:_headerView.contentView.topAnchor
                       constant:kDragHandleTopMargin],
  ]];

#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  UIImage* logoImage = MakeSymbolMulticolor(
      CustomSymbolWithPointSize(kMulticolorChromeballSymbol, kLogoSize));
#else
  UIImage* logoImage =
      CustomSymbolWithPointSize(kChromeProductSymbol, kLogoSize);
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)

  UIImageView* logoImageView = [[UIImageView alloc] initWithImage:logoImage];
  logoImageView.translatesAutoresizingMaskIntoConstraints = NO;

  UILabel* logoLabel = [[UILabel alloc] init];
  logoLabel.translatesAutoresizingMaskIntoConstraints = NO;
  logoLabel.text =
      l10n_util::GetNSString(IDS_IOS_CONTEXTUAL_PANEL_BRANDING_TITLE);
  UIFont* productFont =
      ios::provider::GetBrandedProductRegularFont(kLogoLabelFontSize);
  logoLabel.font = [[[UIFontMetrics alloc]
      initForTextStyle:UIFontTextStyleCaption1] scaledFontForFont:productFont];
  logoLabel.adjustsFontForContentSizeCategory = YES;
  logoLabel.textColor = [UIColor colorNamed:kGrey700Color];

  UIStackView* logo = [[UIStackView alloc]
      initWithArrangedSubviews:@[ logoImageView, logoLabel ]];
  logo.translatesAutoresizingMaskIntoConstraints = NO;
  logo.spacing = 5;
  logo.alignment = UIStackViewAlignmentCenter;

  logo.isAccessibilityElement = true;
  logo.accessibilityLabel = l10n_util::GetNSString(
      IDS_IOS_CONTEXTUAL_PANEL_BRANDING_ACCESSIBILITY_LABEL);

  [_headerView.contentView addSubview:logo];
  [NSLayoutConstraint activateConstraints:@[
    [logo.centerXAnchor
        constraintEqualToAnchor:_headerView.contentView.centerXAnchor],
    [logo.topAnchor constraintEqualToAnchor:_headerView.contentView.topAnchor
                                   constant:kLogoTopMargin],
    [_headerView.bottomAnchor constraintEqualToAnchor:logo.bottomAnchor
                                             constant:kLogoBottomMargin],
  ]];

  [self createCloseButton];
  [_headerView.contentView addSubview:_closeButton];
  [NSLayoutConstraint activateConstraints:@[
    [_headerView.contentView.trailingAnchor
        constraintEqualToAnchor:_closeButton.trailingAnchor
                       constant:kCloseButtonTrailingMargin],
    [_closeButton.centerYAnchor constraintEqualToAnchor:logo.centerYAnchor],
  ]];

  [self.view layoutIfNeeded];
  [self.sheetDisplayController
      setContentHeight:[self preferredHeightForContent]];

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(accessibilityReduceTransparencySettingDidChange)
             name:UIAccessibilityReduceTransparencyStatusDidChangeNotification
           object:nil];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  _appearanceTime = base::Time::Now();
  UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                  _headerView);
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];

  base::UmaHistogramCustomTimes(
      "IOS.ContextualPanel.VisibleTime", base::Time::Now() - _appearanceTime,
      kVisibleTimeHistogramMin, kVisibleTimeHistogramMax,
      kVisibleTimeHistogramBucketCount);

  // First alert all visible cells that they will disappear.
  for (NSIndexPath* indexPath in _collectionView.indexPathsForVisibleItems) {
    UICollectionViewCell* cell =
        [_collectionView cellForItemAtIndexPath:indexPath];
    PanelItemCollectionViewCell* panelCell =
        base::apple::ObjCCast<PanelItemCollectionViewCell>(cell);
    [self updateTimeDisplayedForCell:panelCell atIndexPath:indexPath];

    [panelCell cellDidDisappear];
  }

  std::string entrypointBlockName =
      base::SysNSStringToUTF8([self.metricsDelegate entrypointInfoBlockName]);
  BOOL wasLoudEntrypoint = [self.metricsDelegate wasLoudEntrypoint];

  for (NSString* key in _panelBlocksMetricsDataDict) {
    PanelBlockMetricsData* data = _panelBlocksMetricsDataDict[key];
    BOOL wasVisible = data.timeVisible >= kVisibleTimeThreshold;
    std::string blockName = base::SysNSStringToUTF8(key);

    std::string uptimeHistogramName =
        std::string("IOS.ContextualPanel.InfoBlockUptime.").append(blockName);
    base::UmaHistogramTimes(uptimeHistogramName, data.timeVisible);

    std::string impressionTypeHistogramName =
        std::string("IOS.ContextualPanel.InfoBlockImpression.")
            .append(blockName);
    PanelBlockImpressionType blockImpressionType;
    if (!wasVisible) {
      blockImpressionType = PanelBlockImpressionType::NeverVisible;
    } else {
      if (blockName == entrypointBlockName) {
        if (wasLoudEntrypoint) {
          blockImpressionType =
              PanelBlockImpressionType::VisibleAndLoudEntrypoint;
        } else {
          blockImpressionType =
              PanelBlockImpressionType::VisibleAndSmallEntrypoint;
        }
      } else {
        if (wasLoudEntrypoint) {
          blockImpressionType =
              PanelBlockImpressionType::VisibleAndOtherWasLoudEntrypoint;
        } else {
          blockImpressionType =
              PanelBlockImpressionType::VisibleAndOtherWasSmallEntrypoint;
        }
      }
    }
    base::UmaHistogramEnumeration(impressionTypeHistogramName,
                                  blockImpressionType);
  }
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];

  [self addAccessibilityTransparencyWorkaround];

  [self setCollectionViewContentInset];
  [self setCollectionViewScrollIndicatorInsets];
}

- (void)accessibilityReduceTransparencySettingDidChange {
  [self addAccessibilityTransparencyWorkaround];

  _headerViewAccessibilityBackground.hidden =
      !UIAccessibilityIsReduceTransparencyEnabled();
}

- (void)viewSafeAreaInsetsDidChange {
  [super viewSafeAreaInsetsDidChange];

  [self setCollectionViewScrollIndicatorInsets];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];

  [self.traitCollectionDelegate traitCollectionDidChangeForViewController:self];
}

// Removes the white-ish background color of one of UIVisualEffectView's
// subviews that is not desired for this feature.
- (void)addAccessibilityTransparencyWorkaround {
  for (UIView* subview in _headerView.subviews) {
    // Replace any non-nil backgrounds with clear.
    if (subview.backgroundColor) {
      subview.backgroundColor = UIColor.clearColor;
    }
  }
}

#pragma mark - Public methods

- (void)setPanelBlocks:(NSArray<PanelBlockData*>*)panelBlocks {
  _panelBlocks = [panelBlocks copy];

  if (_diffableDataSource) {
    [_diffableDataSource applySnapshot:[self dataSnapshot]
                  animatingDifferences:NO];
  }
}

#pragma mark - Private

// Generates and returns a data source snapshot for the current data.
- (NSDiffableDataSourceSnapshot<NSString*, NSString*>*)dataSnapshot {
  NSDiffableDataSourceSnapshot<NSString*, NSString*>* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot appendSectionsWithIdentifiers:@[ kSectionIdentifier ]];
  NSMutableArray<NSString*>* itemIdentifiers = [[NSMutableArray alloc] init];
  for (PanelBlockData* data in _panelBlocks) {
    [itemIdentifiers addObject:data.blockType];
  }
  [snapshot appendItemsWithIdentifiers:itemIdentifiers];
  return snapshot;
}

// Target for the close button.
- (void)closeButtonTapped {
  base::UmaHistogramEnumeration("IOS.ContextualPanel.DismissedReason",
                                ContextualPanelDismissedReason::UserDismissed);
  [self.contextualSheetCommandHandler closeContextualSheet];
}

// Looks up the correct registration for the provided item. Wrapper for
// UICollectionViewDiffableDataSource's cellProvider parameter.
- (UICollectionViewCell*)
    diffableDataSourceCellProviderForCollectionView:
        (UICollectionView*)collectionView
                                          indexPath:(NSIndexPath*)indexPath
                                     itemIdentifier:(id)itemIdentifier {
  UICollectionViewCellRegistration* registration =
      [_panelBlocks[indexPath.row] cellRegistration];
  UICollectionViewCell* cell = [collectionView
      dequeueConfiguredReusableCellWithRegistration:registration
                                       forIndexPath:indexPath
                                               item:itemIdentifier];
  return cell;
}

- (CGFloat)preferredHeightForContent {
  // The collection view takes up the entire view, so find the preferred size
  // of the collection view.
  CGFloat height = _collectionView.contentSize.height +
                   _collectionView.contentInset.top +
                   _collectionView.contentInset.bottom;

  return height;
}

- (void)updateTimeDisplayedForCell:(PanelItemCollectionViewCell*)cell
                       atIndexPath:(NSIndexPath*)indexPath {
  PanelBlockData* panelData = _panelBlocks[indexPath.row];

  if (!_panelBlocksMetricsDataDict[panelData.blockType]) {
    _panelBlocksMetricsDataDict[panelData.blockType] =
        [[PanelBlockMetricsData alloc] init];
  }
  PanelBlockMetricsData* metricsData =
      _panelBlocksMetricsDataDict[panelData.blockType];
  metricsData.timeVisible += cell.timeSinceAppearance;
}

- (void)setCollectionViewScrollIndicatorInsets {
  // The bottom inset should not include the safe area height.
  _collectionView.verticalScrollIndicatorInsets = UIEdgeInsetsMake(
      _headerView.bounds.size.height, 0,
      _bottomToolbarHeight - self.view.safeAreaInsets.bottom, 0);
}

- (void)setCollectionViewContentInset {
  _collectionView.contentInset =
      UIEdgeInsetsMake(_headerView.bounds.size.height, 0,
                       _bottomToolbarHeight + kContentBottomMargin, 0);
}

#pragma mark - View Initialization

// Creates the layout for the collection view.
- (UICollectionViewLayout*)createLayout {
  NSCollectionLayoutSize* itemSize = [NSCollectionLayoutSize
      sizeWithWidthDimension:[NSCollectionLayoutDimension
                                 fractionalWidthDimension:1.]
             heightDimension:[NSCollectionLayoutDimension
                                 estimatedDimension:200]];
  NSCollectionLayoutItem* item =
      [NSCollectionLayoutItem itemWithLayoutSize:itemSize];

  NSCollectionLayoutSize* groupSize = [NSCollectionLayoutSize
      sizeWithWidthDimension:[NSCollectionLayoutDimension
                                 fractionalWidthDimension:1.]
             heightDimension:[NSCollectionLayoutDimension
                                 estimatedDimension:200]];
  NSCollectionLayoutGroup* group =
      [NSCollectionLayoutGroup verticalGroupWithLayoutSize:groupSize
                                                  subitems:@[ item ]];

  NSCollectionLayoutSection* section =
      [NSCollectionLayoutSection sectionWithGroup:group];
  return [[UICollectionViewCompositionalLayout alloc] initWithSection:section];
}

// Creates and initializes `_collectionView`.
- (void)createCollectionView {
  _collectionView =
      [[UICollectionView alloc] initWithFrame:CGRectZero
                         collectionViewLayout:[self createLayout]];
  _collectionView.translatesAutoresizingMaskIntoConstraints = NO;
  _collectionView.backgroundColor = UIColor.clearColor;
  [self setCollectionViewContentInset];
  [self setCollectionViewScrollIndicatorInsets];
  _collectionView.contentInsetAdjustmentBehavior =
      UIScrollViewContentInsetAdjustmentNever;
  _collectionView.delegate = self;

  __weak __typeof(self) weakSelf = self;
  auto cellProvider =
      ^UICollectionViewCell*(UICollectionView* collectionView,
                             NSIndexPath* indexPath, id itemIdentifier) {
        return [weakSelf
            diffableDataSourceCellProviderForCollectionView:collectionView
                                                  indexPath:indexPath
                                             itemIdentifier:itemIdentifier];
      };

  _diffableDataSource = [[UICollectionViewDiffableDataSource alloc]
      initWithCollectionView:_collectionView
                cellProvider:cellProvider];

  _collectionView.dataSource = _diffableDataSource;

  [_diffableDataSource applySnapshot:[self dataSnapshot]
                animatingDifferences:NO];
}

// Creates and initializes `_closeButton`.
- (void)createCloseButton {
  UIButtonConfiguration* closeButtonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  // The image itself is set below in the configurationUpdateHandler, which
  // is called before the button appears for the first time as well.
  closeButtonConfiguration.contentInsets = NSDirectionalEdgeInsetsZero;
  closeButtonConfiguration.buttonSize = UIButtonConfigurationSizeSmall;
  closeButtonConfiguration.accessibilityLabel =
      l10n_util::GetNSString(IDS_CLOSE);
  __weak __typeof(self) weakSelf = self;
  _closeButton = [UIButton
      buttonWithConfiguration:closeButtonConfiguration
                primaryAction:[UIAction actionWithHandler:^(UIAction* action) {
                  [weakSelf closeButtonTapped];
                }]];
  _closeButton.translatesAutoresizingMaskIntoConstraints = NO;
  _closeButton.accessibilityIdentifier = kCloseButtonAccessibilityIdentifier;
  _closeButton.pointerInteractionEnabled = YES;
  _closeButton.configurationUpdateHandler = ^(UIButton* button) {
    UIButtonConfiguration* updatedConfig = button.configuration;
    switch (button.state) {
      case UIControlStateHighlighted:
        updatedConfig.image = CloseButtonImage(YES);
        break;
      case UIControlStateNormal:
        updatedConfig.image = CloseButtonImage(NO);
        break;
    }
    button.configuration = updatedConfig;
  };
}

- (void)createDragHandleView {
  _dragHandleView = [[UIView alloc] init];
  _dragHandleView.translatesAutoresizingMaskIntoConstraints = NO;
  _dragHandleView.backgroundColor = UIColor.systemGray2Color;
  [_dragHandleView
      addInteraction:[[UIPointerInteraction alloc] initWithDelegate:self]];
  _dragHandleView.layer.cornerRadius = kDragHandleHeight / 2;

  [NSLayoutConstraint activateConstraints:@[
    [_dragHandleView.heightAnchor constraintEqualToConstant:kDragHandleHeight],
    [_dragHandleView.widthAnchor constraintEqualToConstant:kDragHandleWidth],
  ]];
}

- (void)createBackground {
  UIBlurEffect* backgroundBlurEffect =
      [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemThickMaterial];
  _backgroundVisualEffectView =
      [[UIVisualEffectView alloc] initWithEffect:backgroundBlurEffect];
  _backgroundVisualEffectView.translatesAutoresizingMaskIntoConstraints = NO;

  UIView* scrim = [[UIView alloc] init];
  scrim.translatesAutoresizingMaskIntoConstraints = NO;
  // The scrim should be black 3% opacity in both light and dark mode.
  scrim.backgroundColor = [UIColor.blackColor colorWithAlphaComponent:0.03];
  [_backgroundVisualEffectView.contentView addSubview:scrim];
  AddSameConstraints(_backgroundVisualEffectView.contentView, scrim);
}

#pragma mark - PanelContentConsumer

- (void)updateBottomToolbarHeight:(CGFloat)height {
  _bottomToolbarHeight = height;
  if (_collectionView) {
    UIEdgeInsets insets = _collectionView.contentInset;
    insets.bottom = height + kContentBottomMargin;
    _collectionView.contentInset = insets;
    [self setCollectionViewScrollIndicatorInsets];
    [self.sheetDisplayController
        setContentHeight:[self preferredHeightForContent]];
  }
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
       willDisplayCell:(UICollectionViewCell*)cell
    forItemAtIndexPath:(NSIndexPath*)indexPath {
  PanelItemCollectionViewCell* panelCell =
      base::apple::ObjCCast<PanelItemCollectionViewCell>(cell);
  [panelCell cellWillAppear];
}

- (void)collectionView:(UICollectionView*)collectionView
    didEndDisplayingCell:(UICollectionViewCell*)cell
      forItemAtIndexPath:(NSIndexPath*)indexPath {
  PanelItemCollectionViewCell* panelCell =
      base::apple::ObjCCast<PanelItemCollectionViewCell>(cell);
  [self updateTimeDisplayedForCell:panelCell atIndexPath:indexPath];

  [panelCell cellDidDisappear];
}

#pragma mark - UIPointerInteractionDelegate

- (UIPointerStyle*)pointerInteraction:(UIPointerInteraction*)interaction
                       styleForRegion:(UIPointerRegion*)region {
  // If the view is no longer in a window due to a race condition, no
  // pointer style is needed.
  if (!interaction.view.window) {
    return nil;
  }

  DCHECK_EQ(_dragHandleView, interaction.view);

  UITargetedPreview* preview =
      [[UITargetedPreview alloc] initWithView:_dragHandleView];
  UIPointerEffect* effect =
      [UIPointerHighlightEffect effectWithPreview:preview];

  // Make the pointer frame slightly larger than the view, like the system drag
  // handle.
  CGRect pointerFrame = CGRectInset(_dragHandleView.frame, -5, -5);
  UIPointerShape* shape = [UIPointerShape shapeWithRoundedRect:pointerFrame];

  UIPointerStyle* style = [UIPointerStyle styleWithEffect:effect shape:shape];
  return style;
}

@end
