// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_web_controller.h"

#import <WebKit/WebKit.h>

#import "base/apple/foundation_util.h"
#import "base/containers/contains.h"
#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/ios/block_types.h"
#import "base/ios/ios_util.h"
#import "base/json/string_escape.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "build/branding_buildflags.h"
#import "ios/web/common/annotations_utils.h"
#import "ios/web/common/crw_input_view_provider.h"
#import "ios/web/common/crw_web_view_content_view.h"
#import "ios/web/common/features.h"
#import "ios/web/common/uikit_ui_util.h"
#import "ios/web/common/url_util.h"
#import "ios/web/download/crw_web_view_download.h"
#import "ios/web/find_in_page/java_script_find_in_page_manager_impl.h"
#import "ios/web/history_state_util.h"
#import "ios/web/js_features/scroll_helper/scroll_helper_java_script_feature.h"
#import "ios/web/js_messaging/java_script_feature_util_impl.h"
#import "ios/web/js_messaging/web_view_js_utils.h"
#import "ios/web/js_messaging/web_view_web_state_map.h"
#import "ios/web/navigation/crw_error_page_helper.h"
#import "ios/web/navigation/crw_js_navigation_handler.h"
#import "ios/web/navigation/crw_navigation_item_holder.h"
#import "ios/web/navigation/crw_web_view_navigation_observer.h"
#import "ios/web/navigation/crw_web_view_navigation_observer_delegate.h"
#import "ios/web/navigation/crw_wk_navigation_handler.h"
#import "ios/web/navigation/crw_wk_navigation_states.h"
#import "ios/web/navigation/navigation_context_impl.h"
#import "ios/web/navigation/wk_back_forward_list_item_holder.h"
#import "ios/web/navigation/wk_navigation_util.h"
#import "ios/web/public/annotations/annotations_text_manager.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/find_in_page/crw_find_interaction.h"
#import "ios/web/public/permissions/permissions.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_client.h"
#import "ios/web/security/crw_cert_verification_controller.h"
#import "ios/web/security/crw_ssl_status_updater.h"
#import "ios/web/text_fragments/text_fragments_manager_impl.h"
#import "ios/web/web_state/crw_web_view.h"
#import "ios/web/web_state/ui/crw_context_menu_controller.h"
#import "ios/web/web_state/ui/crw_web_controller_container_view.h"
#import "ios/web/web_state/ui/crw_web_request_controller.h"
#import "ios/web/web_state/ui/crw_web_view_proxy_impl.h"
#import "ios/web/web_state/ui/crw_wk_ui_handler.h"
#import "ios/web/web_state/ui/crw_wk_ui_handler_delegate.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#import "ios/web/web_state/user_interaction_state.h"
#import "ios/web/web_state/web_state_impl.h"
#import "ios/web/web_state/web_view_internal_creation_util.h"
#import "ios/web/web_view/content_type_util.h"
#import "ios/web/web_view/wk_web_view_util.h"
#import "net/base/apple/url_conversions.h"
#import "services/metrics/public/cpp/ukm_builders.h"
#import "url/gurl.h"

using web::NavigationManager;
using web::NavigationManagerImpl;
using web::WebState;
using web::WebStateImpl;

