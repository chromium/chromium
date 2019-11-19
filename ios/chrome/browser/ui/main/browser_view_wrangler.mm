// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/browser_view_wrangler.h"

#include "base/files/file_path.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/crash_report/crash_report_helper.h"
#import "ios/chrome/browser/device_sharing/device_sharing_manager.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/sessions/session_ios.h"
#import "ios/chrome/browser/sessions/session_service_ios.h"
#import "ios/chrome/browser/sessions/session_window_ios.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/browser_view/browser_coordinator.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller_dependency_factory.h"
#import "ios/chrome/browser/url_loading/app_url_loading_service.h"
#import "ios/chrome/browser/web_state_list/active_web_state_observation_forwarder.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Internal implementation of BrowserInterface -- for the most part a wrapper
// around BrowserCoordinator.
@interface WrangledBrowser : NSObject <BrowserInterface>

@property(nonatomic, weak, readonly) BrowserCoordinator* coordinator;

- (instancetype)initWithCoordinator:(BrowserCoordinator*)coordinator;

@end

@implementation WrangledBrowser

- (instancetype)initWithCoordinator:(BrowserCoordinator*)coordinator {
  if (self = [super init]) {
    _coordinator = coordinator;
  }
  return self;
}

- (UIViewController*)viewController {
  return self.coordinator.viewController;
}

- (BrowserViewController*)bvc {
  return self.coordinator.viewController;
}

- (TabModel*)tabModel {
  return self.coordinator.tabModel;
}

- (Browser*)browser {
  return self.coordinator.browser;
}

- (ios::ChromeBrowserState*)browserState {
  return self.coordinator.viewController.browserState;
}

- (BOOL)userInteractionEnabled {
  return self.coordinator.active;
}

- (void)setUserInteractionEnabled:(BOOL)userInteractionEnabled {
  self.coordinator.active = userInteractionEnabled;
}

- (BOOL)incognito {
  return self.browserState->IsOffTheRecord();
}

- (void)clearPresentedStateWithCompletion:(ProceduralBlock)completion
                           dismissOmnibox:(BOOL)dismissOmnibox {
  [self.coordinator clearPresentedStateWithCompletion:completion
                                       dismissOmnibox:dismissOmnibox];
}

@end

@interface BrowserViewWrangler () <WebStateListObserving, CRWWebStateObserver> {
  ios::ChromeBrowserState* _browserState;
  __weak id<ApplicationCommands> _applicationCommandEndpoint;
  __weak id<BrowserStateStorageSwitching> _storageSwitcher;
  AppUrlLoadingService* _appURLLoadingService;
  BOOL _isShutdown;

  std::unique_ptr<Browser> _mainBrowser;
  std::unique_ptr<Browser> _otrBrowser;
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  std::unique_ptr<WebStateListObserverBridge> _webStateListForwardingObserver;
  // Bridge to observe WebState from Objective-C. This observer will be used to
  // only monitor active webStates.
  std::unique_ptr<web::WebStateObserverBridge> _activeWebStateObserver;
  // Forwards observer methods for The active WebState in each WebStateList
  // monitored by the BrowserViewWrangler.
  std::map<WebStateList*, std::unique_ptr<ActiveWebStateObservationForwarder>>
      _activeWebStateObservationForwarders;
}

@property(nonatomic, strong, readwrite) WrangledBrowser* mainInterface;
@property(nonatomic, strong, readwrite) WrangledBrowser* incognitoInterface;

// Backing objects.
@property(nonatomic) BrowserCoordinator* mainBrowserCoordinator;
@property(nonatomic) BrowserCoordinator* incognitoBrowserCoordinator;
//@property(nonatomic, readonly) TabModel* mainTabModel;
//@property(nonatomic, readonly) TabModel* otrTabModel;
@property(nonatomic, readonly) Browser* mainBrowser;
@property(nonatomic, readonly) Browser* otrBrowser;

// Responsible for maintaining all state related to sharing to other devices.
// Redeclared readwrite from the readonly declaration in the Testing interface.
@property(nonatomic, strong, readwrite)
    DeviceSharingManager* deviceSharingManager;

