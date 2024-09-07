// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_test_case.h"

#import <objc/runtime.h>

#import <memory>

#import "base/apple/bundle_locations.h"
#import "base/base_paths.h"
#import "base/command_line.h"
#import "base/ios/ios_util.h"
#import "base/path_service.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/policy/model/policy_earl_grey_utils.h"
#import "ios/chrome/browser/web/model/features.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case_app_interface.h"
#import "ios/chrome/test/earl_grey/scoped_allow_crash_on_startup.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/third_party/edo/src/Service/Sources/EDOClientService.h"
#import "ios/web/common/features.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

namespace {

// This flag indicates whether +setUpForTestCase has been executed in a test
// case.
bool gExecutedSetUpForTestCase = false;

bool gIsMockAuthenticationDisabled = false;

// YES the test is for startup.
bool gStartupTest = false;

NSString* const kFlakyEarlGreyTestTargetSuffix =
    @"_flaky_eg2tests_module-Runner";
NSString* const kMultitaskingEarlGreyTestTargetName =
    @"ios_chrome_multitasking_eg2tests_module-Runner";

// Returns a list of test names that run in multitasking test suite.
NSArray* multitaskingTests() {
  NSMutableArray* tests = [NSMutableArray arrayWithArray:@[
    // Integration tests
    @"testContextMenuOpenInNewTab",     // ContextMenuTestCase
    @"testContextMenuOpenInNewWindow",  // ContextMenuTestCase
    @"testSwitchToMain",                // CookiesTestCase
    // TODO(crbug.com/40896793) Re-enable this flaky test on multitasking.
    // @"testSwitchToIncognito",              // CookiesTestCase
    @"testFindDefaultFormAssistControls",  // FormInputTestCase
    @"testTabDeletion",                    // TabUsageRecorderTestCase
    @"testAutoTranslate",                  // TranslateTestCase

    // Settings tests
    @"testSignInPopUpAccountOnSyncSettings",   // AccountCollectionsTestCase
    @"testAutofillProfileEditing",             // AutofillSettingsTestCase
    @"testAccessibilityOfBlockPopupSettings",  // BlockPopupsTestCase
    @"testClearCookies",                       // SettingsTestCase
    @"testAccessibilityOfTranslateSettings",   // TranslateUITestCase

    // UI tests
    @"testActivityServiceControllerPrintAfterRedirectionToUnprintablePage",
    // ActivityServiceControllerTestCase
    @"testDismissOnDestroy",  // AlertCoordinatorTestCase
    // TODO(crbug.com/40927812): Re-enable this test.
    // @"testAddRemoveBookmark",       // BookmarksTestCase
    @"testJavaScriptInOmnibox",        // BrowserViewControllerTestCase
    @"testChooseCastReceiverChooser",  // CastReceiverTestCase
    @"testErrorPage",                  // ErrorPageTestCase
    @"testFindInPage",                 // FindInPageTestCase
    @"testDismissFirstRun",            // FirstRunTestCase
    // TODO(crbug.com/41407180) Failing after move to Xcode 10.
    // @"testLongPDFScroll",                         // FullscreenTestCase
    @"testDeleteHistory",                         // HistoryUITestCase
    @"testInfobarsDismissOnNavigate",             // InfobarTestCase
    @"testShowJavaScriptAlert",                   // JavaScriptDialogTestCase
    @"testKeyboardCommands_RecentTabsPresented",  // KeyboardCommandsTestCase
    @"testAccessibilityOnMostVisited",            // NewTabPageTestCase
    @"testPrintNormalPage",                       // PrintCoordinatorTestCase
    @"testQRScannerUIIsShown",    // QRScannerViewControllerTestCase
    @"testMarkMixedEntriesRead",  // ReadingListTestCase
    @"testClosedTabAppearsInRecentTabsPanel",  // RecentTabsTableTestCase
    @"testSafeModeSendingCrashReport",         // SafeModeTestCase
    @"testSignInOneUser",          // SigninInteractionControllerTestCase
    @"testSwitchTabs",             // StackViewTestCase
    @"testTabStripSwitchTabs",     // TabStripTestCase
    @"testTabHistoryMenu",         // TabHistoryPopupControllerTestCase
    @"testEnteringTabSwitcher",    // TabSwitcherControllerTestCase
    @"testEnterURL",               // ToolbarTestCase
    @"testOpenAndCloseToolsMenu",  // ToolsPopupMenuTestCase
    @"testUserFeedbackPageOpenPrivacyPolicy",  // UserFeedbackTestCase
    @"testVersion",                            // WebUITestCase
  ]];

  if (base::ios::IsRunningOnIOS17OrLater()) {
    // TODO(crbug.com/40925281): Test is failing on iOS17.
    [tests removeObject:@"testQRScannerUIIsShown"];
  }
  return tests;
}

const CFTimeInterval kDrainTimeout = 5;

bool IsAppInAllowedCrashState() {
  return ScopedAllowCrashOnStartup::IsActive() &&
         ![[AppLaunchManager sharedManager] appIsLaunched];
}

bool IsMockAuthenticationSetUp() {
  // `SetUpMockAuthentication` enables the fake sync server so checking
  // `isFakeSyncServerSetUp` here is sufficient to determine mock authentication
  // state.
  return [ChromeEarlGrey isFakeSyncServerSetUp];
}

void SetUpMockAuthentication() {
  [ChromeTestCaseAppInterface setUpMockAuthentication];
}

void TearDownMockAuthentication() {
  [ChromeTestCaseAppInterface tearDownMockAuthentication];
}

void ResetAuthentication() {
  [ChromeTestCaseAppInterface resetAuthentication];
}

}  // namespace