namespace {
char const kFullScreenStateHistogram[] = "IOS.Fullscreen.State";

// Disables logic to update CRWWebController's `_currentURLLoadWasTriggered`
// when setting a WKWebView's interaction state.
BASE_FEATURE(kIOSSessionRestoreLoadTriggerKillSwitch,
             "IOSSessionRestoreLoadTriggerKillSwitch",
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace

// TODO(crbug.com/40746865): Allow usage of iOS15 interactionState on iOS 14 SDK
// based builds.
#if !defined(__IPHONE_15_0) || __IPHONE_OS_VERSION_MAX_ALLOWED < __IPHONE_15_0
@interface WKWebView (Additions)
@property(nonatomic, nullable, copy) id interactionState;
@end
#endif

@interface CRWWebController () <CRWWKNavigationHandlerDelegate,
                                CRWInputViewProvider,
                                CRWSSLStatusUpdaterDataSource,
                                CRWSSLStatusUpdaterDelegate,
                                CRWWebControllerContainerViewDelegate,
                                CRWWebViewNavigationObserverDelegate,
                                CRWWebRequestControllerDelegate,
                                CRWWebViewScrollViewProxyObserver,
                                CRWWKNavigationHandlerDelegate,
                                CRWWKUIHandlerDelegate,
                                UIDropInteractionDelegate,
                                WKNavigationDelegate> {
  // The view used to display content.  Must outlive `_webViewProxy`. The
  // container view should be accessed through this property rather than
  // `self.view` from within this class, as `self.view` triggers creation while
  // `self.containerView` will return nil if the view hasn't been instantiated.
  CRWWebControllerContainerView* _containerView;
  // YES if the current URL load was triggered in Web Controller. NO by default
  // and after web usage was disabled. Used by `-loadCurrentURLIfNecessary` to
  // prevent extra loads.
  BOOL _currentURLLoadWasTrigerred;
  BOOL _isBeingDestroyed;  // YES if in the process of closing.
  // The actual URL of the document object (i.e., the last committed URL).
  // TODO(crbug.com/41213672): Remove this in favor of just updating the
  // navigation manager and treating that as authoritative.
  GURL _documentURL;
  // Actions to execute once the page load is complete.
  NSMutableArray* _pendingLoadCompleteActions;
  // Flag to say if browsing is enabled.
  BOOL _webUsageEnabled;
  // Default URL (about:blank).
  GURL _defaultURL;

  // Updates SSLStatus for current navigation item.
  CRWSSLStatusUpdater* _SSLStatusUpdater;

  // Controller used for certs verification to help with blocking requests with
  // bad SSL cert, presenting SSL interstitials and determining SSL status for
  // Navigation Items.
  CRWCertVerificationController* _certVerificationController;

  // State of user interaction with web content.
  web::UserInteractionState _userInteractionState;
}

// The WKNavigationDelegate handler class.
@property(nonatomic, readonly, strong)
    CRWWKNavigationHandler* navigationHandler;
@property(nonatomic, readonly, strong)
    CRWJSNavigationHandler* jsNavigationHandler;
// The WKUIDelegate handler class.
@property(nonatomic, readonly, strong) CRWWKUIHandler* UIHandler;

// YES if in the process of closing.
@property(nonatomic, readwrite, assign) BOOL beingDestroyed;

// If `contentView_` contains a web view, this is the web view it contains.
// If not, it's nil. When setting the property, it performs basic setup.
@property(weak, nonatomic) WKWebView* webView;
// The scroll view of `webView`.
@property(weak, nonatomic, readonly) UIScrollView* webScrollView;

@property(nonatomic, strong, readonly)
    CRWWebViewNavigationObserver* webViewNavigationObserver;

// Dictionary where keys are the names of WKWebView properties and values are
// selector names which should be called when a corresponding property has
// changed. e.g. @{ @"URL" : @"webViewURLDidChange" } means that
// -[self webViewURLDidChange] must be called every time when WKWebView.URL is
// changed.
@property(weak, nonatomic, readonly) NSDictionary* WKWebViewObservers;

// Url request controller.
@property(nonatomic, strong, readonly)
    CRWWebRequestController* requestController;

@property(nonatomic, readonly) web::WebState* webState;
// WebStateImpl instance associated with this CRWWebController, web controller
// does not own this pointer.
@property(nonatomic, readonly) web::WebStateImpl* webStateImpl;

// Returns the x, y offset the content has been scrolled.
@property(nonatomic, readonly) CGPoint scrollPosition;

// The touch tracking recognizer allowing us to decide if a navigation has user
// gesture. Lazily created.
@property(nonatomic, strong, readonly)
    CRWTouchTrackingRecognizer* touchTrackingRecognizer;

// A custom drop interaction that is added alongside the web view's default drop
// interaction.
@property(nonatomic, strong) UIDropInteraction* customDropInteraction;

// Session Information
// -------------------
// The associated NavigationManagerImpl.
@property(nonatomic, readonly) NavigationManagerImpl* navigationManagerImpl;
// TODO(crbug.com/40506829): Remove these functions and replace with more
// appropriate NavigationItem getters.
// Returns the navigation item for the current page.
@property(nonatomic, readonly) web::NavigationItemImpl* currentNavItem;

// ContextMenu controller, handling the interactions with the context menu.
@property(nonatomic, strong) CRWContextMenuController* contextMenuController;

// Called following navigation completion to generate final navigation lifecycle
// events. Navigation is considered complete when the document has finished
// loading, or when other page load mechanics are completed on a
// non-document-changing URL change.
- (void)didFinishNavigation:(web::NavigationContextImpl*)context;
// Update the appropriate parts of the model and broadcast to the embedder. This
// may be called multiple times and thus must be idempotent.
- (void)loadCompleteWithSuccess:(BOOL)loadSuccess
                     forContext:(web::NavigationContextImpl*)context;
// Finds all the scrollviews in the view hierarchy and makes sure they do not
// interfere with scroll to top when tapping the statusbar.
- (void)optOutScrollsToTopForSubviews;
// Updates SSL status for the current navigation item based on the information
// provided by web view.
- (void)updateSSLStatusForCurrentNavigationItem;

@end

@implementation CRWWebController

// Synthesize as it is readonly.
@synthesize touchTrackingRecognizer = _touchTrackingRecognizer;

#pragma mark - Object lifecycle

- (instancetype)initWithWebState:(WebStateImpl*)webState {
  self = [super init];
  if (self) {
    _webStateImpl = webState;
    _webUsageEnabled = YES;

    _allowsBackForwardNavigationGestures = YES;

    DCHECK(_webStateImpl);
    // Content area is lazily instantiated.
    _defaultURL = GURL(url::kAboutBlankURL);
    _requestController = [[CRWWebRequestController alloc] init];
    _requestController.delegate = self;
    _webViewProxy = [[CRWWebViewProxyImpl alloc] initWithWebController:self];
    [[_webViewProxy scrollViewProxy] addObserver:self];
    _pendingLoadCompleteActions = [[NSMutableArray alloc] init];
    web::BrowserState* browserState = _webStateImpl->GetBrowserState();
    _certVerificationController = [[CRWCertVerificationController alloc]
        initWithBrowserState:browserState];
    web::JavaScriptFindInPageManagerImpl::CreateForWebState(_webStateImpl);
    web::TextFragmentsManagerImpl::CreateForWebState(_webStateImpl);

    if (!browserState->IsOffTheRecord()) {
      web::AnnotationsTextManager::CreateForWebState(_webStateImpl);
    }

    _navigationHandler = [[CRWWKNavigationHandler alloc] initWithDelegate:self];

    _jsNavigationHandler = [[CRWJSNavigationHandler alloc] init];

    _UIHandler = [[CRWWKUIHandler alloc] init];
    _UIHandler.delegate = self;

    _webViewNavigationObserver = [[CRWWebViewNavigationObserver alloc] init];
    _webViewNavigationObserver.delegate = self;
  }
  return self;
}

- (void)dealloc {
  DCHECK([NSThread isMainThread]);
  DCHECK(_isBeingDestroyed);  // 'close' must have been called already.
  DCHECK(!_webView);
}

#pragma mark - Public property accessors

- (void)setWebUsageEnabled:(BOOL)enabled {
  if (_webUsageEnabled == enabled)
    return;
  // WKWebView autoreleases its WKProcessPool on removal from superview.
  // Deferring WKProcessPool deallocation may lead to issues with cookie
  // clearing and and Browsing Data Partitioning implementation.
  @autoreleasepool {
    if (!enabled) {
      [self removeWebView];
    }
  }

  _webUsageEnabled = enabled;

  // WKWebView autoreleases its WKProcessPool on removal from superview.
  // Deferring WKProcessPool deallocation may lead to issues with cookie
  // clearing and and Browsing Data Partitioning implementation.
  @autoreleasepool {
    if (enabled) {
      // Don't create the web view; let it be lazy created as needed.

      // The gesture is removed when the web usage is disabled. Add it back when
      // it is enabled again.
      [_containerView addGestureRecognizer:[self touchTrackingRecognizer]];
    } else {
      if (_touchTrackingRecognizer) {
        [_containerView removeGestureRecognizer:_touchTrackingRecognizer];
        _touchTrackingRecognizer.touchTrackingDelegate = nil;
        _touchTrackingRecognizer = nil;
      }
      _currentURLLoadWasTrigerred = NO;
    }
  }
}

- (UIView*)view {
  [self ensureContainerViewCreated];
  DCHECK(_containerView);
  return _containerView;
}

- (id<CRWWebViewNavigationProxy>)webViewNavigationProxy {
  return static_cast<id<CRWWebViewNavigationProxy>>(self.webView);
}

- (double)loadingProgress {
  return [self.webView estimatedProgress];
}

- (BOOL)isWebProcessCrashed {
  return self.navigationHandler.webProcessCrashed;
}

- (BOOL)isUserInteracting {
  return _userInteractionState.IsUserInteracting(self.webView);
}

- (void)setAllowsBackForwardNavigationGestures:
    (BOOL)allowsBackForwardNavigationGestures {
  // Store it to an instance variable as well as
  // self.webView.allowsBackForwardNavigationGestures because self.webView may
  // be nil. When self.webView is nil, it will be set later in -setWebView:.
  _allowsBackForwardNavigationGestures = allowsBackForwardNavigationGestures;
  self.webView.allowsBackForwardNavigationGestures =
      allowsBackForwardNavigationGestures;
}

#pragma mark - Private properties accessors

- (void)setWebView:(WKWebView*)webView {
  DCHECK_NE(_webView, webView);

  // Unwind the old web view.

  // Remove KVO and WK*Delegate before calling methods on WKWebView so that
  // handlers won't receive unnecessary callbacks.
  [_webView setNavigationDelegate:nil];
  [_webView setUIDelegate:nil];
  for (NSString* keyPath in self.WKWebViewObservers) {
    [_webView removeObserver:self forKeyPath:keyPath];
  }
  self.webViewNavigationObserver.webView = nil;

  web::WebViewWebStateMap::FromBrowserState(
      self.webStateImpl->GetBrowserState())
      ->SetAssociatedWebViewForWebState(webView, self.webStateImpl);

  if (_webView) {
    self.webStateImpl->RemoveAllWebFrames();

    [_webView stopLoading];
    [_webView removeFromSuperview];

    // Since the WKWebView is about to be released, the kvo for the `loading`
    // state will not be received. Without manually setting loading to false,
    // the tab will appear to be endlessly loading until the next page load
    // completes.
    self.webStateImpl->SetIsLoading(false);
  }

  // Set up the new web view.
  _webView = webView;

  if (_webView) {
    [_webView setNavigationDelegate:self.navigationHandler];
    [_webView setUIDelegate:self.UIHandler];
    for (NSString* keyPath in self.WKWebViewObservers) {
      [_webView addObserver:self forKeyPath:keyPath options:0 context:nullptr];
    }

    _webView.allowsBackForwardNavigationGestures =
        _allowsBackForwardNavigationGestures;
  }
  self.webViewNavigationObserver.webView = _webView;

  [self setDocumentURL:_defaultURL context:nullptr];
}

- (UIScrollView*)webScrollView {
  return self.webView.scrollView;
}

- (NSDictionary*)WKWebViewObservers {
  NSMutableDictionary<NSString*, NSString*>* observers =
      [[NSMutableDictionary alloc] initWithDictionary:@{
        @"serverTrust" : @"webViewSecurityFeaturesDidChange",
        @"hasOnlySecureContent" : @"webViewSecurityFeaturesDidChange",
        @"title" : @"webViewTitleDidChange",
        @"cameraCaptureState" : @"webViewCameraCaptureStateDidChange",
        @"microphoneCaptureState" : @"webViewMicrophoneCaptureStateDidChange",
        @"underPageBackgroundColor" :
            @"webViewUnderPageBackgroundColorDidChange",
      }];

  if (web::GetWebClient()->EnableFullscreenAPI()) {
    [observers addEntriesFromDictionary:@{
      @"fullscreenState" : @"fullscreenStateDidChange"
    }];
  }

  return observers;
}

- (WebState*)webState {
  return _webStateImpl;
}

- (CGPoint)scrollPosition {
  return self.webScrollView.contentOffset;
}

- (CRWTouchTrackingRecognizer*)touchTrackingRecognizer {
  if (!_touchTrackingRecognizer) {
    _touchTrackingRecognizer =
        [[CRWTouchTrackingRecognizer alloc] initWithTouchTrackingDelegate:self];
  }
  return _touchTrackingRecognizer;
}

- (BOOL)isCover {
  return _containerView.cover;
}

#pragma mark Navigation and Session Information

- (NavigationManagerImpl*)navigationManagerImpl {
  return self.webStateImpl ? &(self.webStateImpl->GetNavigationManagerImpl())
                           : nil;
}

- (web::NavigationItemImpl*)currentNavItem {
  return self.navigationManagerImpl
             ? self.navigationManagerImpl->GetCurrentItemImpl()
             : nullptr;
}

#pragma mark - ** Public Methods **

#pragma mark - Header public methods

- (web::NavigationItemImpl*)lastPendingItemForNewNavigation {
  WKNavigation* navigation =
      [self.navigationHandler.navigationStates
              lastNavigationWithPendingItemInNavigationContext];
  if (!navigation)
    return nullptr;
  web::NavigationContextImpl* context =
      [self.navigationHandler.navigationStates contextForNavigation:navigation];
  return context->GetItem();
}

// Caller must reset the delegate before calling.
- (void)close {
  self.webStateImpl->CancelDialogs();

  _SSLStatusUpdater = nil;
  [self.navigationHandler close];
  [self.UIHandler close];
  [self.jsNavigationHandler close];
  [self.requestController close];
  [self.webViewNavigationObserver close];

  // Mark the destruction sequence has started, in case someone else holds a
  // strong reference and tries to continue using the tab.
  DCHECK(!_isBeingDestroyed);
  _isBeingDestroyed = YES;

  // Remove the web view now. Otherwise, delegate callbacks occur.
  [self removeWebView];

  // Explicitly reset content to clean up views and avoid dangling KVO
  // observers.
  [_containerView resetContentForShutdown:YES];

  _webStateImpl = nullptr;

  DCHECK(!self.webView);
  // TODO(crbug.com/41284914): Don't set the delegate to nil.
  [_containerView setDelegate:nil];
  _touchTrackingRecognizer.touchTrackingDelegate = nil;
  [[_webViewProxy scrollViewProxy] removeObserver:self];
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (BOOL)isViewAlive {
  return !self.navigationHandler.webProcessCrashed &&
         [_containerView isViewAlive];
}

- (BOOL)contentIsHTML {
  return self.webView &&
         web::IsContentTypeHtml(self.webState->GetContentsMimeType());
}

- (GURL)currentURL {
  // TODO(crbug.com/40528091): Investigate if this method is still needed and if
  // it can be implemented using NavigationManager API after removal of legacy
  // navigation stack.
  if (self.webView) {
    return _documentURL;
  }

  web::NavigationItem* item =
      self.navigationManagerImpl->GetLastCommittedItem();
  if (item) {
    // This special case is added for any app specific URLs that have been
    // rewritten to about:// URLs.
    if (item->GetURL().SchemeIs(url::kAboutScheme) &&
        web::GetWebClient()->IsAppSpecificURL(item->GetVirtualURL())) {
      return item->GetURL();
    }
    return item->GetVirtualURL();
  }
  return GURL();
}

- (void)reloadWithRendererInitiatedNavigation:(BOOL)rendererInitiated {
  // Clear last user interaction.
  // TODO(crbug.com/41211432): Move to after the load commits, in the subclass
  // implementation. This will be inaccurate if the reload fails or is
  // cancelled.
  _userInteractionState.SetLastUserInteraction(nullptr);
  base::RecordAction(base::UserMetricsAction("Reload"));
  [_requestController reloadWithRendererInitiatedNavigation:rendererInitiated];
}

- (void)stopLoading {
  base::RecordAction(base::UserMetricsAction("Stop"));
  // Discard all pending items before notifying WebState observers
  self.navigationManagerImpl->DiscardNonCommittedItems();
  for (__strong id navigation in
       [self.navigationHandler.navigationStates pendingNavigations]) {
    if (navigation == [NSNull null]) {
      // null is a valid navigation object passed to WKNavigationDelegate
      // callbacks and represents window opening action.
      navigation = nil;
    }
    // This will remove pending item for navigations which may still call
    // WKNavigationDelegate callbacks see (crbug.com/969915).
    web::NavigationContextImpl* context =
        [self.navigationHandler.navigationStates
            contextForNavigation:navigation];
    context->ReleaseItem();
  }

  [self.webView stopLoading];
  [self.navigationHandler stopLoading];
}

- (void)loadCurrentURLWithRendererInitiatedNavigation:(BOOL)rendererInitiated {
  // If the content view doesn't exist, the tab has either been evicted, or
  // never displayed. Bail, and let the URL be loaded when the tab is shown.
  if (!_containerView)
    return;

  // NavigationManagerImpl needs WKWebView to load native views, but WKWebView
  // cannot be created while web usage is disabled to avoid breaking clearing
  // browser data. Bail now and let the URL be loaded when web usage is enabled
  // again. This can happen when purging web pages when an interstitial is
  // presented over a native view. See https://crbug.com/865985 for details.
  if (!_webUsageEnabled)
    return;

  _currentURLLoadWasTrigerred = YES;

  [_requestController
      loadCurrentURLWithRendererInitiatedNavigation:rendererInitiated];
}

- (void)loadCurrentURLIfNecessary {
  if (self.navigationHandler.webProcessCrashed) {
    // Log a user reloading a previously crashed renderer.
    base::RecordAction(
        base::UserMetricsAction("IOSMobileReloadCrashedRenderer"));
    [self loadCurrentURLWithRendererInitiatedNavigation:NO];
  } else if (!_currentURLLoadWasTrigerred) {
    [self ensureContainerViewCreated];

    // TODO(crbug.com/41361784): end the practice of calling `loadCurrentURL`
    // when it is possible there is no current URL. If the call performs
    // necessary initialization, break that out.
    [self loadCurrentURLWithRendererInitiatedNavigation:NO];
  }
}

- (void)loadData:(NSData*)data
        MIMEType:(NSString*)MIMEType
          forURL:(const GURL&)URL {
  [_requestController loadData:data MIMEType:MIMEType forURL:URL];
}

- (void)loadSimulatedRequest:(const GURL&)URL
          responseHTMLString:(NSString*)responseHTMLString {
  NSURLRequest* request =
      [[NSURLRequest alloc] initWithURL:net::NSURLWithGURL(URL)];
  [self.webView loadSimulatedRequest:request
                  responseHTMLString:responseHTMLString];
}

- (void)loadSimulatedRequest:(const GURL&)URL
                responseData:(NSData*)responseData
                    MIMEType:(NSString*)MIMEType {
  NSURL* url = net::NSURLWithGURL(URL);
  NSURLRequest* request = [[NSURLRequest alloc] initWithURL:url];
  NSURLResponse* response =
      [[NSURLResponse alloc] initWithURL:url
                                MIMEType:MIMEType
                   expectedContentLength:responseData.length
                        textEncodingName:nil];

  [self.webView loadSimulatedRequest:request
                            response:response
                        responseData:responseData];
}

// Loads the HTML into the page at the given URL. Only for testing purpose.
- (void)loadHTML:(NSString*)HTML forURL:(const GURL&)URL {
  [_requestController loadHTML:HTML forURL:URL];
}

- (void)recordStateInHistory {
  // Only record the state if:
  // - the current NavigationItem's URL matches the current URL, and
  // - the user has interacted with the page.
}

- (void)setVisible:(BOOL)visible {
  _visible = visible;
}

- (void)wasShown {
  self.visible = YES;

  // WebKit adds a drop interaction to a subview (WKContentView) of WKWebView's
  // scrollView when the web view is added to the view hierarchy.
  [self addCustomURLDropInteractionIfNeeded];
}

- (void)wasHidden {
  self.visible = NO;
  if (_isBeingDestroyed)
    return;
  [self recordStateInHistory];
}

- (void)setKeepsRenderProcessAlive:(BOOL)keepsRenderProcessAlive {
  _keepsRenderProcessAlive = keepsRenderProcessAlive;
  [_containerView
      updateWebViewContentViewForContainerWindow:_containerView.window];
}

- (void)goToBackForwardListItem:(WKBackForwardListItem*)wk_item
                 navigationItem:(web::NavigationItem*)item
       navigationInitiationType:(web::NavigationInitiationType)type
                 hasUserGesture:(BOOL)hasUserGesture {
  WKNavigation* navigation;
  // Where possible, call `goBack` or `goForward` since WebKit has logic
  // specific to those functions for skipping over maliciously-added items. See
  // crbug.com/40072465 for an example.
  if (wk_item == self.webView.backForwardList.backItem) {
    navigation = [self.webView goBack];
  } else if (wk_item == self.webView.backForwardList.forwardItem) {
    navigation = [self.webView goForward];
  } else {
    navigation = [self.webView goToBackForwardListItem:wk_item];
  }

  GURL URL = net::GURLWithNSURL(wk_item.URL);

  self.webStateImpl->ClearWebUI();

  // This navigation can be an iframe navigation, but it's not possible to
  // distinguish it from the main frame navigation, so context still has to be
  // created.
  std::unique_ptr<web::NavigationContextImpl> context =
      web::NavigationContextImpl::CreateNavigationContext(
          self.webStateImpl, URL, hasUserGesture,
          static_cast<ui::PageTransition>(
              item->GetTransitionType() |
              ui::PageTransition::PAGE_TRANSITION_FORWARD_BACK),
          type == web::NavigationInitiationType::RENDERER_INITIATED);
  context->SetNavigationItemUniqueID(item->GetUniqueID());
  bool isSameDocument = web::GURLByRemovingRefFromGURL(URL) ==
                        web::GURLByRemovingRefFromGURL(_documentURL);
  if (isSameDocument) {
    context->SetIsSameDocument(true);
  } else {
    self.navigationHandler.navigationState = web::WKNavigationState::REQUESTED;
  }

  if ([CRWErrorPageHelper isErrorPageFileURL:URL]) {
    context->SetLoadingErrorPage(true);
  }

  web::WKBackForwardListItemHolder* holder =
      web::WKBackForwardListItemHolder::FromNavigationItem(item);
  holder->set_navigation_type(WKNavigationTypeBackForward);
  context->SetIsPost(
      (holder && [holder->http_method() isEqualToString:@"POST"]) ||
      item->HasPostData());

  if (holder) {
    context->SetMimeType(holder->mime_type());
  }

  [self.navigationHandler.navigationStates setContext:std::move(context)
                                        forNavigation:navigation];
  [self.navigationHandler.navigationStates
           setState:web::WKNavigationState::REQUESTED
      forNavigation:navigation];
}

- (void)takeSnapshotWithRect:(CGRect)rect
                  completion:(void (^)(UIImage*))completion {
  if (!self.webView) {
    dispatch_async(dispatch_get_main_queue(), ^{
      completion(nil);
    });
    return;
  }

  WKSnapshotConfiguration* configuration =
      [[WKSnapshotConfiguration alloc] init];
  CGRect convertedRect = [self.webView convertRect:rect fromView:self.view];
  configuration.rect = convertedRect;
  __weak CRWWebController* weakSelf = self;
  [self.webView
      takeSnapshotWithConfiguration:configuration
                  completionHandler:^(UIImage* snapshot, NSError* error) {
                    // Pass nil to the completion block if there is an error
                    // or if the web view has been removed before the
                    // snapshot is finished.  `snapshot` can sometimes be
                    // corrupt if it's sent due to the WKWebView's
                    // deallocation, so callbacks received after
                    // `-removeWebView` are ignored to prevent crashing.
                    if (error || !weakSelf.webView) {
                      if (error) {
                        DLOG(ERROR)
                            << "WKWebView snapshot error: "
                            << base::SysNSStringToUTF8(error.description);
                      }
                      completion(nil);
                    } else {
                      completion(snapshot);
                    }
                  }];
}

- (void)createFullPagePDFWithCompletion:(void (^)(NSData*))completionBlock {
  // Invoke the `completionBlock` with nil rather than a blank PDF for certain
  // URLs or if there is a javascript dialog running.
  const GURL& URL = self.webState->GetLastCommittedURL();
  if (![self contentIsHTML] || !URL.is_valid() ||
      web::GetWebClient()->IsAppSpecificURL(URL) ||
      self.webStateImpl->IsJavaScriptDialogRunning()) {
    dispatch_async(dispatch_get_main_queue(), ^{
      completionBlock(nil);
    });
    return;
  }
  web::CreateFullPagePdf(self.webView, base::BindOnce(completionBlock));
}

- (void)closeMediaPresentations {
  if (@available(iOS 16, *)) {
    if (self.webView.fullscreenState == WKFullscreenStateInFullscreen ||
        self.webView.fullscreenState == WKFullscreenStateEnteringFullscreen) {
      [self.webView closeAllMediaPresentationsWithCompletionHandler:^{
      }];
      return;
    }
  }

  [self.webView requestMediaPlaybackStateWithCompletionHandler:^(
                    WKMediaPlaybackState mediaPlaybackState) {
    if (mediaPlaybackState == WKMediaPlaybackStateNone) {
      return;
    }

    // Completion handler is needed to avoid a crash when called.
    [self.webView closeAllMediaPresentationsWithCompletionHandler:^{
    }];
  }];
}

- (void)removeWebViewFromViewHierarchyForShutdown:(BOOL)shutdown {
  [_containerView resetContentForShutdown:shutdown];
}

- (void)addWebViewToViewHierarchy {
  [self displayWebView];
}

- (BOOL)setSessionStateData:(NSData*)data {
  NSData* interactionState = data;

  // Old versions of chrome wrapped interactionState in a keyed unarchiver.
  // This step was unnecessary. Rather than migrate all blobs over, simply
  // check for an unarchiver here. NSKeyed data will start with 'bplist00',
  // which differs from the header of a WebKit session coding (0x00000002).
  // This logic can be removed after this change has gone live for a while.
  constexpr char kArchiveHeader[] = "bplist00";
  if (data.length > strlen(kArchiveHeader) &&
      memcmp(data.bytes, kArchiveHeader, strlen(kArchiveHeader)) == 0) {
    NSError* error = nil;
    NSKeyedUnarchiver* unarchiver =
        [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:&error];
    if (!unarchiver || error) {
      DLOG(WARNING) << "Error creating unarchiver for session state data: "
                    << base::SysNSStringToUTF8([error description]);
      return NO;
    }
    unarchiver.requiresSecureCoding = NO;
    interactionState =
        [unarchiver decodeObjectForKey:NSKeyedArchiveRootObjectKey];
    if (!interactionState) {
      DLOG(WARNING) << "Error decoding interactionState.";
      return NO;
    }
  }
  [self ensureWebViewCreated];
  DCHECK_EQ(self.webView.backForwardList.currentItem, nil);
  self.navigationHandler.blockUniversalLinksOnNextDecidePolicy = true;
  [self.webView setInteractionState:interactionState];
  if (!base::FeatureList::IsEnabled(kIOSSessionRestoreLoadTriggerKillSwitch)) {
    _currentURLLoadWasTrigerred = YES;
  }
  return YES;
}

- (web::PermissionState)stateForPermission:(web::Permission)permission {
  WKMediaCaptureState captureState;
  switch (permission) {
    case web::PermissionCamera:
      captureState = self.webView.cameraCaptureState;
      break;
    case web::PermissionMicrophone:
      captureState = self.webView.microphoneCaptureState;
      break;
  }
  switch (captureState) {
    case WKMediaCaptureStateActive:
      return web::PermissionStateAllowed;
    case WKMediaCaptureStateMuted:
      return web::PermissionStateBlocked;
    case WKMediaCaptureStateNone:
      return web::PermissionStateNotAccessible;
  }
}

- (void)setState:(web::PermissionState)state
    forPermission:(web::Permission)permission {
  WKMediaCaptureState captureState;
  switch (state) {
    case web::PermissionStateAllowed:
      captureState = WKMediaCaptureStateActive;
      break;
    case web::PermissionStateBlocked:
      captureState = WKMediaCaptureStateMuted;
      break;
    case web::PermissionStateNotAccessible:
      captureState = WKMediaCaptureStateNone;
      break;
  }
  switch (permission) {
    case web::PermissionCamera:
      [self.webView setCameraCaptureState:captureState completionHandler:nil];
      break;
    case web::PermissionMicrophone:
      [self.webView setMicrophoneCaptureState:captureState
                            completionHandler:nil];
      break;
  }
}

- (NSDictionary<NSNumber*, NSNumber*>*)statesForAllPermissions {
  return @{
    @(web::PermissionCamera) :
        @([self stateForPermission:web::PermissionCamera]),
    @(web::PermissionMicrophone) :
        @([self stateForPermission:web::PermissionMicrophone])
  };
}

- (NSData*)sessionStateData {
  return self.webView.interactionState;
}

- (void)handleViewportFit:(BOOL)isCover {
  _containerView.cover = isCover;
  [_containerView layoutSubviews];
}

- (void)handleNavigationHashChange {
  web::NavigationItemImpl* currentItem = self.currentNavItem;
  if (currentItem) {
    currentItem->SetIsCreatedFromHashChange(true);
  }
}

- (void)handleNavigationWillChangeState {
  [self.jsNavigationHandler handleNavigationWillChangeState];
}

- (void)handleNavigationDidPushStateMessage:(base::Value::Dict*)dict {
  [self.jsNavigationHandler
      handleNavigationDidPushStateMessage:dict
                                 webState:_webStateImpl
                           hasUserGesture:self.isUserInteracting
                     userInteractionState:&_userInteractionState
                               currentURL:self.currentURL];
  [self updateSSLStatusForCurrentNavigationItem];
}

- (void)handleNavigationDidReplaceStateMessage:(base::Value::Dict*)dict {
  [self.jsNavigationHandler
      handleNavigationDidReplaceStateMessage:dict
                                    webState:_webStateImpl
                              hasUserGesture:self.isUserInteracting
                        userInteractionState:&_userInteractionState
                                  currentURL:self.currentURL];
}

- (void)downloadCurrentPageToDestinationPath:(NSString*)destination
                                    delegate:
                                        (id<CRWWebViewDownloadDelegate>)delegate
                                     handler:(void (^)(id<CRWWebViewDownload>))
                                                 handler {
  const NavigationManagerImpl* navigationManager = self.navigationManagerImpl;
  GURL url = navigationManager->GetLastCommittedItem()
                 ? navigationManager->GetLastCommittedItem()->GetURL()
                 : [self currentURL];

  NSURLRequest* request = [NSURLRequest requestWithURL:net::NSURLWithGURL(url)];

  CRWWebViewDownload* download =
      [[CRWWebViewDownload alloc] initWithPath:destination
                                       request:request
                                       webview:self.webView
                                      delegate:delegate];
  [download startDownload];
  handler(download);
}

- (BOOL)findInteractionSupported {
  if (@available(iOS 16, *)) {
    // The `findInteraction` property only exists for iOS 16 or later, if there
    // is a web view.
    return self.webView != nil;
  }

  return false;
}

- (void)setFindInteractionEnabled:(BOOL)enabled {
  if (@available(iOS 16, *)) {
    self.webView.findInteractionEnabled = enabled;
  }
}

- (BOOL)findInteractionEnabled {
  if (@available(iOS 16, *)) {
    return self.webView.findInteractionEnabled;
  }

  return NO;
}

- (id<CRWFindInteraction>)findInteraction API_AVAILABLE(ios(16)) {
  if (self.webView.findInteraction) {
    return [[CRWFindInteraction alloc]
        initWithUIFindInteraction:self.webView.findInteraction];
  }
  return nil;
}

- (id)activityItem {
  if (!self.webView || ![_containerView webViewContentView]) {
    return nil;
  }
  DCHECK([self.webView isKindOfClass:[WKWebView class]]);
  return self.webView;
}

- (UIColor*)themeColor {
  return self.webView.themeColor;
}

- (UIColor*)underPageBackgroundColor {
  return self.webView.underPageBackgroundColor;
}

#pragma mark - JavaScript

- (void)retrieveExistingFramesInContentWorld:(WKContentWorld*)contentWorld {
  web::RegisterExistingFrames(self.webView, contentWorld);
}

- (void)executeJavaScript:(NSString*)javascript
        completionHandler:(void (^)(id result, NSError* error))completion {
  __block void (^stack_completion_block)(id result, NSError* error) =
      [completion copy];
  web::ExecuteJavaScript(self.webView, javascript, ^(id value, NSError* error) {
    if (error) {
      DLOG(WARNING) << "Script execution failed with error: "
                    << base::SysNSStringToUTF16(
                           error.userInfo[NSLocalizedDescriptionKey]);
    }
    if (stack_completion_block) {
      stack_completion_block(value, error);
    }
  });
}

- (void)executeUserJavaScript:(NSString*)javascript
            completionHandler:(void (^)(id result, NSError* error))completion {
  // For security reasons, executing JavaScript on pages with app-specific URLs
  // is not allowed, because those pages may have elevated privileges.
  if (web::GetWebClient()->IsAppSpecificURL(
          self.webStateImpl->GetLastCommittedURL())) {
    if (completion) {
      dispatch_async(dispatch_get_main_queue(), ^{
        NSError* error = [[NSError alloc]
            initWithDomain:web::kJSEvaluationErrorDomain
                      code:web::JS_EVALUATION_ERROR_CODE_REJECTED
                  userInfo:nil];
        completion(nil, error);
      });
    }
    return;
  }

  [self touched:YES];

  [self executeJavaScript:javascript completionHandler:completion];
}

#pragma mark - CRWTouchTrackingDelegate (Public)

- (void)touched:(BOOL)touched {
  _userInteractionState.SetTapInProgress(touched);
  if (touched) {
    _userInteractionState.SetUserInteractionRegisteredSincePageLoaded(true);
    if (_isBeingDestroyed)
      return;
    const NavigationManagerImpl* navigationManager = self.navigationManagerImpl;
    GURL mainDocumentURL =
        navigationManager->GetLastCommittedItem()
            ? navigationManager->GetLastCommittedItem()->GetURL()
            : [self currentURL];
    _userInteractionState.SetLastUserInteraction(
        std::make_unique<web::UserInteractionEvent>(mainDocumentURL));
    [self hideAnnotationsHighlight];
  }
}

#pragma mark - Context Menu

// Hides annotations highlights triggered by context menu.
- (void)hideAnnotationsHighlight {
  web::AnnotationsTextManager* manager =
      web::AnnotationsTextManager::FromWebState(_webStateImpl);
  if (manager) {
    manager->RemoveHighlight();
  }
}

#pragma mark - ** Private Methods **

- (void)setDocumentURL:(const GURL&)newURL
               context:(web::NavigationContextImpl*)context {
  GURL oldDocumentURL = _documentURL;
  if (newURL != _documentURL && newURL.is_valid()) {
    _documentURL = newURL;
    _userInteractionState.SetUserInteractionRegisteredSinceLastUrlChange(false);
  }
  if (context && !context->IsLoadingErrorPage() &&
      !context->IsLoadingHtmlString() && !newURL.SchemeIs(url::kAboutScheme) &&
      self.webView) {
    // On iOS13, WebKit started changing the URL visible webView.URL when
    // opening a new tab and then writing to it, e.g.
    // window.open('javascript:document.write(1)').  This URL is never commited,
    // so it should be OK to ignore this URL change.
    if (oldDocumentURL.IsAboutBlank() &&
        !self.webStateImpl->GetNavigationManager()->GetLastCommittedItem() &&
        !self.webView.loading) {
      return;
    }

    // Ignore mismatches triggered by a WKWebView out-of-sync back forward list.
    if (![self.webView.backForwardList.currentItem.URL
            isEqual:self.webView.URL]) {
      return;
    }

    GURL documentOrigin = newURL.DeprecatedGetOriginAsURL();
    web::NavigationItem* committedItem =
        self.webStateImpl->GetNavigationManager()->GetLastCommittedItem();
    GURL committedURL = committedItem ? committedItem->GetURL() : GURL();
    GURL committedOrigin = committedURL.DeprecatedGetOriginAsURL();

    DCHECK_EQ(documentOrigin, committedOrigin)
        << "Old and new URL detection system have a mismatch";
  }
}

- (BOOL)isUserInitiatedAction:(WKNavigationAction*)action {
  return _userInteractionState.IsUserInteracting(self.webView);
}

// Adds a custom drop interaction to the same subview of `self.webScrollView`
// that already has a default drop interaction.
- (void)addCustomURLDropInteractionIfNeeded {
  BOOL subviewWithDefaultInteractionFound = NO;
  for (UIView* subview in self.webScrollView.subviews) {
    BOOL defaultInteractionFound = NO;
    BOOL customInteractionFound = NO;
    for (id<UIInteraction> interaction in subview.interactions) {
      if ([interaction isKindOfClass:[UIDropInteraction class]]) {
        if (interaction == self.customDropInteraction) {
          customInteractionFound = YES;
        } else {
          DCHECK(!defaultInteractionFound &&
                 !subviewWithDefaultInteractionFound)
              << "There should be only one default drop interaction in the "
                 "webScrollView.";
          defaultInteractionFound = YES;
          subviewWithDefaultInteractionFound = YES;
        }
      }
    }
    if (customInteractionFound) {
      // The custom interaction must be added after the default drop interaction
      // to work properly.
      [subview removeInteraction:self.customDropInteraction];
      [subview addInteraction:self.customDropInteraction];
    } else if (defaultInteractionFound) {
      if (!self.customDropInteraction) {
        self.customDropInteraction =
            [[UIDropInteraction alloc] initWithDelegate:self];
      }
      [subview addInteraction:self.customDropInteraction];
    }
  }
}

#pragma mark - End of loading

- (void)didFinishNavigation:(web::NavigationContextImpl*)context {
  // This can be called at multiple times after the document has loaded. Do
  // nothing if the document has already loaded.
  if (self.navigationHandler.navigationState ==
      web::WKNavigationState::FINISHED)
    return;

  web::NavigationItem* pendingOrCommittedItem =
      self.navigationManagerImpl->GetPendingItem();
  if (!pendingOrCommittedItem)
    pendingOrCommittedItem = self.navigationManagerImpl->GetLastCommittedItem();
  if (pendingOrCommittedItem) {
    // This stores the UserAgent that was used to load the item.
    if (pendingOrCommittedItem->GetUserAgentType() ==
            web::UserAgentType::NONE &&
        web::wk_navigation_util::URLNeedsUserAgentType(
            pendingOrCommittedItem->GetURL())) {
      pendingOrCommittedItem->SetUserAgentType(
          self.webStateImpl->GetUserAgentForNextNavigation(
              pendingOrCommittedItem->GetURL()));
    }
  }

  if (_webView.allowsBackForwardNavigationGestures !=
      _allowsBackForwardNavigationGestures) {
    _webView.allowsBackForwardNavigationGestures =
        _allowsBackForwardNavigationGestures;
  }

  BOOL success = !context || !context->GetError();
  [self loadCompleteWithSuccess:success forContext:context];

  // WebKit adds a drop interaction to a subview (WKContentView) of WKWebView's
  // scrollView when a new WebProcess finishes launching. This can be loading
  // the first page, navigating cross-domain, or recovering from a WebProcess
  // crash. Add a custom drop interaction alongside the default drop
  // interaction.
  [self addCustomURLDropInteractionIfNeeded];
}

- (void)loadCompleteWithSuccess:(BOOL)loadSuccess
                     forContext:(web::NavigationContextImpl*)context {
  // The webView may have been torn down. Be safe and do nothing if that's
  // happened.
  if (self.navigationHandler.navigationState != web::WKNavigationState::STARTED)
    return;

  const GURL currentURL([self currentURL]);

  self.navigationHandler.navigationState = web::WKNavigationState::FINISHED;

  [self optOutScrollsToTopForSubviews];

  // Perform post-load-finished updates.
  [_requestController didFinishWithURL:currentURL
                           loadSuccess:loadSuccess
                               context:context];

  // Execute the pending LoadCompleteActions.
  for (ProceduralBlock action in _pendingLoadCompleteActions) {
    action();
  }
  [_pendingLoadCompleteActions removeAllObjects];
}

#pragma mark - CRWWebControllerContainerViewDelegate

- (CRWWebViewProxyImpl*)contentViewProxyForContainerView:
    (CRWWebControllerContainerView*)containerView {
  return _webViewProxy;
}

- (BOOL)shouldKeepRenderProcessAliveForContainerView:
    (CRWWebControllerContainerView*)containerView {
  return self.shouldKeepRenderProcessAlive;
}

- (void)containerView:(CRWWebControllerContainerView*)containerView
    storeWebViewInWindow:(UIView*)viewToStash {
  [web::GetWebClient()->GetWindowedContainer() addSubview:viewToStash];
}

#pragma mark - CRWWebViewScrollViewProxyObserver

- (void)webViewScrollViewDidZoom:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
}

- (void)webViewScrollViewDidResetContentSize:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
}

// Under WKWebView, JavaScript can execute asynchronously. User can start
// scrolling and calls to window.scrollTo executed during scrolling will be
// treated as "during user interaction" and can cause app to go fullscreen.
// This is a workaround to use this webViewScrollViewIsDragging flag to ignore
// window.scrollTo while user is scrolling. See crbug.com/554257
- (void)webViewScrollViewWillBeginDragging:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  web::java_script_features::GetScrollHelperJavaScriptFeature()
      ->SetWebViewScrollViewIsDragging(self.webState, true);
}

