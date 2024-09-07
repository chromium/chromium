// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/notreached.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/settings/bar_button_activity_indicator.h"
#import "ios/chrome/browser/ui/settings/cells/settings_cells_constants.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_constants.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Height of the space used by header/footer when none is set. Default is
// `estimatedSection{Header|Footer}Height`.
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

@synthesize applicationHandler = _applicationHandler;
@synthesize browserHandler = _browserHandler;
@synthesize settingsHandler = _settingsHandler;
@synthesize snackbarHandler = _snackbarHandler;

#pragma mark - Public

- (void)updateUIForEditState {
  // Update toolbar.
  [self.navigationController setToolbarHidden:self.shouldHideToolbar
                                     animated:YES];

  // Update edit button.
  if ([self shouldShowEditDoneButton] && self.tableView.editing) {
    self.navigationItem.rightBarButtonItem =
        [self createEditModeDoneButtonForToolbar:NO];
  } else if (self.shouldShowEditButton) {
    self.navigationItem.rightBarButtonItem =
        [self createEditButtonForToolbar:NO];
  } else {
    self.navigationItem.rightBarButtonItem = [self doneButtonIfNeeded];
  }

  // Update Cancel/Back button.
  if (self.showCancelDuringEditing) {
    self.navigationItem.leftBarButtonItem =
        self.tableView.editing ? [self createEditModeCancelButton]
                               : self.customLeftBarButtonItem;
  }

  // The following two lines cause the table view to refresh the cell heights
  // with animation without reloading the cells. This is needed for
  // cells that can be significantly taller in edit mode.
  [self.tableView beginUpdates];
  [self.tableView endUpdates];
}

- (void)updatedToolbarForEditState {
  if (self.shouldHideToolbar) {
    return;
  }

  UIBarButtonItem* flexibleSpace = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                           target:nil
                           action:nil];

  UIBarButtonItem* toolbarLeftButton = flexibleSpace;
  if (self.customLeftToolbarButton) {
    toolbarLeftButton = self.customLeftToolbarButton;
  } else if (self.tableView.editing && self.shouldShowDeleteButtonInToolbar) {
    toolbarLeftButton = self.deleteButton;
  }

  UIBarButtonItem* toolbarRightButton = flexibleSpace;
  if (self.customRightToolbarButton) {
    toolbarRightButton = self.customRightToolbarButton;
  } else if (self.tableView.editing) {
    toolbarRightButton = [self createEditModeDoneButtonForToolbar:YES];
  } else {
    toolbarRightButton = [self createEditButtonForToolbar:YES];
  }

  [self
      setToolbarItems:@[ toolbarLeftButton, flexibleSpace, toolbarRightButton ]
             animated:YES];

  if (self.tableView.editing) {
    self.deleteButton.enabled = NO;
  }
}

- (void)reloadData {
  [self loadModel];
  [self.tableView reloadData];
}

- (void)configureHandlersForRootViewController:
    (id<SettingsRootViewControlling>)controller {
  controller.applicationHandler = self.applicationHandler;
  controller.browserHandler = self.browserHandler;
  controller.settingsHandler = self.settingsHandler;
  controller.snackbarHandler = self.snackbarHandler;
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

  [super viewDidLoad];
  self.styler.cellBackgroundColor =
      [UIColor colorNamed:kGroupedSecondaryBackgroundColor];
  self.styler.cellTitleColor = [UIColor colorNamed:kTextPrimaryColor];
  self.tableView.estimatedSectionHeaderHeight = kEstimatedHeaderFooterHeight;
  self.tableView.estimatedRowHeight = kSettingsCellDefaultHeight;
  self.tableView.estimatedSectionFooterHeight = kEstimatedHeaderFooterHeight;
  self.tableView.separatorInset =
      UIEdgeInsetsMake(0, kTableViewSeparatorInset, 0, 0);

  self.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;

  self.customLeftBarButtonItem = self.navigationItem.leftBarButtonItem;
  self.shouldShowDeleteButtonInToolbar = YES;
  self.extendedLayoutIncludesOpaqueBars = YES;
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  UIBarButtonItem* doneButton = [self doneButtonIfNeeded];
  if (!self.navigationItem.rightBarButtonItem && doneButton) {
    self.navigationItem.rightBarButtonItem = doneButton;
  }
}

- (void)willMoveToParentViewController:(UIViewController*)parent {
  [super willMoveToParentViewController:parent];

  // When the view controller is in editing mode, setEditing might get called
  // after this, which could show the toolbar based on the requirements of the
  // view controller that is being popped out of the navigation controller. This
  // can leave the new top view controller with a toolbar when it doesn't
  // require one. Disabling editing mode to avoid this. See crbug.com/1404111 as
  // an example.
  if (!parent && self.isEditing) {
    [self setEditing:NO animated:NO];
  }

  [self.navigationController setToolbarHidden:YES animated:YES];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent && [self respondsToSelector:@selector(settingsWillBeDismissed)]) {
    [self performSelector:@selector(settingsWillBeDismissed)];
  }
}

