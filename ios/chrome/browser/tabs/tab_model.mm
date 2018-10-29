// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/tab_model.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#import "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics_action.h"
#include "base/stl_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/post_task.h"
#include "components/favicon/ios/web_favicon_driver.h"
#include "components/navigation_metrics/navigation_metrics.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sessions/ios/ios_live_tab.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/chrome_url_util.h"
#include "ios/chrome/browser/crash_loop_detection_util.h"
#import "ios/chrome/browser/geolocation/omnibox_geolocation_controller.h"
#import "ios/chrome/browser/metrics/tab_usage_recorder.h"
#import "ios/chrome/browser/prerender/prerender_service_factory.h"
#include "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/sessions/session_ios.h"
#import "ios/chrome/browser/sessions/session_service_ios.h"
#import "ios/chrome/browser/sessions/session_window_ios.h"
#import "ios/chrome/browser/snapshots/snapshot_cache.h"
#import "ios/chrome/browser/snapshots/snapshot_cache_factory.h"
#import "ios/chrome/browser/snapshots/snapshot_cache_web_state_list_observer.h"
#include "ios/chrome/browser/tab_parenting_global_observer.h"
#import "ios/chrome/browser/tabs/legacy_tab_helper.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model_closing_web_state_observer.h"
#import "ios/chrome/browser/tabs/tab_model_favicon_driver_observer.h"
#import "ios/chrome/browser/tabs/tab_model_list.h"
#import "ios/chrome/browser/tabs/tab_model_notification_observer.h"
#import "ios/chrome/browser/tabs/tab_model_observers.h"
#import "ios/chrome/browser/tabs/tab_model_observers_bridge.h"
#import "ios/chrome/browser/tabs/tab_model_selected_tab_observer.h"
#import "ios/chrome/browser/tabs/tab_model_synced_window_delegate.h"
#import "ios/chrome/browser/tabs/tab_model_web_state_list_delegate.h"
#import "ios/chrome/browser/tabs/tab_parenting_observer.h"
#import "ios/chrome/browser/web/page_placeholder_tab_helper.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_metrics_observer.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer.h"
#import "ios/chrome/browser/web_state_list/web_state_list_serialization.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_state_list_web_usage_enabler.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_state_list_web_usage_enabler_factory.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/certificate_policy_cache.h"
#import "ios/web/public/load_committed_details.h"
#include "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#import "ios/web/public/serializable_user_data_manager.h"
#import "ios/web/public/web_state/navigation_context.h"
#include "ios/web/public/web_state/session_certificate_policy_cache.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"
#include "ios/web/public/web_task_traits.h"
#include "ios/web/public/web_thread.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Updates CRWSessionCertificatePolicyManager's certificate policy cache.
void UpdateCertificatePolicyCacheFromWebState(
    const scoped_refptr<web::CertificatePolicyCache>& policy_cache,
    const web::WebState* web_state) {
  DCHECK(web_state);
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  web_state->GetSessionCertificatePolicyCache()->UpdateCertificatePolicyCache(
      policy_cache);
}

// Populates the certificate policy cache based on the WebStates of
// |web_state_list|.
void RestoreCertificatePolicyCacheFromModel(
    const scoped_refptr<web::CertificatePolicyCache>& policy_cache,
    WebStateList* web_state_list) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  for (int index = 0; index < web_state_list->count(); ++index) {
    UpdateCertificatePolicyCacheFromWebState(
        policy_cache, web_state_list->GetWebStateAt(index));
  }
}

// Scrubs the certificate policy cache of all certificates policies except
// those for the current entries in |web_state_list|.
void CleanCertificatePolicyCache(
    base::CancelableTaskTracker* task_tracker,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const scoped_refptr<web::CertificatePolicyCache>& policy_cache,
    WebStateList* web_state_list) {
  DCHECK(policy_cache);
  DCHECK(web_state_list);
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  task_tracker->PostTaskAndReply(
      task_runner.get(), FROM_HERE,
      base::Bind(&web::CertificatePolicyCache::ClearCertificatePolicies,
                 policy_cache),
      base::Bind(&RestoreCertificatePolicyCacheFromModel, policy_cache,
                 base::Unretained(web_state_list)));
}

// Returns whether |rhs| and |lhs| are different user agent types. If either
// of them is web::UserAgentType::NONE, then return NO.
BOOL IsTransitionBetweenDesktopAndMobileUserAgent(web::UserAgentType lhs,
                                                  web::UserAgentType rhs) {
  if (lhs == web::UserAgentType::NONE)
    return NO;

  if (rhs == web::UserAgentType::NONE)
    return NO;

  return lhs != rhs;
}

