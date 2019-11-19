// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/shell/shell_autofill_delegate.h"

#import <UIKit/UIKit.h>

#import "ios/web_view/shell/shell_risk_data_loader.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ShellAutofillDelegate ()

// Autofill controller.
@property(nonatomic, strong) CWVAutofillController* autofillController;

// Risk data loader.
@property(nonatomic, strong) ShellRiskDataLoader* riskDataLoader;

// Returns an action for a suggestion.
- (UIAlertAction*)actionForSuggestion:(CWVAutofillSuggestion*)suggestion;

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
                            value:(NSString*)value {
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
    UIAlertAction* cancelAction =
        [UIAlertAction actionWithTitle:@"Cancel"
                                 style:UIAlertActionStyleCancel
                               handler:nil];
    [alertController addAction:cancelAction];
    for (CWVAutofillSuggestion* suggestion in suggestions) {
      [alertController addAction:[self actionForSuggestion:suggestion]];
    }

    [UIApplication.sharedApplication.keyWindow.rootViewController
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
                            value:(NSString*)value {
  // Not implemented.
}

- (void)autofillController:(CWVAutofillController*)autofillController
    didBlurOnFieldWithIdentifier:(NSString*)fieldIdentifier
                       fieldType:(NSString*)fieldType
                        formName:(NSString*)formName
                           value:(NSString*)value {
  // Not implemented.
}

- (void)autofillController:(CWVAutofillController*)autofillController
     didSubmitFormWithName:(NSString*)formName
             userInitiated:(BOOL)userInitiated
               isMainFrame:(BOOL)isMainFrame {
  // Not implemented.
}

- (void)autofillControllerDidInsertFormElements:
    (CWVAutofillController*)autofillController {
  // Not implemented.
}

- (void)autofillController:(CWVAutofillController*)autofillController
    decideSavePolicyForAutofillProfile:(CWVAutofillProfile*)autofillProfile
                       decisionHandler:
                           (void (^)(BOOL decision))decisionHandler {
  UIAlertController* alertController = [UIAlertController
      alertControllerWithTitle:@"Save profile?"
                       message:autofillProfile.debugDescription
                preferredStyle:UIAlertControllerStyleActionSheet];
  UIAlertAction* allowAction =
      [UIAlertAction actionWithTitle:@"Allow"
                               style:UIAlertActionStyleDefault
                             handler:^(UIAlertAction* _Nonnull action) {
                               decisionHandler(YES);
                             }];
  UIAlertAction* cancelAction =
      [UIAlertAction actionWithTitle:@"Cancel"
                               style:UIAlertActionStyleCancel
                             handler:^(UIAlertAction* _Nonnull action) {
                               decisionHandler(NO);
                             }];
  [alertController addAction:allowAction];
  [alertController addAction:cancelAction];
  [UIApplication.sharedApplication.keyWindow.rootViewController
      presentViewController:alertController
                   animated:YES
                 completion:nil];
}

- (void)autofillController:(CWVAutofillController*)autofillController
    saveCreditCardWithSaver:(CWVCreditCardSaver*)saver {
  CWVCreditCard* creditCard = saver.creditCard;
  UIAlertController* alertController = [UIAlertController
      alertControllerWithTitle:@"Save card?"
                       message:creditCard.debugDescription
                preferredStyle:UIAlertControllerStyleActionSheet];
  UIAlertAction* allowAction = [UIAlertAction
      actionWithTitle:@"Allow"
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* _Nonnull action) {
                [saver acceptWithRiskData:self.riskDataLoader.riskData
                        completionHandler:^(BOOL cardSaved) {
                          if (!cardSaved) {
                            NSLog(@"Failed to save: %@", saver.creditCard);
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

  [UIApplication.sharedApplication.keyWindow.rootViewController
      presentViewController:alertController
                   animated:YES
                 completion:nil];
}

- (void)autofillController:(CWVAutofillController*)autofillController
    decideSavePolicyForPassword:(CWVPassword*)password
                decisionHandler:(void (^)(CWVPasswordUserDecision decision))
                                    decisionHandler {
  UIAlertController* alertController = [UIAlertController
      alertControllerWithTitle:@"Save password?"
                       message:password.debugDescription
                preferredStyle:UIAlertControllerStyleActionSheet];

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

  [UIApplication.sharedApplication.keyWindow.rootViewController
      presentViewController:alertController
                   animated:YES
                 completion:nil];
}

- (void)autofillController:(CWVAutofillController*)autofillController
    decideUpdatePolicyForPassword:(CWVPassword*)password
                  decisionHandler:(void (^)(CWVPasswordUserDecision decision))
                                      decisionHandler {
  UIAlertController* alertController = [UIAlertController
      alertControllerWithTitle:@"Update password?"
                       message:password.debugDescription
                preferredStyle:UIAlertControllerStyleActionSheet];

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

  [UIApplication.sharedApplication.keyWindow.rootViewController
      presentViewController:alertController
                   animated:YES
                 completion:nil];
}

- (void)autofillController:(CWVAutofillController*)autofillController
    verifyCreditCardWithVerifier:(CWVCreditCardVerifier*)verifier {
  [UIApplication.sharedApplication.keyWindow endEditing:YES];

  UIAlertController* alertController =
      [UIAlertController alertControllerWithTitle:@"Verify Card"
                                          message:@"Enter CVC"
                                   preferredStyle:UIAlertControllerStyleAlert];

  __weak UIAlertController* weakAlertController = alertController;
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
                           storeLocally:NO
                               riskData:self.riskDataLoader.riskData
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

  [UIApplication.sharedApplication.keyWindow.rootViewController
      presentViewController:alertController
                   animated:YES
                 completion:nil];
}

#pragma mark - Private Methods

- (UIAlertAction*)actionForSuggestion:(CWVAutofillSuggestion*)suggestion {
  NSString* title =
      [NSString stringWithFormat:@"%@ %@", suggestion.value,
                                 suggestion.displayDescription ?: @""];
  return [UIAlertAction actionWithTitle:title
                                  style:UIAlertActionStyleDefault
                                handler:^(UIAlertAction* _Nonnull action) {
                                  [_autofillController
                                       acceptSuggestion:suggestion
                                      completionHandler:nil];
                                  [UIApplication.sharedApplication.keyWindow
                                      endEditing:YES];
                                }];
}

@end
