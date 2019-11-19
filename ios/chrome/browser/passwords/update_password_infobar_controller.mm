// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/update_password_infobar_controller.h"

#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/infobars/confirm_infobar_controller+protected.h"
#import "ios/chrome/browser/infobars/infobar_controller+protected.h"
#include "ios/chrome/browser/passwords/ios_chrome_update_password_infobar_delegate.h"
#import "ios/chrome/browser/ui/elements/selector_coordinator.h"
#import "ios/chrome/browser/ui/infobars/confirm_infobar_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface UpdatePasswordInfoBarController ()<SelectorCoordinatorDelegate> {
  IOSChromeUpdatePasswordInfoBarDelegate* _delegate;
}

// The base view controller from which to present UI.
@property(nonatomic, readwrite, weak) UIViewController* baseViewController;

@property(nonatomic, strong) SelectorCoordinator* selectorCoordinator;

// Action for any of the user defined links.
- (void)infobarLinkDidPress:(NSUInteger)tag;

@end

@implementation UpdatePasswordInfoBarController

@synthesize baseViewController = _baseViewController;
@synthesize selectorCoordinator = _selectorCoordinator;

- (instancetype)
initWithBaseViewController:(UIViewController*)baseViewController
           infoBarDelegate:(IOSChromeUpdatePasswordInfoBarDelegate*)delegate {
  self = [super initWithInfoBarDelegate:delegate];
  if (self) {
    _baseViewController = baseViewController;
    _delegate = delegate;
  }
  return self;
}

- (void)infobarLinkDidPress:(NSUInteger)tag {
  DCHECK(self.baseViewController);
  self.selectorCoordinator = [[SelectorCoordinator alloc]
      initWithBaseViewController:self.baseViewController];
  self.selectorCoordinator.delegate = self;
  self.selectorCoordinator.options =
      [NSOrderedSet orderedSetWithArray:_delegate->GetAccounts()];
  self.selectorCoordinator.defaultOption =
      base::SysUTF16ToNSString(_delegate->selected_account());
  [self.selectorCoordinator start];
}

#pragma mark SelectorCoordinatorDelegate

- (void)selectorCoordinator:(SelectorCoordinator*)coordinator
    didCompleteWithSelection:(NSString*)selection {
  _delegate->set_selected_account(base::SysNSStringToUTF16(selection));
  [self updateInfobarLabel:self.view];
}

@end
