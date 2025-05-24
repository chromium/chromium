// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_background_color_picker_view_controller.h"

#import "ios/chrome/browser/home_customization/ui/home_customization_color_palette_configuration.h"
#import "ios/chrome/browser/home_customization/ui/home_cutomization_color_palette_cell.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

// Define constants within the namespace
namespace {
// The width and height of each color palette cell in the collection view.
const CGFloat kColorCellSize = 48.0;

// The vertical spacing between rows of cells.
const CGFloat kLineSpacing = 28.0;

// The horizontal spacing between cells in the same row.
const CGFloat kItemSpacing = 20.0;

// The top padding for the section in the collection view.
const CGFloat kSectionInsetTop = 20.0;

// The left and right padding for the section in the collection view.
const CGFloat kSectionInsetSides = 27.0;

// The bottom padding for the section in the collection view.
const CGFloat kSectionInsetBottom = 20.0;
}  // namespace

@interface HomeCustomizationBackgroundColorPickerViewController () {
  // An array storing the available color palette configurations,
  // ordered by their index in the palette.
  NSArray<HomeCustomizationColorPaletteConfiguration*>*
      _colorPaletteConfigurations;

  // The `UICollectionViewCellRegistration` for registering  and configuring the
  // `HomeCustomizationColorPaletteCell` in the collection view.
  UICollectionViewCellRegistration* _colorCellRegistration;
}
@end

@implementation HomeCustomizationBackgroundColorPickerViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.title = l10n_util::GetNSStringWithFixup(
      IDS_IOS_HOME_CUSTOMIZATION_BACKGROUND_PICKER_COLOR_TITLE);

  self.view.backgroundColor = [UIColor systemBackgroundColor];

  UIBarButtonItem* dismissButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemClose
                           target:self
                           action:@selector(dismissCustomizationMenuPage)];

  dismissButton.accessibilityIdentifier = kNavigationBarDismissButtonIdentifier;

  self.navigationItem.rightBarButtonItem = dismissButton;
  self.navigationItem.backBarButtonItem.accessibilityIdentifier =
      kNavigationBarBackButtonIdentifier;
  [self.navigationItem setHidesBackButton:YES];

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
           configurationHandler:^(
               HomeCustomizationColorPaletteCell* cell, NSIndexPath* indexPath,
               HomeCustomizationColorPaletteConfiguration* configuration) {
             cell.configuration = configuration;
           }];

  UICollectionView* collectionView =
      [[UICollectionView alloc] initWithFrame:CGRectZero
                         collectionViewLayout:layout];
  collectionView.dataSource = self;

  collectionView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:collectionView];

  [NSLayoutConstraint activateConstraints:@[
    [collectionView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [collectionView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [collectionView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [collectionView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor]
  ]];
}

#pragma mark - HomeCustomizationBackgroundColorPickerConsumer

- (void)setColorPaletteConfigurations:
    (NSArray<HomeCustomizationColorPaletteConfiguration*>*)
        colorPaletteConfigurations {
  _colorPaletteConfigurations = colorPaletteConfigurations;
}

#pragma mark - UICollectionViewDelegate

- (NSInteger)collectionView:(UICollectionView*)collectionView
     numberOfItemsInSection:(NSInteger)section {
  return _colorPaletteConfigurations.count;
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  HomeCustomizationColorPaletteConfiguration* configuration =
      _colorPaletteConfigurations[indexPath.item];

  if (indexPath.item >= 0) {
    std::size_t index = static_cast<std::size_t>(indexPath.item);
    if (index < _colorPaletteConfigurations.count) {
      return [collectionView
          dequeueConfiguredReusableCellWithRegistration:_colorCellRegistration
                                           forIndexPath:indexPath
                                                   item:configuration];
    }
  }

  return nil;
}

#pragma mark - Private

- (void)dismissCustomizationMenuPage {
  [self dismissViewControllerAnimated:YES completion:nil];
}

@end
