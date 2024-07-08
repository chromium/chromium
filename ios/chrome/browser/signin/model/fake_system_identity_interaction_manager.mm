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
  // Container StackView
  UIStackView* stackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ addAccountButton, cancelButton ]];
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

- (NSString*)lastStartAuthActivityUserEmail {
  return _lastStartAuthActivityUserEmail;
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
      if (gUsingUnknownCapabilities) {
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
