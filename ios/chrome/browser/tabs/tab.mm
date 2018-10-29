// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/tab.h"

#import <CoreLocation/CoreLocation.h>
#import <UIKit/UIKit.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/ios/block_types.h"
#include "base/json/string_escape.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/scoped_observer.h"
#include "base/strings/string_split.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/google/core/common/google_util.h"
#include "components/history/core/browser/history_context.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/top_sites.h"
#include "components/history/ios/browser/web_state_top_sites_observer.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/prefs/pref_service.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/search_engines/template_url_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/download/download_manager_tab_helper.h"
#include "ios/chrome/browser/experimental_flags.h"
#import "ios/chrome/browser/find_in_page/find_in_page_controller.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/history/history_tab_helper.h"
#include "ios/chrome/browser/history/top_sites_factory.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/metrics/tab_usage_recorder.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/snapshots/snapshot_cache.h"
#import "ios/chrome/browser/snapshots/snapshot_cache_factory.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#import "ios/chrome/browser/tabs/legacy_tab_helper.h"
#import "ios/chrome/browser/tabs/tab_dialog_delegate.h"
#import "ios/chrome/browser/tabs/tab_helper_util.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_private.h"
#include "ios/chrome/browser/translate/chrome_ios_translate_client.h"
#import "ios/chrome/browser/u2f/u2f_controller.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/commands/show_signin_command.h"
#import "ios/chrome/browser/ui/open_in_controller.h"
#import "ios/chrome/browser/ui/overscroll_actions/overscroll_actions_controller.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/voice/voice_search_navigations_tab_helper.h"
#import "ios/chrome/browser/web/page_placeholder_tab_helper.h"
#import "ios/chrome/browser/web/tab_id_tab_helper.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#include "ios/web/public/favicon_status.h"
#include "ios/web/public/favicon_url.h"
#include "ios/web/public/interstitials/web_interstitial.h"
#include "ios/web/public/load_committed_details.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/referrer.h"
#import "ios/web/public/serializable_user_data_manager.h"
#include "ios/web/public/ssl_status.h"
#include "ios/web/public/url_scheme_util.h"
#include "ios/web/public/url_util.h"
#include "ios/web/public/web_client.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"
#import "ios/web/public/web_state/navigation_context.h"
#import "ios/web/public/web_state/ui/crw_generic_content_view.h"
#import "ios/web/public/web_state/web_state.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"
#include "ios/web/public/web_thread.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/web_state_impl.h"
#include "net/base/escape.h"
#include "net/base/filename_util.h"
#import "net/base/mac/url_conversions.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cert/x509_certificate.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "url/origin.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kTabUrlStartedLoadingNotificationForCrashReporting =
    @"kTabUrlStartedLoadingNotificationForCrashReporting";
NSString* const kTabUrlMayStartLoadingNotificationForCrashReporting =
    @"kTabUrlMayStartLoadingNotificationForCrashReporting";
NSString* const kTabIsShowingExportableNotificationForCrashReporting =
    @"kTabIsShowingExportableNotificationForCrashReporting";
NSString* const kTabClosingCurrentDocumentNotificationForCrashReporting =
    @"kTabClosingCurrentDocumentNotificationForCrashReporting";

NSString* const kTabUrlKey = @"url";

@interface Tab ()<CRWWebStateObserver> {
  __weak TabModel* _parentTabModel;
  ios::ChromeBrowserState* _browserState;

  OpenInController* _openInController;

  // The Overscroll controller responsible for displaying the
  // overscrollActionsView above the toolbar.
  OverscrollActionsController* _overscrollActionsController;

  // WebStateImpl for this tab.
  web::WebStateImpl* _webStateImpl;

  // Allows Tab to conform CRWWebStateDelegate protocol.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;

  // Universal Second Factor (U2F) call controller.
  U2FController* _secondFactorController;

}

// Returns the OpenInController for this tab.
- (OpenInController*)openInController;

// Handles exportable files if possible.
- (void)handleExportableFile:(net::HttpResponseHeaders*)headers;

@end

@implementation Tab

@synthesize browserState = _browserState;
@synthesize overscrollActionsController = _overscrollActionsController;
@synthesize overscrollActionsControllerDelegate =
    overscrollActionsControllerDelegate_;
@synthesize dialogDelegate = dialogDelegate_;

#pragma mark - Initializers

- (instancetype)initWithWebState:(web::WebState*)webState {
  DCHECK(webState);
  self = [super init];
  if (self) {
    // TODO(crbug.com/620465): Tab should only use public API of WebState.
    // Remove this cast once this is the case.
    _webStateImpl = static_cast<web::WebStateImpl*>(webState);
    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    _webStateImpl->AddObserver(_webStateObserver.get());

    _browserState =
        ios::ChromeBrowserState::FromBrowserState(webState->GetBrowserState());
  }
  return self;
}

#pragma mark - NSObject protocol

- (void)dealloc {
  // The WebState owns the Tab, so -webStateDestroyed: should be called before
  // -dealloc and _webStateImpl set to nullptr.
  DCHECK(!_webStateImpl);
}