- (void)webViewScrollViewDidEndDragging:
            (CRWWebViewScrollViewProxy*)webViewScrollViewProxy
                         willDecelerate:(BOOL)decelerate {
  web::java_script_features::GetScrollHelperJavaScriptFeature()
      ->SetWebViewScrollViewIsDragging(self.webState, false);
}

#pragma mark - Fullscreen

- (void)optOutScrollsToTopForSubviews {
  NSMutableArray* stack =
      [NSMutableArray arrayWithArray:[self.webScrollView subviews]];
  while (stack.count) {
    UIView* current = [stack lastObject];
    [stack removeLastObject];
    [stack addObjectsFromArray:[current subviews]];
    if ([current isKindOfClass:[UIScrollView class]])
      static_cast<UIScrollView*>(current).scrollsToTop = NO;
  }
}

CrFullscreenState CrFullscreenStateFromWKFullscreenState(
    WKFullscreenState state) API_AVAILABLE(ios(16.0)) {
  switch (state) {
    case WKFullscreenStateEnteringFullscreen:
      return CrFullscreenState::kEnteringFullscreen;
    case WKFullscreenStateExitingFullscreen:
      return CrFullscreenState::kExitingFullscreen;
    case WKFullscreenStateInFullscreen:
      return CrFullscreenState::kInFullscreen;
    case WKFullscreenStateNotInFullscreen:
      return CrFullscreenState::kNotInFullScreen;
    default:
      NOTREACHED_IN_MIGRATION();
      return CrFullscreenState::kNotInFullScreen;
  }
}