// Sets up the given |tabModel| for use.  If |restorePersistedState| is YES,
// then any existing tabs that have been saved for |browserState| will be
// loaded; otherwise, the tab model will be left empty.
- (void)setUpTabModel:(TabModel*)tabModel
         withBrowserState:(ios::ChromeBrowserState*)browserState
    restorePersistedState:(BOOL)restorePersistedState;

// Setters for the main and otr Browsers.
- (void)setMainBrowser:(std::unique_ptr<Browser>)browser;
- (void)setOtrBrowser:(std::unique_ptr<Browser>)browser;

// Creates a new off-the-record ("incognito") browser state for |_browserState|,
// then creates and sets up a TabModel and returns a Browser for the result.
- (std::unique_ptr<Browser>)buildOtrBrowser:(BOOL)restorePersistedState;

// Creates the correct BrowserCoordinator for the corresponding browser state
// and Browser.
- (BrowserCoordinator*)coordinatorForBrowser:(Browser*)browser;
@end

@implementation BrowserViewWrangler

@synthesize currentInterface = _currentInterface;

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                webStateListObserver:(id<WebStateListObserving>)observer
          applicationCommandEndpoint:
              (id<ApplicationCommands>)applicationCommandEndpoint
                appURLLoadingService:(AppUrlLoadingService*)appURLLoadingService
                     storageSwitcher:
                         (id<BrowserStateStorageSwitching>)storageSwitcher {
  if ((self = [super init])) {
    _browserState = browserState;
    _applicationCommandEndpoint = applicationCommandEndpoint;
    _appURLLoadingService = appURLLoadingService;
    _storageSwitcher = storageSwitcher;
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _webStateListForwardingObserver =
        std::make_unique<WebStateListObserverBridge>(observer);
    _activeWebStateObserver =
        std::make_unique<web::WebStateObserverBridge>(self);
  }
  return self;
}

- (void)dealloc {
  DCHECK(_isShutdown) << "-shutdown must be called before -dealloc";
}

- (void)createMainBrowser {
  _mainBrowser = Browser::Create(_browserState);
  [self setUpTabModel:_mainBrowser->GetTabModel()
           withBrowserState:_browserState
      restorePersistedState:YES];

  // Follow loaded URLs in the main tab model to send those in case of
  // crashes.
  breakpad::MonitorURLsForWebStateList(self.mainBrowser->GetWebStateList());
  ios::GetChromeBrowserProvider()->InitializeCastService(
      self.mainBrowser->GetTabModel());

  // Create the main coordinator, and thus the main interface.
  _mainBrowserCoordinator = [self coordinatorForBrowser:self.mainBrowser];
  [_mainBrowserCoordinator start];
  DCHECK(_mainBrowserCoordinator.viewController);
  _mainInterface =
      [[WrangledBrowser alloc] initWithCoordinator:_mainBrowserCoordinator];
}

#pragma mark - BrowserViewInformation property implementations

- (void)setCurrentInterface:(WrangledBrowser*)interface {
  DCHECK(interface);
  // |interface| must be one of the interfaces this class already owns.
  DCHECK(self.mainInterface == interface ||
         self.incognitoInterface == interface);
  if (self.currentInterface == interface) {
    return;
  }

  if (self.currentInterface) {
    // Tell the current BVC it moved to the background.
    [self.currentInterface.bvc setPrimary:NO];

    // Data storage for the browser is always owned by the current BVC, so it
    // must be updated when switching between BVCs.
    [_storageSwitcher
        changeStorageFromBrowserState:self.currentInterface.browserState
                       toBrowserState:interface.browserState];
  }

  _currentInterface = interface;

  // The internal state of the Handoff Manager depends on the current BVC.
  [self updateDeviceSharingManager];
}

