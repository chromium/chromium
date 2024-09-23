// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_egtest_bundle_main.h"

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>
#import <grpc/grpc.h>
#import <grpcpp/grpcpp.h>
#import <objc/runtime.h>

#import <memory>

#import "base/apple/bundle_locations.h"
#import "base/at_exit.h"
#import "base/check.h"
#import "base/command_line.h"
#import "base/debug/stack_trace.h"
#import "base/i18n/icu_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/types/fixed_array.h"
#import "ios/chrome/test/earl_grey/chrome_egtest_plugin_client.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/base/resource/resource_bundle.h"

using chrome_egtest_plugin::TestPluginClient;
using grpc::Channel;

namespace {

const grpc::string gRPCHost = "localhost:32279";

// Contains startup code for a Chrome EG2 test module. Performs startup tasks
// when constructed and shutdown tasks when destroyed. Callers should create an
// instance of this object before running any tests and destroy the instance
// after tests have completed.
class TestMain {
 public:
  TestMain() {
    NSArray* arguments = NSProcessInfo.processInfo.arguments;

    // Convert NSArray to the required input type of `base::CommandLine::Init`.
    int argc = arguments.count;
    base::FixedArray<const char*> argv(argc);
    std::vector<std::string> argv_store;
    // Avoid using std::vector::push_back (or any other method that could cause
    // the vector to grow) as this will cause the std::string to be copied or
    // moved (depends on the C++ implementation) which may invalidates the
    // pointer returned by std::string::c_str(). Even if the strings are moved,
    // this may cause garbage if std::string uses optimisation for small strings
    // (by returning pointer to the object internals in that case).
    argv_store.resize(argc);
    for (int i = 0; i < argc; i++) {
      argv_store[i] = base::SysNSStringToUTF8(arguments[i]);
      argv[i] = argv_store[i].c_str();
    }

    // Initialize the CommandLine with arguments. ResourceBundle requires
    // CommandLine to exist.
    base::CommandLine::Init(argc, argv.data());

    // Configures the default framework bundle to point to the test module
    // bundle instead of the test runner app.
    base::apple::SetOverrideFrameworkBundle(
        [NSBundle bundleForClass:[ChromeEGTestBundleMain class]]);

    base::i18n::InitializeICU();

    // Load pak files into the ResourceBundle.
    l10n_util::OverrideLocaleWithCocoaLocale();
    const std::string loaded_locale =
        ui::ResourceBundle::InitSharedInstanceWithLocale(
            /*pref_locale=*/std::string(), /*delegate=*/nullptr,
            ui::ResourceBundle::LOAD_COMMON_RESOURCES);
    CHECK(!loaded_locale.empty());
  }

  TestMain(const TestMain&) = delete;
  TestMain& operator=(const TestMain&) = delete;

  ~TestMain() {}

 private:
  base::AtExitManager exit_manager_;
};
}  // namespace

@class XCTSourceCodeSymbolInfo;
@protocol XCTSymbolInfoProviding <NSObject>
- (XCTSourceCodeSymbolInfo*)symbolInfoForAddressInCurrentProcess:(pid_t)pid
                                                           error:
                                                               (NSError**)error;
@end

@interface XCTSymbolicationService
+ (void)setSharedService:(id<XCTSymbolInfoProviding>)arg1;
@end

@interface ChromeEGTestBundleMain () <XCTestObservation> {
  std::unique_ptr<TestMain> _testMain;
  std::unique_ptr<TestPluginClient> _testPluginClient;
}
@end

@implementation ChromeEGTestBundleMain

- (instancetype)init {
  if ((self = [super init])) {
    [[XCTestObservationCenter sharedTestObservationCenter]
        addTestObserver:self];
  }

  // initializing test plugin client iff test plugin server is running on the
  // host and at least one plugin is enabled for this test run
  _testPluginClient = std::make_unique<TestPluginClient>(
      grpc::CreateChannel(gRPCHost, grpc::InsecureChannelCredentials()));
  NSLog(@"Checking whether any test plugins are enabled...");
  std::vector<std::string> enabledPlugins =
      _testPluginClient->ListEnabledPlugins();

  // we will not use the test plugin feature if test runner server side is not
  // running (e.g. tests are run locally), or no plugins are enabled for this
  // test run.
  if (enabledPlugins.size() == 0) {
    NSLog(@"iOS test runner is not running, or no test plugins are enabled. "
          @"Test plugins feature will not be used.");
  } else {
    _testPluginClient->set_is_service_enabled(true);
    NSLog(@"At least one test plugin is enabled. Test plugins features will be "
          @"used throughout tests executions");
  }
  return self;
}

