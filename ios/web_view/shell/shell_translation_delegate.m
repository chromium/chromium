// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/shell/shell_translation_delegate.h"

#import <UIKit/UIKit.h>

@interface ShellTranslationDelegate ()
// Action Sheet to prompt user whether or not the page should be translated.
@property(nonatomic, strong) UIAlertController* beforeTranslateActionSheet;
@end

@implementation ShellTranslationDelegate

@synthesize beforeTranslateActionSheet = _beforeTranslateActionSheet;

- (void)dealloc {
  [_beforeTranslateActionSheet dismissViewControllerAnimated:YES
                                                  completion:nil];
}

#pragma mark - CWVTranslationDelegate methods

- (void)translationController:(CWVTranslationController*)controller
    canOfferTranslationFromLanguage:(CWVTranslationLanguage*)pageLanguage
                         toLanguage:(CWVTranslationLanguage*)userLanguage {
  NSLog(@"%@:%@:%@", NSStringFromSelector(_cmd), pageLanguage, userLanguage);
  __weak ShellTranslationDelegate* weakSelf = self;

  self.beforeTranslateActionSheet = [UIAlertController
      alertControllerWithTitle:nil
                       message:@"Pick Translate Action"
                preferredStyle:UIAlertControllerStyleActionSheet];
  _beforeTranslateActionSheet.popoverPresentationController.sourceView =
      [self anyKeyWindow];
  CGRect bounds = [self anyKeyWindow].bounds;
  _beforeTranslateActionSheet.popoverPresentationController.sourceRect =
      CGRectMake(CGRectGetWidth(bounds) / 2, 60, 1, 1);
  UIAlertAction* cancelAction =
      [UIAlertAction actionWithTitle:@"Nope."
                               style:UIAlertActionStyleCancel
                             handler:^(UIAlertAction* action) {
                               weakSelf.beforeTranslateActionSheet = nil;
                             }];
  [_beforeTranslateActionSheet addAction:cancelAction];

  NSString* translateTitle = [NSString
      stringWithFormat:@"Translate to %@", userLanguage.localizedName];
  UIAlertAction* translateAction = [UIAlertAction
      actionWithTitle:translateTitle
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* action) {
                weakSelf.beforeTranslateActionSheet = nil;
                if (!weakSelf) {
                  return;
                }
                CWVTranslationLanguage* source = pageLanguage;
                CWVTranslationLanguage* target = userLanguage;
                [controller translatePageFromLanguage:source
                                           toLanguage:target
                                        userInitiated:YES];
              }];
  [_beforeTranslateActionSheet addAction:translateAction];

  UIAlertAction* alwaysTranslateAction = [UIAlertAction
      actionWithTitle:@"Always Translate"
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* action) {
                weakSelf.beforeTranslateActionSheet = nil;
                if (!weakSelf) {
                  return;
                }
                CWVTranslationPolicy* policy = [CWVTranslationPolicy
                    translationPolicyAutoTranslateToLanguage:userLanguage];
                [controller setTranslationPolicy:policy
                                 forPageLanguage:pageLanguage];
              }];
  [_beforeTranslateActionSheet addAction:alwaysTranslateAction];

  UIAlertAction* neverTranslateAction = [UIAlertAction
      actionWithTitle:@"Never Translate"
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* action) {
                weakSelf.beforeTranslateActionSheet = nil;
                if (!weakSelf) {
                  return;
                }
                CWVTranslationPolicy* policy =
                    [CWVTranslationPolicy translationPolicyNever];
                [controller setTranslationPolicy:policy
                                 forPageLanguage:pageLanguage];
              }];
  [_beforeTranslateActionSheet addAction:neverTranslateAction];

  [[self anyKeyWindow].rootViewController
      presentViewController:_beforeTranslateActionSheet
                   animated:YES
                 completion:nil];
}

- (void)translationController:(CWVTranslationController*)controller
    didStartTranslationFromLanguage:(CWVTranslationLanguage*)sourceLanguage
                         toLanguage:(CWVTranslationLanguage*)targetLanguage
                      userInitiated:(BOOL)userInitiated {
  NSLog(@"%@:%@:%@:%@", NSStringFromSelector(_cmd), sourceLanguage,
        targetLanguage, @(userInitiated));
}

- (void)translationController:(CWVTranslationController*)controller
    didFinishTranslationFromLanguage:(CWVTranslationLanguage*)sourceLanguage
                          toLanguage:(CWVTranslationLanguage*)targetLanguage
                               error:(nullable NSError*)error {
  NSLog(@"%@:%@:%@:%@", NSStringFromSelector(_cmd), sourceLanguage,
        targetLanguage, error);
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
