// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_test_case.h"

#import <objc/runtime.h>

#include <memory>

#include "base/command_line.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case_app_interface.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/coverage_utils.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "net/test/embedded_test_server/default_handlers.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// This flag indicates whether +setUpForTestCase has been executed in a test
// case.
bool gExecutedSetUpForTestCase = false;

NSString* const kFlakyEarlGreyTestTargetSuffix = @"_flaky_egtests";

// Contains a list of test names that run in multitasking test suite.
NSArray* whiteListedMultitaskingTests = @[
  // Integration tests
  @"testContextMenuOpenInNewTab",        // ContextMenuTestCase
  @"testSwitchToMain",                   // CookiesTestCase
  @"testSwitchToIncognito",              // CookiesTestCase
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
  @"testDismissOnDestroy",                      // AlertCoordinatorTestCase
  @"testAddRemoveBookmark",                     // BookmarksTestCase
  @"testJavaScriptInOmnibox",                   // BrowserViewControllerTestCase
  @"testChooseCastReceiverChooser",             // CastReceiverTestCase
  @"testErrorPage",                             // ErrorPageTestCase
  @"testFindInPage",                            // FindInPageTestCase
  @"testDismissFirstRun",                       // FirstRunTestCase
  // TODO(crbug.com/872788) Failing after move to Xcode 10.
  // @"testLongPDFScroll",                         // FullscreenTestCase
  @"testDeleteHistory",                         // HistoryUITestCase
  @"testInfobarsDismissOnNavigate",             // InfobarTestCase
  @"testShowJavaScriptAlert",                   // JavaScriptDialogTestCase
  @"testKeyboardCommands_RecentTabsPresented",  // KeyboardCommandsTestCase
  @"testAccessibilityOnMostVisited",            // NewTabPageTestCase
  @"testPrintNormalPage",                       // PrintControllerTestCase
  @"testQRScannerUIIsShown",                 // QRScannerViewControllerTestCase
  @"testMarkMixedEntriesRead",               // ReadingListTestCase
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
];

const CFTimeInterval kDrainTimeout = 5;

void SetUpMockAuthentication() {
  [ChromeTestCaseAppInterface setUpMockAuthentication];
}

void TearDownMockAuthentication() {
  [ChromeTestCaseAppInterface tearDownMockAuthentication];
}

void ResetAuthentication() {
  [ChromeTestCaseAppInterface resetAuthentication];
}

void RemoveInfoBarsAndPresentedState() {
  [ChromeTestCaseAppInterface removeInfoBarsAndPresentedState];
}

UIDeviceOrientation GetCurrentDeviceOrientation() {
#if defined(CHROME_EARL_GREY_1)
  return [[UIDevice currentDevice] orientation];
#elif defined(CHROME_EARL_GREY_2)
  return [[GREY_REMOTE_CLASS_IN_APP(UIDevice) currentDevice] orientation];
#endif
}

}  // namespace

#if defined(CHROME_EARL_GREY_2)
GREY_STUB_CLASS_IN_APP_MAIN_QUEUE(ChromeTestCaseAppInterface)
#endif

@interface ChromeTestCase () <AppLaunchManagerObserver> {
  // Block to be executed during object tearDown.
  ProceduralBlock _tearDownHandler;

  // This flag indicates whether test method -setUp steps are executed during a
  // test method.
  BOOL _executedTestMethodSetUp;

  BOOL _isHTTPServerStopped;
  BOOL _isMockAuthenticationDisabled;
  std::unique_ptr<net::EmbeddedTestServer> _testServer;

  // The orientation of the device when entering these tests.
  UIDeviceOrientation _originalOrientation;
}

// Cleans up mock authentication.
+ (void)disableMockAuthentication;

// Sets up mock authentication.
+ (void)enableMockAuthentication;

// Stops the HTTP server. Should only be called when the server is running.
+ (void)stopHTTPServer;

// Starts the HTTP server. Should only be called when the server is stopped.
+ (void)startHTTPServer;

// Returns a NSArray of test names in this class that contain the prefix
// "FLAKY_".
+ (NSArray*)flakyTestNames;

// Returns a NSArray of test names in this class for multitasking test suite.
+ (NSArray*)multitaskingTestNames;
@end

@implementation ChromeTestCase

