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
#import "ios/chrome/browser/sessions/session_ios.h"
#import "ios/chrome/browser/sessions/session_service_ios.h"
#import "ios/chrome/browser/sessions/session_window_ios.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_model_observer.h"
#import "ios/chrome/browser/ui/browser_view_controller.h"
#import "ios/chrome/browser/ui/browser_view_controller_dependency_factory.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BrowserViewWrangler ()<TabModelObserver> {
  ios::ChromeBrowserState* _browserState;
  __weak id<TabModelObserver> _tabModelObserver;
  __weak id<ApplicationCommands> _applicationCommandEndpoint;
  BOOL _isShutdown;
}

// Responsible for maintaining all state related to sharing to other devices.
// Redeclared readwrite from the readonly declaration in the Testing interface.
@property(nonatomic, strong, readwrite)
    DeviceSharingManager* deviceSharingManager;

// Creates a new autoreleased tab model for |browserState|; if |empty| is NO,
// then any existing tabs that have been saved for |browserState| will be
// loaded; otherwise, the tab model will be created empty.
- (TabModel*)tabModelForBrowserState:(ios::ChromeBrowserState*)browserState
                               empty:(BOOL)empty;

// Creates a new off-the-record ("incognito") browser state for |_browserState|,
// then calls -tabModelForBrowserState:empty: and returns the (autoreleased)
// result.
- (TabModel*)buildOtrTabModel:(BOOL)empty;

// Creates the correct BrowserViewController for the corresponding browser state
// and tab model.
- (BrowserViewController*)bvcForBrowserState:
                              (ios::ChromeBrowserState*)browserState
                                    tabModel:(TabModel*)tabModel;
@end

@implementation BrowserViewWrangler

// Properties defined in the BrowserViewInformation protocol.
@synthesize mainBVC = _mainBVC;
@synthesize mainTabModel = _mainTabModel;
@synthesize otrBVC = _otrBVC;
@synthesize otrTabModel = _otrTabModel;
@synthesize currentBVC = _currentBVC;
// Private properies.
@synthesize deviceSharingManager = _deviceSharingManager;

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                    tabModelObserver:(id<TabModelObserver>)tabModelObserver
          applicationCommandEndpoint:
              (id<ApplicationCommands>)applicationCommandEndpoint {
  if ((self = [super init])) {
    _browserState = browserState;
    _tabModelObserver = tabModelObserver;
    _applicationCommandEndpoint = applicationCommandEndpoint;
  }
  return self;
}

- (void)dealloc {
  DCHECK(_isShutdown) << "-shutdown must be called before -dealloc";
}

#pragma mark - BrowserViewInformation property implementations

- (BrowserViewController*)mainBVC {
  if (!_mainBVC) {
    // |_browserState| should always be set before trying to create
    // |_mainBVC|.
    DCHECK(_browserState);
    self.mainBVC =
        [self bvcForBrowserState:_browserState tabModel:self.mainTabModel];
    DCHECK(_mainBVC);
  }
  return _mainBVC;
}

- (void)setMainBVC:(BrowserViewController*)mainBVC {
  if (_mainBVC == mainBVC)
    return;

  if (_mainBVC) {
    [_mainBVC browserStateDestroyed];
    [_mainBVC shutdown];
  }

  _mainBVC = mainBVC;
}

- (TabModel*)mainTabModel {
  if (!_mainTabModel) {
    self.mainTabModel = [self tabModelForBrowserState:_browserState empty:NO];
    // Follow loaded URLs in the main tab model to send those in case of
    // crashes.
    breakpad::MonitorURLsForTabModel(_mainTabModel);
    ios::GetChromeBrowserProvider()->InitializeCastService(_mainTabModel);
  }
  return _mainTabModel;
}

- (void)setMainTabModel:(TabModel*)mainTabModel {
  if (_mainTabModel == mainTabModel)
    return;

  if (_mainTabModel) {
    breakpad::StopMonitoringTabStateForTabModel(_mainTabModel);
    breakpad::StopMonitoringURLsForTabModel(_mainTabModel);
    [_mainTabModel browserStateDestroyed];
    if (_tabModelObserver) {
      [_mainTabModel removeObserver:_tabModelObserver];
    }
    [_mainTabModel removeObserver:self];
  }

  _mainTabModel = mainTabModel;
}

