// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/ui_bundled/vcard_coordinator.h"

#import <ContactsUI/ContactsUI.h>

#import "base/scoped_observation.h"
#import "ios/chrome/browser/download/model/vcard_tab_helper.h"
#import "ios/chrome/browser/download/model/vcard_tab_helper_delegate.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/web_state_list/model/web_state_dependency_installer_bridge.h"
#import "ios/web/public/web_state_observer_bridge.h"

@interface VcardCoordinator () <DependencyInstalling, VcardTabHelperDelegate> {
  // Bridge which observes WebStateList and alerts this coordinator when this
  // needs to register the Mediator with a new WebState.
  std::unique_ptr<WebStateDependencyInstallerBridge> _dependencyInstallerBridge;
}

// NavigationController that contains a viewController used to display a
// contact.
@property(nonatomic, strong) UINavigationController* navigationViewController;

@end

@implementation VcardCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser {
  if ((self = [super initWithBaseViewController:baseViewController
                                        browser:browser])) {
    _dependencyInstallerBridge =
        std::make_unique<WebStateDependencyInstallerBridge>(
            self, browser->GetWebStateList());
  }
  return self;
}

- (void)stop {
  // Reset this observer manually. We want this to go out of scope now, to
  // ensure it detaches before `browser` and its WebStateList get destroyed.
  _dependencyInstallerBridge.reset();

  self.navigationViewController = nil;
}

#pragma mark - DependencyInstalling methods

- (void)installDependencyForWebState:(web::WebState*)webState {
  VcardTabHelper::FromWebState(webState)->set_delegate(self);
}

- (void)uninstallDependencyForWebState:(web::WebState*)webState {
  VcardTabHelper::FromWebState(webState)->set_delegate(nil);
}

#pragma mark - Private

// Dismisses the the `navigationViewController`.
- (void)dismissButtonTapped {
  [self.baseViewController dismissViewControllerAnimated:true completion:nil];
}

// Retreives contact informations from `data` and presents it.
- (void)presentContactVCardFromData:(NSData*)vcardData {
  // TODO(crbug.com/40208267): Vcard download code only support the first
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

  self.navigationViewController = [[UINavigationController alloc]
      initWithRootViewController:contactViewController];
  [self.baseViewController presentViewController:self.navigationViewController
                                        animated:YES
                                      completion:nil];
}

#pragma mark - VcardTabHelperDelegate

- (void)openVcardFromData:(NSData*)vcardData {
  DCHECK(vcardData);
  [self presentContactVCardFromData:vcardData];
}

@end