- (void)setEditing:(BOOL)editing animated:(BOOL)animated {
  [super setEditing:editing animated:animated];
  if (!editing && self.navigationController.topViewController == self) {
    [self.navigationController setToolbarHidden:self.shouldHideToolbar
                                       animated:YES];
  }
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
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
  if ([self.tableViewModel headerForSectionIndex:section])
    return UITableViewAutomaticDimension;
  return ChromeTableViewHeightForHeaderInSection(section);
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  if ([self.tableViewModel footerForSectionIndex:section])
    return UITableViewAutomaticDimension;
  return kDefaultHeaderFooterHeight;
}

#pragma mark - TableViewLinkHeaderFooterItemDelegate

- (void)view:(TableViewLinkHeaderFooterView*)view didTapLinkURL:(CrURL*)URL {
  // Subclass must have a valid dispatcher assigned.
  DCHECK(self.applicationHandler);
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:URL.gurl];
  [self.applicationHandler closeSettingsUIAndOpenURL:command];
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
      base::apple::ObjCCast<SettingsNavigationController>(
          self.navigationController);
  UIBarButtonItem* doneButton = [navigationController doneButton];
  if (_shouldDisableDoneButtonOnEdit) {
    doneButton.enabled = !self.tableView.editing;
  }
  return doneButton;
}

- (UIBarButtonItem*)createEditButtonForToolbar:(BOOL)toolbar {
  // Create a custom Edit bar button item, as Material Navigation Bar does not
  // handle a system UIBarButtonSystemItemEdit item.
  UIBarButtonItem* button = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON)
              style:(toolbar ? UIBarButtonItemStylePlain
                             : UIBarButtonItemStyleDone)target:self
             action:@selector(editButtonPressed)];
  [button setEnabled:[self editButtonEnabled]];
  button.accessibilityIdentifier = kSettingsToolbarEditButtonId;
  return button;
}

- (UIBarButtonItem*)createEditModeDoneButtonForToolbar:(BOOL)toolbar {
  // Create a custom Done bar button item, as Material Navigation Bar does not
  // handle a system UIBarButtonSystemItemDone item.
  UIBarButtonItem* button = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON)
              style:(toolbar ? UIBarButtonItemStylePlain
                             : UIBarButtonItemStyleDone)target:self
             action:@selector(editButtonPressed)];
  button.accessibilityIdentifier = kSettingsToolbarEditDoneButtonId;
  return button;
}

- (UIBarButtonItem*)createEditModeCancelButton {
  // Create a custom Cancel bar button item.
  return [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(cancelEditing)];
}

// Quits editing mode and reloads data to the state before editing.
- (void)cancelEditing {
  [self setEditing:!self.tableView.editing animated:YES];
  [self updateUIForEditState];
  [self reloadData];
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

- (BOOL)showCancelDuringEditing {
  return NO;
}

- (BOOL)shouldShowEditDoneButton {
  return YES;
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

  // Create `waitButton`.
  BOOL displayActivityIndicatorOnTheRight =
      self.navigationItem.rightBarButtonItem != nil;
  CGFloat activityIndicatorDimension =
      (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET)
          ? kActivityIndicatorDimensionIPad
          : kActivityIndicatorDimensionIPhone;
  BarButtonActivityIndicator* indicator = [[BarButtonActivityIndicator alloc]
      initWithFrame:CGRectMake(0.0, 0.0, activityIndicatorDimension,
                               activityIndicatorDimension)];
  UIBarButtonItem* waitButton =
      [[UIBarButtonItem alloc] initWithCustomView:indicator];
  waitButton.accessibilityLabel = kSettingsWaitButtonId;

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
  [self.veil setBackgroundColor:[[UIColor colorNamed:kSolidWhiteColor]
                                    colorWithAlphaComponent:0.5]];
  [self.view addSubview:self.veil];

  // Disable user interaction for the navigation controller view to ensure
  // that the user cannot go back by swipping the navigation's top view
  // controller
  [self.navigationController.view setUserInteractionEnabled:NO];
}

- (void)allowUserInteraction {
  DCHECK(self.navigationController)
      << "`allowUserInteraction` should always be called before this settings"
         " controller is popped or dismissed.";
  [self.navigationController.view setUserInteractionEnabled:YES];

  // Removes the veil that prevents user interaction.
  DCHECK(self.veil);
  [UIView animateWithDuration:0.3
                   animations:^{
                     [self.veil removeFromSuperview];
                   }
                   completion:nil];
  // Need to remove `self.veil` to be able immediately, so
  // `preventUserInteraction` can be called in less than 0.3s after.
  self.veil = nil;

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
      DUMP_WILL_BE_NOTREACHED();
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
