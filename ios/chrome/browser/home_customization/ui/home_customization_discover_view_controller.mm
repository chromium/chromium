// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_discover_view_controller.h"

#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"

@interface HomeCustomizationDiscoverViewController () <UICollectionViewDelegate>

@end

@implementation HomeCustomizationDiscoverViewController {
  // The collection view containing this menu page's content.
  UICollectionView* _collectionView;

  // The diffable data source for the collection view.
  UICollectionViewDiffableDataSource<CustomizationSection*, NSNumber*>*
      _diffableDataSource;

  // Registration for the HomeCustomizationMagicStackCell.
  UICollectionViewCellRegistration* _magicStackCellRegistration;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  [self createCollectionView];

  // The primary view is set as the collection view for better integration with
  // the UISheetPresentationController which presents it.
  self.view = _collectionView;

  [self configureNavigationBar];
}

#pragma mark - Private

// Creates and returns the collection view for the main menu page.
- (void)createCollectionView {
  _collectionView = [[UICollectionView alloc]
             initWithFrame:CGRectZero
      collectionViewLayout:[[UICollectionViewFlowLayout alloc] init]];
  _collectionView.translatesAutoresizingMaskIntoConstraints = NO;
  _collectionView.delegate = self;

  _diffableDataSource = [self createDiffableDataSource];
  _collectionView.dataSource = _diffableDataSource;
}

// Creates and returns the diffable data source for the collection view.
- (UICollectionViewDiffableDataSource*)createDiffableDataSource {
  // Creates the diffable data source with a cell provider used to configure
  // each cell.
  __weak __typeof(self) weakSelf = self;
  auto cellProvider =
      ^UICollectionViewCell*(UICollectionView* collectionView,
                             NSIndexPath* indexPath, NSNumber* itemIdentifier) {
        return [weakSelf configuredCellForIndexPath:indexPath
                                     itemIdentifier:itemIdentifier];
      };
  UICollectionViewDiffableDataSource* diffableDataSource =
      [[UICollectionViewDiffableDataSource alloc]
          initWithCollectionView:_collectionView
                    cellProvider:cellProvider];

  return diffableDataSource;
}

// Returns a configured cell for the given path in the collection view.
- (UICollectionViewCell*)configuredCellForIndexPath:(NSIndexPath*)indexPath
                                     itemIdentifier:(NSNumber*)itemIdentifier {
  return [_collectionView
      dequeueConfiguredReusableCellWithRegistration:_magicStackCellRegistration
                                       forIndexPath:indexPath
                                               item:itemIdentifier];
}

// Sets the title and button to the navigation bar on top of the presenting menu
// page.
- (void)configureNavigationBar {
  // TODO(crbug.com/350990359): Confirm page title.
  self.title = @"Discover";
  UIBarButtonItem* dismissButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemClose
                           target:self
                           action:@selector(dismissCustomizationMenu)];
  dismissButton.accessibilityIdentifier = kNavigationBarDismissButtonIdentifier;
  self.navigationItem.rightBarButtonItem = dismissButton;
  self.navigationItem.leftBarButtonItem.accessibilityIdentifier =
      kNavigationBarBackButtonIdentifier;
}

// Dismisses the presenting view controller.
- (void)dismissCustomizationMenu {
  [self.presentingViewController dismissViewControllerAnimated:YES
                                                    completion:nil];
}

@end
