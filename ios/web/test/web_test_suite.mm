// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/web_test_suite.h"

#import <Foundation/Foundation.h>
#import <ScreenTime/ScreenTime.h>

#import "base/check.h"
#import "base/files/file_path.h"
#import "base/logging.h"
#import "base/memory/ptr_util.h"
#import "base/path_service.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "components/crash/core/common/objc_zombie.h"
#import "ios/testing/verify_custom_webkit.h"
#import "ios/web/public/navigation/url_schemes.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "ui/base/resource/resource_bundle.h"

namespace web {

#if TARGET_OS_SIMULATOR
namespace {

constexpr base::TimeDelta kScreenTimeReadyTimeout = base::Seconds(120);
NSString* const kWebBrowserSettingsObserverKey = @"webBrowserSettingsObserver";
NSString* const kWebBrowserSettingsKey = @"webBrowserSettings";

// Waits for the simulator's `ScreenTimeAgent` and (on iOS 27+)
// `ScreenTimeSettingsAgent` daemons to finish post-boot initialization and
// respond to configuration requests before running tests. Otherwise,
// `WKWebView` frame commits trigger synchronous `ScreenTime` XPC calls on the
// Swift Concurrency cooperative thread pool that contend with
// `BrowserEngineKit` web content process launch on cold-booted simulators.
void EnsureScreenTimeReady() {
  @autoreleasepool {
    STScreenTimeConfigurationObserver* observer =
        [[STScreenTimeConfigurationObserver alloc]
            initWithUpdateQueue:dispatch_get_main_queue()];
    [observer startObserving];
    const bool check_web_browser_settings =
        [observer respondsToSelector:NSSelectorFromString(
                                         kWebBrowserSettingsObserverKey)];
    const bool ready =
        base::test::ios::WaitUntilConditionOrTimeout(kScreenTimeReadyTimeout, ^{
          if (!observer.configuration) {
            return false;
          }
          if (!check_web_browser_settings) {
            return true;
          }
          id settings_observer =
              [observer valueForKey:kWebBrowserSettingsObserverKey];
          if (![settings_observer
                  respondsToSelector:NSSelectorFromString(
                                         kWebBrowserSettingsKey)]) {
            return true;
          }
          return [settings_observer valueForKey:kWebBrowserSettingsKey] != nil;
        });
    LOG_IF(WARNING, !ready)
        << "Timed out waiting for ScreenTime configuration to become ready.";
    [observer stopObserving];
  }
}

}  // namespace
#endif  // TARGET_OS_SIMULATOR

WebTestSuite::WebTestSuite(int argc, char** argv)
    : base::TestSuite(argc, argv),
      web_client_(std::make_unique<FakeWebClient>()) {
  CHECK(IsCustomWebKitLoadedIfRequested());
#if TARGET_OS_SIMULATOR
  DCHECK(ObjcEvilDoers::ZombieEnable(true, 10000));
#endif
}

WebTestSuite::~WebTestSuite() {
  // Verify again at the end of the test run, in case some frameworks were not
  // yet loaded when the constructor ran.
  CHECK(IsCustomWebKitLoadedIfRequested());
}

void WebTestSuite::Initialize() {
  base::TestSuite::Initialize();

#if TARGET_OS_SIMULATOR
  EnsureScreenTimeReady();
#endif

  RegisterWebSchemes();

  // Load the resources produced by //ios/web/test:packed_resources if the test
  // has not already loaded other resources (e.g., by //ios/chrome tests).
  if (!ui::ResourceBundle::HasSharedInstance()) {
    base::FilePath resources_pack_path =
        base::PathService::CheckedGet(base::DIR_ASSETS)
            .Append(FILE_PATH_LITERAL("resources.pak"));
    ui::ResourceBundle::InitSharedInstanceWithPakPath(resources_pack_path);
  }
}

void WebTestSuite::Shutdown() {
  ui::ResourceBundle::CleanupSharedInstance();
  base::TestSuite::Shutdown();
}

}  // namespace web
