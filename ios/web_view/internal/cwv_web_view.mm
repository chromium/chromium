// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/cwv_web_view_internal.h"

#include <memory>
#include <unordered_map>
#include <utility>

#import <WebKit/WebKit.h>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/mac/foundation_util.h"
#import "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#import "components/autofill/ios/browser/autofill_agent.h"
#include "components/autofill/ios/form_util/unique_id_data_tab_helper.h"
#include "components/password_manager/core/browser/password_manager.h"
#import "components/password_manager/ios/password_controller_driver_helper.h"
#import "components/password_manager/ios/shared_password_controller.h"
#import "components/safe_browsing/ios/browser/safe_browsing_url_allow_list.h"
#include "components/url_formatter/elide_url.h"
#include "google_apis/google_api_keys.h"
#import "ios/components/security_interstitials/lookalikes/lookalike_url_container.h"
#import "ios/components/security_interstitials/lookalikes/lookalike_url_tab_allow_list.h"
#import "ios/components/security_interstitials/lookalikes/lookalike_url_tab_helper.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_client.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_query_manager.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_tab_helper.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_unsafe_resource_container.h"
#include "ios/web/public/favicon/favicon_url.h"
#include "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/navigation/referrer.h"
#include "ios/web/public/navigation/reload_type.h"
#import "ios/web/public/ui/context_menu_params.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_view_only/wk_web_view_configuration_util.h"
#include "ios/web_view/internal/app/application_context.h"
#import "ios/web_view/internal/autofill/cwv_autofill_controller_internal.h"
#import "ios/web_view/internal/autofill/web_view_autofill_client_ios.h"
#import "ios/web_view/internal/cwv_back_forward_list_internal.h"
#import "ios/web_view/internal/cwv_favicon_internal.h"
#import "ios/web_view/internal/cwv_html_element_internal.h"
#import "ios/web_view/internal/cwv_navigation_action_internal.h"
#import "ios/web_view/internal/cwv_ssl_status_internal.h"
#import "ios/web_view/internal/cwv_web_view_configuration_internal.h"
#import "ios/web_view/internal/language/web_view_url_language_histogram_factory.h"
#import "ios/web_view/internal/passwords/web_view_password_manager_client.h"
#import "ios/web_view/internal/safe_browsing/web_view_safe_browsing_client_factory.h"
#import "ios/web_view/internal/translate/cwv_translation_controller_internal.h"
#import "ios/web_view/internal/translate/web_view_translate_client.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#include "ios/web_view/internal/web_view_global_state_util.h"
#import "ios/web_view/internal/web_view_java_script_dialog_presenter.h"
#import "ios/web_view/internal/web_view_message_handler_java_script_feature.h"
#import "ios/web_view/internal/web_view_web_state_policy_decider.h"
#import "ios/web_view/public/cwv_navigation_delegate.h"
#import "ios/web_view/public/cwv_preview_element_info.h"
#import "ios/web_view/public/cwv_ui_delegate.h"
#import "ios/web_view/public/cwv_web_view_configuration.h"
#import "net/base/mac/url_conversions.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// A key used in NSCoder to store the session storage object.
NSString* const kSessionStorageKey = @"sessionStorage";

// Converts base::Value expected to be a dictionary or list to NSDictionary or
// NSArray, respectively.
id NSObjectFromCollectionValue(const base::Value* value) {
  DCHECK(value->is_dict() || value->is_list())
      << "Incorrect value type: " << value->type();

  std::string json;
  const bool success = base::JSONWriter::Write(*value, &json);
  DCHECK(success) << "Failed to convert base::Value to JSON";

  NSData* json_data = [NSData dataWithBytes:json.c_str() length:json.length()];
  id ns_object = [NSJSONSerialization JSONObjectWithData:json_data
                                                 options:kNilOptions
                                                   error:nil];
  DCHECK(ns_object) << "Failed to convert JSON to Collection";
  return ns_object;
}

// Converts base::Value to an appropriate Obj-C object.
// |value| must not be null.
id NSObjectFromValue(const base::Value* value) {
  switch (value->type()) {
    case base::Value::Type::NONE:
      return nil;
    case base::Value::Type::BOOLEAN:
      return @(value->GetBool());
    case base::Value::Type::INTEGER:
      return @(value->GetInt());
    case base::Value::Type::DOUBLE:
      return @(value->GetDouble());
    case base::Value::Type::STRING:
      return base::SysUTF8ToNSString(value->GetString());
    case base::Value::Type::BINARY:
      // Unsupported.
      return nil;
    case base::Value::Type::DICT:
    case base::Value::Type::LIST:
      return NSObjectFromCollectionValue(value);
  }
  return nil;
}

