// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_wk_ui_handler.h"

#include "base/strings/sys_string_conversions.h"
#import "ios/web/navigation/wk_navigation_action_util.h"
#import "ios/web/navigation/wk_navigation_util.h"
#import "ios/web/public/ui/java_script_dialog_type.h"
#import "ios/web/public/web_client.h"
#import "ios/web/web_state/ui/crw_context_menu_controller.h"
#import "ios/web/web_state/ui/crw_wk_ui_handler_delegate.h"
#import "ios/web/web_state/user_interaction_state.h"
#import "ios/web/web_state/web_state_impl.h"
#import "ios/web/web_view/wk_security_origin_util.h"
#import "ios/web/webui/mojo_facade.h"
#import "net/base/mac/url_conversions.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CRWWKUIHandler () {
  // Backs up property with the same name.
  std::unique_ptr<web::MojoFacade> _mojoFacade;
}

@property(nonatomic, assign, readonly) web::WebStateImpl* webStateImpl;

// Facade for Mojo API.
@property(nonatomic, readonly) web::MojoFacade* mojoFacade;

@end

@implementation CRWWKUIHandler

#pragma mark - Public

- (void)close {
  _mojoFacade.reset();
}

#pragma mark - Property

- (web::WebStateImpl*)webStateImpl {
  return [self.delegate webStateImplForUIHandler:self];
}

- (web::MojoFacade*)mojoFacade {
  if (!_mojoFacade)
    _mojoFacade = std::make_unique<web::MojoFacade>(self.webStateImpl);
  return _mojoFacade.get();
}

#pragma mark - WKUIDelegate

- (WKWebView*)webView:(WKWebView*)webView
    createWebViewWithConfiguration:(WKWebViewConfiguration*)configuration
               forNavigationAction:(WKNavigationAction*)action
                    windowFeatures:(WKWindowFeatures*)windowFeatures {
  // Do not create windows for non-empty invalid URLs.
  GURL requestURL = net::GURLWithNSURL(action.request.URL);
  if (!requestURL.is_empty() && !requestURL.is_valid()) {
    DLOG(WARNING) << "Unable to open a window with invalid URL: "
                  << requestURL.possibly_invalid_spec();
    return nil;
  }

  NSString* referrer = [action.request
      valueForHTTPHeaderField:web::wk_navigation_util::kReferrerHeaderName];
  GURL openerURL = referrer.length
                       ? GURL(base::SysNSStringToUTF8(referrer))
                       : [self.delegate documentURLForUIHandler:self];

  // There is no reliable way to tell if there was a user gesture, so this code
  // checks if user has recently tapped on web view. TODO(crbug.com/809706):
  // Remove the usage of -userIsInteracting when rdar://19989909 is fixed.
  bool initiatedByUser = [self.delegate UIHandler:self
                            isUserInitiatedAction:action];

  if (UIAccessibilityIsVoiceOverRunning()) {
    // -userIsInteracting returns NO if VoiceOver is On. Inspect action's
    // description, which may contain the information about user gesture for
    // certain link clicks.
    initiatedByUser = initiatedByUser ||
                      web::GetNavigationActionInitiationTypeWithVoiceOverOn(
                          action.description) ==
                          web::NavigationActionInitiationType::kUserInitiated;
  }

  web::WebState* childWebState = self.webStateImpl->CreateNewWebState(
      requestURL, openerURL, initiatedByUser);
  if (!childWebState)
    return nil;

  // WKWebView requires WKUIDelegate to return a child view created with
  // exactly the same |configuration| object (exception is raised if config is
  // different). |configuration| param and config returned by
  // WKWebViewConfigurationProvider are different objects because WKWebView
  // makes a shallow copy of the config inside init, so every WKWebView
  // owns a separate shallow copy of WKWebViewConfiguration.
  return [self.delegate UIHandler:self
      createWebViewWithConfiguration:configuration
                         forWebState:childWebState];
}

- (void)webViewDidClose:(WKWebView*)webView {
  if (self.webStateImpl && self.webStateImpl->HasOpener())
    self.webStateImpl->CloseWebState();
}