- (NSString*)description {
  return
      [NSString stringWithFormat:@"%p ... %@ - %s", self, self.title,
                                 self.webState->GetVisibleURL().spec().c_str()];
}

#pragma mark - Properties

- (NSString*)title {
  base::string16 title;

  web::WebState* webState = self.webState;
  if (!webState->GetNavigationManager()->GetVisibleItem() &&
      DownloadManagerTabHelper::FromWebState(webState)->has_download_task()) {
    title = l10n_util::GetStringUTF16(IDS_DOWNLOAD_TAB_TITLE);
  } else {
    title = webState->GetTitle();
    if (title.empty())
      title = l10n_util::GetStringUTF16(IDS_DEFAULT_TAB_TITLE);
  }

  return base::SysUTF16ToNSString(title);
}

- (NSString*)tabId {
  if (!self.webState) {
    // Tab can outlive WebState, in which case Tab is not valid anymore and
    // tabId should be nil.
    return nil;
  }
  TabIdTabHelper* tab_id_helper = TabIdTabHelper::FromWebState(self.webState);
  DCHECK(tab_id_helper);
  return tab_id_helper->tab_id();
}

- (web::WebState*)webState {
  return _webStateImpl;
}

- (BOOL)canGoBack {
  return self.navigationManager && self.navigationManager->CanGoBack();
}

- (BOOL)canGoForward {
  return self.navigationManager && self.navigationManager->CanGoForward();
}

- (void)setOverscrollActionsControllerDelegate:
    (id<OverscrollActionsControllerDelegate>)
        overscrollActionsControllerDelegate {
  if (overscrollActionsControllerDelegate_ ==
      overscrollActionsControllerDelegate) {
    return;
  }

  // Lazily create a OverscrollActionsController.
  // The check for overscrollActionsControllerDelegate is necessary to avoid
  // recreating a OverscrollActionsController during teardown.
  if (!_overscrollActionsController) {
    _overscrollActionsController = [[OverscrollActionsController alloc]
        initWithWebViewProxy:self.webState->GetWebViewProxy()];
  }
  OverscrollStyle style = OverscrollStyle::REGULAR_PAGE_NON_INCOGNITO;
  if (_browserState->IsOffTheRecord())
    style = OverscrollStyle::REGULAR_PAGE_INCOGNITO;
  [_overscrollActionsController setStyle:style];
  [_overscrollActionsController
      setDelegate:overscrollActionsControllerDelegate];
  [_overscrollActionsController setBrowserState:self.browserState];
  overscrollActionsControllerDelegate_ = overscrollActionsControllerDelegate;
}

- (BOOL)isVoiceSearchResultsTab {
  // TODO(crbug.com/778416): Move this logic entirely into helper.
  // If nothing has been loaded in the Tab, it cannot be displaying a voice
  // search results page.
  web::NavigationItem* item =
      self.webState->GetNavigationManager()->GetVisibleItem();
  if (!item)
    return NO;
  // Navigating through history to a NavigationItem that was created for a voice
  // search query should just be treated like a normal page load.
  if ((item->GetTransitionType() & ui::PAGE_TRANSITION_FORWARD_BACK) != 0)
    return NO;
  // Check whether |item| has been marked as a voice search result navigation.
  return VoiceSearchNavigationTabHelper::FromWebState(self.webState)
      ->IsNavigationFromVoiceSearch(item);
}

- (BOOL)loadFinished {
  return self.webState && !self.webState->IsLoading();
}

#pragma mark - Public API

- (void)setParentTabModel:(TabModel*)model {
  DCHECK(!model || !_parentTabModel);
  _parentTabModel = model;
}

- (UIView*)view {
  if (!self.webState)
    return nil;

  // Record reload of previously-evicted tab.
  if (self.webState->IsEvicted() && [_parentTabModel tabUsageRecorder])
    [_parentTabModel tabUsageRecorder]->RecordPageLoadStart(self.webState);

  // Do not trigger the load if the tab has crashed. SadTabTabHelper is
  // responsible for handing reload logic for crashed tabs.
  if (!self.webState->IsCrashed()) {
    self.webState->GetNavigationManager()->LoadIfNecessary();
  }
  return self.webState->GetView();
}

- (UIView*)viewForPrinting {
  return self.webController.viewForPrinting;
}

// Halt the tab, which amounts to halting its webController.
- (void)terminateNetworkActivity {
  [self.webController terminateNetworkActivity];
}

- (void)dismissModals {
  [_openInController disable];
  [self.webController dismissModals];
}

- (web::NavigationManager*)navigationManager {
  return self.webState ? self.webState->GetNavigationManager() : nullptr;
}

- (void)goBack {
  if (self.navigationManager) {
    DCHECK(self.navigationManager->CanGoBack());
    base::RecordAction(base::UserMetricsAction("Back"));
    self.navigationManager->GoBack();
  }
}

- (void)goForward {
  if (self.navigationManager) {
    DCHECK(self.navigationManager->CanGoForward());
    base::RecordAction(base::UserMetricsAction("Forward"));
    self.navigationManager->GoForward();
  }
}

- (void)willUpdateSnapshot {
  [_overscrollActionsController clear];
}

