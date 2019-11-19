// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/material_components/utils.h"
#import "ios/chrome/browser/ui/settings/bar_button_activity_indicator.h"
#import "ios/chrome/browser/ui/settings/cells/settings_cells_constants.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Height of the space used by header/footer when none is set. Default is
// |estimatedSection{Header|Footer}Height|.
const CGFloat kDefaultHeaderFooterHeight = 10;
// Estimated height of the header/footer, used to speed the constraints.
const CGFloat kEstimatedHeaderFooterHeight = 50;

enum SavedBarButtomItemPositionEnum {
  kUndefinedBarButtonItemPosition,
  kLeftBarButtonItemPosition,
  kRightBarButtonItemPosition
};

// Dimension of the authentication operation activity indicator frame.
const CGFloat kActivityIndicatorDimensionIPad = 64;
const CGFloat kActivityIndicatorDimensionIPhone = 56;
}  // namespace

NSString* const kSettingsToolbarDeleteButtonId =
    @"SettingsToolbarDeleteButtonId";

@interface SettingsRootTableViewController ()

// Delete button for the toolbar.
@property(nonatomic, strong) UIBarButtonItem* deleteButton;

// Item displayed before the user interactions are prevented. This is used to
// store the item while the interaction is prevented.
@property(nonatomic, strong) UIBarButtonItem* savedBarButtonItem;

// Veil preventing interactions with the TableView.
@property(nonatomic, strong) UIView* veil;

// Position of the saved button.
@property(nonatomic, assign)
    SavedBarButtomItemPositionEnum savedBarButtonItemPosition;

@end

@implementation SettingsRootTableViewController

@synthesize dispatcher = _dispatcher;

#pragma mark - Public

- (void)updateUIForEditState {
  if (self.tableView.editing) {
    self.navigationItem.rightBarButtonItem = [self createEditModeDoneButton];
    return;
  }

  [self.navigationController setToolbarHidden:self.shouldHideToolbar
                                     animated:YES];
  if (self.shouldShowEditButton) {
    self.navigationItem.rightBarButtonItem = [self createEditButton];
  } else {
    self.navigationItem.rightBarButtonItem = [self doneButtonIfNeeded];
  }
}

- (void)reloadData {
  [self loadModel];
  [self.tableView reloadData];
}

#pragma mark - Property

- (UIBarButtonItem*)deleteButton {
  if (!_deleteButton) {
    _deleteButton = [[UIBarButtonItem alloc]
        initWithTitle:l10n_util::GetNSString(IDS_IOS_SETTINGS_TOOLBAR_DELETE)
                style:UIBarButtonItemStylePlain
               target:self
               action:@selector(deleteButtonCallback)];
    _deleteButton.accessibilityIdentifier = kSettingsToolbarDeleteButtonId;
    _deleteButton.tintColor = [UIColor colorNamed:kRedColor];
  }
  return _deleteButton;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  UIBarButtonItem* flexibleSpace = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                           target:nil
                           action:nil];
  [self setToolbarItems:@[ flexibleSpace, self.deleteButton, flexibleSpace ]
               animated:YES];
  if (base::FeatureList::IsEnabled(kSettingsRefresh)) {
    self.styler.tableViewBackgroundColor = UIColor.cr_systemBackgroundColor;
  } else {
    self.styler.tableViewBackgroundColor =
        UIColor.cr_systemGroupedBackgroundColor;
  }
  [super viewDidLoad];
  if (base::FeatureList::IsEnabled(kSettingsRefresh)) {
    self.tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
  }
  self.styler.cellBackgroundColor =
      UIColor.cr_secondarySystemGroupedBackgroundColor;
  self.styler.cellTitleColor = UIColor.cr_labelColor;
  self.tableView.estimatedSectionHeaderHeight = kEstimatedHeaderFooterHeight;
  self.tableView.estimatedRowHeight = kSettingsCellDefaultHeight;
  self.tableView.estimatedSectionFooterHeight = kEstimatedHeaderFooterHeight;
  self.tableView.separatorInset =
      UIEdgeInsetsMake(0, kTableViewSeparatorInset, 0, 0);

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

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  [self.navigationController setToolbarHidden:YES animated:YES];
}

- (void)setEditing:(BOOL)editing animated:(BOOL)animated {
  [super setEditing:editing animated:animated];
  if (!editing)
    [self.navigationController setToolbarHidden:self.shouldHideToolbar
                                       animated:YES];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  if (@available(iOS 13, *)) {
  } else {
    // This is a workaround to fix the vertical alignment of the back button.
    // The bug has been fixed in iOS 13. See crbug.com/931173 if needed.
    [self.navigationController.navigationBar setNeedsLayout];
  }
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  if (!self.tableView.editing)
    return;

  if (self.navigationController.toolbarHidden)
    [self.navigationController setToolbarHidden:NO animated:YES];
}

