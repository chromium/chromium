// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/fake_system_identity_interaction_manager.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/test_constants.h"
#import "ios/public/provider/chrome/browser/signin/signin_error_api.h"

namespace {

// Global used to store the +identity of FakeSystemIdentityInteractionManager.
id<SystemIdentity> gFakeSystemIdentityInteractionManagerIdentity = nil;

// Global status on whether capabilities should not be set for the global
// SystemIdentity.
BOOL gUsingUnknownCapabilities;

}  // namespace

@interface FakeAuthActivityViewController : UIViewController
@end

@implementation FakeAuthActivityViewController {
  __weak FakeSystemIdentityInteractionManager* _manager;
}

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

  UIButton* addAccountButton =
      [self createButtonWithTitle:@"Add Account"
                           action:@selector(didTapAddAccount:)
           accessibilitIdentifier:kFakeAuthAddAccountButtonIdentifier];
  UIButton* cancelButton =
      [self createButtonWithTitle:@"Cancel"
                           action:@selector(didTapCancel:)
           accessibilitIdentifier:kFakeAuthCancelButtonIdentifier];
  NSMutableArray<UIButton*>* subviews =
      [NSMutableArray arrayWithObjects:addAccountButton, cancelButton, nil];
  if (!@available(iOS 26, *)) {
    // Up to iOS 18, the view can disappear without calling the callback. This
    // occur when the user turn off and on the screen while iOS asks whether
    // they accept to use google.com to authentify. This button simulate this
    // issue. It can be removed once the minimal version is iOS 26.  See
    // crbug.com/395959814.
    UIButton* dismissButton =
        [self createButtonWithTitle:@"Dismiss without callback"
                             action:@selector(didTapDismiss:)
             accessibilitIdentifier:kFakeAuthDismissButtonIdentifier];
    [subviews addObject:dismissButton];
  }
  // Container StackView
  UIStackView* stackView =
      [[UIStackView alloc] initWithArrangedSubviews:subviews];
  stackView.axis = UILayoutConstraintAxisHorizontal;
  stackView.translatesAutoresizingMaskIntoConstraints = false;
  [self.view addSubview:stackView];
  // Set up constraints.
  NSMutableArray* constraints = [[NSMutableArray alloc] init];
  [constraints addObject:[stackView.topAnchor
                             constraintEqualToAnchor:self.view.topAnchor]];
  [constraints addObject:[stackView.leadingAnchor
                             constraintEqualToAnchor:self.view.leadingAnchor]];
  [NSLayoutConstraint activateConstraints:constraints];
}

#pragma mark - Private methods

- (UIButton*)createButtonWithTitle:(NSString*)title
                            action:(SEL)action
            accessibilitIdentifier:(NSString*)accessibilityIdentitier {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeCustom];
  [button setTitle:title forState:UIControlStateNormal];
  [button setAccessibilityIdentifier:accessibilityIdentitier];
  [button addTarget:self
                action:action
      forControlEvents:UIControlEventTouchUpInside];
  [button sizeToFit];
  return button;
}

- (void)didTapAddAccount:(id)sender {
  [_manager simulateDidTapAddAccount];
}

- (void)didTapCancel:(id)sender {
  [_manager simulateDidTapCancel];
}

// Dismiss the view without informing the manager.
// This simulates UIKit bug crbug.com/395959814.
- (void)didTapDismiss:(id)sender {
  if (@available(iOS 26, *)) {
    // The bug simulated by this function is fixed in iOS 26.
    NOTREACHED();
  }
  [self dismissViewControllerAnimated:YES completion:nil];
}

@end

@implementation FakeSystemIdentityInteractionManager {
  base::WeakPtr<FakeSystemIdentityManager> _manager;
  SigninCompletionBlock _signinCompletion;
  FakeAuthActivityViewController* _authActivityViewController;
  BOOL _isActivityViewPresented;
  NSString* _lastStartAuthActivityUserEmail;
}

+ (void)setIdentity:(id<SystemIdentity>)identity
    withUnknownCapabilities:(BOOL)usingUnknownCapabilities {
  gFakeSystemIdentityInteractionManagerIdentity = identity;
  gUsingUnknownCapabilities = usingUnknownCapabilities;
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
                                        animated:YES];
}