#pragma mark - Security Helpers

- (void)updateSSLStatusForCurrentNavigationItem {
  if (_isBeingDestroyed) {
    return;
  }

  NavigationManagerImpl* navManager = self.navigationManagerImpl;
  web::NavigationItem* currentNavItem = navManager->GetLastCommittedItem();
  if (!currentNavItem) {
    return;
  }

  if (!_SSLStatusUpdater) {
    _SSLStatusUpdater =
        [[CRWSSLStatusUpdater alloc] initWithDataSource:self
                                      navigationManager:navManager];
    [_SSLStatusUpdater setDelegate:self];
  }
  NSString* host = base::SysUTF8ToNSString(_documentURL.host());
  BOOL hasOnlySecureContent = [self.webView hasOnlySecureContent];
  base::apple::ScopedCFTypeRef<SecTrustRef> trust;
  trust.reset([self.webView serverTrust], base::scoped_policy::RETAIN);

  [_SSLStatusUpdater updateSSLStatusForNavigationItem:currentNavItem
                                         withCertHost:host
                                                trust:std::move(trust)
                                 hasOnlySecureContent:hasOnlySecureContent];
}

#pragma mark - WebView Helpers

// Creates a container view if it's not yet created.
- (void)ensureContainerViewCreated {
  if (_containerView)
    return;

  DCHECK(!_isBeingDestroyed);
  // Create the top-level parent view, which will contain the content. Note,
  // this needs to be created with a non-zero size to allow for subviews with
  // autosize constraints to be correctly processed.
  _containerView =
      [[CRWWebControllerContainerView alloc] initWithDelegate:self];

  // This will be resized later, but matching the final frame will minimize
  // re-rendering.
  UIView* browserContainer = self.webStateImpl->GetWebViewContainer();
  if (browserContainer) {
    _containerView.frame = browserContainer.bounds;
  } else {
    // Use the screen size because the application's key window and the
    // container may still be nil.
    _containerView.frame = GetAnyKeyWindow() ? GetAnyKeyWindow().bounds
                                             : UIScreen.mainScreen.bounds;
  }

  DCHECK(!CGRectIsEmpty(_containerView.frame));

  [_containerView addGestureRecognizer:[self touchTrackingRecognizer]];
}

