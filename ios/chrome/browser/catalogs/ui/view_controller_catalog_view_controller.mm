// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/catalogs/ui/view_controller_catalog_view_controller.h"

#import "ios/chrome/browser/alert_view/ui_bundled/alert_action.h"
#import "ios/chrome/browser/alert_view/ui_bundled/alert_view_controller.h"
#import "ios/chrome/browser/catalogs/ui/demo_button_stack_view_controller.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"

namespace {

// Sections used in ViewController Catalog page.
enum SectionIdentifier {
  kSectionIdentifierViewController = kSectionIdentifierEnumZero,
};

// Item types used per ViewController section.
enum ItemType {
  kItemTypeAlertViewController = kItemTypeEnumZero,
  kItemTypeButtonStackViewController,
  kItemTypeConfirmationAlertViewController,
};

// Spacing Constant.
const CGFloat kImageTopSpacing = 20;

}  // namespace

@implementation ViewControllerCatalogViewController {
  // Configuration used for all `ButtonStackViewController` subclasses.
  ButtonStackConfiguration* _configuration;
}

- (instancetype)init {
  return [super initWithStyle:UITableViewStyleInsetGrouped];
}

- (void)viewDidLoad {
  [super viewDidLoad];

  _configuration = [[ButtonStackConfiguration alloc] init];
  _configuration.primaryActionString = @"Primary";
  _configuration.secondaryActionString = @"Secondary";
  _configuration.tertiaryActionString = @"Tertiary";

  self.title = @"ViewController Catalog";

  [self loadModel];
}

- (void)loadModel {
  [super loadModel];

  TableViewModel<TableViewItem*>* model = self.tableViewModel;

  TableViewTextItem* alertItem =
      [[TableViewTextItem alloc] initWithType:kItemTypeAlertViewController];
  alertItem.text = @"AlertViewController";

  TableViewTextItem* buttonStackItem = [[TableViewTextItem alloc]
      initWithType:kItemTypeButtonStackViewController];
  buttonStackItem.text = @"ButtonStackViewController";

  TableViewTextItem* confirmationAlertItem = [[TableViewTextItem alloc]
      initWithType:kItemTypeConfirmationAlertViewController];
  confirmationAlertItem.text = @"ConfirmationAlertViewController";

  // Add sections.
  [model addSectionWithIdentifier:kSectionIdentifierViewController];

  // Add items.
  [model addItem:alertItem
      toSectionWithIdentifier:kSectionIdentifierViewController];
  [model addItem:buttonStackItem
      toSectionWithIdentifier:kSectionIdentifierViewController];
  [model addItem:confirmationAlertItem
      toSectionWithIdentifier:kSectionIdentifierViewController];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  switch ([self.tableViewModel itemTypeForIndexPath:indexPath]) {
    case kItemTypeAlertViewController: {
      [self presentViewController:[self configuredAlertViewController]
                         animated:YES
                       completion:nil];
      break;
    }
    case kItemTypeButtonStackViewController: {
      [self presentViewController:[self configuredButtonStackViewController]
                         animated:YES
                       completion:nil];
      break;
    }
    case kItemTypeConfirmationAlertViewController: {
      [self
          presentViewController:[self configuredConfirmationAlertViewController]
                       animated:YES
                     completion:nil];
      break;
    }
  }
}

#pragma mark - Private

// Initializes and configures the `AlertViewController`.
- (AlertViewController*)configuredAlertViewController {
  AlertViewController* alertViewController = [[AlertViewController alloc] init];

  // This alert is designed for contexts where full-screen coverage is not
  // desired.
  alertViewController.modalPresentationStyle =
      UIModalPresentationOverCurrentContext;
  alertViewController.modalTransitionStyle =
      UIModalTransitionStyleCrossDissolve;

  [alertViewController setTitle:@"Alert title"];
  [alertViewController
      setMessage:@"Use this alert to avoid covering the entire screen."];

  __weak AlertViewController* weakAlert = alertViewController;

  void (^dismissAction)(AlertAction*) = ^(AlertAction* action) {
    [weakAlert dismissViewControllerAnimated:YES completion:nil];
  };

  NSArray<NSArray<AlertAction*>*>* actions = @[
    @[ [AlertAction actionWithTitle:@"Default"
                              style:UIAlertActionStyleDefault
                            handler:dismissAction] ],
    @[ [AlertAction actionWithTitle:@"Cancel"
                              style:UIAlertActionStyleCancel
                            handler:dismissAction] ],
    @[ [AlertAction actionWithTitle:@"Destructive"
                              style:UIAlertActionStyleDestructive
                            handler:dismissAction] ]
  ];

  [alertViewController setActions:actions];

  return alertViewController;
}

// Initializes and configures the `ButtonStackViewController`.
- (DemoButtonStackViewController*)configuredButtonStackViewController {
  // The `ButtonStackViewController` should always be subclassed.
  DemoButtonStackViewController* buttonStackViewController =
      [[DemoButtonStackViewController alloc]
          initWithConfiguration:_configuration];

  return buttonStackViewController;
}

// Initializes and configures the `ConfirmationAlertViewController`.
- (ConfirmationAlertViewController*)configuredConfirmationAlertViewController {
  ConfirmationAlertViewController* confirmationAlertViewController =
      [[ConfirmationAlertViewController alloc]
          initWithConfiguration:_configuration];

  confirmationAlertViewController.titleString = @"This is the Title string";
  confirmationAlertViewController.subtitleString =
      @"This is the subtitle string";

  confirmationAlertViewController.aboveTitleView =
      [self configuredLabelWithString:@"This is the aboveTitleView"];
  confirmationAlertViewController.underTitleView =
      [self configuredLabelWithString:@"This is the underTitleView"];

  confirmationAlertViewController.image =
      [UIImage imageNamed:@"collaboration_signin_background"];
  confirmationAlertViewController.customSpacingBeforeImage = kImageTopSpacing;

  return confirmationAlertViewController;
}

#pragma mark - Private Helpers

// Returns a `UILabel` with the desired string.
- (UILabel*)configuredLabelWithString:(NSString*)string {
  UILabel* label = [[UILabel alloc] init];
  label.text = string;
  label.translatesAutoresizingMaskIntoConstraints = NO;
  return label;
}

@end