// Returns whether TabUsageRecorder::RecordPageLoadStart should be called for
// the given navigation.
BOOL ShouldRecordPageLoadStartForNavigation(
    web::NavigationContext* navigation) {
  web::NavigationManager* navigation_manager =
      navigation->GetWebState()->GetNavigationManager();

  web::NavigationItem* last_committed_item =
      navigation_manager->GetLastCommittedItem();
  if (!last_committed_item) {
    // Opening a child window and loading URL there.
    // http://crbug.com/773160
    return NO;
  }

  web::NavigationItem* pending_item = navigation_manager->GetPendingItem();
  if (pending_item) {
    if (IsTransitionBetweenDesktopAndMobileUserAgent(
            pending_item->GetUserAgentType(),
            last_committed_item->GetUserAgentType())) {
      // Switching between Desktop and Mobile user agent.
      return NO;
    }
  }

  ui::PageTransition transition = navigation->GetPageTransition();
  if (!ui::PageTransitionIsNewNavigation(transition)) {
    // Back/forward navigation or reload.
    return NO;
  }

  if ((transition & ui::PAGE_TRANSITION_CLIENT_REDIRECT) != 0) {
    // Client redirect.
    return NO;
  }

  static const ui::PageTransition kRecordedPageTransitionTypes[] = {
      ui::PAGE_TRANSITION_TYPED,
      ui::PAGE_TRANSITION_LINK,
      ui::PAGE_TRANSITION_GENERATED,
      ui::PAGE_TRANSITION_AUTO_BOOKMARK,
      ui::PAGE_TRANSITION_FORM_SUBMIT,
      ui::PAGE_TRANSITION_KEYWORD,
      ui::PAGE_TRANSITION_KEYWORD_GENERATED,
  };

  for (size_t i = 0; i < base::size(kRecordedPageTransitionTypes); ++i) {
    const ui::PageTransition recorded_type = kRecordedPageTransitionTypes[i];
    if (ui::PageTransitionCoreTypeIs(transition, recorded_type)) {
      return YES;
    }
  }

  return NO;
}

// Records metrics for the interface's orientation.
void RecordInterfaceOrientationMetric() {
  switch ([[UIApplication sharedApplication] statusBarOrientation]) {
    case UIInterfaceOrientationPortrait:
    case UIInterfaceOrientationPortraitUpsideDown:
      UMA_HISTOGRAM_BOOLEAN("Tab.PageLoadInPortrait", YES);
      break;
    case UIInterfaceOrientationLandscapeLeft:
    case UIInterfaceOrientationLandscapeRight:
      UMA_HISTOGRAM_BOOLEAN("Tab.PageLoadInPortrait", NO);
      break;
    case UIInterfaceOrientationUnknown:
      // TODO(crbug.com/228832): Convert from a boolean histogram to an
      // enumerated histogram and log this case as well.
      break;
  }
}

// Records metrics for main frame navigation.
void RecordMainFrameNavigationMetric(web::WebState* web_state) {
  DCHECK(web_state);
  DCHECK(web_state->GetBrowserState());
  DCHECK(web_state->GetNavigationManager());
  web::NavigationItem* item =
      web_state->GetNavigationManager()->GetLastCommittedItem();
  navigation_metrics::RecordMainFrameNavigation(
      item ? item->GetVirtualURL() : GURL::EmptyGURL(), true,
      web_state->GetBrowserState()->IsOffTheRecord());
}

}  // anonymous namespace

@interface TabModel ()<CRWWebStateObserver, WebStateListObserving> {
  // Delegate for the WebStateList.
  std::unique_ptr<WebStateListDelegate> _webStateListDelegate;

  // Underlying shared model implementation.
  std::unique_ptr<WebStateList> _webStateList;

  // WebStateListObservers reacting to modifications of the model (may send
  // notification, translate and forward events, update metrics, ...).
  std::vector<std::unique_ptr<WebStateListObserver>> _webStateListObservers;

  // Strong references to id<WebStateListObserving> wrapped by non-owning
  // WebStateListObserverBridges.
  NSArray<id<WebStateListObserving>>* _retainedWebStateListObservers;

  // The delegate for sync.
  std::unique_ptr<TabModelSyncedWindowDelegate> _syncedWindowDelegate;

  // The observer that calls -notifyNewTabWillOpen on this object.
  TabModelNotificationObserver* _tabModelNotificationObserver;

  // Counters for metrics.
  WebStateListMetricsObserver* _webStateListMetricsObserver;

  // Backs up property with the same name.
  std::unique_ptr<TabUsageRecorder> _tabUsageRecorder;
  // Saves session's state.
  SessionServiceIOS* _sessionService;
  // List of TabModelObservers.
  TabModelObservers* _observers;

  // Used to ensure thread-safety of the certificate policy management code.
  base::CancelableTaskTracker _clearPoliciesTaskTracker;