// Overrides testInvocations so the set of tests run can be modified, as
// necessary.
+ (NSArray*)testInvocations {
#if defined(CHROME_EARL_GREY_1)
  NSError* error = nil;
  [[EarlGrey selectElementWithMatcher:grey_systemAlertViewShown()]
      assertWithMatcher:grey_nil()
                  error:&error];
  if (error != nil) {
    NSLog(@"System alert view is present, so skipping all tests!");
#if TARGET_IPHONE_SIMULATOR
    return @[];
#else
    // Invoke XCTFail via call to stubbed out test.
    NSMethodSignature* signature =
        [ChromeTestCase instanceMethodSignatureForSelector:@selector
                        (failAllTestsDueToSystemAlertVisible)];
    NSInvocation* systemAlertTest =
        [NSInvocation invocationWithMethodSignature:signature];
    systemAlertTest.selector = @selector(failAllTestsDueToSystemAlertVisible);
    return @[ systemAlertTest ];
#endif  // !TARGET_IPHONE_SIMULATOR
  }
#endif  // defined(CHROME_EARL_GREY_1)

  // Return specific list of tests based on the target.
  NSString* targetName = [NSBundle mainBundle].infoDictionary[@"CFBundleName"];
  if ([targetName hasSuffix:kFlakyEarlGreyTestTargetSuffix]) {
    // Only run FLAKY_ tests for flaky test suites.
    return [self flakyTestNames];
  } else if ([targetName isEqualToString:@"ios_chrome_multitasking_egtests"]) {
    // Only run white listed tests for the multitasking test suite.
    return [self multitaskingTestNames];
  } else {
    return [super testInvocations];
  }
}

#if defined(CHROME_EARL_GREY_1)
+ (void)setUp {
  [super setUp];
  [ChromeTestCase setUpHelper];
}
#elif defined(CHROME_EARL_GREY_2)
+ (void)setUpForTestCase {
  [super setUpForTestCase];
  [ChromeTestCase setUpHelper];
  gExecutedSetUpForTestCase = true;
}
#endif  // CHROME_EARL_GREY_2

// Tear down called once for the class, to shutdown mock authentication and
// the HTTP server.
+ (void)tearDown {
  [[self class] disableMockAuthentication];
  [[self class] stopHTTPServer];
  [super tearDown];
  gExecutedSetUpForTestCase = false;
}

