// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_interaction_manager.h"

#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_interaction_manager.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_interaction_manager_constants.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#include "ios/public/provider/chrome/browser/signin/signin_error_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FakeAddAccountViewController : UIViewController {
  __weak FakeChromeIdentityInteractionManager* _manager;
  UIButton* _cancelButton;
  UIButton* _signInButton;
}
@end

@interface FakeChromeIdentityInteractionManager () {
  SigninCompletionCallback _completionCallback;
  UIViewController* _viewController;
}

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

@implementation FakeChromeIdentityInteractionManager

static ChromeIdentity* _identity = nil;

+ (void)setIdentity:(ChromeIdentity*)identity {
  _identity = identity;
}

+ (ChromeIdentity*)identity {
  return _identity;
}

- (void)addAccountWithPresentingViewController:(UIViewController*)viewController
                                    completion:
                                        (SigninCompletionCallback)completion {
  _completionCallback = completion;
  _viewController =
      [[FakeAddAccountViewController alloc] initWithInteractionManager:self];
  [self.delegate interactionManager:self
              presentViewController:_viewController
                           animated:YES
                         completion:nil];
}

- (void)addAccountWithPresentingViewController:(UIViewController*)viewController
                                     userEmail:(NSString*)userEmail
                                    completion:
                                        (SigninCompletionCallback)completion {
  [self addAccountWithPresentingViewController:viewController
                                    completion:completion];
}

- (void)cancelAddAccountWithAnimation:(BOOL)animated
                           completion:(void (^)(void))completion {
  [self dismissAndRunCompletionCallbackWithError:[self canceledError]
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
  [self dismissAndRunCompletionCallbackWithError:[self canceledError]
                                        animated:YES
                                      completion:nil];
}

- (void)addAccountViewControllerDidThrowUnhandledError {
  [self dismissAndRunCompletionCallbackWithError:[self unhandledError]
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
                                      completion:(void (^)(void))completion {
  if (!_viewController) {
    [self runCompletionCallbackWithError:error];
    return;
  }
  [self.delegate interactionManager:self
      dismissViewControllerAnimated:animated
                         completion:^{
                           [self runCompletionCallbackWithError:error];
                           if (completion) {
                             completion();
                           }
                         }];
}

- (void)runCompletionCallbackWithError:(NSError*)error {
  _viewController = nil;
  if (_completionCallback) {
    // Ensure self is not destroyed in the callback.
    NS_VALID_UNTIL_END_OF_SCOPE FakeChromeIdentityInteractionManager*
        strongSelf = self;
    _completionCallback(FakeChromeIdentityInteractionManager.identity, error);
    _completionCallback = nil;
  }
}

- (NSError*)canceledError {
  ios::SigninErrorProvider* provider =
      ios::GetChromeBrowserProvider().GetSigninErrorProvider();
  return [NSError errorWithDomain:provider->GetSigninErrorDomain()
                             code:provider->GetCode(ios::SigninError::CANCELED)
                         userInfo:nil];
}

- (NSError*)unhandledError {
  return [NSError errorWithDomain:@"" code:-1 userInfo:nil];
}

@end
