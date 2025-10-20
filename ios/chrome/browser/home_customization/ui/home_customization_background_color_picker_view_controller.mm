// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_background_color_picker_view_controller.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "ios/chrome/browser/home_customization/ui/background_collection_configuration.h"
#import "ios/chrome/browser/home_customization/ui/background_customization_configuration.h"
#import "ios/chrome/browser/home_customization/ui/centered_flow_layout.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_configuration_mutator.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_picker_action_sheet_consumer.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_custom_color_cell.h"
#import "ios/chrome/browser/home_customization/ui/home_cutomization_color_palette_cell.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

// Define constants within the namespace
namespace {
// The width and height of each color palette cell in the collection view.
const CGFloat kColorCellSize = 60.0;

// The vertical spacing between rows of cells.
const CGFloat kLineSpacing = 23.0;

// The horizontal spacing between cells in the same row.
const CGFloat kItemSpacing = 2.0;

// The top padding for the section in the collection view.
const CGFloat kSectionInsetTop = 20.0;

// The left and right padding for the section in the collection view.
const CGFloat kSectionInsetSides = 25.0;

// The bottom padding for the section in the collection view.
const CGFloat kSectionInsetBottom = 27.0;

// The portion of the width taken by the color cells (from 0.0 to 1.0).
const CGFloat kRelativeRowWidth = 1.0;

// Returns a dynamic UIColor using two named color assets for light and dark
// mode.
UIColor* DynamicNamedColor(NSString* lightName, NSString* darkName) {
  return
      [UIColor colorWithDynamicProvider:^UIColor*(UITraitCollection* traits) {
        BOOL isDark = (traits.userInterfaceStyle == UIUserInterfaceStyleDark);
        return [UIColor colorNamed:isDark ? darkName : lightName];
      }];
}

}  // namespace

@interface HomeCustomizationBackgroundColorPickerViewController () {
  // This view controller's main collection view for displaying content.
  UICollectionView* _collectionView;

  // The background collection configuration to be displayed in this view
  // controller.
  BackgroundCollectionConfiguration* _backgroundCollectionConfiguration;

  // The `UICollectionViewCellRegistration` for registering  and configuring the
  // `HomeCustomizationColorPaletteCell` in the collection view.
  UICollectionViewCellRegistration* _colorCellRegistration;

  // The `UICollectionViewCellRegistration` for registering  and configuring the
  // `HomeCustomizationCustomColorCell` in the collection view.
  UICollectionViewCellRegistration* _customColorCellRegistration;

  // Currently selected color index in the palette.
  NSString* _selectedColorId;

  // The number of times a color option is selected.
  int _colorClickCount;
}
@end

@implementation HomeCustomizationBackgroundColorPickerViewController

@dynamic navigationItem;