@interface ChromeTestCase () <AppLaunchManagerObserver> {
  // Block to be executed during object tearDown.
  ProceduralBlock _tearDownHandler;

  // This flag indicates whether test method -setUp steps are executed during a
  // test method.
  BOOL _executedTestMethodSetUp;

  std::unique_ptr<net::EmbeddedTestServer> _testServer;

  // The orientation of the device when entering these tests.
  UIDeviceOrientation _originalOrientation;
}

// Cleans up mock authentication.
+ (void)disableMockAuthentication;

// Sets up mock authentication.
+ (void)enableMockAuthentication;

// Returns a NSArray of test names in this class that contain the given prefix
+ (NSArray*)testNamesWithPrefix:(NSString*)prefix;

// Returns a NSArray of test names in this class for multitasking test suite.
+ (NSArray*)multitaskingTestNames;

@end

@implementation ChromeTestCase

// Overrides testInvocations so the set of tests run can be modified, as
// necessary.
+ (NSArray*)testInvocations {
  // Return specific list of tests based on the target.
  NSString* targetName = [NSBundle mainBundle].infoDictionary[@"CFBundleName"];
  if ([targetName hasSuffix:kFlakyEarlGreyTestTargetSuffix]) {
    // Only run FLAKY_ tests for flaky test suites.
    return [self testNamesWithPrefix:@"FLAKY"];
  } else if ([targetName isEqualToString:kMultitaskingEarlGreyTestTargetName]) {
    // Only run white listed tests for the multitasking test suite.
    return [self multitaskingTestNames];
  } else if ([[NSProcessInfo.processInfo.environment
                 objectForKey:@"RUN_DISABLED_EARL_GREY_TESTS"] boolValue]) {
    return [self testNamesWithPrefix:@"DISABLED"];
  } else {
    return [super testInvocations];
  }
}

+ (void)setUpForTestCase {
  [super setUpForTestCase];
  [ChromeTestCase setUpHelper];
  gExecutedSetUpForTestCase = true;
}

// Tear down called once for the class, to shutdown mock authentication.
+ (void)tearDown {
  [[self class] disableMockAuthentication];
  [super tearDown];
  gExecutedSetUpForTestCase = false;
  gStartupTest = false;
}

- (net::EmbeddedTestServer*)testServer {
  if (!_testServer) {
    _testServer = std::make_unique<net::EmbeddedTestServer>();
    _testServer->ServeFilesFromDirectory(
        base::PathService::CheckedGet(base::DIR_ASSETS)
            .AppendASCII("ios/testing/data/http_server_files/"));
    net::test_server::RegisterDefaultHandlers(_testServer.get());
  }
  return _testServer.get();
}

// Set up called once per test, to open a new tab.
- (void)setUp {
  // Add this class as an AppLaunchManager observer before [super setUp],
  // as [super setUp] can trigger an app launch.
  [[AppLaunchManager sharedManager] addObserver:self];

  [super setUp];

  [self resetAppState];

  ResetAuthentication();

  // Reset any remaining sign-in state from previous tests.
  [ChromeEarlGrey killWebKitNetworkProcess];
  [ChromeEarlGrey signOutAndClearIdentities];
  if (![ChromeTestCase isStartupTest]) {
    [ChromeEarlGrey openNewTab];
  }
  _executedTestMethodSetUp = YES;

  [ChromeTestCaseAppInterface blockSigninIPH];
}