// Creates a web view if it's not yet created.
- (WKWebView*)ensureWebViewCreated {
  WKWebViewConfiguration* config =
      [self webViewConfigurationProvider].GetWebViewConfiguration();
  return [self ensureWebViewCreatedWithConfiguration:config];
}

// Creates a web view with given `config`. No-op if web view is already created.
- (WKWebView*)ensureWebViewCreatedWithConfiguration:
    (WKWebViewConfiguration*)config {
  if (!self.webView) {
    // This has to be called to ensure the container view of `self.webView` is
    // created. Otherwise `self.webView.frame.size` will be CGSizeZero which
    // fails a DCHECK later.
    [self ensureContainerViewCreated];

    [self setWebView:[self webViewWithConfiguration:config]];
    // The following is not called in -setWebView: as the latter used in unit
    // tests with fake web view, which cannot be added to view hierarchy.
    CHECK(_webUsageEnabled) << "Tried to create a web view while suspended!";

    DCHECK(self.webView);

    [self.webView setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                                      UIViewAutoresizingFlexibleHeight];


    // WKWebViews with invalid or empty frames have exhibited rendering bugs, so
    // resize the view to match the container view upon creation.
    [self.webView setFrame:[_containerView bounds]];
  }

  // If web view is not currently displayed and if the visible NavigationItem
  // should be loaded in this web view, display it immediately.  Otherwise, it
  // will be displayed when the pending load is committed.
  if (![_containerView webViewContentView]) {
    [self displayWebView];
  }

  return self.webView;
}