// Converts base::Value::Dict to NSDictionary.
NSDictionary* NSDictionaryFromDictValue(const base::Value::Dict& value) {
  std::string json;
  const bool success = base::JSONWriter::Write(value, &json);
  DCHECK(success) << "Failed to convert base::Value to JSON";

  NSData* json_data = [NSData dataWithBytes:json.c_str() length:json.length()];
  NSDictionary* ns_dictionary = base::mac::ObjCCastStrict<NSDictionary>(
      [NSJSONSerialization JSONObjectWithData:json_data
                                      options:kNilOptions
                                        error:nil]);
  DCHECK(ns_dictionary) << "Failed to convert JSON to NSDictionary";
  return ns_dictionary;
}

// A WebStateUserData to hold a reference to a corresponding CWVWebView.
class WebViewHolder : public web::WebStateUserData<WebViewHolder> {
 public:
  explicit WebViewHolder(web::WebState* web_state) {}
  CWVWebView* web_view() const { return web_view_; }
  void set_web_view(CWVWebView* web_view) { web_view_ = web_view; }

 private:
  friend class web::WebStateUserData<WebViewHolder>;

  __weak CWVWebView* web_view_ = nil;
  WEB_STATE_USER_DATA_KEY_DECL();
};

WEB_STATE_USER_DATA_KEY_IMPL(WebViewHolder)
}  // namespace

@interface CWVWebView () {
  CWVWebViewConfiguration* _configuration;
  std::unique_ptr<web::WebState> _webState;
  std::unique_ptr<web::WebStateDelegateBridge> _webStateDelegate;
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
  std::unique_ptr<ios_web_view::WebViewWebStatePolicyDecider>
      _webStatePolicyDecider;
  double _estimatedProgress;
  // Handles presentation of JavaScript dialogs.
  std::unique_ptr<ios_web_view::WebViewJavaScriptDialogPresenter>
      _javaScriptDialogPresenter;
  CRWSessionStorage* _cachedSessionStorage;
}

// Redefine these properties as readwrite to define setters, which send KVO
// notifications.
@property(nonatomic, readwrite) double estimatedProgress;
@property(nonatomic, readwrite) BOOL canGoBack;
@property(nonatomic, readwrite) BOOL canGoForward;
@property(nonatomic, readwrite) NSURL* lastCommittedURL;
@property(nonatomic, readwrite) BOOL loading;
@property(nonatomic, readwrite, copy) NSString* title;
@property(nonatomic, readwrite) NSURL* visibleURL;
@property(nonatomic, readwrite) NSString* visibleLocationString;
@property(nonatomic, readwrite) CWVSSLStatus* visibleSSLStatus;

// Updates the availability of the back/forward navigation properties exposed
// through |canGoBack| and |canGoForward|, and also updates |backForwardList|.
- (void)updateNavigationAvailability;
// Updates the URLs exposed through |lastCommittedURL| and |visibleURL|.
- (void)updateCurrentURLs;
// Updates |title| property.
- (void)updateTitle;
// Returns a new CWVAutofillController created from |_webState|.
- (CWVAutofillController*)newAutofillController;
// Returns a new CWVTranslationController created from |_webState|.
- (CWVTranslationController*)newTranslationController;
// Updates |_webState| visiblity.
- (void)updateWebStateVisibility;

@end

namespace {
NSString* gCustomUserAgent = nil;
NSString* gUserAgentProduct = nil;
BOOL gChromeContextMenuEnabled = NO;
}  // namespace

@implementation CWVWebView

@synthesize autofillController = _autofillController;
@synthesize backForwardList = _backForwardList;
@synthesize canGoBack = _canGoBack;
@synthesize canGoForward = _canGoForward;
@synthesize configuration = _configuration;
@synthesize estimatedProgress = _estimatedProgress;
@synthesize lastCommittedURL = _lastCommittedURL;
@synthesize loading = _loading;
@synthesize navigationDelegate = _navigationDelegate;
@synthesize title = _title;
@synthesize translationController = _translationController;
@synthesize UIDelegate = _UIDelegate;
@synthesize visibleURL = _visibleURL;
@synthesize visibleSSLStatus = _visibleSSLStatus;

+ (void)initialize {
  if (self != [CWVWebView class]) {
    return;
  }

  ios_web_view::InitializeGlobalState();
}

+ (BOOL)chromeContextMenuEnabled {
  return gChromeContextMenuEnabled;
}

+ (void)setChromeContextMenuEnabled:(BOOL)newValue {
  gChromeContextMenuEnabled = newValue;
}

+ (NSString*)customUserAgent {
  return gCustomUserAgent;
}

+ (void)setCustomUserAgent:(NSString*)customUserAgent {
  gCustomUserAgent = [customUserAgent copy];
}

+ (NSString*)userAgentProduct {
  return gUserAgentProduct;
}

+ (void)setUserAgentProduct:(NSString*)product {
  gUserAgentProduct = [product copy];
}

