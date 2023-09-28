// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/browser_view_wrangler.h"

#import "base/feature_list.h"
#import "base/files/file_path.h"
#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/crash_report/model/crash_report_helper.h"
#import "ios/chrome/browser/device_sharing/device_sharing_browser_agent.h"
#import "ios/chrome/browser/sessions/session_restoration_browser_agent.h"
#import "ios/chrome/browser/settings/sync/utils/sync_presenter.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browsing_data_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/snapshots/snapshot_browser_agent.h"
#import "ios/chrome/browser/tabs/inactive_tabs/features.h"
#import "ios/chrome/browser/tabs/inactive_tabs/utils.h"
#import "ios/chrome/browser/ui/browser_view/browser_coordinator.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/ui/main/wrangled_browser.h"

// To get access to UseSessionSerializationOptimizations().
// TODO(crbug.com/1383087): remove once the feature is fully launched.
#import "ios/web/common/features.h"

namespace {
// Suffix to append to the session ID when creating an inactive browser.
NSString* kInactiveSessionIDSuffix = @"-Inactive";

}  // namespace

@interface BrowserViewWrangler () {
  ChromeBrowserState* _browserState;
  __weak SceneState* _sceneState;
  __weak id<ApplicationCommands> _applicationCommandEndpoint;
  __weak id<BrowsingDataCommands> _browsingDataCommandEndpoint;
  BOOL _isShutdown;

  std::unique_ptr<Browser> _mainBrowser;
  std::unique_ptr<Browser> _otrBrowser;
}

@property(nonatomic, strong, readwrite) WrangledBrowser* mainInterface;
@property(nonatomic, strong, readwrite) WrangledBrowser* incognitoInterface;

// Backing objects.
@property(nonatomic) BrowserCoordinator* mainBrowserCoordinator;
@property(nonatomic) BrowserCoordinator* incognitoBrowserCoordinator;
@property(nonatomic, readonly) Browser* mainBrowser;
@property(nonatomic, readonly) Browser* inactiveBrowser;
@property(nonatomic, readonly) Browser* otrBrowser;

// The main browser can't be set after creation, but they can be cleared
// (setting them to nullptr).
- (void)clearMainBrowser;
// The inactive browser can't be set after creation, but they can be cleared
// (setting them to nullptr).
- (void)clearInactiveBrowser;
// The OTR browser can be reset after creation.
- (void)setOtrBrowser:(std::unique_ptr<Browser>)browser;

// Sets up an existing browser.
- (void)setupBrowser:(Browser*)browser;

// Cleanup `browser` and associated state before destroying it.
- (void)cleanupBrowser:(Browser*)browser;

// Creates the correct BrowserCoordinator for the corresponding browser state
// and Browser.
- (BrowserCoordinator*)coordinatorForBrowser:(Browser*)browser;
@end

@implementation BrowserViewWrangler

@synthesize currentInterface = _currentInterface;
@synthesize currentBrowserProvider = _currentBrowserProvider;

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState
                          sceneState:(SceneState*)sceneState
          applicationCommandEndpoint:
              (id<ApplicationCommands>)applicationCommandEndpoint
         browsingDataCommandEndpoint:
             (id<BrowsingDataCommands>)browsingDataCommandEndpoint {
  if ((self = [super init])) {
    _browserState = browserState;
    _sceneState = sceneState;
    _applicationCommandEndpoint = applicationCommandEndpoint;
    _browsingDataCommandEndpoint = browsingDataCommandEndpoint;
  }
  return self;
}

- (void)dealloc {
  DCHECK(_isShutdown) << "-shutdown must be called before -dealloc";
}

- (Browser*)createMainBrowser {
  DCHECK(!_mainBrowser);
  _mainBrowser = Browser::Create(_browserState);
  [self setupBrowser:_mainBrowser.get()];
  return _mainBrowser.get();
}

- (void)createMainCoordinatorAndInterface {
  DCHECK(self.mainBrowser);

  // Create the main coordinator, and thus the main interface.
  Browser* mainBrowser = self.mainBrowser;
  _mainBrowserCoordinator = [self coordinatorForBrowser:mainBrowser];
  [_mainBrowserCoordinator start];

  // Restore the session after creating the coordinator.
  [self loadSessionForBrowser:mainBrowser];

  DCHECK(_mainBrowserCoordinator.viewController);
  _mainInterface =
      [[WrangledBrowser alloc] initWithCoordinator:_mainBrowserCoordinator];
}

