// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_background_preset_gallery_picker_view_controller.h"

#import <Foundation/Foundation.h>

#import "base/check.h"
#import "ios/chrome/browser/home_customization/model/background_collection_configuration.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_cell.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_preset_gallery_picker_mutator.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_preset_header_view.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_collection_configurator.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_header_view.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_logo_vendor_provider.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_view_controller_protocol.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

// Define constants within the namespace
namespace {
// The left and right padding for the header in the collection view.
const CGFloat kHeaderInsetSides = 7.5;
}  // namespace

@interface HomeCustomizationBackgroundPresetGalleryPickerViewController () <
    HomeCustomizationViewControllerProtocol> {
  // The configurator for the collection view.
  HomeCustomizationCollectionConfigurator* _collectionConfigurator;

  // Registration for the background cell.
  UICollectionViewCellRegistration* _backgroundCellRegistration;

  // Registration for the collection's header.
  UICollectionViewSupplementaryRegistration* _headerRegistration;

  // A flat map of background customization options, keyed by background ID.
  // Used by HomeCustomizationBackgroundCell to apply backgrounds on the NTP.
  NSMutableDictionary<NSString*, BackgroundCustomizationConfiguration*>*
      _backgroundCustomizationConfigurationMap;

  // A list of background customization configurations grouped by section,
  // each associated with a collection name.
  NSArray<BackgroundCollectionConfiguration*>*
      _backgroundCollectionConfigurations;

  // The id of the selected background cell.
  NSString* _selectedBackgroundId;
}
@end

@implementation HomeCustomizationBackgroundPresetGalleryPickerViewController

// Synthesized from HomeCustomizationViewControllerProtocol.
@synthesize collectionView = _collectionView;
@synthesize diffableDataSource = _diffableDataSource;
@synthesize page = _page;

- (void)viewDidLoad {
  [super viewDidLoad];

  __weak __typeof(self) weakSelf = self;

  _collectionConfigurator = [[HomeCustomizationCollectionConfigurator alloc]
      initWithViewController:self];

  self.title = l10n_util::GetNSStringWithFixup(
      IDS_IOS_HOME_CUSTOMIZATION_BACKGROUND_PICKER_PRESET_GALLERY_TITLE);

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

  UICollectionViewCompositionalLayout* layout =
      [[UICollectionViewCompositionalLayout alloc]
          initWithSectionProvider:^NSCollectionLayoutSection*(
              NSInteger sectionIndex,
              id<NSCollectionLayoutEnvironment> layoutEnvironment) {
            return
                [weakSelf createSectionLayoutWithEnvironment:layoutEnvironment];
          }];

  [self createRegistrations];

  _collectionView = [[UICollectionView alloc] initWithFrame:CGRectZero
                                       collectionViewLayout:layout];
  _collectionView.delegate = self;

  _diffableDataSource = [[UICollectionViewDiffableDataSource alloc]
      initWithCollectionView:_collectionView
                cellProvider:^UICollectionViewCell*(
                    UICollectionView* collectionView, NSIndexPath* indexPath,
                    NSString* itemIdentifier) {
                  return [weakSelf configuredCellForIndexPath:indexPath
                                               itemIdentifier:itemIdentifier];
                }];

  _diffableDataSource.supplementaryViewProvider =
      ^UICollectionReusableView*(UICollectionView* collectionView,
                                 NSString* kind, NSIndexPath* indexPath) {
        return [weakSelf configuredHeaderForIndexPath:indexPath];
      };

  [_diffableDataSource applySnapshot:[self dataSnapshot]
                animatingDifferences:NO];

  _collectionView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_collectionView];

  AddSameConstraints(_collectionView, self.view);
}

#pragma mark - HomeCustomizationBackgroundPresetGalleryPickerConsumer