+ (void)setGoogleAPIKey:(NSString*)googleAPIKey
               clientID:(NSString*)clientID
           clientSecret:(NSString*)clientSecret {
  google_apis::SetAPIKey(base::SysNSStringToUTF8(googleAPIKey));

  std::string clientIDString = base::SysNSStringToUTF8(clientID);
  std::string clientSecretString = base::SysNSStringToUTF8(clientSecret);
  for (size_t i = 0; i < google_apis::CLIENT_NUM_ITEMS; ++i) {
    google_apis::OAuth2Client client =
        static_cast<google_apis::OAuth2Client>(i);
    google_apis::SetOAuth2ClientID(client, clientIDString);
    google_apis::SetOAuth2ClientSecret(client, clientSecretString);
  }
}

+ (CWVWebView*)webViewForWebState:(web::WebState*)webState {
  WebViewHolder* holder = WebViewHolder::FromWebState(webState);
  CWVWebView* webView = holder->web_view();
  DCHECK(webView);
  return webView;
}

- (instancetype)initWithFrame:(CGRect)frame
                configuration:(CWVWebViewConfiguration*)configuration {
  return [self initWithFrame:frame
               configuration:configuration
             WKConfiguration:nil
            createdWKWebView:nil];
}

- (instancetype)initWithFrame:(CGRect)frame
                configuration:(CWVWebViewConfiguration*)configuration
              WKConfiguration:(WKWebViewConfiguration*)wkConfiguration
             createdWKWebView:(WKWebView**)createdWebView {
  self = [super initWithFrame:frame];
  if (self) {
    _configuration = configuration;
    [_configuration registerWebView:self];

    [self resetWebStateWithSessionStorage:nil
                          WKConfiguration:wkConfiguration
                         createdWKWebView:createdWebView];
  }
  return self;
}

- (UIScrollView*)scrollView {
  return [_webState->GetWebViewProxy().scrollViewProxy asUIScrollView];
}

- (BOOL)allowsBackForwardNavigationGestures {
  return _webState->GetWebViewProxy().allowsBackForwardNavigationGestures;
}

- (void)setAllowsBackForwardNavigationGestures:
    (BOOL)allowsBackForwardNavigationGestures {
  _webState->GetWebViewProxy().allowsBackForwardNavigationGestures =
      allowsBackForwardNavigationGestures;
}

- (void)dealloc {
  if (_webState) {
    if (_webStateObserver) {
      _webState->RemoveObserver(_webStateObserver.get());
      _webStateObserver.reset();
    }
    WebViewHolder::RemoveFromWebState(_webState.get());
  }
}

- (void)goBack {
  if (_webState->GetNavigationManager())
    _webState->GetNavigationManager()->GoBack();
}

- (void)goForward {
  if (_webState->GetNavigationManager())
    _webState->GetNavigationManager()->GoForward();
}

- (BOOL)goToBackForwardListItem:(CWVBackForwardListItem*)item {
  if (!_backForwardList) {
    return NO;  // Do nothing if |_backForwardList| is not generated yet.
  }

  if ([item isEqual:_backForwardList.currentItem]) {
    return NO;
  }

  int index = [_backForwardList internalIndexOfItem:item];
  if (index == -1) {
    return NO;
  }

  DCHECK(_webState);
  web::NavigationManager* navigationManager = _webState->GetNavigationManager();
  navigationManager->GoToIndex(index);
  return YES;
}

- (void)reload {
  // |check_for_repost| is false because CWVWebView does not support repost form
  // dialogs.
  _webState->GetNavigationManager()->Reload(web::ReloadType::NORMAL,
                                            false /* check_for_repost */);
}

- (void)stopLoading {
  _webState->Stop();
}

- (void)loadRequest:(NSURLRequest*)request {
  DCHECK_EQ(nil, request.HTTPBodyStream)
      << "request.HTTPBodyStream is not supported.";

  web::NavigationManager::WebLoadParams params(net::GURLWithNSURL(request.URL));
  params.transition_type = ui::PAGE_TRANSITION_TYPED;
  params.extra_headers = [request.allHTTPHeaderFields copy];
  params.post_data = [request.HTTPBody copy];
  _webState->GetNavigationManager()->LoadURLWithParams(params);
  [self updateCurrentURLs];
}

- (void)evaluateJavaScript:(NSString*)javaScriptString
                completion:(void (^)(id, NSError*))completion {
  web::WebFrame* mainFrame =
      _webState->GetPageWorldWebFramesManager()->GetMainWebFrame();
  if (!mainFrame) {
    if (completion) {
      completion(nil, [NSError errorWithDomain:@"org.chromium.chromewebview"
                                          code:0
                                      userInfo:nil]);
    }
    return;
  }

  if (!completion) {
    mainFrame->ExecuteJavaScript(base::SysNSStringToUTF16(javaScriptString));
    return;
  }

  mainFrame->ExecuteJavaScript(
      base::SysNSStringToUTF16(javaScriptString),
      base::BindOnce(^(const base::Value* result, NSError* error) {
        id jsResult = nil;
        if (!error && result) {
          jsResult = NSObjectFromValue(result);
        }
        completion(jsResult, error);
      }));
}