- (void)createInactiveBrowser {
  DCHECK(self.mainBrowser)
      << "Main browser should be created before the inactive one.";
  DCHECK(self.mainInterface)
      << "Main interface should be created before create inactive browser.";

  // Create and restore the inactive browser.
  Browser* inactiveBrowser = self.mainBrowser->CreateInactiveBrowser();
  [self setupBrowser:inactiveBrowser];
  [self loadSessionForBrowser:inactiveBrowser];

  if (IsInactiveTabsEnabled()) {
    // Ensure there is no active element in the restored inactive browser. It
    // can be caused by a flag change, for example.
    // TODO(crbug.com/1412108): Remove the following line as soon as inactive
    // tabs is fully launched. After fully launched the only place where tabs
    // can move from inactive to active is after a settings change, this line
    // will be called at this specific moment.
    MoveTabsFromInactiveToActive(inactiveBrowser, _mainBrowser.get());

    // Moves all tabs that might have become inactive since the last launch.
    MoveTabsFromActiveToInactive(_mainBrowser.get(), inactiveBrowser);
  } else {
    RestoreAllInactiveTabs(inactiveBrowser, _mainBrowser.get());
  }
  _mainInterface.inactiveBrowser = inactiveBrowser;
}

#pragma mark - BrowserProviderInterface

- (id<BrowserProvider>)mainBrowserProvider {
  return self.mainInterface;
}

- (id<BrowserProvider>)incognitoBrowserProvider {
  return self.incognitoInterface;
}

- (BOOL)hasIncognitoBrowserProvider {
  return [self hasIncognitoInterface];
}

#pragma mark - BrowserViewInformation property implementations

- (void)setCurrentInterface:(WrangledBrowser*)interface {
  DCHECK(interface);
  // `interface` must be one of the interfaces this class already owns.
  DCHECK(self.mainInterface == interface ||
         self.incognitoInterface == interface);
  if (self.currentInterface == interface) {
    return;
  }

  if (self.currentInterface) {
    // Tell the current BVC it moved to the background.
    [self.currentInterface setPrimary:NO];
  }

  BOOL incognito = self.incognitoInterface == interface;

  _currentInterface = interface;
  _currentBrowserProvider =
      incognito ? self.incognitoBrowserProvider : self.mainBrowserProvider;

  // Update the shared active URL for the new interface.
  DeviceSharingBrowserAgent::FromBrowser(_currentInterface.browser)
      ->UpdateForActiveBrowser();
}

- (WrangledBrowser*)incognitoInterface {
  if (!_mainInterface)
    return nil;
  if (!_incognitoInterface) {
    _incognitoInterface = [self createOTRInterfaceAfterClosingAllTabs:NO];
  }
  return _incognitoInterface;
}

- (BOOL)hasIncognitoInterface {
  return _incognitoInterface;
}

- (WrangledBrowser*)createOTRInterfaceAfterClosingAllTabs:(BOOL)allTabsClosed {
  DCHECK(!_incognitoInterface);

  // The backing coordinator should not have been created yet.
  DCHECK(!_incognitoBrowserCoordinator);
  ChromeBrowserState* otrBrowserState =
      _browserState->GetOffTheRecordChromeBrowserState();
  DCHECK(otrBrowserState);
  Browser* otrBrowser = self.otrBrowser;

  _incognitoBrowserCoordinator = [self coordinatorForBrowser:otrBrowser];
  [_incognitoBrowserCoordinator start];

  if (!allTabsClosed) {
    // Restore the session after creating the coordinator, but only if not
    // recreating the Off-The-Record UI after closing all the tabs.
    [self loadSessionForBrowser:otrBrowser];
  }

  DCHECK(_incognitoBrowserCoordinator.viewController);
  return [[WrangledBrowser alloc]
      initWithCoordinator:_incognitoBrowserCoordinator];
}

- (Browser*)mainBrowser {
  DCHECK(_mainBrowser.get())
      << "-createMainBrowser must be called before -mainBrowser is accessed.";
  return _mainBrowser.get();
}

- (Browser*)inactiveBrowser {
  Browser* inactiveBrowser = self.mainBrowser->GetInactiveBrowser();
  CHECK(inactiveBrowser)
      << "-createInactiveBrowser must be called before -inactiveBrowser is "
         "accessed and Inactive Tabs feature should be available.";
  return inactiveBrowser;
}

- (Browser*)otrBrowser {
  if (!_otrBrowser) {
    // Ensure the incognito BrowserState is created.
    DCHECK(_browserState);
    ChromeBrowserState* incognitoBrowserState =
        _browserState->GetOffTheRecordChromeBrowserState();
    _otrBrowser = Browser::Create(incognitoBrowserState);
    [self setupBrowser:_otrBrowser.get()];
  }
  return _otrBrowser.get();
}