- (net::EmbeddedTestServer*)testServer {
  if (!_testServer) {
    _testServer = std::make_unique<net::EmbeddedTestServer>();
    NSString* bundlePath = [NSBundle bundleForClass:[self class]].resourcePath;
    _testServer->ServeFilesFromDirectory(
        base::FilePath(base::SysNSStringToUTF8(bundlePath))
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
  [ChromeEarlGrey signOutAndClearAccounts];
  [ChromeEarlGrey openNewTab];
  _executedTestMethodSetUp = YES;
}

// Tear down called once per test, to close all tabs and menus, and clear the
// tracked tests accounts. It also makes sure mock authentication and the HTTP
// server are running.
- (void)tearDown {
  [[AppLaunchManager sharedManager] removeObserver:self];

  if (_tearDownHandler) {
    _tearDownHandler();
  }

  // Clear any remaining test accounts and signed in users.
  [ChromeEarlGrey signOutAndClearAccounts];

  // Re-start anything that was disabled this test, so it is running when the
  // next test starts.
  if (_isHTTPServerStopped) {
    [[self class] startHTTPServer];
    _isHTTPServerStopped = NO;
  }
  if (_isMockAuthenticationDisabled) {
    [[self class] enableMockAuthentication];
    _isMockAuthenticationDisabled = NO;
  }

  // Clean up any UI that may remain open so the next test starts in a clean
  // state.
  [[self class] removeAnyOpenMenusAndInfoBars];
  [[self class] closeAllTabs];

  if (GetCurrentDeviceOrientation() != _originalOrientation) {
    // Rotate the device back to the original orientation, since some tests
    // attempt to run in other orientations.
    [ChromeEarlGrey rotateDeviceToOrientation:_originalOrientation error:nil];
  }
  [super tearDown];
  _executedTestMethodSetUp = NO;
}

#pragma mark - Public methods

- (void)setTearDownHandler:(ProceduralBlock)tearDownHandler {
  // Enforce that only one |_tearDownHandler| is set per test.
  DCHECK(!_tearDownHandler);
  _tearDownHandler = [tearDownHandler copy];
}

+ (void)removeAnyOpenMenusAndInfoBars {
  RemoveInfoBarsAndPresentedState();
  // After programatically removing UI elements, allow Earl Grey's
  // UI synchronization to become idle, so subsequent steps won't start before
  // the UI is in a good state.
  [[GREYUIThreadExecutor sharedInstance]
      drainUntilIdleWithTimeout:kDrainTimeout];
}

+ (void)closeAllTabs {
  [ChromeEarlGrey closeAllTabs];
  [[GREYUIThreadExecutor sharedInstance]
      drainUntilIdleWithTimeout:kDrainTimeout];
}

- (void)disableMockAuthentication {
  // Enforce that disableMockAuthentication can only be called once.
  DCHECK(!_isMockAuthenticationDisabled);
  [[self class] disableMockAuthentication];
  _isMockAuthenticationDisabled = YES;
}

- (void)stopHTTPServer {
  // Enforce that the HTTP server can only be stopped once per test. It should
  // not be stopped if it is not running.
  DCHECK(!_isHTTPServerStopped);
  [[self class] stopHTTPServer];
  _isHTTPServerStopped = YES;
}

#pragma mark - Private methods

+ (void)disableMockAuthentication {
  // Make sure local data is cleared, before disabling mock authentication,
  // where data may be sent to real servers.
  [ChromeEarlGrey signOutAndClearAccounts];
  [ChromeEarlGrey tearDownFakeSyncServer];
  TearDownMockAuthentication();
}

+ (void)enableMockAuthentication {
  SetUpMockAuthentication();
  [ChromeEarlGrey setUpFakeSyncServer];
}

+ (void)stopHTTPServer {
  web::test::HttpServer& server = web::test::HttpServer::GetSharedInstance();
  DCHECK(server.IsRunning());
  server.Stop();
}

+ (void)startHTTPServer {
  web::test::HttpServer& server = web::test::HttpServer::GetSharedInstance();
  NSString* bundlePath = [NSBundle bundleForClass:[self class]].resourcePath;
  server.StartOrDie(base::FilePath(base::SysNSStringToUTF8(bundlePath)));
}

+ (NSArray*)flakyTestNames {
  const char kFlakyTestPrefix[] = "FLAKY";
  unsigned int count = 0;
  Method* methods = class_copyMethodList(self, &count);
  NSMutableArray* flakyTestNames = [NSMutableArray array];
  for (unsigned int i = 0; i < count; i++) {
    SEL selector = method_getName(methods[i]);
    if (std::string(sel_getName(selector)).find(kFlakyTestPrefix) == 0) {
      NSMethodSignature* methodSignature =
          [self instanceMethodSignatureForSelector:selector];
      NSInvocation* invocation =
          [NSInvocation invocationWithMethodSignature:methodSignature];
      invocation.selector = selector;
      [flakyTestNames addObject:invocation];
    }
  }
  free(methods);
  return flakyTestNames;
}

+ (NSArray*)multitaskingTestNames {
  unsigned int count = 0;
  Method* methods = class_copyMethodList(self, &count);
  NSMutableArray* multitaskingTestNames = [NSMutableArray array];
  for (unsigned int i = 0; i < count; i++) {
    SEL selector = method_getName(methods[i]);
    if ([whiteListedMultitaskingTests
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
// It also starts the HTTP server and enables mock authentication.
+ (void)setUpHelper {
  GREYAssertTrue([ChromeEarlGrey isCustomWebKitLoadedIfRequested],
                 @"Unable to load custom WebKit");

  [CoverageUtils configureCoverageReportPath];

  [[self class] startHTTPServer];
  [[self class] enableMockAuthentication];

  // Sometimes on start up there can be infobars (e.g. restore session), so
  // ensure the UI is in a clean state.
  [self removeAnyOpenMenusAndInfoBars];
  [self closeAllTabs];
  [ChromeEarlGrey setContentSettings:CONTENT_SETTING_DEFAULT];

  [CoverageUtils configureCoverageReportPath];
}

// Resets the variables tracking app state.
// Called at the start of a test and when the app is relaunched.
- (void)resetAppState {
  _isHTTPServerStopped = NO;
  _isMockAuthenticationDisabled = NO;
  _tearDownHandler = nil;
  _originalOrientation = GetCurrentDeviceOrientation();
}

#pragma mark - Handling system alerts

- (void)failAllTestsDueToSystemAlertVisible {
  XCTFail("System alerts are present on device. Skipping all tests.");
}

#pragma mark AppLaunchManagerObserver method

- (void)appLaunchManagerDidRelaunchApp:(AppLaunchManager*)appLaunchManager {
  // Do not call +[ChromeTestCase setUpHelper] if the app was relaunched before
  // +setUpForTestCase. +setUpForTestCase will call +setUpHelper, and
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
      [ChromeEarlGrey signOutAndClearAccounts];
      [ChromeEarlGrey openNewTab];
    }
  }
}

@end
