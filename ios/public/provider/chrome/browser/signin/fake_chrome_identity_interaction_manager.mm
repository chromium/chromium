// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_interaction_manager.h"

#import "ios/chrome/browser/signin/system_identity.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_interaction_manager.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_interaction_manager_constants.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#import "ios/public/provider/chrome/browser/signin/signin_error_api.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FakeAddAccountViewController : UIViewController {
  __weak FakeChromeIdentityInteractionManager* _manager;
  UIButton* _cancelButton;
  UIButton* _signInButton;
}
@end

@interface FakeChromeIdentityInteractionManager ()

@property(nonatomic, strong) UIViewController* addAccountViewController;
@property(nonatomic, copy) SigninCompletionCallback completionCallback;
@property(nonatomic, assign, readwrite) BOOL viewControllerPresented;

@end

@implementation FakeAddAccountViewController

- (instancetype)initWithInteractionManager:
    (FakeChromeIdentityInteractionManager*)manager {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _manager = manager;
  }
  return self;
}

- (void)dealloc {
  [_signInButton removeTarget:self
                       action:@selector(didTapSignIn:)
             forControlEvents:UIControlEventTouchUpInside];
  [_cancelButton removeTarget:self
                       action:@selector(didTapCancel:)
             forControlEvents:UIControlEventTouchUpInside];
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // Obnoxious color, this is a test screen.
  self.view.backgroundColor = [UIColor magentaColor];
  self.view.accessibilityIdentifier = kFakeAddAccountViewIdentifier;

  _signInButton = [UIButton buttonWithType:UIButtonTypeCustom];
  [_signInButton setTitle:@"Sign in" forState:UIControlStateNormal];
  [_signInButton addTarget:self
                    action:@selector(didTapSignIn:)
          forControlEvents:UIControlEventTouchUpInside];
  [self.view addSubview:_signInButton];

  _cancelButton = [UIButton buttonWithType:UIButtonTypeCustom];
  [_cancelButton setTitle:@"Cancel" forState:UIControlStateNormal];
  [_cancelButton setAccessibilityIdentifier:@"cancel"];
  [_cancelButton addTarget:self
                    action:@selector(didTapCancel:)
          forControlEvents:UIControlEventTouchUpInside];
  [self.view addSubview:_cancelButton];
}

- (void)viewWillLayoutSubviews {
  [super viewWillLayoutSubviews];

  CGRect bounds = self.view.bounds;
  [_signInButton sizeToFit];
  [_signInButton
      setCenter:CGPointMake(CGRectGetMidX(bounds), CGRectGetMidY(bounds) - 50)];
  [_cancelButton sizeToFit];
  [_cancelButton
      setCenter:CGPointMake(CGRectGetMidX(bounds), CGRectGetMidY(bounds) + 50)];
}

- (void)didTapSignIn:(id)sender {
  [_manager addAccountViewControllerDidTapSignIn];
}

- (void)didTapCancel:(id)sender {
  [_manager addAccountViewControllerDidTapCancel];
}

@end

namespace {

id<SystemIdentity> gFakeChromeIdentityInteractionManagerIdentity = nil;

}

@implementation FakeChromeIdentityInteractionManager

+ (void)setIdentity:(id<SystemIdentity>)identity {
  gFakeChromeIdentityInteractionManagerIdentity = identity;
}

+ (id<SystemIdentity>)identity {
  return gFakeChromeIdentityInteractionManagerIdentity;
}

- (void)addAccountWithPresentingViewController:(UIViewController*)viewController
                                     userEmail:(NSString*)userEmail
                                    completion:
                                        (SigninCompletionCallback)completion {
  self.completionCallback = completion;
  self.addAccountViewController =
      [[FakeAddAccountViewController alloc] initWithInteractionManager:self];
  __weak __typeof(self) weakSelf = self;
  [viewController presentViewController:self.addAccountViewController
                               animated:YES
                             completion:^() {
                               weakSelf.viewControllerPresented = YES;
                             }];
}

- (void)cancelAddAccountAnimated:(BOOL)animated
                      completion:(void (^)(void))completion {
  NSError* error = ios::provider::CreateUserCancelledSigninError();
  [self dismissAndRunCompletionCallbackWithError:error
                                        animated:animated
                                      completion:completion];
}

- (void)addAccountViewControllerDidTapSignIn {
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      FakeChromeIdentityInteractionManager.identity);
  [self dismissAndRunCompletionCallbackWithError:nil
                                        animated:YES
                                      completion:nil];
}

- (void)addAccountViewControllerDidTapCancel {
  NSError* error = ios::provider::CreateUserCancelledSigninError();
  [self dismissAndRunCompletionCallbackWithError:error
                                        animated:YES
                                      completion:nil];
}

- (void)addAccountViewControllerDidThrowUnhandledError {
  NSError* error = [NSError errorWithDomain:@"Unhandled" code:-1 userInfo:nil];
  [self dismissAndRunCompletionCallbackWithError:error
                                        animated:YES
                                      completion:nil];
}

- (void)addAccountViewControllerDidInterrupt {
  // When the add account view is interrupted, its completion callback is
  // invoked with the user cancel error.
  [self addAccountViewControllerDidTapCancel];
}

#pragma mark Helper

- (void)dismissAndRunCompletionCallbackWithError:(NSError*)error
                                        animated:(BOOL)animated
                                      completion:(ProceduralBlock)completion {
  DCHECK(error || FakeChromeIdentityInteractionManager.identity)
      << "An identity should be set to close the dialog successfully, error: "
      << error << ", identity "
      << FakeChromeIdentityInteractionManager.identity;
  DCHECK(self.addAccountViewController);
  DCHECK(self.viewControllerPresented);
  __weak __typeof(self) weakSelf = self;
  [self.addAccountViewController.presentingViewController
      dismissViewControllerAnimated:animated
                         completion:^{
                           __strong __typeof(self) strongSelf = weakSelf;
                           [strongSelf
                               runCompletionCallbackWithError:error
                                                   completion:completion];
                         }];
}

- (void)runCompletionCallbackWithError:(NSError*)error
                            completion:(ProceduralBlock)completion {
  self.addAccountViewController = nil;
  id<SystemIdentity> identity =
      error ? nil : FakeChromeIdentityInteractionManager.identity;
  // Reset the identity for the next usage.
  FakeChromeIdentityInteractionManager.identity = nil;
  if (self.completionCallback) {
    SigninCompletionCallback completionCallback = self.completionCallback;
    self.completionCallback = nil;
    completionCallback(identity, error);
  }
  if (completion) {
    completion();
  }
  self.viewControllerPresented = NO;
}

@end
