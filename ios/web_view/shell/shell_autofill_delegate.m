// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/shell/shell_autofill_delegate.h"

#import <UIKit/UIKit.h>

#import "ios/web_view/shell/shell_risk_data_loader.h"

@interface ShellAutofillDelegate ()

// Autofill controller.
@property(nonatomic, strong) CWVAutofillController* autofillController;

// Risk data loader.
@property(nonatomic, strong) ShellRiskDataLoader* riskDataLoader;

// Returns an action for a suggestion.
- (UIAlertAction*)actionForSuggestion:(CWVAutofillSuggestion*)suggestion
                              atIndex:(NSInteger)index;

@end

@implementation ShellAutofillDelegate

@synthesize autofillController = _autofillController;
@synthesize riskDataLoader = _riskDataLoader;

- (instancetype)init {
  self = [super init];
  if (self) {
    _riskDataLoader = [[ShellRiskDataLoader alloc] init];
  }
  return self;
}

#pragma mark - CWVAutofillControllerDelegate methods

- (void)autofillController:(CWVAutofillController*)autofillController
    didFocusOnFieldWithIdentifier:(NSString*)fieldIdentifier
                        fieldType:(NSString*)fieldType
                         formName:(NSString*)formName
                          frameID:(NSString*)frameID
                            value:(NSString*)value
                    userInitiated:(BOOL)userInitiated {
  _autofillController = autofillController;

  __weak ShellAutofillDelegate* weakSelf = self;
  id completionHandler = ^(NSArray<CWVAutofillSuggestion*>* suggestions) {
    ShellAutofillDelegate* strongSelf = weakSelf;
    if (!suggestions.count || !strongSelf) {
      return;
    }

    UIAlertController* alertController = [UIAlertController
        alertControllerWithTitle:@"Pick a suggestion"
                         message:nil
                  preferredStyle:UIAlertControllerStyleActionSheet];
    alertController.popoverPresentationController.sourceView =
        [self anyKeyWindow];
    CGRect bounds = [self anyKeyWindow].bounds;
    alertController.popoverPresentationController.sourceRect =
        CGRectMake(CGRectGetWidth(bounds) / 2, 60, 1, 1);
    UIAlertAction* cancelAction =
        [UIAlertAction actionWithTitle:@"Cancel"
                                 style:UIAlertActionStyleCancel
                               handler:nil];
    [alertController addAction:cancelAction];
    for (NSUInteger i = 0; i < suggestions.count; ++i) {
      CWVAutofillSuggestion* suggestion = suggestions[i];
      [alertController addAction:[self actionForSuggestion:suggestion
                                                   atIndex:i]];
    }

    [[self anyKeyWindow].rootViewController
        presentViewController:alertController
                     animated:YES
                   completion:nil];
  };
  [autofillController fetchSuggestionsForFormWithName:formName
                                      fieldIdentifier:fieldIdentifier
                                            fieldType:fieldType
                                              frameID:frameID
                                    completionHandler:completionHandler];
}

- (void)autofillController:(CWVAutofillController*)autofillController
    didInputInFieldWithIdentifier:(NSString*)fieldIdentifier
                        fieldType:(NSString*)fieldType
                         formName:(NSString*)formName
                          frameID:(NSString*)frameID
                            value:(NSString*)value
                    userInitiated:(BOOL)userInitiated {
  // TODO(crbug.com/40224850): Fetching suggestions has an important side effect
  // of calling PasswordFormManager::UpdateStateOnUserInput. This will ensure
  // that the typed information can be remembered during the save dialogue.
  // Make this method a no-op once the bug is fixed.
  id completionHandler = ^(NSArray<CWVAutofillSuggestion*>* suggestions) {
    NSLog(@"%@ suggestions: %@", NSStringFromSelector(_cmd), suggestions);
  };
  [autofillController fetchSuggestionsForFormWithName:formName
                                      fieldIdentifier:fieldIdentifier
                                            fieldType:fieldType
                                              frameID:frameID
                                    completionHandler:completionHandler];
}

- (void)autofillController:(CWVAutofillController*)autofillController
    didBlurOnFieldWithIdentifier:(NSString*)fieldIdentifier
                       fieldType:(NSString*)fieldType
                        formName:(NSString*)formName
                         frameID:(NSString*)frameID
                           value:(NSString*)value
                   userInitiated:(BOOL)userInitiated {
  // Not implemented.
}

- (void)autofillController:(CWVAutofillController*)autofillController
     didSubmitFormWithName:(NSString*)formName
                   frameID:(NSString*)frameID
             userInitiated:(BOOL)userInitiated {
  // Not implemented.
}

- (void)autofillController:(CWVAutofillController*)autofillController
              didFindForms:(NSArray<CWVAutofillForm*>*)forms
                   frameID:(NSString*)frameID {
  if (forms.count == 0) {
    return;
  }

  NSArray<NSString*>* debugDescriptions =
      [forms valueForKey:NSStringFromSelector(@selector(debugDescription))];
  NSLog(@"Found forms in frame %@\n%@", frameID, debugDescriptions);
}