- (void)clearMainBrowser {
  if (!_mainBrowser) {
    return;
  }

  [self cleanupBrowser:_mainBrowser.get()];
  _mainBrowser.reset();
}

- (void)clearInactiveBrowser {
  Browser* inactiveBrowser =
      _mainBrowser ? _mainBrowser->GetInactiveBrowser() : nullptr;
  if (!inactiveBrowser) {
    return;
  }

  [self cleanupBrowser:inactiveBrowser];
  _mainBrowser->DestroyInactiveBrowser();
}

- (void)setOtrBrowser:(std::unique_ptr<Browser>)otrBrowser {
  if (_otrBrowser.get()) {
    [self cleanupBrowser:_otrBrowser.get()];
  }

  _otrBrowser = std::move(otrBrowser);
}

- (void)cleanupBrowser:(Browser*)browser {
  DCHECK(browser);

  WebStateList* webStateList = browser->GetWebStateList();
  crash_report_helper::StopMonitoringTabStateForWebStateList(webStateList);
  if (!browser->GetBrowserState()->IsOffTheRecord()) {
    crash_report_helper::StopMonitoringURLsForWebStateList(webStateList);
  }

  // Close all webstates in `webStateList`. Do this in an @autoreleasepool as
  // WebStateList observers will be notified (they are unregistered later). As
  // some of them may be implemented in Objective-C and unregister themselves
  // in their -dealloc method, ensure the -autorelease introduced by ARC are
  // processed before the WebStateList destructor is called.
  @autoreleasepool {
    webStateList->CloseAllWebStates(WebStateList::CLOSE_NO_FLAGS);
  }
}

#pragma mark - Other public methods

