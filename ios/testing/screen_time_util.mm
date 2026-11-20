// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/screen_time_util.h"

#import <Foundation/Foundation.h>
#import <ScreenTime/ScreenTime.h>

#import "base/logging.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"

#if TARGET_OS_SIMULATOR
namespace {

constexpr base::TimeDelta kScreenTimeReadyTimeout = base::Seconds(120);
NSString* const kWebBrowserSettingsObserverKey = @"webBrowserSettingsObserver";
NSString* const kWebBrowserSettingsKey = @"webBrowserSettings";

}  // namespace
#endif  // TARGET_OS_SIMULATOR

void EnsureScreenTimeReady() {
#if TARGET_OS_SIMULATOR
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
#endif  // TARGET_OS_SIMULATOR
}