- (BrowserViewController*)otrBVC {
  if (!_otrBVC) {
    // |_browserState| should always be set before trying to create
    // |_otrBVC|.
    DCHECK(_browserState);
    ios::ChromeBrowserState* otrBrowserState =
        _browserState->GetOffTheRecordChromeBrowserState();
    DCHECK(otrBrowserState);
    self.otrBVC =
        [self bvcForBrowserState:otrBrowserState tabModel:self.otrTabModel];
    DCHECK(_otrBVC);
  }
  return _otrBVC;
}

- (void)setOtrBVC:(BrowserViewController*)otrBVC {
  if (_otrBVC == otrBVC)
    return;

  if (_otrBVC) {
    [_otrBVC browserStateDestroyed];
    [_otrBVC shutdown];
  }

  _otrBVC = otrBVC;
}

- (TabModel*)otrTabModel {
  if (!_otrTabModel) {
    self.otrTabModel = [self buildOtrTabModel:NO];
  }
  return _otrTabModel;
}

- (void)setOtrTabModel:(TabModel*)otrTabModel {
  if (_otrTabModel == otrTabModel)
    return;

  if (_otrTabModel) {
    breakpad::StopMonitoringTabStateForTabModel(_otrTabModel);
    [_otrTabModel browserStateDestroyed];
    if (_tabModelObserver) {
      [_otrTabModel removeObserver:_tabModelObserver];
    }
    [_otrTabModel removeObserver:self];
  }

  _otrTabModel = otrTabModel;
}

- (void)setCurrentBVC:(BrowserViewController*)bvc
      storageSwitcher:(id<BrowserStateStorageSwitching>)storageSwitcher {
  DCHECK(bvc != nil);
  // |bvc| should be one of the BrowserViewControllers this class already owns.
  DCHECK(_mainBVC == bvc || _otrBVC == bvc);
  if (self.currentBVC == bvc) {
    return;
  }

  if (self.currentBVC) {
    // Tell the current BVC it moved to the background.
    [self.currentBVC setPrimary:NO];

    // Data storage for the browser is always owned by the current BVC, so it
    // must be updated when switching between BVCs.
    [storageSwitcher changeStorageFromBrowserState:self.currentBVC.browserState
                                    toBrowserState:bvc.browserState];
  }

  self.currentBVC = bvc;

  // The internal state of the Handoff Manager depends on the current BVC.
  [self updateDeviceSharingManager];
}

#pragma mark - BrowserViewInformation methods

- (TabModel*)currentTabModel {
  return self.currentBVC.tabModel;
}

- (ios::ChromeBrowserState*)currentBrowserState {
  return self.currentBVC.browserState;
}

- (void)haltAllTabs {
  [self.mainTabModel haltAllTabs];
  [self.otrTabModel haltAllTabs];
}

- (void)cleanDeviceSharingManager {
  [self.deviceSharingManager updateBrowserState:NULL];
}

#pragma mark - TabModelObserver

- (void)tabModel:(TabModel*)model
    didChangeActiveTab:(Tab*)newTab
           previousTab:(Tab*)previousTab
               atIndex:(NSUInteger)index {
  [self updateDeviceSharingManager];
}

- (void)tabModel:(TabModel*)model didChangeTab:(Tab*)tab {
  [self updateDeviceSharingManager];
}

#pragma mark - Other public methods

- (void)updateDeviceSharingManager {
  if (!self.deviceSharingManager) {
    self.deviceSharingManager = [[DeviceSharingManager alloc] init];
  }
  [self.deviceSharingManager updateBrowserState:_browserState];

  GURL activeURL;
  Tab* currentTab = [self.currentBVC tabModel].currentTab;
  // Set the active URL if there's a current tab and the current BVC is not OTR.
  if (currentTab.webState && self.currentBVC != self.otrBVC) {
    activeURL = currentTab.webState->GetVisibleURL();
  }
  [self.deviceSharingManager updateActiveURL:activeURL];
}

