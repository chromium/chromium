// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/dialogs/dialog_presenter.h"

#include <map>

#include "base/containers/circular_deque.h"
#import "base/ios/block_types.h"
#include "base/logging.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/alert_coordinator/input_alert_coordinator.h"
#import "ios/chrome/browser/ui/dialogs/completion_block_util.h"
#import "ios/chrome/browser/ui/dialogs/dialog_constants.h"
#import "ios/chrome/browser/ui/dialogs/java_script_dialog_blocking_state.h"
#import "ios/chrome/browser/ui/dialogs/nsurl_protection_space_util.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using completion_block_util::AlertCallback;
using completion_block_util::ConfirmCallback;
using completion_block_util::PromptCallback;
using completion_block_util::HTTPAuthCallack;
using completion_block_util::GetSafeJavaScriptAlertCompletion;
using completion_block_util::GetSafeJavaScriptConfirmationCompletion;
using completion_block_util::GetSafeJavaScriptPromptCompletion;
using completion_block_util::GetSafeHTTPAuthCompletion;

@interface DialogPresenter () <CRWWebStateObserver> {
  // Queue of WebStates which correspond to the keys in
  // |_dialogCoordinatorsForWebStates|.
  base::circular_deque<web::WebState*> _queuedWebStates;
  // A map associating queued webStates with their coordinators.
  std::map<web::WebState*, AlertCoordinator*> _dialogCoordinatorsForWebStates;
  // The WebStateObserver listening for navigation events from the queued
  // WebStates.
  std::unique_ptr<web::WebStateObserver> _webStateObserver;
}

// The delegate passed on initialization.
@property(weak, nonatomic, readonly) id<DialogPresenterDelegate> delegate;

// The presenting view controller passed on initialization.
@property(weak, nonatomic, readonly) UIViewController* viewController;

// Whether a modal dialog is currently being shown.
@property(nonatomic, readonly, getter=isShowingDialog) BOOL showingDialog;

// The webState for |presentedDialog|.
@property(nonatomic) web::WebState* presentedDialogWebState;

// The dialog that's currently being shown, if any.
@property(nonatomic, strong) AlertCoordinator* presentedDialogCoordinator;

// The JavaScript dialog blocking confirmation action sheet being shown, if any.
@property(nonatomic, strong) AlertCoordinator* blockingConfirmationCoordinator;

// Adds |context| and |coordinator| to the queue.  If a dialog is not already
// being shown, |coordinator| will be presented.  Otherwise, |coordinator| will
// be displayed once the previously shown dialog is dismissed.
- (void)addDialogCoordinator:(AlertCoordinator*)coordinator
                 forWebState:(web::WebState*)webState;

// Shows the dialog associated with the next context in |contextQueue|.
- (void)showNextDialog;

// Called when |coordinator| is stopped.
- (void)dialogCoordinatorWasStopped:(AlertCoordinator*)coordinator;

// Adds buttons to |alertCoordinator|.  A confirmation button with |label| as
// the text will be added for |confirmAction|, and a cancel button will be added
// for |cancelAction|.
- (void)setUpAlertCoordinator:(AlertCoordinator*)alertCoordinator
                confirmAction:(ProceduralBlock)confirmAction
                 cancelAction:(ProceduralBlock)cancelAction
                      OKLabel:(NSString*)label;

// Sets up the JavaScript dialog blocking option for |alertCoordinator|.
// Overrides |alertCoordinator|'s |startAction| to call
// JavaScriptDialogWasShown(). Depending on the value of
// ShouldShowDialogBlockingOption() for |webState|, optionally adds a button to
// |alertCoordinator| allowing for the blocking of future dialogs.  In addition
// to blocking dialogs for the WebState, the added button will call
// |alertCoordinator|'s |cancelAction|.
- (void)setUpBlockingOptionForCoordinator:(AlertCoordinator*)alertCoordinator
                                 webState:(web::WebState*)webState;

// The block to use for the JavaScript dialog blocking option for |coordinator|.
- (ProceduralBlock)blockingActionForCoordinator:(AlertCoordinator*)coordinator;

// Creates a title for the alert based on the URL (|pageURL|), and its
// relationship to the |mainFrameURL| (typically these are identical except for
// when posting alerts from an embedded iframe).
+ (NSString*)localizedTitleForJavaScriptAlertFromPage:(const GURL&)pageURL
                                         mainFrameURL:(const GURL&)mainFrameURL;

@end

@implementation DialogPresenter