- (void)setUIDelegate:(id<CWVUIDelegate>)UIDelegate {
  _UIDelegate = UIDelegate;

  _javaScriptDialogPresenter->SetUIDelegate(_UIDelegate);
}

- (void)setNavigationDelegate:(id<CWVNavigationDelegate>)navigationDelegate {
  _navigationDelegate = navigationDelegate;

  [self attachSecurityInterstitialHelpersToWebStateIfNecessary];
}

#pragma mark - UIResponder

- (BOOL)becomeFirstResponder {
  if (_webState) {
    return [_webState->GetWebViewProxy() becomeFirstResponder];
  } else {
    return [super becomeFirstResponder];
  }
}

#pragma mark - UIView

- (void)didMoveToSuperview {
  [super didMoveToSuperview];

  [self updateWebStateVisibility];
}

#pragma mark - CRWWebStateObserver

- (void)webStateDestroyed:(web::WebState*)webState {
  webState->RemoveObserver(_webStateObserver.get());
  _webStateObserver.reset();
}

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation {
  [self updateNavigationAvailability];

  if (!navigation->IsSameDocument()) {
    SEL oldSelector = @selector(webViewDidStartProvisionalNavigation:);
    if ([_navigationDelegate respondsToSelector:oldSelector]) {
      [_navigationDelegate webViewDidStartProvisionalNavigation:self];
    }
    SEL newSelector = @selector(webViewDidStartNavigation:);
    if ([_navigationDelegate respondsToSelector:newSelector]) {
      [_navigationDelegate webViewDidStartNavigation:self];
    }
  }
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  [self updateNavigationAvailability];
  [self updateCurrentURLs];

  // TODO(crbug.com/898357): Remove this once crbug.com/898357 is fixed.
  [self updateVisibleSSLStatus];

  if (navigation->HasCommitted() && !navigation->IsSameDocument() &&
      [_navigationDelegate
          respondsToSelector:@selector(webViewDidCommitNavigation:)]) {
    [_navigationDelegate webViewDidCommitNavigation:self];
  }

  NSError* error = navigation->GetError();
  SEL selector = @selector(webView:didFailNavigationWithError:);
  if (error && [_navigationDelegate respondsToSelector:selector]) {
    [_navigationDelegate webView:self didFailNavigationWithError:error];
  }
}

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  DCHECK_EQ(_webState.get(), webState);
  if (!success) {
    // Failure callbacks will be handled inside |webState:didFinishNavigation:|.
    return;
  }

  // TODO(crbug.com/1374071): Fragment navigations currently skip calling
  // `webViewDidStartNavigation:` and `webViewDidCommitNavigation:`, and instead
  // only calls `webViewDidFinishNavigation:` below. Fix this inconsistency.
  SEL selector = @selector(webViewDidFinishNavigation:);
  if ([_navigationDelegate respondsToSelector:selector]) {
    [_navigationDelegate webViewDidFinishNavigation:self];
  }
}

- (void)webState:(web::WebState*)webState
    didChangeLoadingProgress:(double)progress {
  self.estimatedProgress = progress;
}

- (void)webStateDidChangeBackForwardState:(web::WebState*)webState {
  [self updateNavigationAvailability];
}

- (void)webStateDidStopLoading:(web::WebState*)webState {
  self.loading = _webState->IsLoading();
}

- (void)webStateDidStartLoading:(web::WebState*)webState {
  self.loading = _webState->IsLoading();
}

- (void)webStateDidChangeTitle:(web::WebState*)webState {
  [self updateTitle];
}

- (void)webStateDidChangeVisibleSecurityState:(web::WebState*)webState {
  [self updateVisibleSSLStatus];
}

- (void)renderProcessGoneForWebState:(web::WebState*)webState {
  SEL selector = @selector(webViewWebContentProcessDidTerminate:);
  if ([_navigationDelegate respondsToSelector:selector]) {
    [_navigationDelegate webViewWebContentProcessDidTerminate:self];
  }
}

#pragma mark - CRWWebStateDelegate

