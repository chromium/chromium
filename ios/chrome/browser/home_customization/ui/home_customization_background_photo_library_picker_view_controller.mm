// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_background_photo_library_picker_view_controller.h"

#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@implementation HomeCustomizationBackgroundPhotoLibraryPickerViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.title = l10n_util::GetNSStringWithFixup(
      IDS_IOS_HOME_CUSTOMIZATION_BACKGROUND_PICKER_PHOTO_LIBRARY_TITLE);
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
}

#pragma mark - UICollectionViewDelegate

- (NSInteger)collectionView:(UICollectionView*)collectionView
     numberOfItemsInSection:(NSInteger)section {
  return 0;
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  return nil;
}

#pragma mark - Private

- (void)dismissCustomizationMenuPage {
  [self dismissViewControllerAnimated:YES completion:nil];
}

@end
