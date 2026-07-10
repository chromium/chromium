// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/catalogs/ui/view_controller_catalog_view_controller.h"

#import "ios/chrome/browser/alert_view/ui_bundled/alert_action.h"
#import "ios/chrome/browser/alert_view/ui_bundled/alert_view_controller.h"
#import "ios/chrome/browser/catalogs/ui/demo_button_stack_view_controller.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"

namespace {

// Sections used in ViewController Catalog page.
enum SectionIdentifier {
  kSectionIdentifierViewController = kSectionIdentifierEnumZero,
};

// Item types used per ViewController section.
enum ItemType {
  kItemTypeAlertViewController = kItemTypeEnumZero,
  kItemTypeButtonStackViewController,
};

}  // namespace

@implementation ViewControllerCatalogViewController

- (instancetype)init {
  return [super initWithStyle:UITableViewStyleInsetGrouped];
}

- (void)viewDidLoad {
  [super viewDidLoad];

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

  // Add sections.
  [model addSectionWithIdentifier:kSectionIdentifierViewController];

  // Add items.
  [model addItem:alertItem
      toSectionWithIdentifier:kSectionIdentifierViewController];
  [model addItem:buttonStackItem
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
  }
}

#pragma mark - Private

// Initializes and configures the AlertViewController.
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
  ButtonStackConfiguration* configuration =
      [[ButtonStackConfiguration alloc] init];
  configuration.primaryActionString = @"Primary";
  configuration.secondaryActionString = @"Secondary";
  configuration.tertiaryActionString = @"Tertiary";

  // The `ButtonStackViewController` should always be subclassed.
  DemoButtonStackViewController* buttonStackViewController =
      [[DemoButtonStackViewController alloc]
          initWithConfiguration:configuration];

  return buttonStackViewController;
}

@end