- (web::WebState*)webState:(web::WebState*)webState
    createNewWebStateForURL:(const GURL&)URL
                  openerURL:(const GURL&)openerURL
            initiatedByUser:(BOOL)initiatedByUser {
  SEL selector =
      @selector(webView:createWebViewWithConfiguration:forNavigationAction:);
  if (![_UIDelegate respondsToSelector:selector]) {
    return nullptr;
  }

  NSURLRequest* request =
      [[NSURLRequest alloc] initWithURL:net::NSURLWithGURL(URL)];

  // The current implemention can't get the real navigation type for the
  // navigation action which causes a new web view be created. So uses
  // `CWVNavigationTypeNewWindow` before the real navigation type can be gotten
  // here.
  CWVNavigationAction* navigationAction =
      [[CWVNavigationAction alloc] initWithRequest:request
                                     userInitiated:initiatedByUser
                                    navigationType:CWVNavigationTypeNewWindow];
  CWVWebView* webView = [_UIDelegate webView:self
              createWebViewWithConfiguration:_configuration
                         forNavigationAction:navigationAction];
  if (!webView) {
    return nullptr;
  }
  web::WebState* webViewWebState = webView->_webState.get();
  webViewWebState->SetHasOpener(true);
  return webViewWebState;
}

- (void)closeWebState:(web::WebState*)webState {
  SEL selector = @selector(webViewDidClose:);
  if ([_UIDelegate respondsToSelector:selector]) {
    [_UIDelegate webViewDidClose:self];
  }
}

- (web::WebState*)webState:(web::WebState*)webState
         openURLWithParams:(const web::WebState::OpenURLParams&)params {
  web::NavigationManager::WebLoadParams load_params(params.url);
  load_params.referrer = params.referrer;
  load_params.transition_type = params.transition;
  load_params.is_renderer_initiated = params.is_renderer_initiated;
  load_params.virtual_url = params.virtual_url;
  _webState->GetNavigationManager()->LoadURLWithParams(load_params);
  [self updateCurrentURLs];
  return _webState.get();
}

- (web::JavaScriptDialogPresenter*)javaScriptDialogPresenterForWebState:
    (web::WebState*)webState {
  return _javaScriptDialogPresenter.get();
}

- (void)webState:(web::WebState*)webState
    handlePermissions:(NSArray<NSNumber*>*)permissions
      decisionHandler:(web::WebStatePermissionDecisionHandler)decisionHandler
    API_AVAILABLE(ios(15.0)) {
  DCHECK(decisionHandler);
  CWVMediaCaptureType mediaCaptureType;
  BOOL cameraPermissionRequested =
      [permissions containsObject:@(web::PermissionCamera)];
  BOOL micPermissionRequested =
      [permissions containsObject:@(web::PermissionMicrophone)];
  if (cameraPermissionRequested && micPermissionRequested) {
    mediaCaptureType = CWVMediaCaptureTypeCameraAndMicrophone;
  } else if (cameraPermissionRequested) {
    mediaCaptureType = CWVMediaCaptureTypeCamera;
  } else if (micPermissionRequested) {
    mediaCaptureType = CWVMediaCaptureTypeMicrophone;
  } else {
    NOTREACHED() << "Unknown media permissions";
  }

  SEL selector = @selector(webView:
      requestMediaCapturePermissionForType:decisionHandler:);
  if ([_UIDelegate respondsToSelector:selector]) {
    [_UIDelegate webView:self
        requestMediaCapturePermissionForType:mediaCaptureType
                             decisionHandler:^(CWVPermissionDecision decision) {
                               switch (decision) {
                                 case CWVPermissionDecisionPrompt:
                                   decisionHandler(
                                       web::
                                           PermissionDecisionShowDefaultPrompt);
                                   break;
                                 case CWVPermissionDecisionGrant:
                                   decisionHandler(
                                       web::PermissionDecisionGrant);
                                   break;
                                 case CWVPermissionDecisionDeny:
                                   decisionHandler(web::PermissionDecisionDeny);
                                   break;
                               }
                             }];
  } else {
    decisionHandler(web::PermissionDecisionShowDefaultPrompt);
  }
}

- (void)webState:(web::WebState*)webState
    contextMenuConfigurationForParams:(const web::ContextMenuParams&)params
                    completionHandler:(void (^)(UIContextMenuConfiguration*))
                                          completionHandler {
  SEL selector = @selector(webView:
      contextMenuConfigurationForElement:completionHandler:);
  if ([_UIDelegate respondsToSelector:selector]) {
    NSURL* hyperlink = net::NSURLWithGURL(params.link_url);
    NSURL* mediaSource = net::NSURLWithGURL(params.src_url);
    CWVHTMLElement* HTMLElement =
        [[CWVHTMLElement alloc] initWithHyperlink:hyperlink
                                      mediaSource:mediaSource
                                             text:params.text];

    [_UIDelegate webView:self
        contextMenuConfigurationForElement:HTMLElement
                         completionHandler:completionHandler];
  } else {
    completionHandler(nil);
  }
}

- (void)webState:(web::WebState*)webState
    contextMenuWillCommitWithAnimator:
        (id<UIContextMenuInteractionCommitAnimating>)animator {
  SEL selector = @selector(webView:contextMenuWillCommitWithAnimator:);
  if ([_UIDelegate respondsToSelector:selector]) {
    [_UIDelegate webView:self contextMenuWillCommitWithAnimator:animator];
  }
}

