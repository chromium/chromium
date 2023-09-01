// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/shell/app/ios/web_tests_support_ios.h"

#import <UIKit/UIKit.h>

#include "base/message_loop/message_pump.h"
#include "base/message_loop/message_pump_apple.h"
#include "content/public/app/content_main.h"
#include "content/public/app/content_main_runner.h"
#include "content/public/common/main_function_params.h"
#include "content/shell/app/shell_main_delegate.h"

static int g_argc = 0;
static const char** g_argv = nullptr;
static std::unique_ptr<content::ContentMainRunner> g_main_runner;
static std::unique_ptr<content::ShellMainDelegate> g_main_delegate;

@interface WebTestApplication : UIApplication
- (BOOL)isRunningTests;
@end

@implementation WebTestApplication
// Need to return YES for testing. If not, a lot of timeouts happen during the
// test.
- (BOOL)isRunningTests {
  return YES;
}
@end

@interface WebTestDelegate : UIResponder <UIApplicationDelegate>
@end

@implementation WebTestDelegate

- (UISceneConfiguration*)application:(UIApplication*)application
    configurationForConnectingSceneSession:
        (UISceneSession*)connectingSceneSession
                                   options:(UISceneConnectionOptions*)options {
  UISceneConfiguration* configuration =
      [[UISceneConfiguration alloc] initWithName:nil
                                     sessionRole:connectingSceneSession.role];
  return configuration;
}

- (BOOL)application:(UIApplication*)application
    willFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  g_main_delegate = std::make_unique<content::ShellMainDelegate>();
  content::ContentMainParams params(g_main_delegate.get());
  params.argc = g_argc;
  params.argv = g_argv;
  g_main_runner = content::ContentMainRunner::Create();
  content::RunContentProcess(std::move(params), g_main_runner.get());
  return YES;
}

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  return YES;
}

- (BOOL)application:(UIApplication*)application
    shouldSaveSecureApplicationState:(NSCoder*)coder {
  return YES;
}

@end

namespace {

std::unique_ptr<base::MessagePump> CreateMessagePumpForUIForTests() {
  return std::unique_ptr<base::MessagePump>(new base::MessagePumpCFRunLoop());
}

}  // namespace

void InitIOSWebTestMessageLoop() {
  base::MessagePump::OverrideMessagePumpForUIFactory(
      &CreateMessagePumpForUIForTests);
}

int RunWebTestsFromIOSApp(int argc, const char** argv) {
  g_argc = argc;
  g_argv = argv;
  InitIOSWebTestMessageLoop();
  @autoreleasepool {
    return UIApplicationMain(argc, const_cast<char**>(argv),
                             @"WebTestApplication", @"WebTestDelegate");
  }
}