- (void)simulateDidTapCancel {
  [self cancelAuthActivityAnimated:YES];
}

- (void)simulateDidThrowUnhandledError {
  NSError* error = [NSError errorWithDomain:@"Unhandled" code:-1 userInfo:nil];
  [self dismissAndRunCompletionCallbackWithError:error
                                        identity:nil
                                        animated:YES];
}

- (void)simulateDidInterrupt {
  // When the auth activity view is interrupted, its completion callback is
  // invoked with the user cancel error.
  [self simulateDidTapCancel];
}

- (void)simulateDisappearingView {
  [self dismissViewAnimated:NO];
}

#pragma mark - SystemIdentityInteractionManager

- (void)startAuthActivityWithViewController:(UIViewController*)viewController
                                  userEmail:(NSString*)userEmail
                                 completion:(SigninCompletionBlock)completion {
  CHECK(completion, base::NotFatalUntil::M140);
  CHECK(viewController, base::NotFatalUntil::M140);
  _lastStartAuthActivityUserEmail = userEmail;
  if (userEmail.length) {
    [FakeSystemIdentityInteractionManager
                    setIdentity:[FakeSystemIdentity identityWithEmail:userEmail]
        withUnknownCapabilities:NO];
  }
  _signinCompletion = completion;
  _authActivityViewController =
      [[FakeAuthActivityViewController alloc] initWithManager:self];
  BOOL isIPad =
      UIDevice.currentDevice.userInterfaceIdiom == UIUserInterfaceIdiomPad;
  _authActivityViewController.modalPresentationStyle =
      isIPad ? UIModalPresentationFormSheet : UIModalPresentationFullScreen;

  __weak FakeSystemIdentityInteractionManager* weakSelf = self;
  [viewController presentViewController:_authActivityViewController
                               animated:YES
                             completion:^{
                               [weakSelf onActivityViewPresented];
                             }];
}

- (void)cancelAuthActivityAnimated:(BOOL)animated {
  NSError* error = ios::provider::CreateUserCancelledSigninError();
  [self dismissAndRunCompletionCallbackWithError:error
                                        identity:nil
                                        animated:animated];
}

#pragma mark - Properties

- (BOOL)isActivityViewPresented {
  return _isActivityViewPresented;
}

- (NSString*)lastStartAuthActivityUserEmail {
  return _lastStartAuthActivityUserEmail;
}

#pragma mark - Private methods

- (void)dismissAndRunCompletionCallbackWithError:(NSError*)error
                                        identity:(id<SystemIdentity>)identity
                                        animated:(BOOL)animated {
  DCHECK(_authActivityViewController);
  DCHECK(_isActivityViewPresented);
  DCHECK(error || identity)
      << "An identity must be set to close the dialog successfully";

  // Clear the global identity before next interaction.
  gFakeSystemIdentityInteractionManagerIdentity = nil;

  if (identity) {
    FakeSystemIdentityManager* manager = _manager.get();
    if (manager) {
      if (manager->ContainsIdentity(identity)) {
        manager->ClearPersistentAuthErrorForAccount(
            CoreAccountId::FromGaiaId(identity.gaiaId));
      } else if (gUsingUnknownCapabilities) {
        manager->AddIdentityWithUnknownCapabilities(identity);
      } else {
        manager->AddIdentity(identity);
      }
    } else {
      // Fail with an error if the identity manager has been destroyed.
      error = [NSError errorWithDomain:@"RuntimeError" code:-1 userInfo:nil];
      identity = nil;
    }
  }

  [self dismissViewAnimated:animated];
  if (_signinCompletion) {
    SigninCompletionBlock signinCompletion = nil;
    std::swap(_signinCompletion, signinCompletion);
    signinCompletion(identity, error);
  }
}

- (void)onActivityViewPresented {
  DCHECK(!_isActivityViewPresented);
  _isActivityViewPresented = YES;
}

- (void)onActivityViewDismissed {
  _authActivityViewController = nil;
  _isActivityViewPresented = NO;
}

- (void)dismissViewAnimated:(BOOL)animated {
  __weak FakeSystemIdentityInteractionManager* weakSelf = self;
  [_authActivityViewController.presentingViewController
      dismissViewControllerAnimated:animated
                         completion:^{
                           [weakSelf onActivityViewDismissed];
                         }];
}

@end