@synthesize active = _active;
@synthesize delegate = _delegate;
@synthesize viewController = _viewController;
@synthesize presentedDialogCoordinator = _presentedDialogCoordinator;
@synthesize blockingConfirmationCoordinator = _blockingConfirmationCoordinator;
@synthesize presentedDialogWebState = _presentedDialogWebState;

- (instancetype)initWithDelegate:(id<DialogPresenterDelegate>)delegate
        presentingViewController:(UIViewController*)viewController {
  if ((self = [super init])) {
    DCHECK(delegate);
    DCHECK(viewController);
    _delegate = delegate;
    _viewController = viewController;
    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
  }
  return self;
}

#pragma mark - Accessors

- (void)setActive:(BOOL)active {
  if (_active != active) {
    _active = active;
    [self tryToPresent];
  }
}

- (BOOL)isShowingDialog {
  DCHECK_EQ(self.presentedDialogWebState != nullptr,
            self.presentedDialogCoordinator != nil);
  return self.presentedDialogCoordinator != nil;
}

#pragma mark - Public

- (void)runJavaScriptAlertPanelWithMessage:(NSString*)message
                                requestURL:(const GURL&)requestURL
                                  webState:(web::WebState*)webState
                         completionHandler:(void (^)(void))completionHandler {
  AlertCallback safeCallback =
      GetSafeJavaScriptAlertCompletion(completionHandler);
  NSString* title = [DialogPresenter
      localizedTitleForJavaScriptAlertFromPage:requestURL
                                  mainFrameURL:webState->GetLastCommittedURL()];
  AlertCoordinator* alertCoordinator =
      [[AlertCoordinator alloc] initWithBaseViewController:self.viewController
                                                     title:title
                                                   message:message];

  // Handler.
  __weak DialogPresenter* weakSelf = self;
  __weak AlertCoordinator* weakCoordinator = alertCoordinator;
  ProceduralBlock OKHandler = ^{
    if (safeCallback)
      safeCallback();
    [weakSelf dialogCoordinatorWasStopped:weakCoordinator];
  };

  // Add button.
  [alertCoordinator addItemWithTitle:l10n_util::GetNSString(IDS_OK)
                              action:OKHandler
                               style:UIAlertActionStyleDefault];

  // Add cancel handler.
  alertCoordinator.cancelAction = safeCallback;
  alertCoordinator.noInteractionAction = safeCallback;

  // Blocking option setup.
  [self setUpBlockingOptionForCoordinator:alertCoordinator webState:webState];

  [self addDialogCoordinator:alertCoordinator forWebState:webState];
}

- (void)runJavaScriptConfirmPanelWithMessage:(NSString*)message
                                  requestURL:(const GURL&)requestURL
                                    webState:(web::WebState*)webState
                           completionHandler:
                               (void (^)(BOOL isConfirmed))completionHandler {
  ConfirmCallback safeCallback =
      GetSafeJavaScriptConfirmationCompletion(completionHandler);
  NSString* title = [DialogPresenter
      localizedTitleForJavaScriptAlertFromPage:requestURL
                                  mainFrameURL:webState->GetLastCommittedURL()];
  AlertCoordinator* alertCoordinator =
      [[AlertCoordinator alloc] initWithBaseViewController:self.viewController
                                                     title:title
                                                   message:message];

  // Actions.
  ProceduralBlock confirmAction = ^{
    if (safeCallback)
      safeCallback(YES);
  };

  ProceduralBlock cancelAction = ^{
    if (safeCallback)
      safeCallback(NO);
  };

  // Coordinator Setup.
  NSString* OKLabel = l10n_util::GetNSString(IDS_OK);
  [self setUpAlertCoordinator:alertCoordinator
                confirmAction:confirmAction
                 cancelAction:cancelAction
                      OKLabel:OKLabel];

  // Blocking option setup.
  [self setUpBlockingOptionForCoordinator:alertCoordinator webState:webState];

  [self addDialogCoordinator:alertCoordinator forWebState:webState];
}