- (void)setBackgroundCollectionConfigurations:
            (NSArray<BackgroundCollectionConfiguration*>*)
                backgroundCollectionConfigurations
                         selectedBackgroundId:(NSString*)selectedBackgroundId {
  NSMutableDictionary<NSString*, BackgroundCustomizationConfiguration*>*
      backgroundCustomizationConfigurationMap =
          [NSMutableDictionary dictionary];

  // Flattens all background configurations from the collections into a single
  // map.
  for (BackgroundCollectionConfiguration* BackgroundCollectionConfiguration in
           backgroundCollectionConfigurations) {
    for (BackgroundCustomizationConfiguration* backgroundConfiguration in
             BackgroundCollectionConfiguration.configurations) {
      [backgroundCustomizationConfigurationMap
          setObject:backgroundConfiguration
             forKey:backgroundConfiguration.configurationID];
    }
  }

  _selectedBackgroundId = selectedBackgroundId;
  _backgroundCustomizationConfigurationMap =
      backgroundCustomizationConfigurationMap;
  _backgroundCollectionConfigurations = backgroundCollectionConfigurations;
}

#pragma mark - UICollectionViewDelegate

// Returns a configured header for the given path in the collection view.
- (UICollectionViewCell*)configuredHeaderForIndexPath:(NSIndexPath*)indexPath {
  return [_collectionView
      dequeueConfiguredReusableSupplementaryViewWithRegistration:
          _headerRegistration
                                                    forIndexPath:indexPath];
}

// Returns a configured cell for the given index path and item identifier.
- (UICollectionViewCell*)configuredCellForIndexPath:(NSIndexPath*)indexPath
                                     itemIdentifier:(NSString*)itemIdentifier {
  return [_collectionView
      dequeueConfiguredReusableCellWithRegistration:_backgroundCellRegistration
                                       forIndexPath:indexPath
                                               item:itemIdentifier];
}

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  NSString* itemIdentifier =
      [_diffableDataSource itemIdentifierForIndexPath:indexPath];
  BackgroundCustomizationConfiguration* backgroundConfiguration =
      _backgroundCustomizationConfigurationMap[itemIdentifier];

  [self.mutator applyBackgroundForConfiguration:backgroundConfiguration];
}

- (void)collectionView:(UICollectionView*)collectionView
       willDisplayCell:(HomeCustomizationBackgroundCell*)cell
    forItemAtIndexPath:(NSIndexPath*)indexPath {
  NSString* itemIdentifier =
      [_diffableDataSource itemIdentifierForIndexPath:indexPath];
  BackgroundCustomizationConfiguration* backgroundConfiguration =
      _backgroundCustomizationConfigurationMap[itemIdentifier];

  if (!backgroundConfiguration.thumbnailURL.is_empty()) {
    [self.mutator
        fetchBackgroundCustomizationThumbnailURLImage:backgroundConfiguration
                                                          .thumbnailURL
                                           completion:^(UIImage* image) {
                                             [cell updateBackgroundImage:image];
                                           }];
  }
}

#pragma mark - HomeCustomizationViewControllerProtocol

- (NSCollectionLayoutSection*)
      sectionForIndex:(NSInteger)sectionIndex
    layoutEnvironment:(id<NSCollectionLayoutEnvironment>)layoutEnvironment {
  return [_collectionConfigurator
      backgroundCellSectionForLayoutEnvironment:layoutEnvironment];
}

#pragma mark - Private

// Creates a data snapshot representing the content of the collection view.
- (NSDiffableDataSourceSnapshot<CustomizationSection*, NSString*>*)
    dataSnapshot {
  NSDiffableDataSourceSnapshot<NSString*, NSString*>* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];

  for (BackgroundCollectionConfiguration* BackgroundCollectionConfiguration in
           _backgroundCollectionConfigurations) {
    [snapshot appendSectionsWithIdentifiers:@[
      BackgroundCollectionConfiguration.collectionName
    ]];
    NSMutableArray* backgroundIds = [NSMutableArray array];

    for (BackgroundCustomizationConfiguration* backgroundConfiguration in
             BackgroundCollectionConfiguration.configurations) {
      [backgroundIds addObject:backgroundConfiguration.configurationID];
    }

    [snapshot appendItemsWithIdentifiers:backgroundIds
               intoSectionWithIdentifier:BackgroundCollectionConfiguration
                                             .collectionName];
  }

  return snapshot;
}

