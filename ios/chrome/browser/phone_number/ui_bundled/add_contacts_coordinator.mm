// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/phone_number/ui_bundled/add_contacts_coordinator.h"

#import <Contacts/Contacts.h>
#import <ContactsUI/ContactsUI.h>

#import "ios/chrome/browser/shared/model/browser/browser.h"

@interface AddContactsCoordinator () <CNContactViewControllerDelegate>
@end

@implementation AddContactsCoordinator {
  // The view controller managed by this coordinator.
  CNContactViewController* _viewController;

  // The phone number used to create a contact.
  NSString* _phoneNumber;
}

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                               phoneNumber:(NSString*)phoneNumber {
  self = [super initWithBaseViewController:baseViewController browser:browser];
  if (self) {
    _phoneNumber = phoneNumber;
  }
  return self;
}

- (void)start {
  CNContactStore* store = [[CNContactStore alloc] init];
  CNMutableContact* contact = [[CNMutableContact alloc] init];
  CNPhoneNumber* number =
      [[CNPhoneNumber alloc] initWithStringValue:_phoneNumber];
  CNLabeledValue* labelValue =
      [[CNLabeledValue alloc] initWithLabel:CNLabelPhoneNumberMain
                                      value:number];
  contact.phoneNumbers = @[ labelValue ];

  _viewController =
      [CNContactViewController viewControllerForUnknownContact:contact];
  _viewController.delegate = self;
  _viewController.allowsEditing = YES;
  _viewController.allowsActions = YES;
  _viewController.contactStore = store;

  // The cancel button on the right side of the nav bar.
  UIBarButtonItem* cancelButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(stop)];
  _viewController.navigationItem.rightBarButtonItem = cancelButtonItem;
  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_viewController.presentingViewController dismissViewControllerAnimated:YES
                                                               completion:nil];
  _viewController.delegate = nil;
  _viewController = nil;
}

#pragma mark - CNContactViewControllerDelegate

- (void)contactViewController:(CNContactViewController*)viewController
       didCompleteWithContact:(CNContact*)contact {
  [self stop];
}

@end
