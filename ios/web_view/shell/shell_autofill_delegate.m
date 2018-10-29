// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/shell/shell_autofill_delegate.h"

#import <UIKit/UIKit.h>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ShellAutofillDelegate ()
// Alert controller to present autofill suggestions.
@property(nonatomic, strong) UIAlertController* alertController;

// Autofill controller.
@property(nonatomic, strong) CWVAutofillController* autofillController;

// Presents |alertController| as a modal view controller.
- (void)presentAlertController:(UIAlertController*)alertController;

// Returns a new alert controller with |title| and |message|.
- (UIAlertController*)newAlertControllerWithTitle:(NSString*)title
                                          message:(NSString*)message;

// Returns an action for a suggestion.
- (UIAlertAction*)actionForSuggestion:(CWVAutofillSuggestion*)suggestion;

@end

@implementation ShellAutofillDelegate

@synthesize alertController = _alertController;
@synthesize autofillController = _autofillController;

- (void)dealloc {
  [_alertController dismissViewControllerAnimated:YES completion:nil];
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
    // Dismiss the previous alert dialog so that we can popup a new one.
    [strongSelf->_alertController dismissViewControllerAnimated:NO
                                                     completion:nil];
    UIAlertController* alertController =
        [self newAlertControllerWithTitle:@"Pick a suggestion" message:nil];
    UIAlertAction* cancelAction =
        [UIAlertAction actionWithTitle:@"Cancel"
                                 style:UIAlertActionStyleCancel
                               handler:nil];
    [alertController addAction:cancelAction];
    for (CWVAutofillSuggestion* suggestion in suggestions) {
      [alertController addAction:[self actionForSuggestion:suggestion]];
    }
    UIAlertAction* clearAction = [UIAlertAction
        actionWithTitle:@"Clear"
                  style:UIAlertActionStyleDefault
                handler:^(UIAlertAction* _Nonnull action) {
                  [autofillController clearFormWithName:formName
                                        fieldIdentifier:fieldIdentifier
                                                frameID:frameID
                                      completionHandler:nil];
                }];
    [alertController addAction:clearAction];

    [strongSelf presentAlertController:alertController];
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
  [_alertController dismissViewControllerAnimated:YES completion:nil];
  _alertController = nil;
  _autofillController = nil;
}

- (void)autofillController:(CWVAutofillController*)autofillController
     didSubmitFormWithName:(NSString*)formName
             userInitiated:(BOOL)userInitiated
               isMainFrame:(BOOL)isMainFrame {
  // Not implemented.
}

- (void)autofillController:(CWVAutofillController*)autofillController
    decidePolicyForLocalStorageOfCreditCard:(CWVCreditCard*)creditCard
                            decisionHandler:
                                (void (^)(CWVStoragePolicy))decisionHandler {
  NSString* cardSummary = [NSString
      stringWithFormat:@"%@ %@ %@/%@", creditCard.cardHolderFullName,
                       creditCard.cardNumber, creditCard.expirationMonth,
                       creditCard.expirationYear];
  UIAlertController* alertController =
      [self newAlertControllerWithTitle:@"Choose local store policy"
                                message:cardSummary];
  UIAlertAction* allowAction =
      [UIAlertAction actionWithTitle:@"Allow"
                               style:UIAlertActionStyleDefault
                             handler:^(UIAlertAction* _Nonnull action) {
                               decisionHandler(CWVStoragePolicyAllow);
                             }];
  UIAlertAction* cancelAction =
      [UIAlertAction actionWithTitle:@"Cancel"
                               style:UIAlertActionStyleCancel
                             handler:^(UIAlertAction* _Nonnull action) {
                               decisionHandler(CWVStoragePolicyReject);
                             }];
  [alertController addAction:allowAction];
  [alertController addAction:cancelAction];

  [self presentAlertController:alertController];
}

- (void)autofillController:(CWVAutofillController*)autofillController
    decidePasswordSavingPolicyForUsername:(NSString*)userName
                          decisionHandler:(void (^)(CWVPasswordUserDecision))
                                              decisionHandler {
  UIAlertController* alertController =
      [self newAlertControllerWithTitle:@"Save Password"
                                message:
                                    @"Do you want to save your password on "
                                    @"this site?"];

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

  [self presentAlertController:alertController];
}

- (void)autofillController:(CWVAutofillController*)autofillController
    decidePasswordUpdatingPolicyForUsername:(NSString*)userName
                            decisionHandler:(void (^)(CWVPasswordUserDecision))
                                                decisionHandler {
  UIAlertController* alertController = [self
      newAlertControllerWithTitle:@"Update Password"
                          message:
                              [NSString
                                  stringWithFormat:
                                      @"Do you want to update your password "
                                      @"for %@ on this site?",
                                      userName]];

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

  [self presentAlertController:alertController];
}

#pragma mark - Private Methods

- (UIAlertController*)newAlertControllerWithTitle:(NSString*)title
                                          message:(NSString*)message {
  return [UIAlertController
      alertControllerWithTitle:title
                       message:message
                preferredStyle:UIAlertControllerStyleActionSheet];
}

- (void)presentAlertController:(UIAlertController*)alertController {
  [[UIApplication sharedApplication].keyWindow.rootViewController
      presentViewController:alertController
                   animated:YES
                 completion:nil];
  self.alertController = alertController;
}

- (UIAlertAction*)actionForSuggestion:(CWVAutofillSuggestion*)suggestion {
  NSString* title =
      [NSString stringWithFormat:@"%@ %@", suggestion.value,
                                 suggestion.displayDescription ?: @""];
  return [UIAlertAction actionWithTitle:title
                                  style:UIAlertActionStyleDefault
                                handler:^(UIAlertAction* _Nonnull action) {
                                  [_autofillController fillSuggestion:suggestion
                                                    completionHandler:nil];
                                  [UIApplication.sharedApplication.keyWindow
                                      endEditing:YES];
                                }];
}

@end