  // Used to observe owned Tabs' WebStates.
  std::unique_ptr<web::WebStateObserver> _webStateObserver;
}

// Session window for the contents of the tab model.
@property(nonatomic, readonly) SessionIOS* sessionForSaving;
// Whether the underlying WebStateList's web usage is enabled.
@property(nonatomic, readonly, getter=isWebUsageEnabled) BOOL webUsageEnabled;

// Helper method to restore a saved session and control if the state should
// be persisted or not. Used to implement the public -restoreSessionWindow:
// method and restoring session in the initialiser.
- (BOOL)restoreSessionWindow:(SessionWindowIOS*)window
                persistState:(BOOL)persistState;

@end

@implementation TabModel

@synthesize browserState = _browserState;

#pragma mark - Overriden

- (void)dealloc {
  // browserStateDestroyed should always have been called before destruction.
  DCHECK(!_browserState);

  // Make sure the observers do clean after themselves.
  DCHECK([_observers empty]);
}

#pragma mark - Public methods

- (Tab*)currentTab {
  web::WebState* webState = _webStateList->GetActiveWebState();
  return webState ? LegacyTabHelper::GetTabForWebState(webState) : nil;
}

- (void)setCurrentTab:(Tab*)newTab {
  int indexOfTab = _webStateList->GetIndexOfWebState(newTab.webState);
  DCHECK_NE(indexOfTab, WebStateList::kInvalidIndex);
  _webStateList->ActivateWebStateAt(indexOfTab);
}

- (TabModelSyncedWindowDelegate*)syncedWindowDelegate {
  return _syncedWindowDelegate.get();
}

- (TabUsageRecorder*)tabUsageRecorder {
  return _tabUsageRecorder.get();
}

- (BOOL)isOffTheRecord {
  return _browserState && _browserState->IsOffTheRecord();
}

- (BOOL)isEmpty {
  return _webStateList->empty();
}

- (NSUInteger)count {
  DCHECK_GE(_webStateList->count(), 0);
  return static_cast<NSUInteger>(_webStateList->count());
}

- (WebStateList*)webStateList {
  return _webStateList.get();
}

- (instancetype)initWithSessionWindow:(SessionWindowIOS*)window
                       sessionService:(SessionServiceIOS*)service
                         browserState:(ios::ChromeBrowserState*)browserState {
  if ((self = [super init])) {
    _observers = [TabModelObservers observers];

    _webStateListDelegate =
        std::make_unique<TabModelWebStateListDelegate>(self);
    _webStateList = std::make_unique<WebStateList>(_webStateListDelegate.get());

    _browserState = browserState;
    DCHECK(_browserState);

    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);

    // Normal browser states are the only ones to get tab restore. Tab sync
    // handles incognito browser states by filtering on profile, so it's
    // important to the backend code to always have a sync window delegate.
    if (!_browserState->IsOffTheRecord()) {
      // Set up the usage recorder before tabs are created.
      _tabUsageRecorder = std::make_unique<TabUsageRecorder>(
          _webStateList.get(),
          PrerenderServiceFactory::GetForBrowserState(browserState));
    }
    _syncedWindowDelegate =
        std::make_unique<TabModelSyncedWindowDelegate>(_webStateList.get());

    // There must be a valid session service defined to consume session windows.
    DCHECK(service);
    _sessionService = service;

    NSMutableArray<id<WebStateListObserving>>* retainedWebStateListObservers =
        [[NSMutableArray alloc] init];

    TabModelClosingWebStateObserver* tabModelClosingWebStateObserver = [
        [TabModelClosingWebStateObserver alloc]
        initWithTabModel:self
          restoreService:IOSChromeTabRestoreServiceFactory::GetForBrowserState(
                             _browserState)];
    [retainedWebStateListObservers addObject:tabModelClosingWebStateObserver];

    _webStateListObservers.push_back(
        std::make_unique<WebStateListObserverBridge>(self));

    _webStateListObservers.push_back(
        std::make_unique<WebStateListObserverBridge>(
            tabModelClosingWebStateObserver));

    SnapshotCache* snapshotCache =
        SnapshotCacheFactory::GetForBrowserState(_browserState);
    if (snapshotCache) {
      _webStateListObservers.push_back(
          std::make_unique<SnapshotCacheWebStateListObserver>(snapshotCache));
    }

    _webStateListObservers.push_back(std::make_unique<TabParentingObserver>());

    TabModelSelectedTabObserver* tabModelSelectedTabObserver =
        [[TabModelSelectedTabObserver alloc] initWithTabModel:self];
    [retainedWebStateListObservers addObject:tabModelSelectedTabObserver];
    _webStateListObservers.push_back(
        std::make_unique<WebStateListObserverBridge>(
            tabModelSelectedTabObserver));

    TabModelObserversBridge* tabModelObserversBridge =
        [[TabModelObserversBridge alloc] initWithTabModel:self
                                        tabModelObservers:_observers];
    [retainedWebStateListObservers addObject:tabModelObserversBridge];
    _webStateListObservers.push_back(
        std::make_unique<WebStateListObserverBridge>(tabModelObserversBridge));

    _webStateListObservers.push_back(
        std::make_unique<TabModelFaviconDriverObserver>(self, _observers));

    auto webStateListMetricsObserver =
        std::make_unique<WebStateListMetricsObserver>();
    _webStateListMetricsObserver = webStateListMetricsObserver.get();
    _webStateListObservers.push_back(std::move(webStateListMetricsObserver));

    auto tabModelNotificationObserver =
        std::make_unique<TabModelNotificationObserver>(self);
    _tabModelNotificationObserver = tabModelNotificationObserver.get();
    _webStateListObservers.push_back(std::move(tabModelNotificationObserver));

    for (const auto& webStateListObserver : _webStateListObservers)
      _webStateList->AddObserver(webStateListObserver.get());
    _retainedWebStateListObservers = [retainedWebStateListObservers copy];

    if (window) {
      DCHECK([_observers empty]);
      // Restore the session and reset the session metrics (as the event have
      // not been generated by the user but by a cold start cycle).
      [self restoreSessionWindow:window persistState:NO];
      [self resetSessionMetrics];
    }

    // Register for resign active notification.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(willResignActive:)
               name:UIApplicationWillResignActiveNotification
             object:nil];
    // Register for background notification.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(applicationDidEnterBackground:)
               name:UIApplicationDidEnterBackgroundNotification
             object:nil];

    // Associate with ios::ChromeBrowserState.
    TabModelList::RegisterTabModelWithChromeBrowserState(_browserState, self);
  }
  return self;
}

