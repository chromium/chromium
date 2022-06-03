// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "testing/libfuzzer/fuzzer_support_ios/fuzzer_support.h"

#import <UIKit/UIKit.h>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Springboard/Frontboard will kill any iOS/MacCatalyst app that fails to check
// in after launch within a given time. Starting a UIApplication before invoking
// fuzzer prevents this from happening.

// Since the executable isn't likely to be a real iOS UI, the delegate puts up a
// window displaying the app name. If a bunch of apps using MainHook are being
// run in a row, this provides an indication of which one is currently running.

static int g_argc;
static char** g_argv;

namespace fuzzer {
typedef int (*UserCallback)(const uint8_t* Data, std::size_t Size);
int FuzzerDriver(int* argc, char*** argv, UserCallback Callback);
}  // namespace fuzzer

namespace {
// TODO(crbug.com/1261537): Remove this when the function is provided by
// libFuzzer.
extern "C" __attribute__((visibility("default"))) int LLVMFuzzerRunDriver(
    int* argc,
    char*** argv,
    int (*UserCb)(const uint8_t* Data, std::size_t Size)) {
  return fuzzer::FuzzerDriver(argc, argv, UserCb);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

void PopulateUIWindow(UIWindow* window) {
  [window setBackgroundColor:[UIColor whiteColor]];
  [window makeKeyAndVisible];
  CGRect bounds = [[UIScreen mainScreen] bounds];
  // Add a label with the app name.
  UILabel* label = [[UILabel alloc] initWithFrame:bounds];
  label.text = [[NSProcessInfo processInfo] processName];
  label.textAlignment = NSTextAlignmentCenter;
  [window addSubview:label];

  // An NSInternalInconsistencyException is thrown if the app doesn't have a
  // root view controller. Set an empty one here.
  [window setRootViewController:[[UIViewController alloc] init]];
}
}

@interface UIApplication (Testing)
- (void)_terminateWithStatus:(int)status;
@end

// No-op scene delegate for libFuzzer. Note that this is created along with
// the application delegate, so they need to be separate objects (the same
// object can't be both the app and scene delegate, since new scene delegates
// are created for each scene).
@interface ChromeLibFuzzerSceneDelegate : NSObject <UIWindowSceneDelegate> {
  UIWindow* _window;
}
- (void)runFuzzer;
@end

@interface ChromeLibFuzzerDelegate : NSObject {
}
@end

@implementation ChromeLibFuzzerSceneDelegate

- (void)scene:(UIScene*)scene
    willConnectToSession:(UISceneSession*)session
                 options:(UISceneConnectionOptions*)connectionOptions
    API_AVAILABLE(ios(13), macCatalyst(13.0)) {
  _window =
      [[UIWindow alloc] initWithWindowScene:static_cast<UIWindowScene*>(scene)];
  PopulateUIWindow(_window);
  static dispatch_once_t once;
  // Delay 0.3 seconds to allow NSMenuBarScene to be created and thus app won't
  // be killed by the watchdog tracking that.
  dispatch_once(&once, ^{
    [self performSelector:@selector(runFuzzer) withObject:nil afterDelay:0.3];
  });
}

- (void)sceneDidDisconnect:(UIScene*)scene
    API_AVAILABLE(ios(13), macCatalyst(13.0)) {
  _window = nil;
}

- (void)runFuzzer {
  int exitStatus =
      LLVMFuzzerRunDriver(&g_argc, &g_argv, &LLVMFuzzerTestOneInput);

  // If a test app is too fast, it will exit before Instruments has has a
  // a chance to initialize and no test results will be seen.
  [NSThread sleepUntilDate:[NSDate dateWithTimeIntervalSinceNow:2.0]];

  // Use the hidden selector to try and cleanly take down the app (otherwise
  // things can think the app crashed even on a zero exit status).
  UIApplication* application = [UIApplication sharedApplication];
  [application _terminateWithStatus:exitStatus];

  exit(exitStatus);
}

@end

@implementation ChromeLibFuzzerDelegate

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  return YES;
}

@end

namespace ios_fuzzer {
void RunFuzzerFromIOSApp(int argc, char* argv[]) {
  g_argc = argc;
  g_argv = argv;
  @autoreleasepool {
    int exit_status =
        UIApplicationMain(g_argc, g_argv, nil, @"ChromeLibFuzzerDelegate");
    exit(exit_status);
  }
}

}  // namespace ios_fuzzer