- (void)tableView:(UITableView*)tableView
    didDeselectRowAtIndexPath:(NSIndexPath*)indexPath {
  if (!self.tableView.editing)
    return;

  if (self.tableView.indexPathsForSelectedRows.count == 0)
    [self.navigationController setToolbarHidden:self.shouldHideToolbar
                                       animated:YES];
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  if ([self.tableViewModel headerForSection:section])
    return UITableViewAutomaticDimension;
  return kDefaultHeaderFooterHeight;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  if ([self.tableViewModel footerForSection:section])
    return UITableViewAutomaticDimension;
  return kDefaultHeaderFooterHeight;
}

#pragma mark - TableViewLinkHeaderFooterItemDelegate

- (void)view:(TableViewLinkHeaderFooterView*)view didTapLinkURL:(GURL)URL {
  // Subclass must have a valid dispatcher assigned.
  DCHECK(self.dispatcher);
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:URL];
  [self.dispatcher closeSettingsUIAndOpenURL:command];
}

#pragma mark - Private

- (void)deleteButtonCallback {
  [self deleteItems:self.tableView.indexPathsForSelectedRows];
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

- (UIBarButtonItem*)createEditModeDoneButton {
  // Create a custom Done bar button item, as Material Navigation Bar does not
  // handle a system UIBarButtonSystemItemDone item.
  return [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON)
              style:UIBarButtonItemStyleDone
             target:self
             action:@selector(editButtonPressed)];
}

#pragma mark - Subclassing

- (BOOL)shouldHideToolbar {
  return YES;
}

- (BOOL)shouldShowEditButton {
  return NO;
}

- (BOOL)editButtonEnabled {
  return NO;
}

- (void)editButtonPressed {
  [self setEditing:!self.tableView.editing animated:YES];
  [self updateUIForEditState];
}

- (void)deleteItems:(NSArray<NSIndexPath*>*)indexPaths {
  [self.tableView
      performBatchUpdates:^{
        [self removeFromModelItemAtIndexPaths:indexPaths];
        [self.tableView
            deleteRowsAtIndexPaths:indexPaths
                  withRowAnimation:UITableViewRowAnimationAutomatic];
      }
               completion:nil];
}

- (void)preventUserInteraction {
  DCHECK(!self.savedBarButtonItem);
  DCHECK_EQ(kUndefinedBarButtonItemPosition, self.savedBarButtonItemPosition);

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
    self.savedBarButtonItem = self.navigationItem.rightBarButtonItem;
    self.savedBarButtonItemPosition = kRightBarButtonItemPosition;
    self.navigationItem.rightBarButtonItem = waitButton;
    [self.navigationItem.leftBarButtonItem setEnabled:NO];
  } else {
    self.savedBarButtonItem = self.navigationItem.leftBarButtonItem;
    self.savedBarButtonItemPosition = kLeftBarButtonItemPosition;
    self.navigationItem.leftBarButtonItem = waitButton;
  }

  // Adds a veil that covers the collection view and prevents user interaction.
  DCHECK(self.view);
  DCHECK(!self.veil);
  self.veil = [[UIView alloc] initWithFrame:self.view.bounds];
  [self.veil setAutoresizingMask:(UIViewAutoresizingFlexibleWidth |
                                  UIViewAutoresizingFlexibleHeight)];
  [self.veil setBackgroundColor:[UIColor colorWithWhite:1.0 alpha:0.5]];
  [self.view addSubview:self.veil];

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
  DCHECK(self.veil);
  [UIView animateWithDuration:0.3
      animations:^{
        [self.veil removeFromSuperview];
      }
      completion:^(BOOL finished) {
        self.veil = nil;
      }];

  DCHECK(self.savedBarButtonItem);
  switch (self.savedBarButtonItemPosition) {
    case kLeftBarButtonItemPosition:
      self.navigationItem.leftBarButtonItem = self.savedBarButtonItem;
      break;
    case kRightBarButtonItemPosition:
      self.navigationItem.rightBarButtonItem = self.savedBarButtonItem;
      [self.navigationItem.leftBarButtonItem setEnabled:YES];
      break;
    default:
      NOTREACHED();
      break;
  }
  self.savedBarButtonItem = nil;
  self.savedBarButtonItemPosition = kUndefinedBarButtonItemPosition;
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (BOOL)presentationControllerShouldDismiss:
    (UIPresentationController*)presentationController {
  return YES;
}

@end