- (void)notifyTabOfUrlMayStartLoading:(const GURL&)url {
  NSString* urlString = base::SysUTF8ToNSString(url.spec());
  if ([urlString length]) {
    [[NSNotificationCenter defaultCenter]
        postNotificationName:kTabUrlMayStartLoadingNotificationForCrashReporting
                      object:self
                    userInfo:@{kTabUrlKey : urlString}];
  }
}

#pragma mark - Public API (relatinge to User agent)

- (BOOL)usesDesktopUserAgent {
  if (!self.navigationManager)
    return NO;

  web::NavigationItem* visibleItem = self.navigationManager->GetVisibleItem();
  return visibleItem &&
         visibleItem->GetUserAgentType() == web::UserAgentType::DESKTOP;
}

- (void)reloadWithUserAgentType:(web::UserAgentType)userAgentType {
  web::NavigationManager* navigationManager = [self navigationManager];
  DCHECK(navigationManager);
  navigationManager->ReloadWithUserAgentType(userAgentType);
}

#pragma mark - Public API (relating to U2F)

- (void)evaluateU2FResultFromURL:(const GURL&)URL {
  DCHECK(_secondFactorController);
  [_secondFactorController evaluateU2FResultFromU2FURL:URL
                                              webState:self.webState];
}

- (GURL)XCallbackFromRequestURL:(const GURL&)requestURL
                      originURL:(const GURL&)originURL {
  // Create U2FController object lazily.
  if (!_secondFactorController)
    _secondFactorController = [[U2FController alloc] init];
  return [_secondFactorController
      XCallbackFromRequestURL:requestURL
                    originURL:originURL
                       tabURL:self.webState->GetLastCommittedURL()
                        tabID:self.tabId];
}

#pragma mark - CRWWebStateObserver protocol

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation {
  // Notify tab of Url may start loading, this notification is not sent in cases
  // of app launching, history api navigations, and hash change navigations.
  [self notifyTabOfUrlMayStartLoading:navigation->GetUrl()];

  [self.dialogDelegate cancelDialogForTab:self];
  [_openInController disable];
}

- (void)webState:(web::WebState*)webState
    didLoadPageWithSuccess:(BOOL)loadSuccess {
  DCHECK([self loadFinished]);

  if (loadSuccess) {
    scoped_refptr<net::HttpResponseHeaders> headers =
        _webStateImpl->GetHttpResponseHeaders();
    [self handleExportableFile:headers.get()];
  }
}

- (void)renderProcessGoneForWebState:(web::WebState*)webState {
  DCHECK(webState == _webStateImpl);
  [self.dialogDelegate cancelDialogForTab:self];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webStateImpl, webState);
  self.overscrollActionsControllerDelegate = nil;

  [_openInController detachFromWebController];
  _openInController = nil;
  [_overscrollActionsController invalidate];
  _overscrollActionsController = nil;

  // Cancel any queued dialogs.
  [self.dialogDelegate cancelDialogForTab:self];

  _webStateImpl->RemoveObserver(_webStateObserver.get());
  _webStateObserver.reset();
  _webStateImpl = nullptr;
}

#pragma mark - Private methods

- (OpenInController*)openInController {
  if (!_openInController) {
    _openInController = [[OpenInController alloc]
        initWithURLLoaderFactory:_browserState->GetSharedURLLoaderFactory()
                   webController:self.webController];
    _openInController.baseView = self.view;
  }
  return _openInController;
}

- (void)handleExportableFile:(net::HttpResponseHeaders*)headers {
  // Only "application/pdf" is supported for now.
  if (self.webState->GetContentsMimeType() != "application/pdf")
    return;

  [[NSNotificationCenter defaultCenter]
      postNotificationName:kTabIsShowingExportableNotificationForCrashReporting
                    object:self];
  // Try to generate a filename by first looking at |content_disposition_|, then
  // at the last component of WebState's last committed URL and if both of these
  // fail use the default filename "document".
  std::string contentDisposition;
  if (headers)
    headers->GetNormalizedHeader("content-disposition", &contentDisposition);
  std::string defaultFilename =
      l10n_util::GetStringUTF8(IDS_IOS_OPEN_IN_FILE_DEFAULT_TITLE);
  const GURL& lastCommittedURL = self.webState->GetLastCommittedURL();
  base::string16 filename =
      net::GetSuggestedFilename(lastCommittedURL, contentDisposition,
                                "",                 // referrer-charset
                                "",                 // suggested-name
                                "application/pdf",  // mime-type
                                defaultFilename);
  [[self openInController]
      enableWithDocumentURL:lastCommittedURL
          suggestedFilename:base::SysUTF16ToNSString(filename)];
}

@end

#pragma mark - TestingSupport

@implementation Tab (TestingSupport)

- (TabModel*)parentTabModel {
  return _parentTabModel;
}

// TODO(crbug.com/620465): this require the Tab's WebState to be a WebStateImpl,
// remove this helper once this is no longer true (and fix the unit tests).
- (CRWWebController*)webController {
  return _webStateImpl ? _webStateImpl->GetWebController() : nil;
}

@end
