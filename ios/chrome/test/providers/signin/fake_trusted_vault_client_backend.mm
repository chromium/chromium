// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/providers/signin/fake_trusted_vault_client_backend.h"

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "base/functional/callback.h"

namespace {

using CompletionBlock = TrustedVaultClientBackend::CompletionBlock;

// Domain for fake trusted vault client backend errors.
NSString* const kFakeTrustedVaultClientBackendErrorDomain =
    @"FakeTrustedVaultClientBackendErrorDomain";

}  // namespace

@interface FakeTrustedVaultClientBackendViewController : UIViewController

// Completion to call once the view controller is dismissed.
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
    KeysFetchedCallback completion) {
  // Return the keys for passkeys domain, so the `UpdateGPMPinForAccount` can be
  // tested.
  if (security_domain_id == trusted_vault::SecurityDomainId::kPasskeys) {
    std::move(completion).Run({{1, 2, 3}});
  }

  // Otherwise do nothing.
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
  // Return the non-degraded status for passkeys domain, so the
  // `UpdateGPMPinForAccount` can be tested.
  if (security_domain_id == trusted_vault::SecurityDomainId::kPasskeys) {
    std::move(completion).Run(false);
  }

  // Otherwise do nothing.
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

void FakeTrustedVaultClientBackend::UpdateGPMPinForAccount(
    id<SystemIdentity> identity,
    trusted_vault::SecurityDomainId security_domain_id,
    UINavigationController* navigationController,
    UIView* brandedNavigationItemTitleView,
    UpdateGPMPinCompletionCallback completion) {
  CHECK_EQ(security_domain_id, trusted_vault::SecurityDomainId::kPasskeys);

  // Since the real update view controller cannot be displayed, return an error.
  // This should be handled on the caller side and can be tested.
  // TODO(crbug.com/358342483): Add method to set what kind of error should be
  // returned. Same for FetchKeys() and GetDegradedRecoverabilityStatus().
  std::move(completion)
      .Run([NSError errorWithDomain:kFakeTrustedVaultClientBackendErrorDomain
                               code:1
                           userInfo:nil]);
}

void FakeTrustedVaultClientBackend::SimulateUserCancel() {
  DCHECK(view_controller_);
  [view_controller_ simulateUserCancel];
  view_controller_ = nil;
}
