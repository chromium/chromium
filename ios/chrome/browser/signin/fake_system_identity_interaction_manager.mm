// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/fake_system_identity_interaction_manager.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/signin/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/test_constants.h"
#import "ios/public/provider/chrome/browser/signin/signin_error_api.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Global used to store the +identity of FakeSystemIdentityInteractionManager.
id<SystemIdentity> gFakeSystemIdentityInteractionManagerIdentity = nil;

}  // namespace

@interface FakeAuthActivityViewController : UIViewController {
  __weak FakeSystemIdentityInteractionManager* _manager;
  UIButton* _cancelButton;
}

- (instancetype)initWithManager:(FakeSystemIdentityInteractionManager*)manager
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

@end

@implementation FakeAuthActivityViewController

- (instancetype)initWithManager:(FakeSystemIdentityInteractionManager*)manager {
  if ((self = [super initWithNibName:nil bundle:nil])) {
    _manager = manager;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  // Obnovious color, this is a test screen.
  UIView* mainView = self.view;
  mainView.backgroundColor = [UIColor magentaColor];
  mainView.accessibilityIdentifier = kFakeAuthActivityViewIdentifier;

  _cancelButton = [self addButtonWithTitle:@"Cancel"
                                    action:@selector(didTapCancel:)
                    accessibilitIdentifier:kFakeAuthCancelButtonIdentifier];
}

- (void)viewWillLayoutSubviews {
  [super viewWillLayoutSubviews];

  const CGRect bounds = self.view.bounds;
  const CGFloat midX = CGRectGetMidX(bounds);
  const CGFloat midY = CGRectGetMidY(bounds);

  [self sizeButtonToFitWithCenter:CGPointMake(midX, midY) button:_cancelButton];
}

#pragma mark - Private methods

- (UIButton*)addButtonWithTitle:(NSString*)title
                         action:(SEL)action
         accessibilitIdentifier:(NSString*)accessibilityIdentitier {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeCustom];
  [button setTitle:title forState:UIControlStateNormal];
  [button setAccessibilityIdentifier:accessibilityIdentitier];
  [button addTarget:self
                action:action
      forControlEvents:UIControlEventTouchUpInside];
  [self.view addSubview:button];
  return button;
}

- (void)sizeButtonToFitWithCenter:(CGPoint)center button:(UIButton*)button {
  [button setCenter:center];
  [button sizeToFit];
}

- (void)didTapCancel:(id)sender {
  [_manager simulateDidTapCancel];
}

@end

@implementation FakeSystemIdentityInteractionManager {
  base::WeakPtr<FakeSystemIdentityManager> _manager;
  SigninCompletionBlock _signinCompletion;
  FakeAuthActivityViewController* _authActivityViewController;
  BOOL _isActivityViewPresented;
}

- (instancetype)initWithManager:
    (base::WeakPtr<FakeSystemIdentityManager>)manager {
  if ((self = [super init])) {
    _manager = manager;
  }
  return self;
}

- (void)simulateDidTapAddAccount {
  id<SystemIdentity> identity = nil;
  std::swap(gFakeSystemIdentityInteractionManagerIdentity, identity);

  [self dismissAndRunCompletionCallbackWithError:nil
                                        identity:identity
                                        animated:YES
                                      completion:nil];
}

- (void)simulateDidTapCancel {
  [self cancelAuthActivityAnimated:YES completion:nil];
}

- (void)simulateDidThrowUnhandledError {
  NSError* error = [NSError errorWithDomain:@"Unhandled" code:-1 userInfo:nil];
  [self dismissAndRunCompletionCallbackWithError:error
                                        identity:nil
                                        animated:YES
                                      completion:nil];
}

- (void)simulateDidInterrupt {
  // When the auth activity view is interrupted, its completion callback is
  // invoked with the user cancel error.
  [self simulateDidTapCancel];
}

#pragma mark - SystemIdentityInteractionManager

- (void)startAuthActivityWithViewController:(UIViewController*)viewController
                                  userEmail:(NSString*)userEmail
                                 completion:(SigninCompletionBlock)completion {
  DCHECK(completion);
  _signinCompletion = completion;
  _authActivityViewController =
      [[FakeAuthActivityViewController alloc] initWithManager:self];

  __weak FakeSystemIdentityInteractionManager* weakSelf = self;
  [viewController presentViewController:_authActivityViewController
                               animated:YES
                             completion:^{
                               [weakSelf onActivityViewPresented];
                             }];
}

- (void)cancelAuthActivityAnimated:(BOOL)animated
                        completion:(ProceduralBlock)completion {
  NSError* error = ios::provider::CreateUserCancelledSigninError();
  [self dismissAndRunCompletionCallbackWithError:error
                                        identity:nil
                                        animated:animated
                                      completion:completion];
}

#pragma mark - Properties

- (BOOL)isActivityViewPresented {
  return _isActivityViewPresented;
}

+ (id<SystemIdentity>)identity {
  return gFakeSystemIdentityInteractionManagerIdentity;
}

+ (void)setIdentity:(id<SystemIdentity>)identity {
  gFakeSystemIdentityInteractionManagerIdentity = identity;
}

#pragma mark - Private methods

- (void)dismissAndRunCompletionCallbackWithError:(NSError*)error
                                        identity:(id<SystemIdentity>)identity
                                        animated:(BOOL)animated
                                      completion:(ProceduralBlock)completion {
  DCHECK(_authActivityViewController);
  DCHECK(_isActivityViewPresented);
  DCHECK(error || identity)
      << "An identity must be set to close the dialog successfully";

  // Clear the global identity before next interaction.
  gFakeSystemIdentityInteractionManagerIdentity = nil;

  if (identity) {
    FakeSystemIdentityManager* manager = _manager.get();
    if (manager) {
      manager->AddIdentity(identity);
    } else {
      // Fail with an error if the identity manager has been destroyed.
      error = [NSError errorWithDomain:@"RuntimeError" code:-1 userInfo:nil];
      identity = nil;
    }
  }

  __weak FakeSystemIdentityInteractionManager* weakSelf = self;
  [_authActivityViewController.presentingViewController
      dismissViewControllerAnimated:animated
                         completion:^{
                           [weakSelf runCompletionCallbackWithError:error
                                                           identity:identity
                                                         completion:completion];
                         }];
}

- (void)runCompletionCallbackWithError:(NSError*)error
                              identity:(id<SystemIdentity>)identity
                            completion:(ProceduralBlock)completion {
  _authActivityViewController = nil;
  if (_signinCompletion) {
    SigninCompletionBlock signinCompletion = nil;
    std::swap(_signinCompletion, signinCompletion);
    signinCompletion(identity, error);
  }
  if (completion) {
    completion();
  }
  _isActivityViewPresented = NO;
}

- (void)onActivityViewPresented {
  DCHECK(!_isActivityViewPresented);
  _isActivityViewPresented = YES;
}

@end
