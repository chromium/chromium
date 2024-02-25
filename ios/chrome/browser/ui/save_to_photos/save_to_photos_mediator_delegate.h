// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SAVE_TO_PHOTOS_SAVE_TO_PHOTOS_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SAVE_TO_PHOTOS_SAVE_TO_PHOTOS_MEDIATOR_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"

@class AccountPickerConfiguration;
@protocol SystemIdentity;

// Delegate for the Save to Photos mediator.
@protocol SaveToPhotosMediatorDelegate

// Show and hide the account picker with a given configuration. If the
// `selectedIdentity` is not nil, it will override the value presented by the
// account picker by default.
- (void)showAccountPickerWithConfiguration:
            (AccountPickerConfiguration*)configuration
                          selectedIdentity:(id<SystemIdentity>)selectedIdentity;
- (void)hideAccountPicker;
// Start/stop the validation spinner in the account picker. It is used to
// indicate ongoing progress of the image upload.
- (void)startValidationSpinnerForAccountPicker;
- (void)stopValidationSpinnerForAccountPicker;

// Show and hide an alert with "Try Again" and "Cancel" options.
- (void)showTryAgainOrCancelAlertWithTitle:(NSString*)title
                                   message:(NSString*)message
                             tryAgainTitle:(NSString*)tryAgainTitle
                            tryAgainAction:(ProceduralBlock)tryAgainAction
                               cancelTitle:(NSString*)cancelTitle
                              cancelAction:(ProceduralBlock)cancelAction;

// Show and hide StoreKit.
- (void)showStoreKitWithProductIdentifier:(NSString*)productIdentifer
                            providerToken:(NSString*)providerToken
                            campaignToken:(NSString*)campaignToken;
- (void)hideStoreKit;

// Show a snackbar with the given `message`, a button with label `buttonText`.
// `messageAction` is called when the button is tapped.
- (void)showSnackbarWithMessage:(NSString*)message
                     buttonText:(NSString*)buttonText
                  messageAction:(ProceduralBlock)messageAction
               completionAction:(void (^)(BOOL))completionAction;

// Hide Save to Photos UI.
- (void)hideSaveToPhotos;

@end

#endif  // IOS_CHROME_BROWSER_UI_SAVE_TO_PHOTOS_SAVE_TO_PHOTOS_MEDIATOR_DELEGATE_H_