// Tear down called once per test, to close all tabs and menus, and clear the
// tracked tests accounts. It also makes sure mock authentication is running.
- (void)tearDown {
  const bool appShouldBeRunning = !IsAppInAllowedCrashState();

  if (appShouldBeRunning) {
    // Clear multiwindow root and any extra windows.
    [ChromeEarlGrey closeAllExtraWindows];
    [EarlGrey setRootMatcherForSubsequentInteractions:nil];
  }

  [[AppLaunchManager sharedManager] removeObserver:self];

  if (_tearDownHandler) {
    _tearDownHandler();
  }

  if (appShouldBeRunning) {
    // EG syncs with WKWebView loading. Stops all loadings to prevent these from
    // failing rest of tearDown actions.
    [ChromeEarlGrey stopAllWebStatesLoading];

    // Clear any remaining test accounts and signed in users.
    [ChromeEarlGrey killWebKitNetworkProcess];
    [ChromeEarlGrey signOutAndClearIdentities];

    [[self class] enableMockAuthentication];

    // Clean up any UI that may remain open so the next test starts in a clean
    // state.
    if (![ChromeTestCase isStartupTest]) {
      // If a native context menu is presented on the screen, try to dismiss it.
      [ChromeEarlGreyUI dismissContextMenuIfPresent];
      [[self class] removeAnyOpenMenusAndInfoBars];
    }
    [[self class] closeAllTabs];

    // Clear testing policies to make sure they don't change the browser's
    // behavior in follow-up tests.
    policy_test_utils::ClearPolicies();
  }

  if ([[GREY_REMOTE_CLASS_IN_APP(UIDevice) currentDevice] orientation] !=
      _originalOrientation) {
    // Rotate the device back to the original orientation, since some tests
    // attempt to run in other orientations.
    [EarlGrey rotateDeviceToOrientation:_originalOrientation error:nil];
  }
  [super tearDown];
  _executedTestMethodSetUp = NO;
}

#pragma mark - Public methods

- (void)setTearDownHandler:(ProceduralBlock)tearDownHandler {
  // Enforce that only one `_tearDownHandler` is set per test.
  DCHECK(!_tearDownHandler);
  _tearDownHandler = [tearDownHandler copy];
}

+ (void)removeAnyOpenMenusAndInfoBars {
  NSUUID* uuid = [NSUUID UUID];
  // Removes all the UI elements.
  [ChromeTestCaseAppInterface
      removeInfoBarsAndPresentedStateWithCompletionUUID:uuid];
  ConditionBlock condition = ^{
    return [ChromeTestCaseAppInterface isCompletionInvokedWithUUID:uuid];
  };
  NSString* errorMessage =
      @"+[ChromeTestCaseAppInterface "
      @"removeInfoBarsAndPresentedStateWithCompletionUUID:] completion failed";
  // Waits until the UI elements are removed.
  bool completionInvoked = base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, condition);
  GREYAssertTrue(completionInvoked, errorMessage);
}

+ (void)closeAllTabs {
  [ChromeEarlGrey closeAllTabs];
  GREYWaitForAppToIdleWithTimeout(kDrainTimeout, @"App failed to idle");
}

- (void)disableMockAuthentication {
  [[self class] disableMockAuthentication];
}

- (void)enableMockAuthentication {
  [[self class] enableMockAuthentication];
}

- (BOOL)isRunningTest:(SEL)selector {
  return [[self currentTestMethodName]
      isEqualToString:NSStringFromSelector(selector)];
}

- (void)triggerRestoreByRestartingApplication {
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
}

+ (void)testForStartup {
  gStartupTest = YES;
}

+ (BOOL)isStartupTest {
  return gStartupTest;
}

#pragma mark - Private methods

+ (void)disableMockAuthentication {
  if (IsAppInAllowedCrashState()) {
    // Avoid attempting to send messages to an app that's not running.
    return;
  }
  if (!IsMockAuthenticationSetUp()) {
    return;
  }
  gIsMockAuthenticationDisabled = YES;

  // Make sure local data is cleared, before disabling mock authentication,
  // where data may be sent to real servers.
  // Remove all identities in FakeChromeIdentityService.
  [ChromeEarlGrey signOutAndClearIdentities];
  // Make sure any data on the fake sync server is cleared between tests, or
  // when explicitly resetting app data. This should happen after signout (to
  // avoid lots of "data was deleted" invalidations arriving on the client).
  [ChromeEarlGrey clearFakeSyncServerData];
  [ChromeEarlGrey tearDownFakeSyncServer];
  // Switch from FakeChromeIdentityService to ChromeIdentityServiceImpl.
  TearDownMockAuthentication();
}

+ (void)enableMockAuthentication {
  if (IsAppInAllowedCrashState()) {
    // Avoid attempting to send messages to an app that's not running.
    return;
  }
  if (IsMockAuthenticationSetUp()) {
    return;
  }
  gIsMockAuthenticationDisabled = NO;

  SetUpMockAuthentication();
  [ChromeEarlGrey setUpFakeSyncServer];
}