- (BOOL)restoreSessionWindow:(SessionWindowIOS*)window {
  return [self restoreSessionWindow:window persistState:YES];
}

- (void)saveSessionImmediately:(BOOL)immediately {
  // Do nothing if there are tabs in the model but no selected tab. This is
  // a transitional state.
  if ((!self.currentTab && _webStateList->count()) || !_browserState)
    return;
  NSString* statePath =
      base::SysUTF8ToNSString(_browserState->GetStatePath().AsUTF8Unsafe());
  __weak TabModel* weakSelf = self;
  SessionIOSFactory sessionFactory = ^{
    return weakSelf.sessionForSaving;
  };
  [_sessionService saveSession:sessionFactory
                     directory:statePath
                   immediately:immediately];
}

- (Tab*)tabAtIndex:(NSUInteger)index {
  DCHECK_LE(index, static_cast<NSUInteger>(INT_MAX));
  return LegacyTabHelper::GetTabForWebState(
      _webStateList->GetWebStateAt(static_cast<int>(index)));
}

- (NSUInteger)indexOfTab:(Tab*)tab {
  int index = _webStateList->GetIndexOfWebState(tab.webState);
  if (index == WebStateList::kInvalidIndex)
    return NSNotFound;

  DCHECK_GE(index, 0);
  return static_cast<NSUInteger>(index);
}

- (Tab*)insertTabWithURL:(const GURL&)URL
                referrer:(const web::Referrer&)referrer
              transition:(ui::PageTransition)transition
                  opener:(Tab*)parentTab
             openedByDOM:(BOOL)openedByDOM
                 atIndex:(NSUInteger)index
            inBackground:(BOOL)inBackground {
  web::NavigationManager::WebLoadParams params(URL);
  params.referrer = referrer;
  params.transition_type = transition;
  return [self insertTabWithLoadParams:params
                                opener:parentTab
                           openedByDOM:openedByDOM
                               atIndex:index
                          inBackground:inBackground];
}

- (Tab*)insertOpenByDOMTabWithOpener:(Tab*)opener {
  DCHECK(_browserState);
  web::WebState::CreateParams createParams(_browserState);
  createParams.created_with_opener = YES;
  std::unique_ptr<web::WebState> webState = web::WebState::Create(createParams);

  int insertionFlags =
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE;
  int insertedIndex = _webStateList->InsertWebState(
      _webStateList->count(), std::move(webState), insertionFlags,
      WebStateOpener(opener.webState));

  return LegacyTabHelper::GetTabForWebState(
      _webStateList->GetWebStateAt(insertedIndex));
}

