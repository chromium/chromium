// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/coordinator/lens_result_page_mediator.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import <memory>

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/context_menu/ui_bundled/context_menu_configuration_provider.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_result_page_mediator_delegate.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_result_page_consumer.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/snackbar_util.h"
#import "ios/chrome/browser/tabs/model/tab_helper_util.h"
#import "ios/chrome/browser/web/model/web_navigation_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/navigation/web_state_policy_decider_bridge.h"
#import "ios/web/public/ui/context_menu_params.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_delegate.h"
#import "ios/web/public/web_state_delegate_bridge.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "net/base/apple/url_conversions.h"
#import "net/base/url_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

/// Returns whether the navigation is allowed inside of the result page.
BOOL IsValidURLToOpenInResultsPage(const GURL& URL) {
  std::string_view host = URL.host_piece();
  return base::EqualsCaseInsensitiveASCII(host, "google.com") ||
         base::EqualsCaseInsensitiveASCII(host, "www.google.com") ||
         base::EqualsCaseInsensitiveASCII(host, "translate.google.com");
}

/// Detect special URL that requests the bottom sheet resize.
BOOL IsMinimizeBottomSheetURL(const GURL& URL) {
  if (!URL.SchemeIs("ae-action")) {
    return NO;
  }
  std::string_view host = URL.host_piece();
  return base::EqualsCaseInsensitiveASCII(host, "resultpanel-header-show");
}

/// Detect special URL that requests the bottom sheet resize.
BOOL IsMaximizeBottomSheetURL(const GURL& URL) {
  if (!URL.SchemeIs("ae-action")) {
    return NO;
  }
  std::string_view host = URL.host_piece();
  return base::EqualsCaseInsensitiveASCII(host, "resultpanel-header-hide");
}

// Maps `value` of the closed interval [`in_min`, `in_max`] to
// [`out_min`, `out_max`].
float IntervalMap(float value,
                  float in_min,
                  float in_max,
                  float out_min,
                  float out_max) {
  CHECK_GE(value, in_min);
  CHECK_LE(value, in_max);
  return out_min + (value - in_min) * (out_max - out_min) / (in_max - in_min);
}

// Value of the progress bar when lens request starts.
const CGFloat kProgressBarLensRequestStarted = 0.15f;

// Value of the progress bar when a response is received.
const CGFloat kProgressBarLensResponseReceived = 0.40f;

// Value of an empty progress bar.
const CGFloat kProgressBarEmpty = 0.0f;

// Value of a full progress bar.
const CGFloat kProgressBarFull = 1.0f;

// Query parameter for dark mode.
inline constexpr char kDarkModeParameterKey[] = "cs";
inline constexpr char kDarkModeParameterLightValue[] = "0";
inline constexpr char kDarkModeParameterDarkValue[] = "1";

}  // namespace

@interface LensResultPageMediator () <CRWWebStateDelegate,
                                      CRWWebStateObserver,
                                      CRWWebStatePolicyDecider>
@end

@implementation LensResultPageMediator {
  /// WebState for lens results.
  std::unique_ptr<web::WebState> _webState;
  /// WebState delegate from the browser.
  raw_ptr<web::WebStateDelegate> _browserWebStateDelegate;
  /// Web state policy decider.
  std::unique_ptr<web::WebStatePolicyDeciderBridge> _policyDeciderBridge;
  /// Whether the browser is off the record.
  BOOL _isIncognito;
  /// Web state delegate.
  std::unique_ptr<web::WebStateDelegateBridge> _webStateDelegateBridge;
  /// Bridges C++ WebStateObserver methods to this mediator.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;
  /// Whether the inflight request was initiated by Lens.
  BOOL _isInflightRequestLensInitiated;
  /// WebStateList of the presenting browser.
  base::WeakPtr<WebStateList> _webStateList;
  /// Whether the interface is in dark mode.
  BOOL _isDarkMode;
  /// The last commited progress to the loading bar.
  float _lastCommitedProgress;
}

