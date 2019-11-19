// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/settings_root_collection_view_controller.h"

#include "base/logging.h"
#import "base/mac/foundation_util.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_cell_constants.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/material_components/chrome_app_bar_view_controller.h"
#import "ios/chrome/browser/ui/settings/bar_button_activity_indicator.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Collections/src/MaterialCollections.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
enum SavedBarButtomItemPositionEnum {
  kUndefinedBarButtonItemPosition,
  kLeftBarButtonItemPosition,
  kRightBarButtonItemPosition
};

// Dimension of the authentication operation activity indicator frame.
const CGFloat kActivityIndicatorDimensionIPad = 64;
const CGFloat kActivityIndicatorDimensionIPhone = 56;

}  // namespace

@implementation SettingsRootCollectionViewController {
  SavedBarButtomItemPositionEnum savedBarButtonItemPosition_;
  UIBarButtonItem* savedBarButtonItem_;
  UIView* veil_;
}

@synthesize shouldHideDoneButton = shouldHideDoneButton_;
@synthesize collectionViewAccessibilityIdentifier =
    collectionViewAccessibilityIdentifier_;
@synthesize dispatcher = _dispatcher;

- (void)viewDidLoad {
  [super viewDidLoad];
  self.collectionView.accessibilityIdentifier =
      self.collectionViewAccessibilityIdentifier;

  // Customize collection view settings.
  self.collectionView.backgroundColor = UIColor.cr_systemGroupedBackgroundColor;
  self.styler.cellStyle = MDCCollectionViewCellStyleGrouped;
  self.styler.separatorColor = UIColorFromRGB(kUIKitSeparatorColor);
  self.appBarViewController.headerView.backgroundColor =
      UIColor.cr_systemGroupedBackgroundColor;
  self.styler.separatorInset = UIEdgeInsetsMake(0, 16, 0, 16);

  self.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  UIBarButtonItem* doneButton = [self doneButtonIfNeeded];
  if (!self.navigationItem.rightBarButtonItem && doneButton) {
    self.navigationItem.rightBarButtonItem = doneButton;
  }
}

- (UIViewController*)childViewControllerForStatusBarHidden {
  return nil;
}

- (UIViewController*)childViewControllerForStatusBarStyle {
  return nil;
}

- (UIBarButtonItem*)doneButtonIfNeeded {
  if (self.shouldHideDoneButton) {
    return nil;
  }
  SettingsNavigationController* navigationController =
      base::mac::ObjCCast<SettingsNavigationController>(
          self.navigationController);
  return [navigationController doneButton];
}

- (UIBarButtonItem*)createEditButton {
  // Create a custom Edit bar button item, as Material Navigation Bar does not
  // handle a system UIBarButtonSystemItemEdit item.
  UIBarButtonItem* button = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON)
              style:UIBarButtonItemStyleDone
             target:self
             action:@selector(editButtonPressed)];
  [button setEnabled:[self editButtonEnabled]];
  return button;
}

- (UIBarButtonItem*)createEditDoneButton {
  // Create a custom Done bar button item, as Material Navigation Bar does not
  // handle a system UIBarButtonSystemItemDone item.
  return [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON)
              style:UIBarButtonItemStyleDone
             target:self
             action:@selector(editButtonPressed)];
}

- (void)updateEditButton {
  if ([self.editor isEditing]) {
    self.navigationItem.rightBarButtonItem = [self createEditDoneButton];
  } else if ([self shouldShowEditButton]) {
    self.navigationItem.rightBarButtonItem = [self createEditButton];
  } else {
    self.navigationItem.rightBarButtonItem = [self doneButtonIfNeeded];
  }
}

- (void)editButtonPressed {
  [self.editor setEditing:![self.editor isEditing] animated:YES];
  [self updateEditButton];
}

- (void)reloadData {
  [self loadModel];
  [self.collectionView reloadData];
}

#pragma mark - CollectionViewFooterLinkDelegate

- (void)cell:(CollectionViewFooterCell*)cell didTapLinkURL:(GURL)URL {
  // Subclass must have a valid dispatcher assigned.
  DCHECK(self.dispatcher);
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:URL];
  [self.dispatcher closeSettingsUIAndOpenURL:command];
}

#pragma mark - Subclassing

- (BOOL)shouldShowEditButton {
  return NO;
}

- (BOOL)editButtonEnabled {
  return NO;
}

- (void)preventUserInteraction {
  DCHECK(!savedBarButtonItem_);
  DCHECK_EQ(kUndefinedBarButtonItemPosition, savedBarButtonItemPosition_);

  // Create |waitButton|.
  BOOL displayActivityIndicatorOnTheRight =
      self.navigationItem.rightBarButtonItem != nil;
  CGFloat activityIndicatorDimension = IsIPadIdiom()
                                           ? kActivityIndicatorDimensionIPad
                                           : kActivityIndicatorDimensionIPhone;
  BarButtonActivityIndicator* indicator = [[BarButtonActivityIndicator alloc]
      initWithFrame:CGRectMake(0.0, 0.0, activityIndicatorDimension,
                               activityIndicatorDimension)];
  UIBarButtonItem* waitButton =
      [[UIBarButtonItem alloc] initWithCustomView:indicator];

  if (displayActivityIndicatorOnTheRight) {
    // If there is a right bar button item, then it is the "Done" button.
    savedBarButtonItem_ = self.navigationItem.rightBarButtonItem;
    savedBarButtonItemPosition_ = kRightBarButtonItemPosition;
    self.navigationItem.rightBarButtonItem = waitButton;
    [self.navigationItem.leftBarButtonItem setEnabled:NO];
  } else {
    savedBarButtonItem_ = self.navigationItem.leftBarButtonItem;
    savedBarButtonItemPosition_ = kLeftBarButtonItemPosition;
    self.navigationItem.leftBarButtonItem = waitButton;
  }

  // Adds a veil that covers the collection view and prevents user interaction.
  DCHECK(self.view);
  DCHECK(!veil_);
  veil_ = [[UIView alloc] initWithFrame:self.view.bounds];
  [veil_ setAutoresizingMask:(UIViewAutoresizingFlexibleWidth |
                              UIViewAutoresizingFlexibleHeight)];
  [veil_ setBackgroundColor:[UIColor colorWithWhite:1.0 alpha:0.5]];
  [self.view addSubview:veil_];

  // Disable user interaction for the navigation controller view to ensure
  // that the user cannot go back by swipping the navigation's top view
  // controller
  [self.navigationController.view setUserInteractionEnabled:NO];
}

- (void)allowUserInteraction {
  DCHECK(self.navigationController)
      << "|allowUserInteraction| should always be called before this settings"
         " controller is popped or dismissed.";
  [self.navigationController.view setUserInteractionEnabled:YES];

  // Removes the veil that prevents user interaction.
  DCHECK(veil_);
  [UIView animateWithDuration:0.3
      animations:^{
        [veil_ removeFromSuperview];
      }
      completion:^(BOOL finished) {
        veil_ = nil;
      }];

  DCHECK(savedBarButtonItem_);
  switch (savedBarButtonItemPosition_) {
    case kLeftBarButtonItemPosition:
      self.navigationItem.leftBarButtonItem = savedBarButtonItem_;
      break;
    case kRightBarButtonItemPosition:
      self.navigationItem.rightBarButtonItem = savedBarButtonItem_;
      [self.navigationItem.leftBarButtonItem setEnabled:YES];
      break;
    default:
      NOTREACHED();
      break;
  }
  savedBarButtonItem_ = nil;
  savedBarButtonItemPosition_ = kUndefinedBarButtonItemPosition;
}

@end