+ (NSArray*)testNamesWithPrefix:(NSString*)prefix {
  unsigned int count = 0;
  Method* methods = class_copyMethodList(self, &count);
  NSMutableArray* testNames = [NSMutableArray array];
  for (unsigned int i = 0; i < count; i++) {
    SEL selector = method_getName(methods[i]);
    if (base::StartsWith(sel_getName(selector), prefix.UTF8String)) {
      NSMethodSignature* methodSignature =
          [self instanceMethodSignatureForSelector:selector];
      NSInvocation* invocation =
          [NSInvocation invocationWithMethodSignature:methodSignature];
      invocation.selector = selector;
      [testNames addObject:invocation];
    }
  }
  free(methods);
  return testNames;
}

+ (NSArray*)multitaskingTestNames {
  unsigned int count = 0;
  Method* methods = class_copyMethodList(self, &count);
  NSMutableArray* multitaskingTestNames = [NSMutableArray array];
  for (unsigned int i = 0; i < count; i++) {
    SEL selector = method_getName(methods[i]);
    if ([multitaskingTests()
            containsObject:base::SysUTF8ToNSString(sel_getName(selector))]) {
      NSMethodSignature* methodSignature =
          [self instanceMethodSignatureForSelector:selector];
      NSInvocation* invocation =
          [NSInvocation invocationWithMethodSignature:methodSignature];
      invocation.selector = selector;
      [multitaskingTestNames addObject:invocation];
    }
  }
  free(methods);
  return multitaskingTestNames;
}

// Called from +setUp or when the host app is relaunched.
// Dismisses and revert browser settings to default.
// It also enables mock authentication.
+ (void)setUpHelper {
  GREYAssertTrue([ChromeEarlGrey isCustomWebKitLoadedIfRequested],
                 @"Unable to load custom WebKit");

  [[self class] enableMockAuthentication];

  // Sometimes on start up there can be infobars (e.g. restore session), so
  // ensure the UI is in a clean state.
  if (![ChromeTestCase isStartupTest]) {
    [[self class] removeAnyOpenMenusAndInfoBars];
    [self closeAllTabs];
  }
  [ChromeEarlGrey setPopupPrefValue:CONTENT_SETTING_DEFAULT];

  // Enforce the assumption that the tests are runing in portrait.
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait error:nil];

  // Clear multiwindow root and any extra windows. Once in `setUpForTestCase`
  // (in case of crashes) and on every `tearDown`.
  [ChromeEarlGrey closeAllExtraWindows];
  [EarlGrey setRootMatcherForSubsequentInteractions:nil];
}

// Resets the application state.
// Called at the start of a test and when the app is relaunched.
- (void)resetAppState {
  [[self class] disableMockAuthentication];
  [[self class] enableMockAuthentication];

  [ChromeEarlGrey resetDesktopContentSetting];

  gIsMockAuthenticationDisabled = NO;
  _tearDownHandler = nil;
  _originalOrientation = [[XCUIDevice sharedDevice] orientation];
}

// Returns the method name, e.g. "testSomething" of the test that is currently
// running. The name is extracted from the string for the test's name property,
// e.g. "-[DemographicsTestCase testSomething]".
- (NSString*)currentTestMethodName {
  int testNameStart = [self.name rangeOfString:@"test"].location;
  return [self.name
      substringWithRange:NSMakeRange(testNameStart,
                                     self.name.length - testNameStart - 1)];
}

#pragma mark - Handling system alerts

- (void)failAllTestsDueToSystemAlertVisible {
  XCTFail("System alerts are present on device. Skipping all tests.");
}

#pragma mark AppLaunchManagerObserver method

- (void)appLaunchManagerDidRelaunchApp:(AppLaunchManager*)appLaunchManager
                             runResets:(BOOL)runResets {
  if (!runResets) {
    // Check stored flags and restore to app status before relaunch.
    if (!gIsMockAuthenticationDisabled) {
      [[self class] enableMockAuthentication];
    }
    return;
  }
  // Do not call +[ChromeTestCase setUpHelper] if the app was relaunched
  // before +setUpForTestCase. +setUpForTestCase will call +setUpHelper, and
  // +setUpHelper can not be called twice during setup process.
  if (gExecutedSetUpForTestCase) {
    [ChromeTestCase setUpHelper];

    // Do not call test method setup steps if the app was relaunched before
    // -setUp is executed. If do so, two new tabs will be opened before test
    // method starts.
    if (_executedTestMethodSetUp) {
      [self resetAppState];

      ResetAuthentication();

      // Reset any remaining sign-in state from previous tests.
      [ChromeEarlGrey signOutAndClearIdentities];
      if (![ChromeTestCase isStartupTest]) {
        [ChromeEarlGrey openNewTab];
      }
    }
  }
}

@end
