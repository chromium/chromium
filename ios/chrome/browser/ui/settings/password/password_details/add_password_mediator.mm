// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/add_password_mediator.h"

#include "base/strings/sys_string_conversions.h"
#include "components/password_manager/core/browser/form_parsing/form_parser.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "ios/chrome/browser/passwords/password_check_observer_bridge.h"
#import "ios/chrome/browser/ui/settings/password/password_details/add_password_mediator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_controller_delegate.h"
#include "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::SysNSStringToUTF16;
using base::SysUTF8ToNSString;

@interface AddPasswordMediator () <PasswordDetailsTableViewControllerDelegate> {
  // Password Check manager.
  IOSChromePasswordCheckManager* _manager;
}

// Caches the password form data submitted by the user. This value is set only
// when the user tries to save a credential which has username and site similar
// to an existing credential.
@property(nonatomic, readonly) absl::optional<password_manager::PasswordForm>
    cachedPasswordForm;

// Delegate for this mediator.
@property(nonatomic, weak) id<AddPasswordMediatorDelegate> delegate;

@end

@implementation AddPasswordMediator

- (instancetype)initWithDelegate:(id<AddPasswordMediatorDelegate>)delegate
            passwordCheckManager:(IOSChromePasswordCheckManager*)manager {
  self = [super init];
  if (self) {
    _delegate = delegate;
    _manager = manager;
  }
  return self;
}

#pragma mark - PasswordDetailsTableViewControllerDelegate

- (void)passwordDetailsViewController:
            (PasswordDetailsTableViewController*)viewController
               didEditPasswordDetails:(PasswordDetails*)password {
  NOTREACHED();
}

- (void)passwordDetailsViewController:
            (PasswordDetailsTableViewController*)viewController
        didAddPasswordDetailsWithSite:(NSString*)website
                             username:(NSString*)username
                             password:(NSString*)password {
  password_manager::PasswordForm passwordForm;
  GURL gurl = net::GURLWithNSURL([NSURL URLWithString:website]);
  DCHECK(gurl.is_valid());

  passwordForm.url = password_manager_util::StripAuthAndParams(gurl);
  passwordForm.signon_realm =
      password_manager::GetSignonRealm(passwordForm.url);
  passwordForm.username_value = SysNSStringToUTF16(username);
  passwordForm.password_value = SysNSStringToUTF16(password);
  passwordForm.in_store = password_manager::PasswordForm::Store::kProfileStore;

  for (const auto& form : _manager->GetAllCredentials()) {
    if (form.signon_realm == passwordForm.signon_realm &&
        form.username_value == passwordForm.username_value) {
      _cachedPasswordForm = passwordForm;
      [self.delegate
          showReplacePasswordAlert:username
                           hostUrl:SysUTF8ToNSString(passwordForm.url.host())];
      return;
    }
  }

  _manager->AddPasswordForm(passwordForm);
  [self.delegate dismissPasswordDetailsTableViewController];
}

- (void)didCancelAddPasswordDetails {
  [self.delegate dismissPasswordDetailsTableViewController];
}

- (void)didConfirmReplaceExistingCredential {
  DCHECK(self.cachedPasswordForm);
  for (const auto& form : _manager->GetAllCredentials()) {
    if (form.signon_realm == self.cachedPasswordForm->signon_realm &&
        form.username_value == self.cachedPasswordForm->username_value) {
      _manager->EditPasswordForm(form, self.cachedPasswordForm->username_value,
                                 self.cachedPasswordForm->password_value);
      break;
    }
  }
  [self.delegate dismissPasswordDetailsTableViewController];
}

- (BOOL)isUsernameReused:(NSString*)newUsername {
  return NO;
}

@end
