// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/signin/fake_chrome_trusted_vault_service.h"

#import <UIKit/UIKit.h>

#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FakeTrustedVaultViewController : UIViewController

// Completion to call once the view controller is dismiss.
@property(nonatomic, copy) void (^completion)(BOOL success, NSError* error);

@end

@implementation FakeTrustedVaultViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = UIColor.orangeColor;
}

- (void)simulateUserCancel {
  __weak __typeof(self) weakSelf = self;
  [self.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:^() {
                           if (weakSelf.completion) {
                             weakSelf.completion(NO, nil);
                           }
                         }];
}

@end

namespace ios {

FakeChromeTrustedVaultService::FakeChromeTrustedVaultService()
    : trusted_vault_view_controller_(nil) {}

FakeChromeTrustedVaultService::~FakeChromeTrustedVaultService() {}

// static
FakeChromeTrustedVaultService*
FakeChromeTrustedVaultService::GetInstanceFromChromeProvider() {
  return static_cast<ios::FakeChromeTrustedVaultService*>(
      ios::GetChromeBrowserProvider().GetChromeTrustedVaultService());
}

void FakeChromeTrustedVaultService::FetchKeys(
    ChromeIdentity* chrome_identity,
    base::OnceCallback<void(const TrustedVaultSharedKeyList&)> callback) {}

void FakeChromeTrustedVaultService::MarkLocalKeysAsStale(
    ChromeIdentity* chrome_identity,
    base::OnceClosure callback) {}

void FakeChromeTrustedVaultService::GetDegradedRecoverabilityStatus(
    ChromeIdentity* chrome_identity,
    base::OnceCallback<void(bool)> callback) {}

void FakeChromeTrustedVaultService::FixDegradedRecoverability(
    ChromeIdentity* chrome_identity,
    UIViewController* presentingViewController,
    void (^callback)(BOOL success, NSError* error)) {}

void FakeChromeTrustedVaultService::Reauthentication(
    ChromeIdentity* chrome_identity,
    UIViewController* presenting_view_controller,
    void (^callback)(BOOL success, NSError* error)) {
  DCHECK(!trusted_vault_view_controller_);
  trusted_vault_view_controller_ =
      [[FakeTrustedVaultViewController alloc] init];
  trusted_vault_view_controller_.completion = callback;
  [presenting_view_controller
      presentViewController:trusted_vault_view_controller_
                   animated:YES
                 completion:nil];
}

void FakeChromeTrustedVaultService::CancelDialog(BOOL animated,
                                                 void (^callback)(void)) {
  DCHECK(trusted_vault_view_controller_);
  [trusted_vault_view_controller_.presentingViewController
      dismissViewControllerAnimated:animated
                         completion:callback];
  trusted_vault_view_controller_ = nil;
}

void FakeChromeTrustedVaultService::SimulateUserCancel() {
  DCHECK(trusted_vault_view_controller_);
  [trusted_vault_view_controller_ simulateUserCancel];
  trusted_vault_view_controller_ = nil;
}

}  // namespace ios