- (void)webState:(web::WebState*)webState
    didUpdateFaviconURLCandidates:
        (const std::vector<web::FaviconURL>&)candidates {
  if ([_UIDelegate respondsToSelector:@selector(webView:didLoadFavicons:)]) {
    [_UIDelegate webView:self
         didLoadFavicons:[CWVFavicon faviconsFromFaviconURLs:candidates]];
  }
}

- (id<CRWResponderInputView>)webStateInputViewProvider:
    (web::WebState*)webState {
  if (self.inputAccessoryView != nil) {
    return self;
  } else {
    return nil;
  }
}

- (void)addMessageHandler:(void (^)(NSDictionary* payload))handler
               forCommand:(NSString*)nsCommand {
  DCHECK(handler);
  std::string command = base::SysNSStringToUTF8(nsCommand);
  WebViewMessageHandlerJavaScriptFeature::GetInstance()->RegisterHandler(
      command, base::BindRepeating(^(const base::Value::Dict& payload) {
        handler(NSDictionaryFromDictValue(payload));
      }));
}

- (void)removeMessageHandlerForCommand:(NSString*)nsCommand {
  std::string command = base::SysNSStringToUTF8(nsCommand);
  WebViewMessageHandlerJavaScriptFeature::GetInstance()->UnregisterHandler(
      command);
}

#pragma mark - Translation

- (CWVTranslationController*)translationController {
  if (!_translationController) {
    _translationController = [self newTranslationController];
  }
  return _translationController;
}

- (CWVTranslationController*)newTranslationController {
  ios_web_view::WebViewBrowserState* browserState =
      ios_web_view::WebViewBrowserState::FromBrowserState(
          _webState->GetBrowserState());
  auto translateClient = ios_web_view::WebViewTranslateClient::Create(
      browserState, _webState.get());
  return [[CWVTranslationController alloc]
      initWithWebState:_webState.get()
       translateClient:std::move(translateClient)];
}

#pragma mark - Autofill

- (CWVAutofillController*)autofillController {
  if (!_autofillController) {
    _autofillController = [self newAutofillController];
  }
  return _autofillController;
}

- (CWVAutofillController*)newAutofillController {
  UniqueIDDataTabHelper::CreateForWebState(_webState.get());

  auto autofillClient = autofill::WebViewAutofillClientIOS::Create(
      _webState.get(), _configuration.browserState);
  AutofillAgent* autofillAgent = [[AutofillAgent alloc]
      initWithPrefService:_configuration.browserState->GetPrefs()
                 webState:_webState.get()];

  auto passwordManagerClient =
      ios_web_view::WebViewPasswordManagerClient::Create(
          _webState.get(), _configuration.browserState);
  auto passwordManager = std::make_unique<password_manager::PasswordManager>(
      passwordManagerClient.get());

  PasswordFormHelper* formHelper =
      [[PasswordFormHelper alloc] initWithWebState:_webState.get()];
  PasswordSuggestionHelper* suggestionHelper =
      [[PasswordSuggestionHelper alloc] initWithWebState:_webState.get()];
  PasswordControllerDriverHelper* driverHelper =
      [[PasswordControllerDriverHelper alloc] initWithWebState:_webState.get()];
  SharedPasswordController* passwordController =
      [[SharedPasswordController alloc] initWithWebState:_webState.get()
                                                 manager:passwordManager.get()
                                              formHelper:formHelper
                                        suggestionHelper:suggestionHelper
                                            driverHelper:driverHelper];

  return [[CWVAutofillController alloc]
           initWithWebState:_webState.get()
             autofillClient:std::move(autofillClient)
              autofillAgent:autofillAgent
            passwordManager:std::move(passwordManager)
      passwordManagerClient:std::move(passwordManagerClient)
         passwordController:passwordController
          applicationLocale:ios_web_view::ApplicationContext::GetInstance()
                                ->GetApplicationLocale()];
}

#pragma mark - Preserving and Restoring State

- (void)encodeRestorableStateWithCoder:(NSCoder*)coder {
  [super encodeRestorableStateWithCoder:coder];

  // It is possible for this instance to be encoded when the |_webState| is a
  // nullptr, i.e. when this method is called after |shutDown| has occurred.
  CRWSessionStorage* sessionStorage;
  if (_webState) {
    sessionStorage = _webState->BuildSessionStorage();
  } else if (_cachedSessionStorage) {
    sessionStorage = _cachedSessionStorage;
  } else {
    return;
  }
  [coder encodeObject:sessionStorage forKey:kSessionStorageKey];
}