- (instancetype)
     initWithWebStateParams:(const web::WebState::CreateParams&)params
    browserWebStateDelegate:(web::WebStateDelegate*)browserWebStateDelegate
               webStateList:(WebStateList*)webStateList
                isIncognito:(BOOL)isIncognito {
  self = [super init];
  if (self) {
    _browserWebStateDelegate = browserWebStateDelegate;
    _webStateDelegateBridge =
        std::make_unique<web::WebStateDelegateBridge>(self);
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    [self attachWebState:web::WebState::Create(params)];
    _isIncognito = isIncognito;
    if (webStateList) {
      _webStateList = webStateList->AsWeakPtr();
    }
  }
  return self;
}

- (void)setConsumer:(id<LensResultPageConsumer>)consumer {
  _consumer = consumer;
  CHECK(_webState, kLensOverlayNotFatalUntil);
  _webState->SetWebUsageEnabled(true);
  // Mark hidden until the first page has finished loading, preventing a
  // momentary display of the web view's white background.
  [_consumer setWebViewHidden:YES];
  [_consumer setWebView:_webState->GetView()];
}

- (void)setDelegate:(id<LensResultPageMediatorDelegate>)delegate {
  _delegate = delegate;
  if (_webState) {
    [self.delegate lensResultPageDidChangeActiveWebState:_webState.get()];
  }
}

- (void)disconnect {
  if (_webState) {
    [self detachWebState];
    _webState.reset();
  }
  _webStateObserverBridge.reset();
  _webStateDelegateBridge.reset();
}

#pragma mark - LensResultPageMutator

- (void)setIsDarkMode:(BOOL)isDarkMode {
  BOOL darkModeChanged = _isDarkMode != isDarkMode;

  _isDarkMode = isDarkMode;

  if (!_webState) {
    return;
  }

  GURL latestLoadedURL = _webState->GetLastCommittedURL();
  BOOL latestURLValid = latestLoadedURL.is_valid();
  if (darkModeChanged && latestURLValid) {
    // The web view is hidden until the page fully loads to prevent a brief
    // flash of mixed dark and light UI elements.
    [_consumer setWebViewHidden:YES];
    [self loadResultsURL:latestLoadedURL];
  }
}

#pragma mark - LensOverlayResultConsumer

- (void)loadResultsURL:(GURL)URL {
  CHECK(_webState, kLensOverlayNotFatalUntil);

  // Add light/dark mode query parameter.
  URL = net::AppendOrReplaceQueryParameter(
      URL, kDarkModeParameterKey,
      _isDarkMode ? kDarkModeParameterDarkValue : kDarkModeParameterLightValue);

  _isInflightRequestLensInitiated = YES;
  [_consumer setLoadingProgress:kProgressBarLensResponseReceived];

  web::NavigationManager::WebLoadParams webParams =
      web::NavigationManager::WebLoadParams(URL);

  // Add variation headers.
  webParams.extra_headers =
      web_navigation_util::VariationHeadersForURL(URL, _isIncognito);

  _webState->GetNavigationManager()->LoadURLWithParams(webParams);
}

- (void)handleSearchRequestStarted {
  _lastCommitedProgress = kProgressBarLensRequestStarted;
  [_consumer setLoadingProgress:kProgressBarLensRequestStarted];
}

- (void)handleSearchRequestErrored {
  _lastCommitedProgress = kProgressBarFull;
  [_consumer setLoadingProgress:kProgressBarFull];
}

#pragma mark - CRWWebStatePolicyDecider