// Creates and configures the section layout using the given layout environment.
- (NSCollectionLayoutSection*)createSectionLayoutWithEnvironment:
    (id<NSCollectionLayoutEnvironment>)layoutEnvironment {
  NSCollectionLayoutSection* section = [_collectionConfigurator
      backgroundCellSectionForLayoutEnvironment:layoutEnvironment];

  // Header.
  NSCollectionLayoutSize* headerSize = [NSCollectionLayoutSize
      sizeWithWidthDimension:[NSCollectionLayoutDimension
                                 fractionalWidthDimension:1.0]
             heightDimension:[NSCollectionLayoutDimension
                                 estimatedDimension:0.0]];
  NSCollectionLayoutBoundarySupplementaryItem* headerItem =
      [NSCollectionLayoutBoundarySupplementaryItem
          boundarySupplementaryItemWithLayoutSize:headerSize
                                      elementKind:
                                          UICollectionElementKindSectionHeader
                                        alignment:NSRectAlignmentTop];
  headerItem.contentInsets =
      NSDirectionalEdgeInsetsMake(0, kHeaderInsetSides, 0, kHeaderInsetSides);

  section.boundarySupplementaryItems = @[ headerItem ];

  return section;
}

// Configuration handler for the header view.
- (void)configureHeaderView:(HomeCustomizationBackgroundPresetHeaderView*)header
                elementKind:(NSString*)elementKind
                  indexPath:(NSIndexPath*)indexPath {
  NSString* collectionName =
      [_diffableDataSource snapshot].sectionIdentifiers[indexPath.section];

  [header setText:collectionName];
}

// Creates the registrations for the collection view.
- (void)createRegistrations {
  __weak __typeof(self) weakSelf = self;

  _backgroundCellRegistration = [UICollectionViewCellRegistration
      registrationWithCellClass:[HomeCustomizationBackgroundCell class]
           configurationHandler:^(HomeCustomizationBackgroundCell* cell,
                                  NSIndexPath* indexPath,
                                  NSString* itemIdentifier) {
             [weakSelf configureBackgroundCell:cell
                                   atIndexPath:indexPath
                            withItemIdentifier:itemIdentifier];
           }];

  _headerRegistration = [UICollectionViewSupplementaryRegistration
      registrationWithSupplementaryClass:
          [HomeCustomizationBackgroundPresetHeaderView class]
                             elementKind:UICollectionElementKindSectionHeader
                    configurationHandler:^(
                        HomeCustomizationBackgroundPresetHeaderView* header,
                        NSString* elementKind, NSIndexPath* indexPath) {
                      [weakSelf configureHeaderView:header
                                        elementKind:elementKind
                                          indexPath:indexPath];
                    }];
}

// Configures a `HomeCustomizationBackgroundCell` with the provided background
// configuration and logo view, and selects it if it matches the currently
// selected background ID.
- (void)configureBackgroundCell:(HomeCustomizationBackgroundCell*)cell
                    atIndexPath:(NSIndexPath*)indexPath
             withItemIdentifier:(NSString*)itemIdentifier {
  BackgroundCustomizationConfiguration* backgroundConfiguration =
      _backgroundCustomizationConfigurationMap[itemIdentifier];
  id<LogoVendor> logoVendor = [self.logoVendorProvider provideLogoVendor];

  [cell configureWithBackgroundOption:backgroundConfiguration
                           logoVendor:logoVendor];

  if ([itemIdentifier isEqualToString:_selectedBackgroundId]) {
    [_collectionView selectItemAtIndexPath:indexPath
                                  animated:NO
                            scrollPosition:UICollectionViewScrollPositionNone];
  }
}

// Dismisses the current customization menu page.
- (void)dismissCustomizationMenuPage {
  [self dismissViewControllerAnimated:YES completion:nil];
}

@end