- (void)webView:(WKWebView*)webView
    runJavaScriptAlertPanelWithMessage:(NSString*)message
                      initiatedByFrame:(WKFrameInfo*)frame
                     completionHandler:(void (^)())completionHandler {
  [self runJavaScriptDialogOfType:web::JAVASCRIPT_DIALOG_TYPE_ALERT
                 initiatedByFrame:frame
                          message:message
                      defaultText:nil
                       completion:^(BOOL, NSString*) {
                         completionHandler();
                       }];
}

- (void)webView:(WKWebView*)webView
    runJavaScriptConfirmPanelWithMessage:(NSString*)message
                        initiatedByFrame:(WKFrameInfo*)frame
                       completionHandler:
                           (void (^)(BOOL result))completionHandler {
  [self runJavaScriptDialogOfType:web::JAVASCRIPT_DIALOG_TYPE_CONFIRM
                 initiatedByFrame:frame
                          message:message
                      defaultText:nil
                       completion:^(BOOL success, NSString*) {
                         if (completionHandler) {
                           completionHandler(success);
                         }
                       }];
}

- (void)webView:(WKWebView*)webView
    runJavaScriptTextInputPanelWithPrompt:(NSString*)prompt
                              defaultText:(NSString*)defaultText
                         initiatedByFrame:(WKFrameInfo*)frame
                        completionHandler:
                            (void (^)(NSString* result))completionHandler {
  GURL origin(web::GURLOriginWithWKSecurityOrigin(frame.securityOrigin));
  if (web::GetWebClient()->IsAppSpecificURL(origin)) {
    std::string mojoResponse =
        self.mojoFacade->HandleMojoMessage(base::SysNSStringToUTF8(prompt));
    completionHandler(base::SysUTF8ToNSString(mojoResponse));
    return;
  }

  [self runJavaScriptDialogOfType:web::JAVASCRIPT_DIALOG_TYPE_PROMPT
                 initiatedByFrame:frame
                          message:prompt
                      defaultText:defaultText
                       completion:^(BOOL, NSString* input) {
                         if (completionHandler) {
                           completionHandler(input);
                         }
                       }];
}

- (BOOL)webView:(WKWebView*)webView
    shouldPreviewElement:(WKPreviewElementInfo*)elementInfo {
  return self.webStateImpl->ShouldPreviewLink(
      net::GURLWithNSURL(elementInfo.linkURL));
}

- (UIViewController*)webView:(WKWebView*)webView
    previewingViewControllerForElement:(WKPreviewElementInfo*)elementInfo
                        defaultActions:
                            (NSArray<id<WKPreviewActionItem>>*)previewActions {
  // Prevent |_contextMenuController| from intercepting the default behavior for
  // the current on-going touch. Otherwise it would cancel the on-going Peek&Pop
  // action and show its own context menu instead (crbug.com/770619).
  [self.contextMenuController allowSystemUIForCurrentGesture];

  return self.webStateImpl->GetPreviewingViewController(
      net::GURLWithNSURL(elementInfo.linkURL));
}

- (void)webView:(WKWebView*)webView
    commitPreviewingViewController:(UIViewController*)previewingViewController {
  return self.webStateImpl->CommitPreviewingViewController(
      previewingViewController);
}

#pragma mark - Helper

// Helper to respond to |webView:runJavaScript...| delegate methods.
// |completionHandler| must not be nil.
- (void)runJavaScriptDialogOfType:(web::JavaScriptDialogType)type
                 initiatedByFrame:(WKFrameInfo*)frame
                          message:(NSString*)message
                      defaultText:(NSString*)defaultText
                       completion:(void (^)(BOOL, NSString*))completionHandler {
  DCHECK(completionHandler);

  // JavaScript dialogs should not be presented if there is no information about
  // the requesting page's URL.
  GURL requestURL = net::GURLWithNSURL(frame.request.URL);
  if (!requestURL.is_valid()) {
    completionHandler(NO, nil);
    return;
  }

  self.webStateImpl->RunJavaScriptDialog(
      requestURL, type, message, defaultText,
      base::BindOnce(^(bool success, NSString* input) {
        completionHandler(success, input);
      }));
}

@end