// -waitForQuiescenceIncludingAnimationsIdle tends to introduce a long
// unnecessary delay, as EarlGrey already checks for animations to complete.
// Swizzling and skipping the following call speeds up test runs.
- (void)disableWaitForIdle {
  SEL originalSelector =
      NSSelectorFromString(@"waitForQuiescenceIncludingAnimationsIdle:");
  SEL swizzledSelector = @selector(skipQuiescenceDelay);
  Method originalMethod = class_getInstanceMethod(
      objc_getClass("XCUIApplicationProcess"), originalSelector);
  Method swizzledMethod =
      class_getInstanceMethod([self class], swizzledSelector);
  method_exchangeImplementations(originalMethod, swizzledMethod);
}

// Empty swizzled method to be invoked by XCTest at the start of each test case.
// Since earl grey synchronizes automatically, do nothing here.
- (void)skipQuiescenceDelay {
}

#pragma mark - XCTestObservation

- (void)testBundleWillStart:(NSBundle*)testBundle {
  DCHECK(!_testMain);
  _testMain = std::make_unique<TestMain>();

  // Ensure that //ios/web and the bulk of //ios/chrome/browser are not compiled
  // into the test module. This is hard to assert at compile time, due to
  // transitive dependencies, but at runtime it's easy to check if certain key
  // classes are present or absent.
  CHECK(NSClassFromString(@"CRWWebController") == nil);
  CHECK(NSClassFromString(@"MainController") == nil);
  CHECK(NSClassFromString(@"BrowserViewController") == nil);

  // Disable aggressive symbolication and disable symbolication service to work
  // around slow XCTest assertion failures. These failures are spending a very
  // long time attempting to symbolicate.
  Class symbolicationService = NSClassFromString(@"XCTSymbolicationService");
  if (symbolicationService != nil) {
    [symbolicationService setSharedService:nil];
  }
  [[NSUserDefaults standardUserDefaults]
      setBool:YES
       forKey:@"XCTDisableAggressiveSymbolication"];

  // Disable long wait for idle messages.
  [self disableWaitForIdle];
}

- (void)testBundleDidFinish:(NSBundle*)testBundle {
  if (_testPluginClient->is_service_enabled()) {
    NSLog(@"calling testBundleWillFinish to test plugin server");
    std::string deviceName =
        base::SysNSStringToUTF8(UIDevice.currentDevice.name);
    _testPluginClient->TestBundleWillFinish(deviceName);
  }

  [[XCTestObservationCenter sharedTestObservationCenter]
      removeTestObserver:self];

  _testMain.reset();
}

- (void)testCaseWillStart:(XCTestCase*)testCase {
  if (_testPluginClient->is_service_enabled()) {
    NSLog(@"calling testCaseWillStart to test plugin server");
    std::string testName = base::SysNSStringToUTF8(testCase.name);
    std::string deviceName =
        base::SysNSStringToUTF8(UIDevice.currentDevice.name);
    _testPluginClient->TestCaseWillStart(testName, deviceName);
  }
}

// this is called when test case failed unexpectedly
- (void)testCase:(XCTestCase*)testCase didRecordIssue:(XCTIssue*)issue {
  if (_testPluginClient->is_service_enabled()) {
    NSLog(@"calling testCaseDidFail to test plugin server");
    std::string testName = base::SysNSStringToUTF8(testCase.name);
    std::string deviceName =
        base::SysNSStringToUTF8(UIDevice.currentDevice.name);
    _testPluginClient->TestCaseDidFail(testName, deviceName);
  }
  NSString* current_stack =
      base::SysUTF8ToNSString(base::debug::StackTrace().ToString());
  NSLog(@"%@", current_stack);
}

- (void)testCaseDidFinish:(XCTestCase*)testCase {
  if (_testPluginClient->is_service_enabled()) {
    NSLog(@"calling testCaseDidFinish to test plugin server");
    std::string testName = base::SysNSStringToUTF8(testCase.name);
    std::string deviceName =
        base::SysNSStringToUTF8(UIDevice.currentDevice.name);
    _testPluginClient->TestCaseDidFinish(testName, deviceName);
  }
}

@end
