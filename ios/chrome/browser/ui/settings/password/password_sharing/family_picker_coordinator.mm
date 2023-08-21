// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/family_picker_coordinator.h"

#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/family_picker_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/family_picker_mediator.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/family_picker_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/family_picker_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/recipient_info.h"

@interface FamilyPickerCoordinator () <
    FamilyPickerViewControllerPresentationDelegate> {
  NSArray<RecipientInfoForIOSDisplay*>* _recipients;
}

// The navigation controller displaying the view controller.
@property(nonatomic, strong)
    TableViewNavigationController* navigationController;

// Main view controller for this coordinator.
@property(nonatomic, strong) FamilyPickerViewController* viewController;

// Main mediator for this coordinator.
@property(nonatomic, strong) FamilyPickerMediator* mediator;

@end

@implementation FamilyPickerCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                recipients:
                                    (NSArray<RecipientInfoForIOSDisplay*>*)
                                        recipients {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _recipients = recipients;
  }
  return self;
}

- (void)start {
  [super start];

  self.viewController =
      [[FamilyPickerViewController alloc] initWithStyle:ChromeTableViewStyle()];
  self.viewController.delegate = self;
  self.mediator = [[FamilyPickerMediator alloc] initWithRecipients:_recipients];
  self.mediator.consumer = self.viewController;
  self.navigationController =
      [[TableViewNavigationController alloc] initWithTable:self.viewController];
  [self.navigationController
      setModalPresentationStyle:UIModalPresentationFormSheet];
  self.navigationController.navigationBar.prefersLargeTitles = NO;

  UISheetPresentationController* sheetPresentationController =
      self.navigationController.sheetPresentationController;
  if (sheetPresentationController) {
    sheetPresentationController.detents =
        @[ [UISheetPresentationControllerDetent mediumDetent] ];
  }

  [self.baseViewController presentViewController:self.navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.viewController = nil;
  self.mediator = nil;
}

#pragma mark - FamilyPickerViewControllerPresentationDelegate

- (void)familyPickerWasDismissed:(FamilyPickerViewController*)controller {
  [self.delegate familyPickerCoordinatorWasDismissed:self];
}

@end
