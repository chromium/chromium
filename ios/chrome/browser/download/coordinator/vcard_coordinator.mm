// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/coordinator/vcard_coordinator.h"

#import <ContactsUI/ContactsUI.h>

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/download/coordinator/vcard_mediator.h"
#import "ios/chrome/browser/download/coordinator/vcard_mediator_delegate.h"
#import "ios/chrome/browser/download/model/vcard_tab_helper.h"
#import "ios/chrome/browser/download/model/vcard_tab_helper_delegate.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/tabs/model/tabs_dependency_installer_bridge.h"

@interface VcardCoordinator () <TabsDependencyInstalling,
                                VcardMediatorDelegate,
                                VcardTabHelperDelegate>
@end

@implementation VcardCoordinator {
  // Bridge which observes WebStateList and alerts this coordinator when this
  // needs to register with a new WebState.
  TabsDependencyInstallerBridge _dependencyInstallerBridge;
  // Mediator that monitors the active WebState and dismisses vCard on
  // navigation.
  VcardMediator* _mediator;
  // NavigationController that contains a viewController used to display a
  // contact.
  UINavigationController* _navigationViewController;
  // The WebState for which `_navigationViewController` is currently presented.
  raw_ptr<web::WebState> _presentedWebState;
}

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser {
  if ((self = [super initWithBaseViewController:baseViewController
                                        browser:browser])) {
    _presentedWebState = nullptr;
    _dependencyInstallerBridge.StartObserving(self, browser);
    _mediator =
        [[VcardMediator alloc] initWithWebStateList:browser->GetWebStateList()];
    _mediator.delegate = self;
  }
  return self;
}

- (void)stop {
  // Stop observing the WebStateList before destroying the bridge object.
  _dependencyInstallerBridge.StopObserving();

  [_mediator disconnect];
  _mediator = nil;

  if (_navigationViewController) {
    [_navigationViewController.presentingViewController
        dismissViewControllerAnimated:NO
                           completion:nil];
    _navigationViewController = nil;
    _presentedWebState = nullptr;
  }
}

#pragma mark - TabsDependencyInstalling methods

- (void)webStateInserted:(web::WebState*)webState {
  VcardTabHelper* tabHelper = VcardTabHelper::FromWebState(webState);
  if (tabHelper) {
    tabHelper->set_delegate(self);
  }
}

- (void)webStateRemoved:(web::WebState*)webState {
  VcardTabHelper* tabHelper = VcardTabHelper::FromWebState(webState);
  if (tabHelper) {
    tabHelper->set_delegate(nil);
  }
}

- (void)webStateDeleted:(web::WebState*)webState {
  // Nothing to do.
}

- (void)newWebStateActivated:(web::WebState*)newActive
           oldActiveWebState:(web::WebState*)oldActive {
  // Nothing to do.
}

#pragma mark - VcardMediatorDelegate

- (void)dismissVcardForWebState:(web::WebState*)webState {
  if (_navigationViewController && _presentedWebState == webState) {
    [_navigationViewController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:nil];
    _navigationViewController = nil;
    _presentedWebState = nullptr;
  }
}

#pragma mark - VcardTabHelperDelegate

- (void)openVcardFromData:(NSData*)vcardData {
  DCHECK(vcardData);
  [self presentContactVCardFromData:vcardData];
}

#pragma mark - Private

// Dismisses the `_navigationViewController`.
- (void)dismissButtonTapped {
  [_navigationViewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  _navigationViewController = nil;
  _presentedWebState = nullptr;
}

// Retrieves contact information from `data` and presents it.
- (void)presentContactVCardFromData:(NSData*)vcardData {
  // TODO(crbug.com/40208267): Vcard download code only supports the first
  // contact.
  CNContact* contact =
      [[CNContactVCardSerialization contactsWithData:vcardData
                                               error:nil] firstObject];
  if (!contact) {
    return;
  }

  CNContactViewController* contactViewController =
      [CNContactViewController viewControllerForUnknownContact:contact];

  contactViewController.allowsEditing = YES;
  contactViewController.contactStore = [[CNContactStore alloc] init];

  UIBarButtonItem* dismissButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(dismissButtonTapped)];
  contactViewController.navigationItem.leftBarButtonItem = dismissButton;

  _navigationViewController = [[UINavigationController alloc]
      initWithRootViewController:contactViewController];
  _presentedWebState = self.browser->GetWebStateList()->GetActiveWebState();
  [self.baseViewController presentViewController:_navigationViewController
                                        animated:YES
                                      completion:nil];
}

@end