// Returns a new autoreleased web view created with given configuration.
- (WKWebView*)webViewWithConfiguration:(WKWebViewConfiguration*)config {
  // Do not attach the context menu controller immediately as the JavaScript
  // delegate must be specified.
  web::UserAgentType defaultUserAgent = web::UserAgentType::AUTOMATIC;
  web::NavigationItem* item = self.currentNavItem;
  web::UserAgentType userAgentType =
      item ? item->GetUserAgentType() : defaultUserAgent;
  if (userAgentType == web::UserAgentType::AUTOMATIC) {
    userAgentType =
        web::GetWebClient()->GetDefaultUserAgent(self.webStateImpl, GURL());
  }

  return web::BuildWKWebView(CGRectZero, config,
                             self.webStateImpl->GetBrowserState(),
                             userAgentType, self);
}

// Wraps the web view in a CRWWebViewContentView and adds it to the container
// view.
- (void)displayWebView {
  if (!self.webView || [_containerView webViewContentView])
    return;

  CrFullscreenState fullScreenState = CrFullscreenState::kNotInFullScreen;
  if (@available(iOS 16.0, *)) {
    fullScreenState =
        CrFullscreenStateFromWKFullscreenState(self.webView.fullscreenState);
  }
  CRWWebViewContentView* webViewContentView =
      [[CRWWebViewContentView alloc] initWithWebView:self.webView
                                          scrollView:self.webScrollView
                                     fullscreenState:fullScreenState];

  if (web::GetWebClient()->EnableLongPressUIContextMenu()) {
    self.contextMenuController =
        [[CRWContextMenuController alloc] initWithWebView:self.webView
                                                 webState:self.webStateImpl
                                            containerView:webViewContentView];
  }

  [_containerView displayWebViewContentView:webViewContentView];
}

- (void)removeWebView {
  if (!self.webView)
    return;

  self.webStateImpl->CancelDialogs();
  self.navigationManagerImpl->DetachFromWebView();

  [self setWebView:nil];
  [self.navigationHandler stopLoading];
  [_containerView resetContentForShutdown:YES];

  // webView:didFailProvisionalNavigation:withError: may never be called after
  // resetting WKWebView, so it is important to clear pending navigations now.
  for (__strong id navigation in
       [self.navigationHandler.navigationStates pendingNavigations]) {
    [self.navigationHandler.navigationStates removeNavigation:navigation];
  }
}