- (void)decodeRestorableStateWithCoder:(NSCoder*)coder {
  [super decodeRestorableStateWithCoder:coder];
  CRWSessionStorage* sessionStorage =
      [coder decodeObjectForKey:kSessionStorageKey];
  [self resetWebStateWithSessionStorage:sessionStorage
                        WKConfiguration:nil
                       createdWKWebView:nil];
}

#pragma mark - Private methods

- (void)updateWebStateVisibility {
  if (_webState == nullptr) {
    return;
  }
  if (self.superview) {
    _webState->WasShown();
  } else {
    _webState->WasHidden();
  }
}

// Creates a WebState instance and assigns it to |_webState|.
// It replaces the old |_webState| if any.
// The WebState is restored from |sessionStorage| if provided.
//
// If |wkConfiguration| is provided, the underlying WKWebView is
// initialized with |wkConfiguration|, and assigned to
// |*createdWKWebView| if |createdWKWebView| is not nil.
// |*createdWKWebView| will be provided only if |wkConfiguration| is provided,
// otherwise it will always be reset to nil.
- (void)resetWebStateWithSessionStorage:(CRWSessionStorage*)sessionStorage
                        WKConfiguration:(WKWebViewConfiguration*)wkConfiguration
                       createdWKWebView:(WKWebView**)createdWebView {
  if (_webState) {
    if (_webStateObserver) {
      _webState->RemoveObserver(_webStateObserver.get());
    }
    WebViewHolder::RemoveFromWebState(_webState.get());
    if (_webState->GetView().superview == self) {
      // The web view provided by the old |_webState| has been added as a
      // subview. It must be removed and replaced with a new |_webState|'s web
      // view, which is added later.
      [_webState->GetView() removeFromSuperview];
    }
  }

  BOOL allowsBackForwardNavigationGestures =
      _webState &&
      _webState->GetWebViewProxy().allowsBackForwardNavigationGestures;

  web::WebState::CreateParams webStateCreateParams(_configuration.browserState);
  if (sessionStorage) {
    _webState = web::WebState::CreateWithStorageSession(webStateCreateParams,
                                                        sessionStorage);
  } else {
    _webState = web::WebState::Create(webStateCreateParams);
  }

  // WARNING: NOTHING should be here between |web::WebState::Create()| and
  // |web::EnsureWebViewCreatedWithConfiguration()|, as this is the requirement
  // of |web::EnsureWebViewCreatedWithConfiguration()|

  WKWebView* webView = nil;
  if (wkConfiguration) {
    // When |wkConfiguration| is nil, |self| could be a newly opened web view
    // e.g., triggered by JavaScript "window.open()" function. In that case, if
    // |self| is not created by the WKWebViewConfiguration provided by WebKit's
    // delegate method
    // (https://cs.chromium.org/chromium/src/ios/web/web_state/ui/crw_wk_ui_handler.mm?q=crw_wk_ui_handler&sq=package:chromium&dr=C&l=61)
    // then calling |web::EnsureWebViewCreatedWithConfiguration()| here would
    // result in a crash (https://crbug.com/1054276). Now, we lazily create the
    // WKWebView inside |_webState| when |wkConfiguration| is not nil, and the
    // correct WKWebViewConfiguration will be passed inside //ios/web.
    webView = web::EnsureWebViewCreatedWithConfiguration(_webState.get(),
                                                         wkConfiguration);
  }

  if (createdWebView) {
    // If the created webView is needed, returns it by the out variable way.
    *createdWebView = webView;
  }

  [self attachSecurityInterstitialHelpersToWebStateIfNecessary];
  WebViewHolder::CreateForWebState(_webState.get());
  WebViewHolder::FromWebState(_webState.get())->set_web_view(self);

  if (!_webStateObserver) {
    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
  }
  _webState->AddObserver(_webStateObserver.get());

  if (_backForwardList) {
    _backForwardList.navigationManager = _webState->GetNavigationManager();
  }
  [self updateWebStateVisibility];

  _webStateDelegate = std::make_unique<web::WebStateDelegateBridge>(self);
  _webState->SetDelegate(_webStateDelegate.get());

  _webStatePolicyDecider =
      std::make_unique<ios_web_view::WebViewWebStatePolicyDecider>(
          _webState.get(), self);

  _javaScriptDialogPresenter =
      std::make_unique<ios_web_view::WebViewJavaScriptDialogPresenter>(self,
                                                                       nullptr);

  _webState->GetWebViewProxy().allowsBackForwardNavigationGestures =
      allowsBackForwardNavigationGestures;

  if (_translationController) {
    id<CWVTranslationControllerDelegate> delegate =
        _translationController.delegate;
    _translationController = [self newTranslationController];
    _translationController.delegate = delegate;
  }

  // Recreate and restore the delegate only if previously lazily loaded.
  if (_autofillController) {
    id<CWVAutofillControllerDelegate> delegate = _autofillController.delegate;
    _autofillController = [self newAutofillController];
    _autofillController.delegate = delegate;
  }

  [self addInternalWebViewAsSubview];

  [self updateNavigationAvailability];
  [self updateCurrentURLs];
  [self updateTitle];
  [self updateVisibleSSLStatus];
  self.loading = NO;
  self.estimatedProgress = 0.0;

  // TODO(crbug.com/873729): The session will not be restored until
  // LoadIfNecessary call. Fix the bug and remove extra call.
  if (sessionStorage) {
    _webState->GetNavigationManager()->LoadIfNecessary();
  }
}