- (void)willDestroyIncognitoBrowserState {
  // It is theoretically possible that a Tab has been added to the webStateList
  // since the deletion has been scheduled. It is unlikely to happen for real
  // because it would require superhuman speed.
  DCHECK(self.hasIncognitoInterface);
  DCHECK(self.otrBrowser->GetWebStateList()->empty());
  DCHECK(_browserState);

  // Remove the OTR browser from the browser list. The browser itself is
  // still alive during this call, so any observers can act on it.
  BrowserList* browserList = BrowserListFactory::GetForBrowserState(
      self.otrBrowser->GetBrowserState());
  browserList->RemoveIncognitoBrowser(self.otrBrowser);

  // Stop watching the OTR webStateList's state for crashes.
  crash_report_helper::StopMonitoringTabStateForWebStateList(
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
}

- (void)incognitoBrowserStateCreated {
  DCHECK(_browserState);
  DCHECK(_browserState->HasOffTheRecordChromeBrowserState());

  // An empty _otrBrowser must be created at this point, because it is then
  // possible to prevent the tabChanged notification being sent. Otherwise,
  // when it is created, a notification with no tabs will be sent, and it will
  // be immediately deleted.
  ChromeBrowserState* incognitoBrowserState =
      _browserState->GetOffTheRecordChromeBrowserState();

  [self setOtrBrowser:Browser::Create(incognitoBrowserState)];
  [self setupBrowser:self.otrBrowser];
  DCHECK(self.otrBrowser->GetWebStateList()->empty());

  // Recreate the off-the-record interface, but do not load the session as
  // we had just closed all the tabs.
  _incognitoInterface = [self createOTRInterfaceAfterClosingAllTabs:YES];

  if (_currentInterface == nil) {
    self.currentInterface = self.incognitoInterface;
  }
}

- (void)shutdown {
  DCHECK(!_isShutdown);
  _isShutdown = YES;

  [self.mainBrowser->GetCommandDispatcher() prepareForShutdown];
  [self.inactiveBrowser->GetCommandDispatcher() prepareForShutdown];
  if ([self hasIncognitoInterface]) {
    [self.otrBrowser->GetCommandDispatcher() prepareForShutdown];
  }

  // At this stage, new BrowserCoordinators shouldn't be lazily constructed by
  // calling their property getters.
  [_mainBrowserCoordinator stop];
  _mainBrowserCoordinator = nil;
  [_incognitoBrowserCoordinator stop];
  _incognitoBrowserCoordinator = nil;

  BrowserList* browserList = BrowserListFactory::GetForBrowserState(
      self.mainBrowser->GetBrowserState());
  browserList->RemoveBrowser(self.inactiveBrowser);
  browserList->RemoveBrowser(self.mainBrowser);
  BrowserList* otrBrowserList = BrowserListFactory::GetForBrowserState(
      self.otrBrowser->GetBrowserState());
  otrBrowserList->RemoveIncognitoBrowser(self.otrBrowser);

  // Handles removing observers, stopping crash key monitoring, and closing all
  // tabs.
  // `clearInactiveBrowser` must be called before `clearMainBrowser`, as the
  // inactive browser is owned by the main browser.
  [self clearInactiveBrowser];
  [self clearMainBrowser];
  // TODO(crbug.com/1416934): Create `clearOtrBrowser` or similar to follow the
  // same logic as `clearMainBrowser` or `clearInactiveBrowser`.
  [self setOtrBrowser:nullptr];

  _browserState = nullptr;
}

#pragma mark - Internal methods

- (BrowserCoordinator*)coordinatorForBrowser:(Browser*)browser {
  BrowserCoordinator* coordinator =
      [[BrowserCoordinator alloc] initWithBaseViewController:nil
                                                     browser:browser];
  return coordinator;
}

- (void)dispatchToEndpointsForBrowser:(Browser*)browser {
  IncognitoReauthSceneAgent* reauthAgent =
      [IncognitoReauthSceneAgent agentFromScene:_sceneState];

  CommandDispatcher* dispatcher = browser->GetCommandDispatcher();
  [dispatcher startDispatchingToTarget:reauthAgent
                           forProtocol:@protocol(IncognitoReauthCommands)];

  [dispatcher startDispatchingToTarget:_applicationCommandEndpoint
                           forProtocol:@protocol(ApplicationCommands)];

  // -startDispatchingToTarget:forProtocol: doesn't pick up protocols the
  // passed protocol conforms to, so ApplicationSettingsCommands is explicitly
  // dispatched to the endpoint as well. Since this is potentially
  // fragile, DCHECK that it should still work (if the endpoint is non-nil).
  DCHECK(!_applicationCommandEndpoint ||
         [_applicationCommandEndpoint
             conformsToProtocol:@protocol(ApplicationSettingsCommands)]);
  [dispatcher startDispatchingToTarget:_applicationCommandEndpoint
                           forProtocol:@protocol(ApplicationSettingsCommands)];
  [dispatcher startDispatchingToTarget:_browsingDataCommandEndpoint
                           forProtocol:@protocol(BrowsingDataCommands)];
}

- (void)setupBrowser:(Browser*)browser {
  ChromeBrowserState* browserState = browser->GetBrowserState();
  BrowserList* browserList =
      BrowserListFactory::GetForBrowserState(browserState);
  if (browserState->IsOffTheRecord()) {
    browserList->AddIncognitoBrowser(browser);
  } else {
    browserList->AddBrowser(browser);
  }

  // Associate the current SceneState with the new browser.
  SceneStateBrowserAgent::CreateForBrowser(browser, _sceneState);

  [self dispatchToEndpointsForBrowser:browser];

  [self setSessionIDForBrowser:browser];

  crash_report_helper::MonitorTabStateForWebStateList(
      browser->GetWebStateList());

  // Follow loaded URLs in the non-incognito browser to send those in case of
  // crashes.
  if (!browserState->IsOffTheRecord()) {
    crash_report_helper::MonitorURLsForWebStateList(browser->GetWebStateList());
  }
}

// Returns the scene session ID with the inactive suffixes if needed.
- (NSString*)sceneSessionIDForBrowser:(Browser*)browser {
  NSString* sessionID = _sceneState.sceneSessionID;
  if (!browser->IsInactive()) {
    return sessionID;
  }
  return [sessionID stringByAppendingString:kInactiveSessionIDSuffix];
}

// Configures the BrowserAgent with the session identifier for `browser`.
- (void)setSessionIDForBrowser:(Browser*)browser {
  NSString* sceneSessionID = [self sceneSessionIDForBrowser:browser];

  SnapshotBrowserAgent::FromBrowser(browser)->SetSessionID(sceneSessionID);

  if (!web::features::UseSessionSerializationOptimizations()) {
    SessionRestorationBrowserAgent::FromBrowser(browser)->SetSessionID(
        sceneSessionID);
  }
}

// Load session for `browser`.
- (void)loadSessionForBrowser:(Browser*)browser {
  if (!web::features::UseSessionSerializationOptimizations()) {
    SessionRestorationBrowserAgent::FromBrowser(browser)->RestoreSession();
  }
}

@end
