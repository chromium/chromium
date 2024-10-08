// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_wk_ui_handler.h"

#import "base/logging.h"
#import "base/metrics/histogram_functions.h"
#import "base/sequence_checker.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/web/common/features.h"
#import "ios/web/navigation/wk_navigation_action_util.h"
#import "ios/web/navigation/wk_navigation_util.h"
#import "ios/web/public/permissions/permissions.h"
#import "ios/web/public/ui/context_menu_params.h"
#import "ios/web/public/web_client.h"
#import "ios/web/web_state/ui/crw_media_capture_permission_request.h"
#import "ios/web/web_state/ui/crw_wk_ui_handler_delegate.h"
#import "ios/web/web_state/user_interaction_state.h"
#import "ios/web/web_state/web_state_impl.h"
#import "ios/web/web_view/wk_security_origin_util.h"
#import "ios/web/webui/mojo_facade.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"
#import "url/origin.h"

namespace {

// Values for UMA permission histograms. These values are based on
// WKMediaCaptureType and persisted to logs. Entries should not be renumbered
// and numeric values should never be reused.
enum class PermissionRequest {
  RequestCamera = 0,
  RequestMicrophone = 1,
  RequestCameraAndMicrophone = 2,
  kMaxValue = RequestCameraAndMicrophone,
};

// Records permission histogram enum for `media_capture_type` on UMA.
void RecordHistogramForPermissionRequestForWKMediaCaptureType(
    WKMediaCaptureType media_capture_type) {
  PermissionRequest type;
  switch (media_capture_type) {
    case WKMediaCaptureTypeCamera:
      type = PermissionRequest::RequestCamera;
      break;
    case WKMediaCaptureTypeMicrophone:
      type = PermissionRequest::RequestMicrophone;
      break;
    case WKMediaCaptureTypeCameraAndMicrophone:
      type = PermissionRequest::RequestCameraAndMicrophone;
      break;
  }
  base::UmaHistogramEnumeration("IOS.Permission.Requests", type);
}

}  // namespace

@interface CRWWKUIHandler () <CRWMediaCapturePermissionPresenter> {
  // Backs up property with the same name.
  std::unique_ptr<web::MojoFacade> _mojoFacade;

  // Check that public API is called from the correct sequence.
  SEQUENCE_CHECKER(_sequenceChecker);
}

@property(nonatomic, assign, readonly) web::WebStateImpl* webStateImpl;

// Facade for Mojo API.
@property(nonatomic, readonly) web::MojoFacade* mojoFacade;

// Task runner that creates this object.
@property(nonatomic, readonly) scoped_refptr<base::SequencedTaskRunner>
    mainTaskRunner;

@end

@implementation CRWWKUIHandler

- (instancetype)init {
  if ((self = [super init])) {
    _mainTaskRunner = base::SequencedTaskRunner::GetCurrentDefault();
    CHECK(_mainTaskRunner);
  }
  return self;
}

#pragma mark - CRWWebViewHandler

- (void)close {
  [super close];
  _mojoFacade.reset();
}

#pragma mark - Property

- (web::WebStateImpl*)webStateImpl {
  return [self.delegate webStateImplForWebViewHandler:self];
}

- (web::MojoFacade*)mojoFacade {
  if (!_mojoFacade)
    _mojoFacade = std::make_unique<web::MojoFacade>(self.webStateImpl);
  return _mojoFacade.get();
}

#pragma mark - WKUIDelegate

- (void)webView:(WKWebView*)webView
    requestMediaCapturePermissionForOrigin:(WKSecurityOrigin*)origin
                          initiatedByFrame:(WKFrameInfo*)frame
                                      type:(WKMediaCaptureType)type
                           decisionHandler:
                               (void (^)(WKPermissionDecision decision))
                                   decisionHandler {
  RecordHistogramForPermissionRequestForWKMediaCaptureType(type);
  CRWMediaCapturePermissionRequest* request =
      [[CRWMediaCapturePermissionRequest alloc]
          initWithDecisionHandler:decisionHandler
                     onTaskRunner:self.mainTaskRunner];
  request.presenter = self;
  GURL securityOrigin = web::GURLOriginWithWKSecurityOrigin(origin);
  if (web::GetWebClient()->EnableFullscreenAPI()) {
    if (@available(iOS 16, *)) {
      if (webView.fullscreenState == WKFullscreenStateInFullscreen ||
          webView.fullscreenState == WKFullscreenStateEnteringFullscreen) {
        [webView closeAllMediaPresentationsWithCompletionHandler:^{
          [request displayPromptForMediaCaptureType:type origin:securityOrigin];
        }];
        return;
      }
    }
  }
  [request displayPromptForMediaCaptureType:type origin:securityOrigin];
}

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
                       : [self.delegate documentURLForWebViewHandler:self];

  // There is no reliable way to tell if there was a user gesture, so this code
  // checks if user has recently tapped on web view. TODO(crbug.com/40561701):
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
  // exactly the same `configuration` object (exception is raised if config is
  // different). `configuration` param and config returned by
  // WKWebViewConfigurationProvider are different objects because WKWebView
  // makes a shallow copy of the config inside init, so every WKWebView
  // owns a separate shallow copy of WKWebViewConfiguration.
  WKWebView* newWebView = [self.delegate UIHandler:self
                    createWebViewWithConfiguration:configuration
                                       forWebState:childWebState];

  if (childWebState->GetDelegate()) {
    childWebState->GetDelegate()->OnNewWebViewCreated(childWebState);
  }

  return newWebView;
}