- (void)autofillController:(CWVAutofillController*)autofillController
    saveCreditCardWithSaver:(CWVCreditCardSaver*)saver {
  CWVCreditCard* creditCard = saver.creditCard;
  UIAlertController* alertController =
      [UIAlertController alertControllerWithTitle:@"Save card?"
                                          message:creditCard.debugDescription
                                   preferredStyle:UIAlertControllerStyleAlert];
  __weak UIAlertController* weakAlertController = alertController;
  __weak ShellAutofillDelegate* weakSelf = self;
  UIAlertAction* allowAction = [UIAlertAction
      actionWithTitle:@"Allow"
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* _Nonnull action) {
                NSString* cardHolderFullName =
                    weakAlertController.textFields[0].text;
                NSString* expirationMonth =
                    weakAlertController.textFields[1].text;
                NSString* expirationYear =
                    weakAlertController.textFields[2].text;
                [saver acceptWithCardHolderFullName:cardHolderFullName
                                    expirationMonth:expirationMonth
                                     expirationYear:expirationYear
                                           riskData:weakSelf.riskDataLoader
                                                        .riskData
                                  completionHandler:^(BOOL cardSaved) {
                                    if (!cardSaved) {
                                      NSLog(@"Failed to save: %@",
                                            saver.creditCard);
                                    }
                                  }];
              }];
  UIAlertAction* cancelAction =
      [UIAlertAction actionWithTitle:@"Cancel"
                               style:UIAlertActionStyleCancel
                             handler:^(UIAlertAction* _Nonnull action) {
                               [saver decline];
                             }];
  [alertController addAction:allowAction];
  [alertController addAction:cancelAction];

  [alertController
      addTextFieldWithConfigurationHandler:^(UITextField* textField) {
        textField.placeholder = @"Card holder full name";
        textField.keyboardType = UIKeyboardTypeDefault;
      }];
  [alertController
      addTextFieldWithConfigurationHandler:^(UITextField* textField) {
        textField.placeholder = @"Expiration month (MM)";
        textField.keyboardType = UIKeyboardTypeNumberPad;
      }];
  [alertController
      addTextFieldWithConfigurationHandler:^(UITextField* textField) {
        textField.placeholder = @"Expiration year (YYYY)";
        textField.keyboardType = UIKeyboardTypeNumberPad;
      }];

  [[self anyKeyWindow].rootViewController presentViewController:alertController
                                                       animated:YES
                                                     completion:nil];
}

- (void)autofillController:(CWVAutofillController*)autofillController
    decideSavePolicyForPassword:(CWVPassword*)password
                decisionHandler:(void (^)(CWVPasswordUserDecision decision))
                                    decisionHandler {
  UIAlertController* alertController =
      [UIAlertController alertControllerWithTitle:@"Save password?"
                                          message:password.debugDescription
                                   preferredStyle:UIAlertControllerStyleAlert];

  UIAlertAction* noAction = [UIAlertAction
      actionWithTitle:@"Not this time"
                style:UIAlertActionStyleCancel
              handler:^(UIAlertAction* _Nonnull action) {
                decisionHandler(CWVPasswordUserDecisionNotThisTime);
              }];
  [alertController addAction:noAction];

  UIAlertAction* neverAction =
      [UIAlertAction actionWithTitle:@"Never"
                               style:UIAlertActionStyleDefault
                             handler:^(UIAlertAction* _Nonnull action) {
                               decisionHandler(CWVPasswordUserDecisionNever);
                             }];
  [alertController addAction:neverAction];

  UIAlertAction* yesAction =
      [UIAlertAction actionWithTitle:@"Save"
                               style:UIAlertActionStyleDefault
                             handler:^(UIAlertAction* _Nonnull action) {
                               decisionHandler(CWVPasswordUserDecisionYes);
                             }];
  [alertController addAction:yesAction];

  [[self anyKeyWindow].rootViewController presentViewController:alertController
                                                       animated:YES
                                                     completion:nil];
}

- (void)autofillController:(CWVAutofillController*)autofillController
    decideUpdatePolicyForPassword:(CWVPassword*)password
                  decisionHandler:(void (^)(CWVPasswordUserDecision decision))
                                      decisionHandler {
  UIAlertController* alertController =
      [UIAlertController alertControllerWithTitle:@"Update password?"
                                          message:password.debugDescription
                                   preferredStyle:UIAlertControllerStyleAlert];

  UIAlertAction* noAction = [UIAlertAction
      actionWithTitle:@"Not this time"
                style:UIAlertActionStyleCancel
              handler:^(UIAlertAction* _Nonnull action) {
                decisionHandler(CWVPasswordUserDecisionNotThisTime);
              }];
  [alertController addAction:noAction];

  UIAlertAction* yesAction =
      [UIAlertAction actionWithTitle:@"Update"
                               style:UIAlertActionStyleDefault
                             handler:^(UIAlertAction* _Nonnull action) {
                               decisionHandler(CWVPasswordUserDecisionYes);
                             }];
  [alertController addAction:yesAction];

  [[self anyKeyWindow].rootViewController presentViewController:alertController
                                                       animated:YES
                                                     completion:nil];
}

