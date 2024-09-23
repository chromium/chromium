// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/providers/signin/fake_trusted_vault_client_backend.h"

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "base/functional/callback.h"

using CompletionBlock = TrustedVaultClientBackend::CompletionBlock;

@interface FakeTrustedVaultClientBackendViewController : UIViewController

// Completion to call once the view controller is dismiss.
@property(nonatomic, copy) CompletionBlock completion;

- (instancetype)initWithCompletion:(CompletionBlock)completion
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// Simulate user cancelling the reauth dialog.
- (void)simulateUserCancel;

@end

@implementation FakeTrustedVaultClientBackendViewController

- (instancetype)initWithCompletion:(CompletionBlock)completion {
  if ((self = [super initWithNibName:nil bundle:nil])) {
    _completion = completion;
  }
  return self;
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

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = UIColor.orangeColor;
}

@end

FakeTrustedVaultClientBackend::FakeTrustedVaultClientBackend() = default;

FakeTrustedVaultClientBackend::~FakeTrustedVaultClientBackend() = default;

void FakeTrustedVaultClientBackend::
    SetDeviceRegistrationPublicKeyVerifierForUMA(VerifierCallback verifier) {
  // Do nothing.
}

void FakeTrustedVaultClientBackend::FetchKeys(
    id<SystemIdentity> identity,
    trusted_vault::SecurityDomainId security_domain_id,
    KeyFetchedCallback completion) {
  // Do nothing.
}

void FakeTrustedVaultClientBackend::MarkLocalKeysAsStale(
    id<SystemIdentity> identity,
    trusted_vault::SecurityDomainId security_domain_id,
    base::OnceClosure completion) {
  // Do nothing.
}

void FakeTrustedVaultClientBackend::GetDegradedRecoverabilityStatus(
    id<SystemIdentity> identity,
    trusted_vault::SecurityDomainId security_domain_id,
    base::OnceCallback<void(bool)> completion) {
  // Do nothing.
}

FakeTrustedVaultClientBackend::CancelDialogCallback
FakeTrustedVaultClientBackend::Reauthentication(
    id<SystemIdentity> identity,
    trusted_vault::SecurityDomainId security_domain_id,
    UIViewController* presenting_view_controller,
    CompletionBlock completion) {
  DCHECK(!view_controller_);
  view_controller_ = [[FakeTrustedVaultClientBackendViewController alloc]
      initWithCompletion:completion];
  [presenting_view_controller presentViewController:view_controller_
                                           animated:YES
                                         completion:nil];
  base::WeakPtr<FakeTrustedVaultClientBackend> weak_ptr =
      weak_ptr_factory_.GetWeakPtr();
  return base::BindOnce(
      [](base::WeakPtr<FakeTrustedVaultClientBackend> weak_ptr, bool animated,
         ProceduralBlock cancel_done_callback) {
        weak_ptr->InternalCancelDialog(animated, cancel_done_callback);
      },
      weak_ptr);
}

FakeTrustedVaultClientBackend::CancelDialogCallback
FakeTrustedVaultClientBackend::FixDegradedRecoverability(
    id<SystemIdentity> identity,
    trusted_vault::SecurityDomainId security_domain_id,
    UIViewController* presenting_view_controller,
    CompletionBlock completion) {
  // Do nothing.
  return base::BindOnce(
      [](bool animated, ProceduralBlock cancel_done_callback) {});
}

void FakeTrustedVaultClientBackend::InternalCancelDialog(
    BOOL animated,
    ProceduralBlock completion) {
  DCHECK(view_controller_);
  [view_controller_.presentingViewController
      dismissViewControllerAnimated:animated
                         completion:completion];
  view_controller_ = nil;
}

void FakeTrustedVaultClientBackend::ClearLocalData(
    id<SystemIdentity> identity,
    trusted_vault::SecurityDomainId security_domain_id,
    base::OnceCallback<void(bool)> completion) {
  // Do nothing.
}

void FakeTrustedVaultClientBackend::GetPublicKeyForIdentity(
    id<SystemIdentity> identity,
    GetPublicKeyCallback completion) {
  // Do nothing.
}

void FakeTrustedVaultClientBackend::SimulateUserCancel() {
  DCHECK(view_controller_);
  [view_controller_ simulateUserCancel];
  view_controller_ = nil;
}