- (void)webViewDidClose:(WKWebView*)webView {
  // This is triggered by a JavaScript `close()` method call, only if the tab
  // was opened using `window.open`. WebKit is checking that this is the case,
  // so we can close the tab unconditionally here.
  if (self.webStateImpl) {
    __weak __typeof(self) weakSelf = self;
    // -webViewDidClose will typically trigger another webState to activate,
    // which may in turn also close. To prevent reentrant modificationre in
    // WebStateList, trigger a PostTask here.
    self.mainTaskRunner->PostTask(FROM_HERE, base::BindOnce(^{
                                    web::WebStateImpl* webStateImpl =
                                        weakSelf.webStateImpl;
                                    if (webStateImpl) {
                                      webStateImpl->CloseWebState();
                                    }
                                  }));
  }
}

- (void)webView:(WKWebView*)webView
    runJavaScriptAlertPanelWithMessage:(NSString*)message
                      initiatedByFrame:(WKFrameInfo*)frame
                     completionHandler:(void (^)())completionHandler {
  DCHECK(completionHandler);
  GURL requestURL = net::GURLWithNSURL(frame.request.URL);
  if (![self shouldPresentJavaScriptDialogForRequestURL:requestURL
                                            isMainFrame:frame.mainFrame]) {
    completionHandler();
    return;
  }

  self.webStateImpl->RunJavaScriptAlertDialog(
      requestURL, message, base::BindOnce(completionHandler));
}

- (void)webView:(WKWebView*)webView
    runJavaScriptConfirmPanelWithMessage:(NSString*)message
                        initiatedByFrame:(WKFrameInfo*)frame
                       completionHandler:
                           (void (^)(BOOL result))completionHandler {
  DCHECK(completionHandler);

  GURL requestURL = net::GURLWithNSURL(frame.request.URL);
  if (![self shouldPresentJavaScriptDialogForRequestURL:requestURL
                                            isMainFrame:frame.mainFrame]) {
    completionHandler(NO);
    return;
  }

  self.webStateImpl->RunJavaScriptConfirmDialog(
      requestURL, message, base::BindOnce(completionHandler));
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

  DCHECK(completionHandler);

  GURL requestURL = net::GURLWithNSURL(frame.request.URL);
  if (![self shouldPresentJavaScriptDialogForRequestURL:requestURL
                                            isMainFrame:frame.mainFrame]) {
    completionHandler(nil);
    return;
  }

  self.webStateImpl->RunJavaScriptPromptDialog(
      requestURL, prompt, defaultText, base::BindOnce(completionHandler));
}

- (void)webView:(WKWebView*)webView
    contextMenuConfigurationForElement:(WKContextMenuElementInfo*)elementInfo
                     completionHandler:
                         (void (^)(UIContextMenuConfiguration* _Nullable))
                             completionHandler {
  web::WebStateDelegate* delegate = self.webStateImpl->GetDelegate();
  if (!delegate) {
    completionHandler(nil);
    return;
  }

  web::ContextMenuParams params;
  params.link_url = net::GURLWithNSURL(elementInfo.linkURL);

  delegate->ContextMenuConfiguration(self.webStateImpl, params,
                                     completionHandler);
}

- (void)webView:(WKWebView*)webView
     contextMenuForElement:(WKContextMenuElementInfo*)elementInfo
    willCommitWithAnimator:
        (id<UIContextMenuInteractionCommitAnimating>)animator {
  web::WebStateDelegate* delegate = self.webStateImpl->GetDelegate();
  if (!delegate) {
    return;
  }

  delegate->ContextMenuWillCommitWithAnimator(self.webStateImpl, animator);
}

#pragma mark - CRWMediaCapturePermissionPresenter

- (web::WebStateImpl*)presentingWebState {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  return self.webStateImpl;
}

#pragma mark - Helper

// Helper that returns whether or not a dialog should be presented for a
// frame with `requestURL`.
- (BOOL)shouldPresentJavaScriptDialogForRequestURL:(const GURL&)requestURL
                                       isMainFrame:(BOOL)isMainFrame {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  // JavaScript dialogs should not be presented if there is no information about
  // the requesting page's URL.
  if (!requestURL.is_valid()) {
    return NO;
  }

  if (isMainFrame && url::Origin::Create(self.webStateImpl->GetVisibleURL()) !=
                         url::Origin::Create(requestURL)) {
    // Dialog was requested by web page's main frame, but visible URL has
    // different origin. This could happen if the user has started a new
    // browser initiated navigation. There is no value in showing dialogs
    // requested by page, which this WebState is about to leave. But presenting
    // the dialog can lead to phishing and other abusive behaviors.
    return NO;
  }

  return YES;
}

@end