- (void)viewDidLoad {
  [super viewDidLoad];

  __weak __typeof(self) weakSelf = self;
  self.title = l10n_util::GetNSStringWithFixup(
      IDS_IOS_HOME_CUSTOMIZATION_BACKGROUND_PICKER_COLOR_TITLE);

  self.view.backgroundColor = [UIColor systemBackgroundColor];

  UICollectionViewFlowLayout* layout = [[CenteredFlowLayout alloc] init];

  layout.itemSize = CGSizeMake(kColorCellSize, kColorCellSize);
  layout.minimumLineSpacing = kLineSpacing;
  layout.minimumInteritemSpacing = kItemSpacing;
  layout.sectionInset =
      UIEdgeInsetsMake(kSectionInsetTop, kSectionInsetSides,
                       kSectionInsetBottom, kSectionInsetSides);

  _colorCellRegistration = [UICollectionViewCellRegistration
      registrationWithCellClass:[HomeCustomizationColorPaletteCell class]
           configurationHandler:^(HomeCustomizationColorPaletteCell* cell,
                                  NSIndexPath* indexPath,
                                  id<BackgroundCustomizationConfiguration>
                                      backgroundConfiguration) {
             [weakSelf configureBackgroundCell:cell
                       backgroundConfiguration:backgroundConfiguration
                                   atIndexPath:indexPath];
           }];

  _customColorCellRegistration = [UICollectionViewCellRegistration
      registrationWithCellClass:[HomeCustomizationCustomColorCell class]
           configurationHandler:^(HomeCustomizationCustomColorCell* cell,
                                  NSIndexPath* indexPath,
                                  id<BackgroundCustomizationConfiguration>
                                      backgroundConfiguration) {
             cell.color = [weakSelf
                 resolvedColorPaletteLightColor:backgroundConfiguration];
           }];

  _collectionView = [[UICollectionView alloc] initWithFrame:CGRectZero
                                       collectionViewLayout:layout];
  _collectionView.dataSource = self;
  _collectionView.delegate = self;
  _collectionView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_collectionView];

  [NSLayoutConstraint activateConstraints:@[
    [_collectionView.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [_collectionView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [_collectionView.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    [_collectionView.widthAnchor constraintEqualToAnchor:self.view.widthAnchor
                                              multiplier:kRelativeRowWidth],
  ]];
}

- (void)viewWillDisappear:(BOOL)animated {
  base::UmaHistogramCounts10000(
      "IOS.HomeCustomization.Background.Color.ClickCount", _colorClickCount);
  [super viewWillDisappear:animated];
}

#pragma mark - HomeCustomizationBackgroundConfigurationConsumer

- (void)setBackgroundCollectionConfigurations:
            (NSArray<BackgroundCollectionConfiguration*>*)
                backgroundCollectionConfigurations
                         selectedBackgroundId:(NSString*)selectedBackgroundId {
  CHECK(backgroundCollectionConfigurations.count == 1);
  _backgroundCollectionConfiguration =
      backgroundCollectionConfigurations.firstObject;
  _selectedColorId = selectedBackgroundId;
}

- (void)currentBackgroundConfigurationChanged:
    (id<BackgroundCustomizationConfiguration>)currentConfiguration {
  NSString* currentItemID = currentConfiguration.configurationID;

  NSUInteger selectedIndex =
      [_backgroundCollectionConfiguration.configurationOrder
          indexOfObject:currentItemID];
  if (selectedIndex == NSNotFound) {
    _selectedColorId = nil;
    return;
  }

  [_collectionView
      selectItemAtIndexPath:[NSIndexPath indexPathForItem:selectedIndex
                                                inSection:0]
                   animated:NO
             scrollPosition:UICollectionViewScrollPositionNone];

  _selectedColorId = currentItemID;
}

#pragma mark - UICollectionViewDelegate

- (NSInteger)collectionView:(UICollectionView*)collectionView
     numberOfItemsInSection:(NSInteger)section {
  return _backgroundCollectionConfiguration.configurationOrder.count + 1;
}

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  std::size_t index = static_cast<std::size_t>(indexPath.item);
  if (index >= _backgroundCollectionConfiguration.configurationOrder.count) {
    return;
  }

  NSString* selectedID =
      _backgroundCollectionConfiguration.configurationOrder[indexPath.item];

  // Prevent background updates when a user clicks on an already selected cell.
  if (_selectedColorId == selectedID) {
    return;
  }

  HomeCustomizationCustomColorCell* customColorCell = [self customColorCell];
  id<BackgroundCustomizationConfiguration> backgroundConfiguration =
      _backgroundCollectionConfiguration.configurations[selectedID];
  _selectedColorId = backgroundConfiguration.configurationID;
  // Synchronize the custom color cell's display color with the current palette
  // configuration.
  customColorCell.color =
      [self resolvedColorPaletteLightColor:backgroundConfiguration];

  [self.mutator applyBackgroundForConfiguration:backgroundConfiguration];

  if (backgroundConfiguration.backgroundStyle ==
      HomeCustomizationBackgroundStyle::kDefault) {
    base::RecordAction(base::UserMetricsAction(
        "IOS.HomeCustomization.Background.ResetDefault.Tapped"));
  }
  _colorClickCount += 1;
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  std::size_t index = static_cast<std::size_t>(indexPath.item);

  if (indexPath.item >= 0) {
    if (index < _backgroundCollectionConfiguration.configurationOrder.count) {
      NSString* selectedID =
          _backgroundCollectionConfiguration.configurationOrder[indexPath.item];
      id<BackgroundCustomizationConfiguration> backgroundConfiguration =
          _backgroundCollectionConfiguration.configurations[selectedID];

      return [collectionView
          dequeueConfiguredReusableCellWithRegistration:_colorCellRegistration
                                           forIndexPath:indexPath
                                                   item:
                                                       backgroundConfiguration];
    } else {
      id<BackgroundCustomizationConfiguration> backgroundConfiguration =
          _backgroundCollectionConfiguration.configurations[_selectedColorId];

      return [collectionView
          dequeueConfiguredReusableCellWithRegistration:
              _customColorCellRegistration
                                           forIndexPath:indexPath
                                                   item:
                                                       backgroundConfiguration];
    }
  }

  return nil;
}

#pragma mark - Private

// Retrieve the light color from the color palette, or use the default color if
// none is available.
- (UIColor*)resolvedColorPaletteLightColor:
    (id<BackgroundCustomizationConfiguration>)backgroundConfiguration {
  return backgroundConfiguration.colorPalette.lightColor
             ?: DynamicNamedColor(@"ntp_background_color", kGrey100Color);
}

// Retrieve the custom color cell corresponding to the last index path in the
// collection view..
- (HomeCustomizationCustomColorCell*)customColorCell {
  NSIndexPath* lastIndexPath = [NSIndexPath
      indexPathForItem:[_collectionView numberOfItemsInSection:0] - 1
             inSection:0];

  return [_collectionView cellForItemAtIndexPath:lastIndexPath];
}

// Configures a `HomeCustomizationColorPaletteCell` with the provided background
// color configuration and selects it if it matches the currently selected
// background color.
- (void)configureBackgroundCell:(HomeCustomizationColorPaletteCell*)cell
        backgroundConfiguration:
            (id<BackgroundCustomizationConfiguration>)backgroundConfiguration
                    atIndexPath:(NSIndexPath*)indexPath {
  if (backgroundConfiguration.backgroundStyle ==
      HomeCustomizationBackgroundStyle::kDefault) {
    NewTabPageColorPalette* defaultColorPalette =
        [[NewTabPageColorPalette alloc] init];

    // The first choice should be the "no background" option (default appearance
    // colors).
    defaultColorPalette.lightColor =
        [self resolvedColorPaletteLightColor:backgroundConfiguration];
    defaultColorPalette.mediumColor =
        [UIColor colorNamed:@"fake_omnibox_solid_background_color"];
    defaultColorPalette.darkColor =
        DynamicNamedColor(kBlueColor, kTextPrimaryColor);
    cell.colorPalette = defaultColorPalette;
    cell.accessibilityLabel = l10n_util::GetNSString(
        IDS_IOS_HOME_CUSTOMIZATION_BACKGROUND_COLOR_DEFAULT_ACCESSIBILITY_LABEL);
  } else {
    cell.colorPalette = backgroundConfiguration.colorPalette;
    cell.accessibilityLabel = backgroundConfiguration.accessibilityName;
  }

  if ([_selectedColorId
          isEqualToString:backgroundConfiguration.configurationID]) {
    [_collectionView selectItemAtIndexPath:indexPath
                                  animated:NO
                            scrollPosition:UICollectionViewScrollPositionNone];
  }
}

@end