// Returns the WKWebViewConfigurationProvider associated with the web
// controller's BrowserState.
- (web::WKWebViewConfigurationProvider&)webViewConfigurationProvider {
  web::BrowserState* browserState = self.webStateImpl->GetBrowserState();
  return web::WKWebViewConfigurationProvider::FromBrowserState(browserState);
}

#pragma mark - CRWWKUIHandlerDelegate

- (WKWebView*)UIHandler:(CRWWKUIHandler*)UIHandler
    createWebViewWithConfiguration:(WKWebViewConfiguration*)configuration
                       forWebState:(web::WebState*)webState {
  CRWWebController* webController =
      web::WebStateImpl::FromWebState(webState)->GetWebController();
  DCHECK(!webController || webState->HasOpener());

  [webController ensureWebViewCreatedWithConfiguration:configuration];
  return webController.webView;
}

- (BOOL)UIHandler:(CRWWKUIHandler*)UIHandler
    isUserInitiatedAction:(WKNavigationAction*)action {
  return [self isUserInitiatedAction:action];
}

#pragma mark - WKNavigationDelegate Helpers

// Called when a page has actually started loading (i.e., for
// a web page the document has actually changed), or after the load request has
// been registered for a non-document-changing URL change. Updates internal
// state not specific to web pages.
- (void)didStartLoading {
  self.navigationHandler.navigationState = web::WKNavigationState::STARTED;
  _userInteractionState.SetUserInteractionRegisteredSincePageLoaded(false);
}

#pragma mark - CRWSSLStatusUpdaterDataSource

- (void)SSLStatusUpdater:(CRWSSLStatusUpdater*)SSLStatusUpdater
    querySSLStatusForTrust:(base::apple::ScopedCFTypeRef<SecTrustRef>)trust
                      host:(NSString*)host
         completionHandler:(StatusQueryHandler)completionHandler {
  [_certVerificationController querySSLStatusForTrust:std::move(trust)
                                                 host:host
                                    completionHandler:completionHandler];
}

#pragma mark - CRWSSLStatusUpdaterDelegate

- (void)SSLStatusUpdater:(CRWSSLStatusUpdater*)SSLStatusUpdater
    didChangeSSLStatusForNavigationItem:(web::NavigationItem*)navigationItem {
  web::NavigationItem* visibleItem =
      self.webStateImpl->GetNavigationManager()->GetVisibleItem();
  if (navigationItem == visibleItem)
    self.webStateImpl->DidChangeVisibleSecurityState();
}

#pragma mark - KVO Observation

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  DCHECK(!self.beingDestroyed);
  NSString* dispatcherSelectorName = self.WKWebViewObservers[keyPath];
  DCHECK(dispatcherSelectorName);
  if (dispatcherSelectorName) {
    // With ARC memory management, it is not known what a method called
    // via a selector will return. If a method returns a retained value
    // (e.g. NS_RETURNS_RETAINED) that returned object will leak as ARC is
    // unable to property insert the correct release calls for it.
    // All selectors used here return void and take no parameters so it's safe
    // to call a function mapping to the method implementation manually.
    SEL selector = NSSelectorFromString(dispatcherSelectorName);
    IMP methodImplementation = [self methodForSelector:selector];
    if (methodImplementation) {
      void (*methodCallFunction)(id, SEL) =
          reinterpret_cast<void (*)(id, SEL)>(methodImplementation);
      methodCallFunction(self, selector);
    }
  }
}

// Called when WKWebView certificateChain or hasOnlySecureContent property has
// changed.
- (void)webViewSecurityFeaturesDidChange {
  if (self.navigationHandler.navigationState ==
      web::WKNavigationState::REQUESTED) {
    // Do not update SSL Status for pending load. It will be updated in
    // `webView:didCommitNavigation:` callback.
    return;
  }
  web::NavigationItem* item =
      self.webStateImpl->GetNavigationManager()->GetLastCommittedItem();
  // SSLStatus is manually set in CRWWKNavigationHandler for SSL errors, so
  // skip calling the update method in these cases.
  if (item && !net::IsCertStatusError(item->GetSSL().cert_status)) {
    [self updateSSLStatusForCurrentNavigationItem];
  }
}

// Called when WKWebView title has been changed.
- (void)webViewTitleDidChange {
  // WKWebView's title becomes empty when the web process dies; ignore that
  // update.
  if (self.navigationHandler.webProcessCrashed) {
    DCHECK_EQ(self.webView.title.length, 0U);
    return;
  }

  web::WKNavigationState lastNavigationState =
      [self.navigationHandler.navigationStates lastAddedNavigationState];
  bool hasPendingNavigation =
      lastNavigationState == web::WKNavigationState::REQUESTED ||
      lastNavigationState == web::WKNavigationState::STARTED ||
      lastNavigationState == web::WKNavigationState::REDIRECTED;

  if (!hasPendingNavigation) {
    // Do not update the title if there is a navigation in progress because
    // there is no way to tell if KVO change fired for new or previous page.
    [self.navigationHandler
        setLastCommittedNavigationItemTitle:self.webView.title];
  }
}

// Called when WKWebView cameraCaptureState property has changed.
- (void)webViewCameraCaptureStateDidChange {
  self.webStateImpl->OnStateChangedForPermission(web::PermissionCamera);
}

// Called when WKWebView microphoneCaptureState property has changed.
- (void)webViewMicrophoneCaptureStateDidChange {
  self.webStateImpl->OnStateChangedForPermission(web::PermissionMicrophone);
}

// Called when WKWebView underPageBackgroundColor property has changed.
- (void)webViewUnderPageBackgroundColorDidChange {
  self.webStateImpl->OnUnderPageBackgroundColorChanged();
}

- (void)fullscreenStateDidChange {
  if (@available(iOS 16.0, *)) {
    CrFullscreenState fullScreenState =
        CrFullscreenStateFromWKFullscreenState(self.webView.fullscreenState);
    [_containerView updateWebViewContentViewFullscreenState:fullScreenState];
    // Update state for `fullscreenModeOn` so that we can expose the current
    // status of fullscreen mode through different interfaces.
    _webPageInFullscreenMode =
        fullScreenState == CrFullscreenState::kInFullscreen;
    base::UmaHistogramEnumeration(kFullScreenStateHistogram, fullScreenState);
  }
}

#pragma mark - CRWWebViewHandlerDelegate

- (web::WebStateImpl*)webStateImplForWebViewHandler:
    (CRWWebViewHandler*)handler {
  return self.webStateImpl;
}

- (const GURL&)documentURLForWebViewHandler:(CRWWebViewHandler*)handler {
  return _documentURL;
}

- (web::UserInteractionState*)userInteractionStateForWebViewHandler:
    (CRWWebViewHandler*)handler {
  return &_userInteractionState;
}

- (void)webViewHandlerUpdateSSLStatusForCurrentNavigationItem:
    (CRWWebViewHandler*)handler {
  [self updateSSLStatusForCurrentNavigationItem];
}

- (void)webViewHandler:(CRWWebViewHandler*)handler
    didFinishNavigation:(web::NavigationContextImpl*)context {
  [self didFinishNavigation:context];
}

- (void)ensureWebViewCreatedForWebViewHandler:(CRWWebViewHandler*)handler {
  [self ensureWebViewCreated];
}

- (WKWebView*)webViewForWebViewHandler:(CRWWebViewHandler*)handler {
  return self.webView;
}

#pragma mark - CRWWebViewNavigationObserverDelegate

- (CRWWKNavigationHandler*)navigationHandlerForNavigationObserver:
    (CRWWebViewNavigationObserver*)navigationObserver {
  return self.navigationHandler;
}

- (void)navigationObserver:(CRWWebViewNavigationObserver*)navigationObserver
      didChangeDocumentURL:(const GURL&)documentURL
                forContext:(web::NavigationContextImpl*)context {
  [self setDocumentURL:documentURL context:context];
}

- (void)navigationObserver:(CRWWebViewNavigationObserver*)navigationObserver
    didChangePageWithContext:(web::NavigationContextImpl*)context {
  [self.navigationHandler webPageChangedWithContext:context
                                            webView:self.webView];
}

- (void)navigationObserver:(CRWWebViewNavigationObserver*)navigationObserver
                didLoadNewURL:(const GURL&)webViewURL
    forSameDocumentNavigation:(BOOL)isSameDocumentNavigation {
  std::unique_ptr<web::NavigationContextImpl> newContext =
      [_requestController registerLoadRequestForURL:webViewURL
                             sameDocumentNavigation:isSameDocumentNavigation
                                     hasUserGesture:NO
                                  rendererInitiated:YES];
  [self.navigationHandler webPageChangedWithContext:newContext.get()
                                            webView:self.webView];
  newContext->SetHasCommitted(!isSameDocumentNavigation);
  self.webStateImpl->OnNavigationFinished(newContext.get());
  // TODO(crbug.com/41359661): It is OK, but very brittle, to call
  // `didFinishNavigation:` here because the gating condition is mutually
  // exclusive with the condition below. Refactor this method after
  // deprecating self.navigationHandler.pendingNavigationInfo.
  if (newContext->GetWKNavigationType() == WKNavigationTypeBackForward) {
    [self didFinishNavigation:newContext.get()];
  }
}