- (void)shouldAllowRequest:(NSURLRequest*)request
               requestInfo:(web::WebStatePolicyDecider::RequestInfo)requestInfo
           decisionHandler:(PolicyDecisionHandler)decisionHandler {
  GURL URL = net::GURLWithNSURL(request.URL);
  if (requestInfo.target_frame_is_main && !IsValidURLToOpenInResultsPage(URL)) {
    decisionHandler(web::WebStatePolicyDecider::PolicyDecision::Cancel());

    if (URL.IsAboutBlank()) {
      return;
    }

    if (IsMaximizeBottomSheetURL(URL)) {
      [self.presentationDelegate requestMaximizeBottomSheet];
      return;
    }

    if (IsMinimizeBottomSheetURL(URL)) {
      [self.presentationDelegate requestMinimizeBottomSheet];
      return;
    }

    OpenNewTabCommand* command =
        [[OpenNewTabCommand alloc] initWithURL:URL
                                      referrer:web::Referrer()
                                   inIncognito:_isIncognito
                                  inBackground:NO
                                      appendTo:OpenPosition::kCurrentTab];
    [self.applicationHandler openURLInNewTab:command];
    [self.delegate
         lensResultPageMediator:self
        didOpenNewTabFromSource:lens::LensOverlayNewTabSource::kWebNavigation];
  } else {
    decisionHandler(web::WebStatePolicyDecider::PolicyDecision::Allow());
  }
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  [_consumer setWebViewHidden:NO];
}

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigationContext {
  BOOL isSameDocument = navigationContext->IsSameDocument();
  // Disregard same document navigation from initiating progress loading.
  if (!isSameDocument) {
    _lastCommitedProgress = 0;
  }
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigationContext {
  _isInflightRequestLensInitiated = NO;
}

- (void)webState:(web::WebState*)webState
    didChangeLoadingProgress:(double)webStateProgress {
  float progress = webStateProgress;

  // If the current navigation is the direct product of a Lens Overlay search,
  // then the progress from before the search should be factored in, and the
  // webState loading progress should be adjusted to reflect the remaining
  // portion of the overall progress.
  if (_isInflightRequestLensInitiated) {
    progress =
        IntervalMap(webStateProgress, kProgressBarEmpty, kProgressBarFull,
                    kProgressBarLensResponseReceived, kProgressBarFull);
  }

  if (progress <= _lastCommitedProgress) {
    return;
  }

  _lastCommitedProgress = progress;
  [_consumer setLoadingProgress:progress];
}

#pragma mark - CRWWebStateDelegate

- (void)webState:(web::WebState*)webState
    contextMenuConfigurationForParams:(const web::ContextMenuParams&)params
                    completionHandler:(void (^)(UIContextMenuConfiguration*))
                                          completionHandler {
  UIContextMenuConfiguration* configuration =
      [self.contextMenuProvider contextMenuConfigurationForWebState:webState
                                                             params:params];
  completionHandler(configuration);
}

- (void)webState:(web::WebState*)webState
    contextMenuWillCommitWithAnimator:
        (id<UIContextMenuInteractionCommitAnimating>)animator {
  GURL URLToLoad = [self.contextMenuProvider URLToLoad];
  if (!URLToLoad.is_valid()) {
    return;
  }
  if (_webState) {
    web::Referrer referrer(_webState->GetLastCommittedURL(),
                           web::ReferrerPolicyDefault);
    _webState->OpenURL(web::WebState::OpenURLParams(
        URLToLoad, referrer, WindowOpenDisposition::CURRENT_TAB,
        ui::PAGE_TRANSITION_LINK, false));
  }
}

- (UIView*)webViewContainerForWebState:(web::WebState*)webState {
  if (CGRectIsEmpty(self.webViewContainer.frame)) {
    return nil;
  }
  return self.webViewContainer;
}

- (void)closeWebState:(web::WebState*)webState {
  // This should not happen in the result page.
  NOTREACHED(kLensOverlayNotFatalUntil);
}

#pragma mark CRWWebStateDelegate with _browserWebStateDelegate

- (web::WebState*)webState:(web::WebState*)webState
    createNewWebStateForURL:(const GURL&)URL
                  openerURL:(const GURL&)openerURL
            initiatedByUser:(BOOL)initiatedByUser {
  return _browserWebStateDelegate->CreateNewWebState(webState, URL, openerURL,
                                                     initiatedByUser);
}

- (web::WebState*)webState:(web::WebState*)webState
         openURLWithParams:(const web::WebState::OpenURLParams&)params {
  return _browserWebStateDelegate->OpenURLFromWebState(webState, params);
}

- (web::JavaScriptDialogPresenter*)javaScriptDialogPresenterForWebState:
    (web::WebState*)webState {
  return _browserWebStateDelegate->GetJavaScriptDialogPresenter(webState);
}

- (void)webState:(web::WebState*)webState
    handlePermissions:(NSArray<NSNumber*>*)permissions
      decisionHandler:(web::WebStatePermissionDecisionHandler)decisionHandler {
  _browserWebStateDelegate->HandlePermissionsDecisionRequest(
      webState, permissions, decisionHandler);
}

- (void)webState:(web::WebState*)webState
    didRequestHTTPAuthForProtectionSpace:(NSURLProtectionSpace*)protectionSpace
                      proposedCredential:(NSURLCredential*)proposedCredential
                       completionHandler:(void (^)(NSString* username,
                                                   NSString* password))handler {
  _browserWebStateDelegate->OnAuthRequired(
      webState, protectionSpace, proposedCredential, base::BindOnce(handler));
}

// This API can be used to show custom input views in the web view.
- (id<CRWResponderInputView>)webStateInputViewProvider:
    (web::WebState*)webState {
  return _browserWebStateDelegate->GetResponderInputView(webState);
}

#pragma mark - ContextMenuConfigurationProviderDelegate

- (void)contextMenuConfigurationProvider:
            (ContextMenuConfigurationProvider*)configurationProvider
        didOpenNewTabInBackgroundWithURL:(GURL)URL {
  MDCSnackbarMessage* snackbarMessage = CreateSnackbarMessage(
      l10n_util::GetNSString(IDS_IOS_LENS_OVERLAY_NEW_TAB_MESSAGE));
  MDCSnackbarMessageAction* action = [[MDCSnackbarMessageAction alloc] init];
  __weak __typeof__(self) weakSelf = self;
  action.handler = ^() {
    [weakSelf activateWebStateWithURL:URL];
  };
  action.title = l10n_util::GetNSString(IDS_IOS_LENS_OVERLAY_GO_TO_NEW_TAB);
  action.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_LENS_OVERLAY_GO_TO_NEW_TAB);
  snackbarMessage.action = action;
  [self.snackbarHandler showSnackbarMessage:snackbarMessage bottomOffset:0];
  [self.delegate
       lensResultPageMediator:self
      didOpenNewTabFromSource:lens::LensOverlayNewTabSource::kContextMenu];
}