- (void)autofillController:(CWVAutofillController*)autofillController
    verifyCreditCardWithVerifier:(CWVCreditCardVerifier*)verifier {
  [[self anyKeyWindow] endEditing:YES];

  UIAlertController* alertController =
      [UIAlertController alertControllerWithTitle:@"Verify Card"
                                          message:@"Enter CVC"
                                   preferredStyle:UIAlertControllerStyleAlert];

  __weak UIAlertController* weakAlertController = alertController;
  __weak ShellAutofillDelegate* weakSelf = self;
  UIAlertAction* submit = [UIAlertAction
      actionWithTitle:@"Confirm"
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* action) {
                UITextField* textField =
                    weakAlertController.textFields.firstObject;
                NSString* CVC = textField.text;
                [verifier verifyWithCVC:CVC
                        expirationMonth:nil
                         expirationYear:nil
                               riskData:weakSelf.riskDataLoader.riskData
                      completionHandler:^(NSError* error) {
                        if (error) {
                          NSLog(@"Card %@ failed to verify error: %@",
                                verifier.creditCard, error);
                        }
                      }];
              }];

  [alertController addAction:submit];

  UIAlertAction* cancel =
      [UIAlertAction actionWithTitle:@"Cancel"
                               style:UIAlertActionStyleCancel
                             handler:nil];
  [alertController addAction:cancel];

  [alertController
      addTextFieldWithConfigurationHandler:^(UITextField* textField) {
        textField.placeholder = @"CVC";
        textField.keyboardType = UIKeyboardTypeNumberPad;
      }];

  [[self anyKeyWindow].rootViewController presentViewController:alertController
                                                       animated:YES
                                                     completion:nil];
}

- (void)autofillController:(CWVAutofillController*)autofillController
    notifyUserOfPasswordLeakOnURL:(NSURL*)URL
                         leakType:(CWVPasswordLeakType)leakType
                         username:(NSString*)username {
  NSLog(@"Password on %@ is leaked for username %@!", URL, username);
}

- (void)autofillController:(CWVAutofillController*)autofillController
    suggestGeneratedPassword:(NSString*)generatedPassword
             decisionHandler:(void (^)(BOOL accept))decisionHandler {
  NSLog(@"Accepting suggested password: %@", generatedPassword);
  decisionHandler(YES);
}

- (void)autofillController:(CWVAutofillController*)autofillController
    confirmSaveForNewAutofillProfile:(CWVAutofillProfile*)newProfile
                          oldProfile:(nullable CWVAutofillProfile*)oldProfile
                     decisionHandler:
                         (void (^)(CWVAutofillProfileUserDecision decision))
                             decisionHandler {
  NSString* message =
      [NSString stringWithFormat:@"new: %@\nold: %@",
                                 newProfile.debugDescription, oldProfile];
  UIAlertController* alertController = [UIAlertController
      alertControllerWithTitle:@"Confirm save for new profile?"
                       message:message
                preferredStyle:UIAlertControllerStyleAlert];

  UIAlertAction* accept = [UIAlertAction
      actionWithTitle:@"Accept"
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* action) {
                decisionHandler(CWVAutofillProfileUserDecisionAccepted);
              }];
  [alertController addAction:accept];

  UIAlertAction* decline = [UIAlertAction
      actionWithTitle:@"Decline"
                style:UIAlertActionStyleCancel
              handler:^(UIAlertAction* action) {
                decisionHandler(CWVAutofillProfileUserDecisionDeclined);
              }];
  [alertController addAction:decline];

  [[self anyKeyWindow].rootViewController presentViewController:alertController
                                                       animated:YES
                                                     completion:nil];
}

#pragma mark - Private Methods

- (UIAlertAction*)actionForSuggestion:(CWVAutofillSuggestion*)suggestion
                              atIndex:(NSInteger)index {
  NSString* title =
      [NSString stringWithFormat:@"%@ %@", suggestion.value,
                                 suggestion.displayDescription ?: @""];
  __weak ShellAutofillDelegate* weakSelf = self;
  return [UIAlertAction
      actionWithTitle:title
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* action) {
                ShellAutofillDelegate* strongSelf = weakSelf;
                if (!strongSelf) {
                  return;
                }
                [strongSelf.autofillController acceptSuggestion:suggestion
                                                        atIndex:index
                                              completionHandler:nil];
                [[self anyKeyWindow] endEditing:YES];
              }];
}

#pragma mark - Private

- (UIWindow*)anyKeyWindow {
  for (UIWindowScene* windowScene in UIApplication.sharedApplication
           .connectedScenes) {
    NSAssert([windowScene isKindOfClass:[UIWindowScene class]],
             @"UIScene is not a UIWindowScene: %@", windowScene);
    for (UIWindow* window in windowScene.windows) {
      if (window.isKeyWindow) {
        return window;
      }
    }
  }

  return nil;
}

@end