- (void)navigationObserver:(CRWWebViewNavigationObserver*)navigationObserver
    URLDidChangeWithoutDocumentChange:(const GURL&)newURL {
  DCHECK(newURL == net::GURLWithNSURL(self.webView.URL));

  if (base::FeatureList::IsEnabled(
          web::features::kCrashOnUnexpectedURLChange)) {
    if (_documentURL.DeprecatedGetOriginAsURL() !=
        newURL.DeprecatedGetOriginAsURL()) {
      if (!_documentURL.host().empty() &&
          (base::Contains(newURL.username(), _documentURL.host()) ||
           base::Contains(newURL.password(), _documentURL.host()))) {
        CHECK(false);
      }
    }
  }

  DCHECK(_documentURL != newURL);

  // If called during window.history.pushState or window.history.replaceState
  // JavaScript evaluation, only update the document URL. This callback does not
  // have any information about the state object and cannot create (or edit) the
  // navigation entry for this page change. Web controller will sync with
  // history changes when a window.history.didPushState or
  // window.history.didReplaceState message is received, which should happen in
  // the next runloop.
  //
  // Otherwise, simulate the whole delegate flow for a load (since the
  // superclass currently doesn't have a clean separation between URL changes
  // and document changes). Note that the order of these calls is important:
  // registering a load request logically comes before updating the document
  // URL, but also must come first since it uses state that is reset on URL
  // changes.

  // `newNavigationContext` only exists if this method has to create a new
  // context object.
  std::unique_ptr<web::NavigationContextImpl> newNavigationContext;
  if (!self.jsNavigationHandler.changingHistoryState) {
    if ([self.navigationHandler
            contextForPendingMainFrameNavigationWithURL:newURL]) {
      // NavigationManager::LoadURLWithParams() was called with URL that has
      // different fragment comparing to the previous URL.
    } else {
      // This could be:
      //   1.) Renderer-initiated fragment change
      //   2.) Assigning same-origin URL to window.location
      //   3.) Incorrectly handled window.location.replace (crbug.com/307072)
      //   4.) Back-forward same document navigation
      newNavigationContext =
          [_requestController registerLoadRequestForURL:newURL
                                 sameDocumentNavigation:YES
                                         hasUserGesture:NO
                                      rendererInitiated:YES];
    }
  }

  [self setDocumentURL:newURL context:newNavigationContext.get()];

  if (!self.jsNavigationHandler.changingHistoryState) {
    // Pass either newly created context (if it exists) or context that already
    // existed before.
    web::NavigationContextImpl* navigationContext = newNavigationContext.get();
    if (!navigationContext) {
      navigationContext = [self.navigationHandler
          contextForPendingMainFrameNavigationWithURL:newURL];
    }
    navigationContext->SetIsSameDocument(true);
    self.webStateImpl->OnNavigationStarted(navigationContext);
    [self didStartLoading];
    self.navigationManagerImpl->CommitPendingItem(
        navigationContext->ReleaseItem());
    navigationContext->SetHasCommitted(true);
    self.webStateImpl->OnNavigationFinished(navigationContext);

    [self updateSSLStatusForCurrentNavigationItem];
    [self didFinishNavigation:navigationContext];
  }
}

#pragma mark - CRWWKNavigationHandlerDelegate

- (CRWCertVerificationController*)
    certVerificationControllerForNavigationHandler:
        (CRWWKNavigationHandler*)navigationHandler {
  return _certVerificationController;
}

- (void)navigationHandler:(CRWWKNavigationHandler*)navigationHandler
        createWebUIForURL:(const GURL&)URL {
  [_requestController createWebUIForURL:URL];
}

- (void)navigationHandler:(CRWWKNavigationHandler*)navigationHandler
           setDocumentURL:(const GURL&)newURL
                  context:(web::NavigationContextImpl*)context {
  [self setDocumentURL:newURL context:context];
}

- (std::unique_ptr<web::NavigationContextImpl>)
            navigationHandler:(CRWWKNavigationHandler*)navigationHandler
    registerLoadRequestForURL:(const GURL&)URL
       sameDocumentNavigation:(BOOL)sameDocumentNavigation
               hasUserGesture:(BOOL)hasUserGesture
            rendererInitiated:(BOOL)renderedInitiated {
  return [_requestController registerLoadRequestForURL:URL
                                sameDocumentNavigation:sameDocumentNavigation
                                        hasUserGesture:hasUserGesture
                                     rendererInitiated:renderedInitiated];
}

- (void)navigationHandlerDisplayWebView:
    (CRWWKNavigationHandler*)navigationHandler {
  [self displayWebView];
}

- (void)navigationHandlerDidStartLoading:
    (CRWWKNavigationHandler*)navigationHandler {
  [self didStartLoading];
}

- (void)navigationHandlerWebProcessDidCrash:
    (CRWWKNavigationHandler*)navigationHandler {
  self.webStateImpl->CancelDialogs();
  self.webStateImpl->OnRenderProcessGone();
}

- (void)navigationHandler:(CRWWKNavigationHandler*)navigationHandler
    loadCurrentURLWithRendererInitiatedNavigation:(BOOL)rendererInitiated {
  [self loadCurrentURLWithRendererInitiatedNavigation:rendererInitiated];
}

- (void)navigationHandler:(CRWWKNavigationHandler*)navigationHandler
    didCompleteLoadWithSuccess:(BOOL)loadSuccess
                    forContext:(web::NavigationContextImpl*)context {
  [self loadCompleteWithSuccess:loadSuccess forContext:context];
}

- (void)resumeDownloadWithData:(NSData*)data
             completionHandler:(void (^)(WKDownload*))completionHandler {
  // Reports some failure to higher level code if `webView` doesn't exist
  if (!_webView) {
    completionHandler(nil);
    return;
  }
  [_webView resumeDownloadFromResumeData:data
                       completionHandler:completionHandler];
}

#pragma mark - CRWWebRequestControllerDelegate

- (void)webRequestControllerStopLoading:
    (CRWWebRequestController*)requestController {
  [self stopLoading];
}

- (void)webRequestControllerDidStartLoading:
    (CRWWebRequestController*)requestController {
  [self didStartLoading];
}

- (CRWWKNavigationHandler*)webRequestControllerNavigationHandler:
    (CRWWebRequestController*)requestController {
  return self.navigationHandler;
}

#pragma mark -  CRWInputViewProvider

- (id<CRWResponderInputView>)responderInputView {
  web::WebState* webState = self.webStateImpl;
  if (webState && webState->GetDelegate()) {
    return webState->GetDelegate()->GetResponderInputView(webState);
  }
  return nil;
}

#pragma mark - UIDropInteractionDelegate

- (BOOL)dropInteraction:(UIDropInteraction*)interaction
       canHandleSession:(id<UIDropSession>)session {
  return session.items.count == 1U &&
         [session canLoadObjectsOfClass:[NSURL class]];
}

- (UIDropProposal*)dropInteraction:(UIDropInteraction*)interaction
                  sessionDidUpdate:(id<UIDropSession>)session {
  return [[UIDropProposal alloc] initWithDropOperation:UIDropOperationCopy];
}

- (void)dropInteraction:(UIDropInteraction*)interaction
            performDrop:(id<UIDropSession>)session {
  DCHECK_EQ(1U, session.items.count);
  if ([session canLoadObjectsOfClass:[NSURL class]]) {
    __weak CRWWebController* weakSelf = self;
    [session loadObjectsOfClass:[NSURL class]
                     completion:^(NSArray<NSURL*>* objects) {
                       [weakSelf loadUrlObjectsCompletion:objects];
                     }];
  }
}

- (void)loadUrlObjectsCompletion:(NSArray<NSURL*>*)objects {
  GURL URL = net::GURLWithNSURL([objects firstObject]);
  if (!_isBeingDestroyed && URL.is_valid() && URL.SchemeIsHTTPOrHTTPS()) {
    web::NavigationManager::WebLoadParams params(URL);
    params.transition_type = ui::PAGE_TRANSITION_TYPED;
    self.webStateImpl->GetNavigationManager()->LoadURLWithParams(params);
  }
}

#pragma mark - Testing-Only Methods

- (void)injectWebViewContentView:(CRWWebViewContentView*)webViewContentView {
  _currentURLLoadWasTrigerred = NO;
  [self removeWebView];

  [_containerView displayWebViewContentView:webViewContentView];
  [self setWebView:static_cast<WKWebView*>(webViewContentView.webView)];
}

- (void)resetInjectedWebViewContentView {
  _currentURLLoadWasTrigerred = NO;
  [self setWebView:nil];
  [_containerView removeFromSuperview];
  _containerView = nil;
}

- (web::WKNavigationState)navigationState {
  return self.navigationHandler.navigationState;
}

@end
