// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DIALOGS_DIALOG_PRESENTER_H_
#define IOS_CHROME_BROWSER_UI_DIALOGS_DIALOG_PRESENTER_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

@protocol DialogPresenterDelegate;
class GURL;
@class AlertCoordinator;

namespace web {
class WebState;
}

// Handles the queued display of modal dialogs.
@interface DialogPresenter : NSObject

// Whether the DialogPresenter should attempt to show dialogs.  When |active| is
// NO, dialogs will be queued and displayed when the DialogPresenter is
// activated.
@property(nonatomic, assign, getter=isActive) BOOL active;

// Dialogs will be presented from |viewController|.
- (instancetype)initWithDelegate:(id<DialogPresenterDelegate>)delegate
        presentingViewController:(UIViewController*)viewController
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Methods to show JavaScriptDialogs of each JavaScriptAlertType.  If a dialog
// is already being presented, these functions will enqueue a new dialog to be
// shown when the visible dialog is dismissed.  |context| will be retained until
// its associated dialog is dismissed or |-cancelDialogForContext:| is called.
- (void)runJavaScriptAlertPanelWithMessage:(NSString*)message
                                requestURL:(const GURL&)requestURL
                                  webState:(web::WebState*)webState
                         completionHandler:(void (^)(void))completionHandler;
- (void)runJavaScriptConfirmPanelWithMessage:(NSString*)message
                                  requestURL:(const GURL&)requestURL
                                    webState:(web::WebState*)webState
                           completionHandler:
                               (void (^)(BOOL isConfirmed))completionHandler;
- (void)runJavaScriptTextInputPanelWithPrompt:(NSString*)message
                                  defaultText:(NSString*)defaultText
                                   requestURL:(const GURL&)requestURL
                                     webState:(web::WebState*)webState
                            completionHandler:
                                (void (^)(NSString* input))completionHandler;

// Displays an HTTP authentication dialog, which has 2 text fields
// (username and password), Login and Cancel button. If Login was tapped,
// |completionHandler| is called with valid strings which represent username
// and password inputs. If cancel is tapped then |completionHandler| is
// called with nil |user| and nil |password|. Username will be pre-populated
// from provided |credential|.
- (void)runAuthDialogForProtectionSpace:(NSURLProtectionSpace*)protectionSpace
                     proposedCredential:(NSURLCredential*)credential
                               webState:(web::WebState*)webState
                      completionHandler:
                          (void (^)(NSString* user, NSString* password))handler;

// Cancels the display of the dialog associated with |context|.
- (void)cancelDialogForWebState:(web::WebState*)webState;

// Dismisses the currently presented dialog and cancels all queued dialogs.
- (void)cancelAllDialogs;

// Tries to present an alert if needed and possible. Called by the view
// controller which will present the alert to notify that it has dismissed a
// view controller.
- (void)tryToPresent;

@end

@interface DialogPresenter (ExposedForTesting)
// The dialog currently being shown.
@property(nonatomic, readonly) AlertCoordinator* presentedDialogCoordinator;

// Called when |coordinator| is stopped.
- (void)dialogCoordinatorWasStopped:(AlertCoordinator*)coordinator;

@end

// Delegate protocol for DialogPresenter.
@protocol DialogPresenterDelegate<NSObject>

// Called by |presenter| before showing the queued modal dialog associated with
// |context|.
- (void)dialogPresenter:(DialogPresenter*)presenter
    willShowDialogForWebState:(web::WebState*)webState;

// Whether |presenter| should present a dialog.
- (BOOL)shouldDialogPresenterPresentDialog:(DialogPresenter*)presenter;

@end

#endif  // IOS_CHROME_BROWSER_UI_DIALOGS_DIALOG_PRESENTER_H_