- (id<BrowserInterface>)incognitoInterface {
  if (!_mainInterface)
    return nil;
  if (!_incognitoInterface) {
    // The backing coordinator should not have been created yet.
    DCHECK(!_incognitoBrowserCoordinator);
    ios::ChromeBrowserState* otrBrowserState =
        _browserState->GetOffTheRecordChromeBrowserState();
    DCHECK(otrBrowserState);
    _incognitoBrowserCoordinator = [self coordinatorForBrowser:self.otrBrowser];
    [_incognitoBrowserCoordinator start];
    DCHECK(_incognitoBrowserCoordinator.viewController);
    _incognitoInterface = [[WrangledBrowser alloc]
        initWithCoordinator:_incognitoBrowserCoordinator];
  }
  return _incognitoInterface;
}

- (Browser*)mainBrowser {
  DCHECK(_mainBrowser.get())
      << "-createMainBrowser must be called before -mainBrowser is accessed.";
  return _mainBrowser.get();
}

- (Browser*)otrBrowser {
  if (!_otrBrowser) {
    _otrBrowser = [self buildOtrBrowser:YES];
  }
  return _otrBrowser.get();
}

- (void)setMainBrowser:(std::unique_ptr<Browser>)mainBrowser {
  if (_mainBrowser.get()) {
    TabModel* tabModel = self.mainBrowser->GetTabModel();
    WebStateList* webStateList = self.mainBrowser->GetWebStateList();
    breakpad::StopMonitoringTabStateForWebStateList(webStateList);
    breakpad::StopMonitoringURLsForWebStateList(webStateList);
    [tabModel disconnect];
    _activeWebStateObservationForwarders[webStateList] = nullptr;
    webStateList->RemoveObserver(_webStateListObserver.get());
    webStateList->RemoveObserver(_webStateListForwardingObserver.get());
  }

  _mainBrowser = std::move(mainBrowser);
}

- (void)setOtrBrowser:(std::unique_ptr<Browser>)otrBrowser {
  if (_otrBrowser.get()) {
    TabModel* tabModel = self.otrBrowser->GetTabModel();
    WebStateList* webStateList = self.otrBrowser->GetWebStateList();
    breakpad::StopMonitoringTabStateForWebStateList(webStateList);
    [tabModel disconnect];
    _activeWebStateObservationForwarders[webStateList] = nullptr;
    webStateList->RemoveObserver(_webStateListObserver.get());
    webStateList->RemoveObserver(_webStateListForwardingObserver.get());
  }

  _otrBrowser = std::move(otrBrowser);
}

#pragma mark - BrowserViewInformation methods

- (void)cleanDeviceSharingManager {
  [self.deviceSharingManager updateBrowserState:NULL];
}

#pragma mark - WebStateListObserving

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(int)reason {
  if (_isShutdown)
    return;

  [self updateDeviceSharingManager];
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  // Active WebState has update the active URL. Update the DeviceSharingManager
  // state.
  [self updateDeviceSharingManager];
}

#pragma mark - Other public methods

- (void)updateDeviceSharingManager {
  if (!self.deviceSharingManager) {
    self.deviceSharingManager = [[DeviceSharingManager alloc] init];
  }
  [self.deviceSharingManager updateBrowserState:_browserState];

  GURL activeURL;
  web::WebState* activeWebState =
      self.currentInterface.tabModel.webStateList->GetActiveWebState();
  // Set the active URL if there's an active webstate and the current BVC is not
  // OTR.
  if (activeWebState && !self.currentInterface.incognito) {
    activeURL = activeWebState->GetVisibleURL();
  }
  [self.deviceSharingManager updateActiveURL:activeURL];
}

