// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_background_color_picker_view_controller.h"

#import "ios/chrome/browser/home_customization/ui/background_customization_configuration.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_picker_action_sheet_presentation_delegate.h"
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

  // An array storing the available background configurations, ordered by their
  // index in the collection.
  NSArray<id<BackgroundCustomizationConfiguration>>* _backgroundConfigurations;

  // The `UICollectionViewCellRegistration` for registering  and configuring the
  // `HomeCustomizationColorPaletteCell` in the collection view.
  UICollectionViewCellRegistration* _colorCellRegistration;

  // Currently selected color index in the palette.
  NSNumber* _selectedColorIndex;
}
@end

@implementation HomeCustomizationBackgroundColorPickerViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  __weak __typeof(self) weakSelf = self;
  self.title = l10n_util::GetNSStringWithFixup(
      IDS_IOS_HOME_CUSTOMIZATION_BACKGROUND_PICKER_COLOR_TITLE);

  self.view.backgroundColor = [UIColor systemBackgroundColor];

  UICollectionViewFlowLayout* layout =
      [[UICollectionViewFlowLayout alloc] init];

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

  _collectionView = [[UICollectionView alloc] initWithFrame:CGRectZero
                                       collectionViewLayout:layout];
  _collectionView.dataSource = self;
  _collectionView.delegate = self;

  _collectionView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_collectionView];

  [NSLayoutConstraint activateConstraints:@[
    [_collectionView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [_collectionView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [_collectionView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [_collectionView.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor]
  ]];
}

#pragma mark - HomeCustomizationBackgroundColorPickerConsumer

- (void)populateBackgroundCustomizationConfigurations:
            (NSArray<id<BackgroundCustomizationConfiguration>>*)
                backgroundCustomizationConfigurations
                                   selectedColorIndex:
                                       (NSNumber*)selectedColorIndex {
  _backgroundConfigurations = backgroundCustomizationConfigurations;
  _selectedColorIndex = selectedColorIndex;
}

#pragma mark - UICollectionViewDelegate

- (NSInteger)collectionView:(UICollectionView*)collectionView
     numberOfItemsInSection:(NSInteger)section {
  return _backgroundConfigurations.count;
}

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  id<BackgroundCustomizationConfiguration> backgroundConfiguration =
      _backgroundConfigurations[indexPath.item];
  _selectedColorIndex = @(indexPath.item);
  [self.presentationDelegate
      applyBackgroundForConfiguration:backgroundConfiguration];
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  id<BackgroundCustomizationConfiguration> backgroundConfiguration =
      _backgroundConfigurations[indexPath.item];

  if (indexPath.item >= 0) {
    std::size_t index = static_cast<std::size_t>(indexPath.item);
    if (index < _backgroundConfigurations.count) {
      return [collectionView
          dequeueConfiguredReusableCellWithRegistration:_colorCellRegistration
                                           forIndexPath:indexPath
                                                   item:
                                                       backgroundConfiguration];
    }
  }

  return nil;
}

#pragma mark - Private

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
        DynamicNamedColor(@"ntp_background_color", kGrey100Color);
    defaultColorPalette.mediumColor =
        [UIColor colorNamed:@"fake_omnibox_solid_background_color"];
    defaultColorPalette.darkColor =
        DynamicNamedColor(kBlueColor, kTextPrimaryColor);
    cell.colorPalette = defaultColorPalette;
  } else {
    cell.colorPalette = backgroundConfiguration.colorPalette;
  }

  if ([_selectedColorIndex isEqualToNumber:@(indexPath.item)]) {
    [_collectionView selectItemAtIndexPath:indexPath
                                  animated:NO
                            scrollPosition:UICollectionViewScrollPositionNone];
  }
}

@end