- (void)runJavaScriptTextInputPanelWithPrompt:(NSString*)message
                                  defaultText:(NSString*)defaultText
                                   requestURL:(const GURL&)requestURL
                                     webState:(web::WebState*)webState
                            completionHandler:
                                (void (^)(NSString* input))completionHandler {
  PromptCallback safeCallback =
      GetSafeJavaScriptPromptCompletion(completionHandler);
  NSString* title = [DialogPresenter
      localizedTitleForJavaScriptAlertFromPage:requestURL
                                  mainFrameURL:webState->GetLastCommittedURL()];
  InputAlertCoordinator* alertCoordinator = [[InputAlertCoordinator alloc]
      initWithBaseViewController:self.viewController
                           title:title
                         message:message];

  // Actions.
  __weak InputAlertCoordinator* weakCoordinator = alertCoordinator;
  ProceduralBlock confirmAction = ^{
    if (safeCallback) {
      NSString* textInput = [weakCoordinator textFields].firstObject.text;
      safeCallback(textInput ? textInput : @"");
    }
  };

  ProceduralBlock cancelAction = ^{
    if (safeCallback)
      safeCallback(nil);
  };

  // Coordinator Setup.
  NSString* OKLabel = l10n_util::GetNSString(IDS_OK);
  [self setUpAlertCoordinator:alertCoordinator
                confirmAction:confirmAction
                 cancelAction:cancelAction
                      OKLabel:OKLabel];

  // Blocking option setup.
  [self setUpBlockingOptionForCoordinator:alertCoordinator webState:webState];

  // Add text field.
  [alertCoordinator
      addTextFieldWithConfigurationHandler:^(UITextField* textField) {
        textField.text = defaultText;
        textField.accessibilityIdentifier =
            kJavaScriptDialogTextFieldAccessibiltyIdentifier;
      }];

  [self addDialogCoordinator:alertCoordinator forWebState:webState];
}

- (void)runAuthDialogForProtectionSpace:(NSURLProtectionSpace*)protectionSpace
                     proposedCredential:(NSURLCredential*)credential
                               webState:(web::WebState*)webState
                      completionHandler:(void (^)(NSString* user,
                                                  NSString* password))handler {
  HTTPAuthCallack safeCallback = GetSafeHTTPAuthCompletion(handler);
  NSString* title = l10n_util::GetNSStringWithFixup(IDS_LOGIN_DIALOG_TITLE);
  NSString* message =
      nsurlprotectionspace_util::MessageForHTTPAuth(protectionSpace);

  InputAlertCoordinator* alertCoordinator = [[InputAlertCoordinator alloc]
      initWithBaseViewController:self.viewController
                           title:title
                         message:message];

  // Actions.
  __weak InputAlertCoordinator* weakCoordinator = alertCoordinator;
  ProceduralBlock confirmAction = ^{
    if (safeCallback) {
      NSString* username = [[weakCoordinator textFields] objectAtIndex:0].text;
      NSString* password = [[weakCoordinator textFields] objectAtIndex:1].text;
      safeCallback(username, password);
    }
  };

  ProceduralBlock cancelAction = ^{
    if (safeCallback)
      safeCallback(nil, nil);
  };

  // Coordinator Setup.
  NSString* OKLabel =
      l10n_util::GetNSStringWithFixup(IDS_LOGIN_DIALOG_OK_BUTTON_LABEL);
  [self setUpAlertCoordinator:alertCoordinator
                confirmAction:confirmAction
                 cancelAction:cancelAction
                      OKLabel:OKLabel];

  // Add text fields.
  NSString* username = credential.user ? credential.user : @"";
  [alertCoordinator
      addTextFieldWithConfigurationHandler:^(UITextField* textField) {
        textField.text = username;
        textField.placeholder = l10n_util::GetNSString(
            IDS_IOS_HTTP_LOGIN_DIALOG_USERNAME_PLACEHOLDER);
      }];
  [alertCoordinator
      addTextFieldWithConfigurationHandler:^(UITextField* textField) {
        textField.placeholder = l10n_util::GetNSString(
            IDS_IOS_HTTP_LOGIN_DIALOG_PASSWORD_PLACEHOLDER);
        textField.secureTextEntry = YES;
      }];

  [self addDialogCoordinator:alertCoordinator forWebState:webState];
}

- (void)cancelDialogForWebState:(web::WebState*)webState {
  BOOL cancelingPresentedDialog = webState == self.presentedDialogWebState;
  AlertCoordinator* dialogToCancel =
      cancelingPresentedDialog ? self.presentedDialogCoordinator
                               : _dialogCoordinatorsForWebStates[webState];
  DCHECK(!cancelingPresentedDialog || dialogToCancel);
  [dialogToCancel executeCancelHandler];
  [dialogToCancel stop];

  if (cancelingPresentedDialog) {
    DCHECK(_dialogCoordinatorsForWebStates[webState] == nil);
    // Simulate a button tap to trigger showing the next dialog.
    [self dialogCoordinatorWasStopped:dialogToCancel];
  } else if (dialogToCancel) {
    // Clean up queued state.
    auto it =
        std::find(_queuedWebStates.begin(), _queuedWebStates.end(), webState);
    DCHECK(it != _queuedWebStates.end());
    webState->RemoveObserver(_webStateObserver.get());
    _queuedWebStates.erase(it);
    _dialogCoordinatorsForWebStates.erase(webState);
  }
}

