// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_view/tab_lifecycle_mediator.h"

#import "components/signin/ios/browser/account_consistency_service.h"
#import "ios/chrome/browser/autofill/autofill_tab_helper.h"
#import "ios/chrome/browser/download/download_manager_tab_helper.h"
#import "ios/chrome/browser/download/pass_kit_tab_helper.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/overscroll_actions/overscroll_actions_tab_helper.h"
#import "ios/chrome/browser/passwords/password_tab_helper.h"
#import "ios/chrome/browser/prerender/prerender_service.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#import "ios/chrome/browser/ssl/captive_portal_detector_tab_helper.h"
#import "ios/chrome/browser/store_kit/store_kit_tab_helper.h"
#import "ios/chrome/browser/ui/download/download_manager_coordinator.h"
#import "ios/chrome/browser/ui/print/print_controller.h"
#import "ios/chrome/browser/ui/sad_tab/sad_tab_coordinator.h"
#import "ios/chrome/browser/ui/side_swipe/side_swipe_controller.h"
#import "ios/chrome/browser/web/print/print_tab_helper.h"
#import "ios/chrome/browser/web/repost_form_tab_helper.h"
#import "ios/chrome/browser/web/sad_tab_tab_helper.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/webui/net_export_tab_helper.h"
#import "ios/web/public/deprecated/crw_web_controller_util.h"
#import "ui/base/device_form_factor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TabLifecycleMediator () <WebStateListObserving>
@end