#pragma mark - LensWebProvider

- (web::WebState*)webState {
  if (!_webState) {
    return nullptr;
  }
  return _webState.get();
}

#pragma mark - Private

/// Detaches and returns the current web state.
- (std::unique_ptr<web::WebState>)detachWebState {
  CHECK(_webState, kLensOverlayNotFatalUntil);
  _policyDeciderBridge.reset();
  _webState->RemoveObserver(_webStateObserverBridge.get());
  _webState->SetDelegate(nullptr);
  return std::move(_webState);
}

/// Attaches `webState` to the mediator.
- (void)attachWebState:(std::unique_ptr<web::WebState>)webState {
  /// Detach the current web state before attaching a new one.
  CHECK(!_webState, kLensOverlayNotFatalUntil);
  CHECK(!_policyDeciderBridge, kLensOverlayNotFatalUntil);
  _webState = std::move(webState);
  _webState->SetDelegate(_webStateDelegateBridge.get());
  _webState->AddObserver(_webStateObserverBridge.get());
  _policyDeciderBridge =
      std::make_unique<web::WebStatePolicyDeciderBridge>(_webState.get(), self);
  AttachTabHelpers(_webState.get(), TabHelperFilter::kBottomSheet);
  id<CRWWebViewProxy> webViewProxy = _webState->GetWebViewProxy();
  webViewProxy.allowsBackForwardNavigationGestures = NO;
  // Allow the scrollView to cover the safe area.
  webViewProxy.scrollViewProxy.clipsToBounds = NO;

  if (self.consumer) {
    _webState->SetWebUsageEnabled(true);
    [self.consumer setWebView:_webState->GetView()];
  }
  [self.delegate lensResultPageDidChangeActiveWebState:_webState.get()];
}

/// Activates the web state with the given `URL`.
- (void)activateWebStateWithURL:(GURL)URL {
  if (WebStateList* webStateList = _webStateList.get()) {
    int index = webStateList->GetIndexOfWebStateWithURL(URL);
    if (index != WebStateList::kInvalidIndex) {
      webStateList->ActivateWebStateAt(index);
    }
  }
}

#pragma mark - CRWWebStateObserver

- (void)webStateDestroyed:(web::WebState*)webState {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
    _webStateObserverBridge.reset();
  }

  [self.delegate lensResultPageWebStateDestroyed];
}

@end