- (Tab*)insertTabWithLoadParams:
            (const web::NavigationManager::WebLoadParams&)loadParams
                         opener:(Tab*)parentTab
                    openedByDOM:(BOOL)openedByDOM
                        atIndex:(NSUInteger)index
                   inBackground:(BOOL)inBackground {
  DCHECK(_browserState);

  int insertionIndex = WebStateList::kInvalidIndex;
  int insertionFlags = WebStateList::INSERT_NO_FLAGS;
  if (index != TabModelConstants::kTabPositionAutomatically) {
    DCHECK_LE(index, static_cast<NSUInteger>(INT_MAX));
    insertionIndex = static_cast<int>(index);
    insertionFlags |= WebStateList::INSERT_FORCE_INDEX;
  } else if (!ui::PageTransitionCoreTypeIs(loadParams.transition_type,
                                           ui::PAGE_TRANSITION_LINK)) {
    insertionIndex = _webStateList->count();
    insertionFlags |= WebStateList::INSERT_FORCE_INDEX;
  }

  if (!inBackground) {
    insertionFlags |= WebStateList::INSERT_ACTIVATE;
  }

  web::WebState::CreateParams createParams(self.browserState);
  createParams.created_with_opener = openedByDOM;

  std::unique_ptr<web::WebState> webState = web::WebState::Create(createParams);
  webState->GetNavigationManager()->LoadURLWithParams(loadParams);

  insertionIndex = _webStateList->InsertWebState(
      insertionIndex, std::move(webState), insertionFlags,
      WebStateOpener(parentTab.webState));

  return LegacyTabHelper::GetTabForWebState(
      _webStateList->GetWebStateAt(insertionIndex));
}

- (void)moveTab:(Tab*)tab toIndex:(NSUInteger)toIndex {
  DCHECK_LE(toIndex, static_cast<NSUInteger>(INT_MAX));
  int fromIndex = _webStateList->GetIndexOfWebState(tab.webState);
  _webStateList->MoveWebStateAt(fromIndex, static_cast<int>(toIndex));
}

- (void)closeTabAtIndex:(NSUInteger)index {
  DCHECK_LE(index, static_cast<NSUInteger>(INT_MAX));
  _webStateList->CloseWebStateAt(static_cast<int>(index),
                                 WebStateList::CLOSE_USER_ACTION);
}

- (void)closeTab:(Tab*)tab {
  [self closeTabAtIndex:[self indexOfTab:tab]];
}

- (void)closeAllTabs {
  _webStateList->CloseAllWebStates(WebStateList::CLOSE_USER_ACTION);
  [_observers tabModelClosedAllTabs:self];
}

- (void)haltAllTabs {
  for (int index = 0; index < _webStateList->count(); ++index) {
    web::WebState* webState = _webStateList->GetWebStateAt(index);
    Tab* tab = LegacyTabHelper::GetTabForWebState(webState);
    [tab terminateNetworkActivity];
  }
}

- (void)notifyTabChanged:(Tab*)tab {
  [_observers tabModel:self didChangeTab:tab];
}

- (void)notifyTabLoading:(Tab*)tab {
  [_observers tabModel:self willStartLoadingTab:tab];
  [self notifyTabChanged:tab];
  [_observers tabModel:self didStartLoadingTab:tab];
}

- (void)notifyTabFinishedLoading:(Tab*)tab success:(BOOL)success {
  [self notifyTabChanged:tab];
  [_observers tabModel:self didFinishLoadingTab:tab success:success];
}

- (void)notifyNewTabWillOpen:(Tab*)tab inBackground:(BOOL)background {
  [_observers tabModel:self newTabWillOpen:tab inBackground:background];
}

- (void)notifyTabWasDeselected:(Tab*)tab {
  [_observers tabModel:self didDeselectTab:tab];
}

- (void)addObserver:(id<TabModelObserver>)observer {
  [_observers addObserver:observer];
}

- (void)removeObserver:(id<TabModelObserver>)observer {
  [_observers removeObserver:observer];
}

- (void)resetSessionMetrics {
  if (_webStateListMetricsObserver)
    _webStateListMetricsObserver->ResetSessionMetrics();
}

- (void)recordSessionMetrics {
  if (_webStateListMetricsObserver)
    _webStateListMetricsObserver->RecordSessionMetrics();
}

- (void)notifyTabSnapshotChanged:(Tab*)tab withImage:(UIImage*)image {
  DCHECK([NSThread isMainThread]);
  [_observers tabModel:self didChangeTabSnapshot:tab withImage:image];
}

- (void)setPrimary:(BOOL)primary {
  if (_tabUsageRecorder) {
    _tabUsageRecorder->RecordPrimaryTabModelChange(primary,
                                                   self.currentTab.webState);
  }
}