- (void)cancelAllDialogs {
  while (!_queuedWebStates.empty()) {
    [self cancelDialogForWebState:_queuedWebStates.front()];
  }
  if (self.presentedDialogWebState)
    [self cancelDialogForWebState:self.presentedDialogWebState];
}

- (void)tryToPresent {
  // Don't try to present if a JavaScript dialog blocking confirmation sheet is
  // displayed.
  if (self.blockingConfirmationCoordinator)
    return;
  if (!self.showingDialog && _active && !_queuedWebStates.empty() &&
      [self.delegate shouldDialogPresenterPresentDialog:self]) {
    [self showNextDialog];
  }
}

+ (NSString*)localizedTitleForJavaScriptAlertFromPage:(const GURL&)pageURL
                                         mainFrameURL:
                                             (const GURL&)mainFrameURL {
  bool sameOriginAsMainFrame = pageURL.GetOrigin() == mainFrameURL.GetOrigin();
  if (!sameOriginAsMainFrame) {
    return l10n_util::GetNSString(
        IDS_JAVASCRIPT_MESSAGEBOX_TITLE_NONSTANDARD_URL_IFRAME);
  }
  base::string16 title = url_formatter::FormatUrlForSecurityDisplay(
      pageURL, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
  return l10n_util::GetNSStringF(IDS_JAVASCRIPT_MESSAGEBOX_TITLE, title);
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation {
  [self cancelDialogForWebState:webState];
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  if (navigation->HasCommitted() && !navigation->IsSameDocument())
    [self cancelDialogForWebState:webState];
}

- (void)renderProcessGoneForWebState:(web::WebState*)webState {
  [self cancelDialogForWebState:webState];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  [self cancelDialogForWebState:webState];
  webState->RemoveObserver(_webStateObserver.get());
}

#pragma mark - Private methods.

- (void)addDialogCoordinator:(AlertCoordinator*)coordinator
                 forWebState:(web::WebState*)webState {
  DCHECK(coordinator);
  DCHECK(webState);
  DCHECK_NE(webState, self.presentedDialogWebState);
  DCHECK(!_dialogCoordinatorsForWebStates[webState]);
  _queuedWebStates.push_back(webState);
  _dialogCoordinatorsForWebStates[webState] = coordinator;

  webState->AddObserver(_webStateObserver.get());

  if (self.active && !self.showingDialog &&
      [self.delegate shouldDialogPresenterPresentDialog:self]) {
    [self showNextDialog];
  }
}

- (void)showNextDialog {
  DCHECK(self.active);
  DCHECK(!self.showingDialog);
  DCHECK(!_queuedWebStates.empty());
  // Update properties and remove context and the dialog from queue.
  self.presentedDialogWebState = _queuedWebStates.front();
  _queuedWebStates.pop_front();
  self.presentedDialogCoordinator =
      _dialogCoordinatorsForWebStates[self.presentedDialogWebState];
  _dialogCoordinatorsForWebStates.erase(self.presentedDialogWebState);
  // Notify the delegate and display the dialog.
  [self.delegate dialogPresenter:self
       willShowDialogForWebState:self.presentedDialogWebState];
  [self.presentedDialogCoordinator start];
}

- (void)dialogCoordinatorWasStopped:(AlertCoordinator*)coordinator {
  if (coordinator != self.presentedDialogCoordinator)
    return;
  self.presentedDialogWebState->RemoveObserver(_webStateObserver.get());
  self.presentedDialogWebState = nil;
  self.presentedDialogCoordinator = nil;
  self.blockingConfirmationCoordinator = nil;
  if (!_queuedWebStates.empty() &&
      [self.delegate shouldDialogPresenterPresentDialog:self]) {
    [self showNextDialog];
  }
}

- (void)setUpAlertCoordinator:(AlertCoordinator*)alertCoordinator
                confirmAction:(ProceduralBlock)confirmAction
                 cancelAction:(ProceduralBlock)cancelAction
                      OKLabel:(NSString*)label {
  // Handlers.
  __weak DialogPresenter* weakSelf = self;
  __weak AlertCoordinator* weakCoordinator = alertCoordinator;

  ProceduralBlock confirmHandler = ^{
    if (confirmAction)
      confirmAction();
    [weakSelf dialogCoordinatorWasStopped:weakCoordinator];
  };

  ProceduralBlock cancelHandler = ^{
    if (cancelAction)
      cancelAction();
    [weakSelf dialogCoordinatorWasStopped:weakCoordinator];
  };

  // Add buttons.
  [alertCoordinator addItemWithTitle:label
                              action:confirmHandler
                               style:UIAlertActionStyleDefault];
  [alertCoordinator addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                              action:cancelHandler
                               style:UIAlertActionStyleCancel];

  // Add cancel handler.
  alertCoordinator.cancelAction = cancelAction;
  alertCoordinator.noInteractionAction = cancelAction;
}

- (void)setUpBlockingOptionForCoordinator:(AlertCoordinator*)alertCoordinator
                                 webState:(web::WebState*)webState {
  DCHECK(alertCoordinator);
  DCHECK(webState);

  JavaScriptDialogBlockingState::CreateForWebState(webState);
  JavaScriptDialogBlockingState* blockingState =
      JavaScriptDialogBlockingState::FromWebState(webState);

  // Set up the start action.
  ProceduralBlock originalStartAction = alertCoordinator.startAction;
  alertCoordinator.startAction = ^{
    if (originalStartAction)
      originalStartAction();
    blockingState->JavaScriptDialogWasShown();
  };

  // Early return if a blocking option should not be added.
  if (!blockingState->show_blocking_option())
    return;

  ProceduralBlock blockingAction =
      [self blockingActionForCoordinator:alertCoordinator];
  NSString* blockingOptionTitle =
      l10n_util::GetNSString(IDS_IOS_JAVA_SCRIPT_DIALOG_BLOCKING_BUTTON_TEXT);
  [alertCoordinator addItemWithTitle:blockingOptionTitle
                              action:blockingAction
                               style:UIAlertActionStyleDefault];
}

- (ProceduralBlock)blockingActionForCoordinator:(AlertCoordinator*)coordinator {
  __weak DialogPresenter* weakSelf = self;
  __weak AlertCoordinator* weakCoordinator = coordinator;
  __weak UIViewController* weakBaseViewController =
      coordinator.baseViewController;
  ProceduralBlock cancelAction = coordinator.cancelAction;
  return [^{
    // Create the confirmation coordinator.  Use an action sheet on iPhone and
    // an alert on iPhone.
    NSString* confirmMessage =
        l10n_util::GetNSString(IDS_JAVASCRIPT_MESSAGEBOX_SUPPRESS_OPTION);
    AlertCoordinator* confirmationCoordinator =
        IsIPadIdiom() ? [[AlertCoordinator alloc]
                            initWithBaseViewController:weakBaseViewController
                                                 title:nil
                                               message:confirmMessage]
                      : [[ActionSheetCoordinator alloc]
                            initWithBaseViewController:weakBaseViewController
                                                 title:nil
                                               message:confirmMessage
                                                  rect:CGRectZero
                                                  view:nil];
    // Set up button actions.
    ProceduralBlock confirmHandler = ^{
      if (cancelAction)
        cancelAction();
      DialogPresenter* strongSelf = weakSelf;
      if (!strongSelf)
        return;
      web::WebState* webState = [strongSelf presentedDialogWebState];
      JavaScriptDialogBlockingState::FromWebState(webState)
          ->JavaScriptDialogBlockingOptionSelected();
      [strongSelf dialogCoordinatorWasStopped:weakCoordinator];
    };
    ProceduralBlock cancelHandler = ^{
      if (cancelAction)
        cancelAction();
      [weakSelf dialogCoordinatorWasStopped:weakCoordinator];
    };
    NSString* blockingOptionTitle =
        l10n_util::GetNSString(IDS_IOS_JAVA_SCRIPT_DIALOG_BLOCKING_BUTTON_TEXT);
    [confirmationCoordinator addItemWithTitle:blockingOptionTitle
                                       action:confirmHandler
                                        style:UIAlertActionStyleDestructive];
    [confirmationCoordinator addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                                       action:cancelHandler
                                        style:UIAlertActionStyleCancel];
    [weakSelf setBlockingConfirmationCoordinator:confirmationCoordinator];
    [[weakSelf blockingConfirmationCoordinator] start];
  } copy];
}

@end