- (void)destroyAndRebuildIncognitoBrowser {
  // It is theoretically possible that a Tab has been added to |_otrTabModel|
  // since the deletion has been scheduled. It is unlikely to happen for real
  // because it would require superhuman speed.
  DCHECK(![self.otrBrowser->GetTabModel() count]);
  DCHECK(_browserState);

  // Stop watching the OTR webStateList's state for crashes.
  breakpad::StopMonitoringTabStateForWebStateList(
      self.otrBrowser->GetWebStateList());

  // At this stage, a new incognitoBrowserCoordinator shouldn't be lazily
  // constructed by calling the property getter.
  BOOL otrBVCIsCurrent = self.currentInterface == self.incognitoInterface;
  @autoreleasepool {
    // At this stage, a new incognitoBrowserCoordinator shouldn't be lazily
    // constructed by calling the property getter.
    [_incognitoBrowserCoordinator stop];
    _incognitoBrowserCoordinator = nil;
    _incognitoInterface = nil;

    // There's no guarantee the tab model was ever added to the BVC (or even
    // that the BVC was created), so ensure the tab model gets notified.
    [self setOtrBrowser:nullptr];
    if (otrBVCIsCurrent) {
      _currentInterface = nil;
    }
  }

  _browserState->DestroyOffTheRecordChromeBrowserState();

  // An empty _otrTabModel must be created at this point, because it is then
  // possible to prevent the tabChanged notification being sent. Otherwise,
  // when it is created, a notification with no tabs will be sent, and it will
  // be immediately deleted.
  [self setOtrBrowser:[self buildOtrBrowser:NO]];
  DCHECK(![self.otrBrowser->GetTabModel() count]);
  DCHECK(_browserState->HasOffTheRecordChromeBrowserState());

  if (otrBVCIsCurrent) {
    self.currentInterface = self.incognitoInterface;
  }
}

- (void)shutdown {
  DCHECK(!_isShutdown);
  _isShutdown = YES;

  // Disconnect the DeviceSharingManager.
  [self cleanDeviceSharingManager];

  // At this stage, new BrowserCoordinators shouldn't be lazily constructed by
  // calling their property getters.
  [_mainBrowserCoordinator stop];
  _mainBrowserCoordinator = nil;
  [_incognitoBrowserCoordinator stop];
  _incognitoBrowserCoordinator = nil;

  // Handles removing observers, stopping breakpad monitoring, and closing all
  // tabs.
  [self setMainBrowser:nullptr];
  [self setOtrBrowser:nullptr];

  _browserState = nullptr;
}

#pragma mark - Internal methods

- (std::unique_ptr<Browser>)buildOtrBrowser:(BOOL)restorePersistedState {
  DCHECK(_browserState);
  // Ensure that the OTR ChromeBrowserState is created.
  ios::ChromeBrowserState* otrBrowserState =
      _browserState->GetOffTheRecordChromeBrowserState();
  DCHECK(otrBrowserState);

  std::unique_ptr<Browser> browser = Browser::Create(otrBrowserState);
  [self setUpTabModel:browser->GetTabModel()
           withBrowserState:otrBrowserState
      restorePersistedState:restorePersistedState];
  return browser;
}

- (void)setUpTabModel:(TabModel*)tabModel
         withBrowserState:(ios::ChromeBrowserState*)browserState
    restorePersistedState:(BOOL)restorePersistedState {
  DCHECK_EQ(0U, tabModel.count);
  SessionWindowIOS* sessionWindow = nil;
  if (restorePersistedState) {
    // Load existing saved tab model state.
    NSString* statePath =
        base::SysUTF8ToNSString(browserState->GetStatePath().AsUTF8Unsafe());
    SessionIOS* session =
        [[SessionServiceIOS sharedService] loadSessionFromDirectory:statePath];
    if (session) {
      DCHECK_EQ(session.sessionWindows.count, 1u);
      sessionWindow = session.sessionWindows[0];
    }

    [tabModel restoreSessionWindow:sessionWindow forInitialRestore:YES];
  }

  // Add observers.
  _activeWebStateObservationForwarders[tabModel.webStateList] =
      std::make_unique<ActiveWebStateObservationForwarder>(
          tabModel.webStateList, _activeWebStateObserver.get());
  tabModel.webStateList->AddObserver(_webStateListObserver.get());
  tabModel.webStateList->AddObserver(_webStateListForwardingObserver.get());

  breakpad::MonitorTabStateForWebStateList(tabModel.webStateList);
}

- (BrowserCoordinator*)coordinatorForBrowser:(Browser*)browser {
  BrowserCoordinator* coordinator =
      [[BrowserCoordinator alloc] initWithBaseViewController:nil
                                                     browser:browser];
  coordinator.applicationCommandHandler = _applicationCommandEndpoint;
  coordinator.appURLLoadingService = _appURLLoadingService;
  return coordinator;
}

@end