- (NSSet*)currentlyReferencedExternalFiles {
  NSMutableSet* referencedFiles = [NSMutableSet set];
  if (!_browserState)
    return referencedFiles;
  // Check the currently open tabs for external files.
  for (int index = 0; index < _webStateList->count(); ++index) {
    web::WebState* webState = _webStateList->GetWebStateAt(index);
    const GURL& lastCommittedURL = webState->GetLastCommittedURL();
    if (UrlIsExternalFileReference(lastCommittedURL)) {
      [referencedFiles addObject:base::SysUTF8ToNSString(
                                     lastCommittedURL.ExtractFileName())];
    }
    web::NavigationItem* pendingItem =
        webState->GetNavigationManager()->GetPendingItem();
    if (pendingItem && UrlIsExternalFileReference(pendingItem->GetURL())) {
      [referencedFiles addObject:base::SysUTF8ToNSString(
                                     pendingItem->GetURL().ExtractFileName())];
    }
  }
  // Do the same for the recently closed tabs.
  sessions::TabRestoreService* restoreService =
      IOSChromeTabRestoreServiceFactory::GetForBrowserState(_browserState);
  DCHECK(restoreService);
  for (const auto& entry : restoreService->entries()) {
    sessions::TabRestoreService::Tab* tab =
        static_cast<sessions::TabRestoreService::Tab*>(entry.get());
    int navigationIndex = tab->current_navigation_index;
    sessions::SerializedNavigationEntry navigation =
        tab->navigations[navigationIndex];
    GURL URL = navigation.virtual_url();
    if (UrlIsExternalFileReference(URL)) {
      NSString* fileName = base::SysUTF8ToNSString(URL.ExtractFileName());
      [referencedFiles addObject:fileName];
    }
  }
  return referencedFiles;
}

// NOTE: This can be called multiple times, so must be robust against that.
- (void)browserStateDestroyed {
  if (!_browserState)
    return;

  [[NSNotificationCenter defaultCenter] removeObserver:self];
  TabModelList::UnregisterTabModelFromChromeBrowserState(_browserState, self);
  _browserState = nullptr;

  // Clear weak pointer to observers before destroying them.
  _tabModelNotificationObserver = nullptr;
  _webStateListMetricsObserver = nullptr;

  // Close all tabs. Do this in an @autoreleasepool as WebStateList observers
  // will be notified (they are unregistered later). As some of them may be
  // implemented in Objective-C and unregister themselves in their -dealloc
  // method, ensure they -autorelease introduced by ARC are processed before
  // the WebStateList destructor is called.
  @autoreleasepool {
    _webStateList->CloseAllWebStates(WebStateList::CLOSE_NO_FLAGS);
  }

  // Unregister all observers after closing all the tabs as some of them are
  // required to properly clean up the Tabs.
  for (const auto& webStateListObserver : _webStateListObservers)
    _webStateList->RemoveObserver(webStateListObserver.get());
  _webStateListObservers.clear();
  _retainedWebStateListObservers = nil;

  _clearPoliciesTaskTracker.TryCancelAll();
  _tabUsageRecorder.reset();
  _webStateObserver.reset();
}

- (void)navigationCommittedInTab:(Tab*)tab
                    previousItem:(web::NavigationItem*)previousItem {
  if (self.offTheRecord)
    return;
  if (![tab navigationManager])
    return;

  // See if the navigation was within a page; if so ignore it.
  if (previousItem) {
    GURL previousURL = previousItem->GetURL();
    GURL currentURL = [tab navigationManager]->GetVisibleItem()->GetURL();

    url::Replacements<char> replacements;
    replacements.ClearRef();
    if (previousURL.ReplaceComponents(replacements) ==
        currentURL.ReplaceComponents(replacements)) {
      return;
    }
  }

  int tabCount = static_cast<int>(self.count);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Tabs.TabCountPerLoad", tabCount, 1, 200, 50);
}

#pragma mark - Private methods

- (SessionIOS*)sessionForSaving {
  // Build the array of sessions. Copy the session objects as the saving will
  // be done on a separate thread.
  // TODO(crbug.com/661986): This could get expensive especially since this
  // window may never be saved (if another call comes in before the delay).
  return [[SessionIOS alloc]
      initWithWindows:@[ SerializeWebStateList(_webStateList.get()) ]];
}

- (BOOL)isWebUsageEnabled {
  DCHECK(_browserState);
  return WebStateListWebUsageEnablerFactory::GetInstance()
      ->GetForBrowserState(_browserState)
      ->IsWebUsageEnabled();
}