- (void)destroyAndRebuildIncognitoTabModel {
  // It is theoretically possible that a Tab has been added to |_otrTabModel|
  // since the deletion has been scheduled. It is unlikely to happen for real
  // because it would require superhuman speed.
  DCHECK(![_otrTabModel count]);
  DCHECK(_browserState);

  // Stop watching the OTR tab model's state for crashes.
  breakpad::StopMonitoringTabStateForTabModel(self.otrTabModel);

  // At this stage, a new OTR BVC shouldn't be lazily constructed by calling the
  // .otrBVC property getter. Instead, the ivar is accessed directly through the
  // following code.
  BOOL otrBVCIsCurrent = self.currentBVC == _otrBVC;
  @autoreleasepool {
    self.otrBVC = nil;
    // There's no guarantee the tab model was ever added to the BVC (or even
    // that the BVC was created), so ensure the tab model gets notified.
    self.otrTabModel = nil;
    if (otrBVCIsCurrent) {
      _currentBVC = nil;
    }
  }

  _browserState->DestroyOffTheRecordChromeBrowserState();

  // An empty _otrTabModel must be created at this point, because it is then
  // possible to prevent the tabChanged notification being sent. Otherwise,
  // when it is created, a notification with no tabs will be sent, and it will
  // be immediately deleted.
  self.otrTabModel = [self buildOtrTabModel:YES];
  DCHECK(![self.otrTabModel count]);
  DCHECK(_browserState->HasOffTheRecordChromeBrowserState());

  if (otrBVCIsCurrent) {
    _currentBVC = self.otrBVC;
  }
}

- (void)shutdown {
  DCHECK(!_isShutdown);
  _isShutdown = YES;

  if (_tabModelObserver) {
    [_mainTabModel removeObserver:_tabModelObserver];
    [_otrTabModel removeObserver:_tabModelObserver];
    _tabModelObserver = nil;
  }

  [_mainTabModel removeObserver:self];
  [_otrTabModel removeObserver:self];

  // Disconnect the DeviceSharingManager.
  [self cleanDeviceSharingManager];

  // Stop URL monitoring of the main tab model.
  breakpad::StopMonitoringURLsForTabModel(_mainTabModel);

  // Stop Breakpad state monitoring of both tab models (if necessary).
  breakpad::StopMonitoringTabStateForTabModel(_mainTabModel);
  breakpad::StopMonitoringTabStateForTabModel(_otrTabModel);

  // Normally other objects will take care of unhooking the tab models from
  // the browser state, but this code should ensure that it happens regardless.
  [_mainTabModel browserStateDestroyed];
  [_otrTabModel browserStateDestroyed];

  self.mainBVC = nil;
  self.otrBVC = nil;

  _browserState = nullptr;
}

#pragma mark - Internal methods

- (TabModel*)buildOtrTabModel:(BOOL)empty {
  DCHECK(_browserState);
  // Ensure that the OTR ChromeBrowserState is created.
  ios::ChromeBrowserState* otrBrowserState =
      _browserState->GetOffTheRecordChromeBrowserState();
  DCHECK(otrBrowserState);
  return [self tabModelForBrowserState:otrBrowserState empty:empty];
}

- (TabModel*)tabModelForBrowserState:(ios::ChromeBrowserState*)browserState
                               empty:(BOOL)empty {
  SessionWindowIOS* sessionWindow = nil;
  if (!empty) {
    // Load existing saved tab model state.
    NSString* statePath =
        base::SysUTF8ToNSString(browserState->GetStatePath().AsUTF8Unsafe());
    SessionIOS* session =
        [[SessionServiceIOS sharedService] loadSessionFromDirectory:statePath];
    if (session) {
      DCHECK_EQ(session.sessionWindows.count, 1u);
      sessionWindow = session.sessionWindows[0];
    }
  }

  // Create tab model from saved session (nil is ok).
  TabModel* tabModel =
      [[TabModel alloc] initWithSessionWindow:sessionWindow
                               sessionService:[SessionServiceIOS sharedService]
                                 browserState:browserState];
  // Add observers.
  if (_tabModelObserver) {
    [tabModel addObserver:_tabModelObserver];
    [tabModel addObserver:self];
  }
  breakpad::MonitorTabStateForTabModel(tabModel);

  return tabModel;
}

- (BrowserViewController*)bvcForBrowserState:
                              (ios::ChromeBrowserState*)browserState
                                    tabModel:(TabModel*)tabModel {
  BrowserViewControllerDependencyFactory* factory =
      [[BrowserViewControllerDependencyFactory alloc]
          initWithBrowserState:browserState
                  webStateList:[tabModel webStateList]];
  return [[BrowserViewController alloc]
                initWithTabModel:tabModel
                    browserState:browserState
               dependencyFactory:factory
      applicationCommandEndpoint:_applicationCommandEndpoint];
}

@end