@implementation TabLifecycleMediator {
  // Bridge to observe the web state list from Objective-C.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;

  // The web state list this medfiator was initialized with.
  WebStateList* _webStateList;

  // Delegate object for many tab helpers.
  __weak id<CommonTabHelperDelegate> _delegate;

  // Other tab helper dependencies.
  PrerenderService* _prerenderService;
  __weak SideSwipeController* _sideSwipeController;
  __weak SadTabCoordinator* _sadTabCoordinator;
  __weak DownloadManagerCoordinator* _downloadManagerCoordinator;
  __weak CommandDispatcher* _commandDispatcher;
  __weak UIViewController* _baseViewController;
  AccountConsistencyService* _accountConsistencyService;
  __weak id<PassKitTabHelperDelegate> _passKitDelegate;
  __weak PrintController* _printController;
  __weak id<RepostFormTabHelperDelegate> _repostFormDelegate;
  __weak id<StoreKitLauncher> _storeKitLauncher;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                            delegate:(id<CommonTabHelperDelegate>)delegate
                        dependencies:(TabLifecycleDependencies)dependencies {
  if (self = [super init]) {
    _prerenderService = dependencies.prerenderService;
    _sideSwipeController = dependencies.sideSwipeController;
    _sadTabCoordinator = dependencies.sadTabCoordinator;
    _downloadManagerCoordinator = dependencies.downloadManagerCoordinator;
    _commandDispatcher = dependencies.commandDispatcher;
    _baseViewController = dependencies.baseViewController;
    _accountConsistencyService = dependencies.accountConsistencyService;
    _passKitDelegate = dependencies.passKitDelegate;
    _printController = dependencies.printController;
    _repostFormDelegate = dependencies.repostFormDelegate;
    _storeKitLauncher = dependencies.storeKitLauncher;
    // Some dependencies can't be null.
    DCHECK(_downloadManagerCoordinator);

    _webStateList = webStateList;
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _webStateList->AddObserver(_webStateListObserver.get());
    _delegate = delegate;
    for (int index = 0; index < _webStateList->count(); ++index) {
      [self installDelegatesForWebState:_webStateList->GetWebStateAt(index)];
    }
  }
  return self;
}

- (void)disconnect {
  // Stop observing the web state list, then uninstall all delegates.
  _webStateList->RemoveObserver(_webStateListObserver.get());
  for (int index = 0; index < _webStateList->count(); ++index)
    [self uninstallDelegatesForWebState:_webStateList->GetWebStateAt(index)];
}

#pragma mark - WebStateListObserving

- (void)webStateList:(WebStateList*)webStateList
    didDetachWebState:(web::WebState*)webState
              atIndex:(int)atIndex {
  [self uninstallDelegatesForWebState:webState];
}

- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
          withWebState:(web::WebState*)newWebState
               atIndex:(int)atIndex {
  [self uninstallDelegatesForWebState:oldWebState];
  [self installDelegatesForWebState:newWebState];
}

- (void)webStateList:(WebStateList*)webStateList
    didInsertWebState:(web::WebState*)webState
              atIndex:(int)index
           activating:(BOOL)activating {
  [self installDelegatesForWebState:webState];
}

#pragma mark - Private

// Installs the tab helper (and other) delegates.
- (void)installDelegatesForWebState:(web::WebState*)webState {
  // If there is a prerender service, this webstate shouldn't be a prerendered
  // one. (There's no prerender service in incognito, for example).
  DCHECK(!_prerenderService ||
         !_prerenderService->IsWebStatePrerendered(webState));

  // Set up delegates for things that aren't tab helpers.
  if (_accountConsistencyService) {
    _accountConsistencyService->SetWebStateHandler(webState, _delegate);
  }
  web_deprecated::SetSwipeRecognizerProvider(webState, _sideSwipeController);

  // Tab helpers that aren't created until delegates are available.
  CaptivePortalDetectorTabHelper::CreateForWebState(webState, _delegate);
  DownloadManagerTabHelper::CreateForWebState(webState,
                                              _downloadManagerCoordinator);
  NetExportTabHelper::CreateForWebState(webState, _delegate);
  PassKitTabHelper::CreateForWebState(webState, _passKitDelegate);
  RepostFormTabHelper::CreateForWebState(webState, _repostFormDelegate);

  // Existing tab helpers which get delegates assigned.
  AutofillTabHelper::FromWebState(webState)->SetBaseViewController(
      _baseViewController);
  NewTabPageTabHelper::FromWebState(webState)->SetDelegate(_delegate);
  PrintTabHelper::FromWebState(webState)->set_printer(_printController);
  SadTabTabHelper::FromWebState(webState)->SetDelegate(_sadTabCoordinator);
  SnapshotTabHelper::FromWebState(webState)->SetDelegate(_delegate);

  StoreKitTabHelper::FromWebState(webState)->SetLauncher(_storeKitLauncher);

  PasswordTabHelper* passwordTabHelper =
      PasswordTabHelper::FromWebState(webState);
  passwordTabHelper->SetBaseViewController(_baseViewController);
  passwordTabHelper->SetPasswordControllerDelegate(_delegate);
  passwordTabHelper->SetDispatcher(_commandDispatcher);

  // Only assign the overscroll actions helper delegate on phones.
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET) {
    OverscrollActionsTabHelper::FromWebState(webState)->SetDelegate(_delegate);
  }
}

// Uninstalls delegates where needed.
- (void)uninstallDelegatesForWebState:(web::WebState*)webState {
  // Remove delegates for things that aren't tab helpers.
  if (_accountConsistencyService) {
    _accountConsistencyService->RemoveWebStateHandler(webState);
  }
  web_deprecated::SetSwipeRecognizerProvider(webState, nil);

  // Remove delegates for tab helpers which may otherwise do bad things during
  // shutdown.
  SnapshotTabHelper::FromWebState(webState)->SetDelegate(nil);
  NewTabPageTabHelper::FromWebState(webState)->SetDelegate(nil);
  PrintTabHelper::FromWebState(webState)->set_printer(nil);
  StoreKitTabHelper::FromWebState(webState)->SetLauncher(nil);

  PasswordTabHelper* passwordTabHelper =
      PasswordTabHelper::FromWebState(webState);
  passwordTabHelper->SetBaseViewController(nil);
  passwordTabHelper->SetPasswordControllerDelegate(nil);
  passwordTabHelper->SetDispatcher(nil);

  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET) {
    OverscrollActionsTabHelper::FromWebState(webState)->SetDelegate(nil);
  }
}

@end