- (BOOL)restoreSessionWindow:(SessionWindowIOS*)window
                persistState:(BOOL)persistState {
  DCHECK(_browserState);
  DCHECK(window);

  // Disable calling -notifyNewTabWillOpen: while restoring a session as it
  // breaks the BVC (see crbug.com/763964).
  base::ScopedClosureRunner enableTabModelNotificationObserver;
  if (_tabModelNotificationObserver) {
    _tabModelNotificationObserver->SetDisabled(true);
    enableTabModelNotificationObserver.ReplaceClosure(
        base::BindOnce(&TabModelNotificationObserver::SetDisabled,
                       base::Unretained(_tabModelNotificationObserver), false));
  }

  if (!window.sessions.count)
    return NO;

  int oldCount = _webStateList->count();
  DCHECK_GE(oldCount, 0);

  web::WebState::CreateParams createParams(_browserState);
  DeserializeWebStateList(
      _webStateList.get(), window,
      base::BindRepeating(&web::WebState::CreateWithStorageSession,
                          createParams));

  DCHECK_GT(_webStateList->count(), oldCount);
  int restoredCount = _webStateList->count() - oldCount;
  DCHECK_EQ(window.sessions.count, static_cast<NSUInteger>(restoredCount));

  scoped_refptr<web::CertificatePolicyCache> policyCache =
      web::BrowserState::GetCertificatePolicyCache(_browserState);

  std::vector<web::WebState*> restoredWebStates;
  if (_tabUsageRecorder)
    restoredWebStates.reserve(window.sessions.count);

  for (int index = oldCount; index < _webStateList->count(); ++index) {
    web::WebState* webState = _webStateList->GetWebStateAt(index);
    web::NavigationItem* visible_item =
        webState->GetNavigationManager()->GetVisibleItem();

    if (!(visible_item &&
          visible_item->GetVirtualURL() == kChromeUINewTabURL)) {
      PagePlaceholderTabHelper::FromWebState(webState)
          ->AddPlaceholderForNextNavigation();
    }

    if (visible_item && visible_item->GetVirtualURL().is_valid()) {
      favicon::WebFaviconDriver::FromWebState(webState)->FetchFavicon(
          visible_item->GetVirtualURL(), /*is_same_document=*/false);
    }

    // Restore the CertificatePolicyCache (note that webState is invalid after
    // passing it via move semantic to -initWithWebState:model:).
    UpdateCertificatePolicyCacheFromWebState(policyCache, webState);

    if (_tabUsageRecorder)
      restoredWebStates.push_back(webState);
  }

  // If there was only one tab and it was the new tab page, clobber it.
  BOOL closedNTPTab = NO;
  if (oldCount == 1) {
    Tab* tab = [self tabAtIndex:0];
    BOOL hasPendingLoad =
        tab.webState->GetNavigationManager()->GetPendingItem() != nullptr;
    if (!hasPendingLoad &&
        tab.webState->GetLastCommittedURL() == kChromeUINewTabURL) {
      [self closeTab:tab];
      closedNTPTab = YES;
      oldCount = 0;
    }
  }
  if (_tabUsageRecorder) {
    _tabUsageRecorder->InitialRestoredTabs(self.currentTab.webState,
                                           restoredWebStates);
  }

  return closedNTPTab;
}

#pragma mark - Notification Handlers

// Called when UIApplicationWillResignActiveNotification is received.
- (void)willResignActive:(NSNotification*)notify {
  if (self.webUsageEnabled && self.currentTab) {
    [SnapshotCacheFactory::GetForBrowserState(_browserState)
        willBeSavedGreyWhenBackgrounding:self.currentTab.tabId];
  }
}