// Adds the web view provided by |_webState| as a subview unless it has already.
- (void)addInternalWebViewAsSubview {
  UIView* subview = _webState->GetView();
  if (subview.superview == self) {
    return;
  }
  subview.frame = self.bounds;
  subview.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  [self addSubview:subview];
}

- (CWVBackForwardList*)backForwardList {
  if (!_backForwardList) {
    _backForwardList = [[CWVBackForwardList alloc]
        initWithNavigationManager:_webState->GetNavigationManager()];
  }
  return _backForwardList;
}

- (void)updateNavigationAvailability {
  self.canGoBack = _webState && _webState->GetNavigationManager()->CanGoBack();
  self.canGoForward =
      _webState && _webState->GetNavigationManager()->CanGoForward();

  self.backForwardList.navigationManager = _webState->GetNavigationManager();
}

- (void)updateCurrentURLs {
  self.lastCommittedURL = net::NSURLWithGURL(_webState->GetLastCommittedURL());
  self.visibleURL = net::NSURLWithGURL(_webState->GetVisibleURL());
  self.visibleLocationString = base::SysUTF16ToNSString(
      url_formatter::FormatUrlForSecurityDisplay(_webState->GetVisibleURL()));
}

- (void)updateTitle {
  self.title = base::SysUTF16ToNSString(_webState->GetTitle());
}

- (void)updateVisibleSSLStatus {
  web::NavigationItem* visibleItem =
      _webState->GetNavigationManager()->GetVisibleItem();
  if (visibleItem) {
    self.visibleSSLStatus =
        [[CWVSSLStatus alloc] initWithInternalStatus:visibleItem->GetSSL()];
  } else {
    self.visibleSSLStatus = nil;
  }
}

- (void)attachSecurityInterstitialHelpersToWebStateIfNecessary {
  if (!_webState) {
    return;
  }

  // Lookalike URLs should only be intercepted if handled by the delegate.
  if ([_navigationDelegate respondsToSelector:@selector
                           (webView:handleLookalikeURLWithHandler:)]) {
    LookalikeUrlTabHelper::CreateForWebState(_webState.get());
    LookalikeUrlTabAllowList::CreateForWebState(_webState.get());
    LookalikeUrlContainer::CreateForWebState(_webState.get());
  } else {
    LookalikeUrlTabHelper::RemoveFromWebState(_webState.get());
    LookalikeUrlTabAllowList::RemoveFromWebState(_webState.get());
    LookalikeUrlContainer::RemoveFromWebState(_webState.get());
  }

  // Unsafe URLs should only be intercepted if handled by the delegate.
  if ([_navigationDelegate
          respondsToSelector:@selector(webView:handleUnsafeURLWithHandler:)]) {
    SafeBrowsingClient* client =
        ios_web_view::WebViewSafeBrowsingClientFactory::GetForBrowserState(
            _webState->GetBrowserState());
    SafeBrowsingQueryManager::CreateForWebState(_webState.get(), client);
    SafeBrowsingTabHelper::CreateForWebState(_webState.get(), client);
    SafeBrowsingUrlAllowList::CreateForWebState(_webState.get());
    SafeBrowsingUnsafeResourceContainer::CreateForWebState(_webState.get());
  } else {
    SafeBrowsingQueryManager::RemoveFromWebState(_webState.get());
    SafeBrowsingTabHelper::RemoveFromWebState(_webState.get());
    SafeBrowsingUrlAllowList::RemoveFromWebState(_webState.get());
    SafeBrowsingUnsafeResourceContainer::RemoveFromWebState(_webState.get());
  }
}

#pragma mark - Internal Methods

- (void)shutDown {
  if (_webState) {
    // CWVBackForwardList is unsafe to use after shutting down.
    _backForwardList.navigationManager = nil;
    // To handle the case where -[CWVWebView encodeRestorableStateWithCoder:] is
    // called after this method, precompute the session storage so it may be
    // used during encoding later.
    _cachedSessionStorage = _webState->BuildSessionStorage();
    if (_webStateObserver) {
      _webState->RemoveObserver(_webStateObserver.get());
      _webStateObserver.reset();
    }
    WebViewHolder::RemoveFromWebState(_webState.get());
    _webState.reset();
  }
}

@end
