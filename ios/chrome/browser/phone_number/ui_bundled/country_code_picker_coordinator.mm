// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/phone_number/ui_bundled/country_code_picker_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/add_contacts_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/phone_number/ui_bundled/country_code_picker_view_controller.h"

@implementation CountryCodePickerCoordinator {
  CountryCodePickerViewController* _viewController;
}

- (void)start {
  _viewController = [[CountryCodePickerViewController alloc]
      initWithPhoneNumber:self.phoneNumber];
  _viewController.addContactsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), AddContactsCommands);
  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];

  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_viewController.presentingViewController dismissViewControllerAnimated:YES
                                                               completion:nil];
  _viewController = nil;
}

@end