// Called when UIApplicationDidEnterBackgroundNotification is received.
- (void)applicationDidEnterBackground:(NSNotification*)notify {
  if (!_browserState)
    return;

  // Evict all the certificate policies except for the current entries of the
  // active sessions.
  CleanCertificatePolicyCache(
      &_clearPoliciesTaskTracker,
      base::CreateSingleThreadTaskRunnerWithTraits({web::WebThread::IO}),
      web::BrowserState::GetCertificatePolicyCache(_browserState),
      _webStateList.get());

  // Normally, the session is saved after some timer expires but since the app
  // is about to enter the background send YES to save the session immediately.
  [self saveSessionImmediately:YES];

  // Write out a grey version of the current website to disk.
  if (self.webUsageEnabled && self.currentTab) {
    [SnapshotCacheFactory::GetForBrowserState(_browserState)
        saveGreyInBackgroundForSessionID:self.currentTab.tabId];
  }
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState
    didCommitNavigationWithDetails:
        (const web::LoadCommittedDetails&)load_details {
  Tab* tab = LegacyTabHelper::GetTabForWebState(webState);
  [self notifyTabLoading:tab];

  web::NavigationItem* previousItem = nullptr;
  if (load_details.previous_item_index >= 0) {
    DCHECK(webState->GetNavigationManager());
    previousItem = webState->GetNavigationManager()->GetItemAtIndex(
        load_details.previous_item_index);
  }

  [self navigationCommittedInTab:tab previousItem:previousItem];

  // Sending a notification about the url change for crash reporting.
  // TODO(crbug.com/661675): Consider using the navigation entry committed
  // notification now that it's in the right place.
  const std::string& lastCommittedURL = webState->GetLastCommittedURL().spec();
  if (!lastCommittedURL.empty()) {
    [[NSNotificationCenter defaultCenter]
        postNotificationName:kTabUrlStartedLoadingNotificationForCrashReporting
                      object:tab
                    userInfo:@{
                      kTabUrlKey : base::SysUTF8ToNSString(lastCommittedURL)
                    }];
  }
}

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation {
  Tab* tab = LegacyTabHelper::GetTabForWebState(webState);

  // In order to avoid false positive in the crash loop detection, disable the
  // counter as soon as an URL is loaded. This requires an user action and is a
  // significant source of crashes. Ignore NTP as it is loaded by default after
  // a crash.
  if (navigation->GetUrl().host_piece() != kChromeUINewTabHost) {
    static dispatch_once_t dispatch_once_token;
    dispatch_once(&dispatch_once_token, ^{
      crash_util::ResetFailedStartupAttemptCount();
    });
  }

  if (_tabUsageRecorder && ShouldRecordPageLoadStartForNavigation(navigation)) {
    _tabUsageRecorder->RecordPageLoadStart(webState);
  }

  [self notifyTabChanged:tab];
  [[NSNotificationCenter defaultCenter]
      postNotificationName:
          kTabClosingCurrentDocumentNotificationForCrashReporting
                    object:tab];

  DCHECK(webState->GetNavigationManager());
  web::NavigationItem* navigationItem =
      webState->GetNavigationManager()->GetPendingItem();

  // TODO(crbug.com/676129): the pending item is not correctly set when the
  // page is reloading, use the last committed item if pending item is null.
  // Remove this once tracking bug is fixed.
  if (!navigationItem) {
    navigationItem = webState->GetNavigationManager()->GetLastCommittedItem();
  }

  [[OmniboxGeolocationController sharedInstance]
      addLocationToNavigationItem:navigationItem
                     browserState:ios::ChromeBrowserState::FromBrowserState(
                                      webState->GetBrowserState())];
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  Tab* tab = LegacyTabHelper::GetTabForWebState(webState);
  [self notifyTabChanged:tab];
}

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  DCHECK(!webState->IsLoading());
  Tab* tab = LegacyTabHelper::GetTabForWebState(webState);
  [self notifyTabFinishedLoading:tab success:success];

  RecordInterfaceOrientationMetric();
  RecordMainFrameNavigationMetric(webState);

  [[OmniboxGeolocationController sharedInstance] finishPageLoadForTab:tab
                                                          loadSuccess:success];
}

- (void)webState:(web::WebState*)webState
    didChangeLoadingProgress:(double)progress {
  // TODO(crbug.com/546406): It is probably possible to do something smarter,
  // but the fact that this is not always sent will have to be taken into
  // account.
  Tab* tab = LegacyTabHelper::GetTabForWebState(webState);
  [self notifyTabChanged:tab];
}

- (void)webStateDidChangeTitle:(web::WebState*)webState {
  Tab* tab = LegacyTabHelper::GetTabForWebState(webState);
  [self notifyTabChanged:tab];
}

- (void)webStateDidChangeVisibleSecurityState:(web::WebState*)webState {
  Tab* tab = LegacyTabHelper::GetTabForWebState(webState);
  [self notifyTabChanged:tab];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  // The TabModel is removed from WebState's observer when the WebState is
  // detached from WebStateList which happens before WebState destructor,
  // so this method should never be called.
  NOTREACHED();
}

- (void)webStateDidStopLoading:(web::WebState*)webState {
  Tab* tab = LegacyTabHelper::GetTabForWebState(webState);
  [self notifyTabChanged:tab];
}

#pragma mark - WebStateListObserving

- (void)webStateList:(WebStateList*)webStateList
    didInsertWebState:(web::WebState*)webState
              atIndex:(int)index
           activating:(BOOL)activating {
  DCHECK(webState);
  webState->AddObserver(_webStateObserver.get());
}

- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
          withWebState:(web::WebState*)newWebState
               atIndex:(int)atIndex {
  DCHECK(oldWebState);
  DCHECK(newWebState);
  newWebState->AddObserver(_webStateObserver.get());
  oldWebState->RemoveObserver(_webStateObserver.get());
}

- (void)webStateList:(WebStateList*)webStateList
    didDetachWebState:(web::WebState*)webState
              atIndex:(int)atIndex {
  DCHECK(webState);
  webState->RemoveObserver(_webStateObserver.get());
}

@end
