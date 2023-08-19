// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "base/at_exit.h"
#import "base/debug/crash_logging.h"
#import "base/strings/sys_string_conversions.h"
#import "build/blink_buildflags.h"
#import "components/component_updater/component_updater_paths.h"
#import "ios/chrome/app/chrome_main_module_buildflags.h"
#import "ios/chrome/app/startup/ios_chrome_main.h"
#import "ios/chrome/app/startup/ios_enable_sandbox_dump_buildflags.h"
#import "ios/chrome/browser/crash_report/crash_helper.h"
#import "ios/chrome/browser/shared/model/paths/paths.h"
#import "ios/public/provider/chrome/browser/primes/primes_api.h"

#if BUILDFLAG(IOS_ENABLE_SANDBOX_DUMP)
#import "ios/chrome/app/startup/sandbox_dump.h"  // nogncheck
#endif  // BUILDFLAG(IOS_ENABLE_SANDBOX_DUMP)

#if BUILDFLAG(USE_CHROME_BLINK_MAIN_MODULE) && BUILDFLAG(USE_BLINK)
#import "base/apple/bundle_locations.h"

// Dummy class used to locate the containing NSBundle.
@interface ChromeMainBundleLocator : NSObject
@end

@implementation ChromeMainBundleLocator
@end
#endif  // BUILDFLAG(USE_CHROME_BLINK_MAIN_MODULE) && BUILDFLAG(USE_BLINK)

extern "C" {
// This function must be marked with NO_STACK_PROTECTOR or it may crash on
// return, see the --change-stack-guard-on-fork command line flag.
__attribute__((visibility("default"))) int NO_STACK_PROTECTOR
ChromeMain(int argc, char** argv);
}

namespace {

NSString* const kUIApplicationDelegateInfoKey = @"UIApplicationDelegate";

void StartCrashController() {
  @autoreleasepool {
    crash_helper::Start();
  }
}

void SetTextDirectionIfPseudoRTLEnabled() {
  @autoreleasepool {
    NSUserDefaults* standard_defaults = [NSUserDefaults standardUserDefaults];
    if ([standard_defaults boolForKey:@"EnablePseudoRTL"]) {
      NSDictionary* pseudoDict = @{
        @"AppleTextDirection" : @"YES",
        @"NSForceRightToLeftWritingDirection" : @"YES"
      };
      [standard_defaults registerDefaults:pseudoDict];
    }
  }
}

int RunUIApplicationMain(int argc, char* argv[]) {
  @autoreleasepool {
    // Fetch the name of the UIApplication delegate stored in the application
    // Info.plist under the "UIApplicationDelegate" key.
    NSString* delegate_class_name = [[NSBundle mainBundle]
        objectForInfoDictionaryKey:kUIApplicationDelegateInfoKey];
    CHECK(delegate_class_name);

    return UIApplicationMain(argc, argv, nil, delegate_class_name);
  }
}

void RegisterPathProviders() {
  @autoreleasepool {
    ios::RegisterPathProvider();

    // Bundled components are not supported on ios, so DIR_USER_DATA is passed
    // for all three arguments.
    component_updater::RegisterPathProvider(
        ios::DIR_USER_DATA, ios::DIR_USER_DATA, ios::DIR_USER_DATA);
  }
}

}  // namespace

int ChromeMain(int argc, char* argv[]) {
  IOSChromeMain::InitStartTime();

#if BUILDFLAG(USE_CHROME_BLINK_MAIN_MODULE) && BUILDFLAG(USE_BLINK)
  // We package use_blink resources up into the framework itself so we need to
  // override the framework bundle.
  base::apple::SetOverrideFrameworkBundle(
      [NSBundle bundleForClass:[ChromeMainBundleLocator class]]);
#endif

#if BUILDFLAG(IOS_ENABLE_SANDBOX_DUMP)
  // Dumps the sandbox if needed. This must be called as soon as possible,
  // before actions are done on the sandbox.
  // This is a blocking call.
  DumpSandboxIfRequested();
#endif  // BUILDFLAG(IOS_ENABLE_SANDBOX_DUMP)

  // Set NSUserDefaults keys to force pseudo-RTL if needed.
  SetTextDirectionIfPseudoRTLEnabled();

  // Create this here since it's needed to start the crash handler.
  base::AtExitManager at_exit;

  // Start Primes logging if it's supported.
  if (ios::provider::IsPrimesSupported()) {
    ios::provider::PrimesStartLogging();
  }

  // The Crash Controller is started here even if the user opted out since we
  // don't have yet preferences. Later on it is stopped if the user opted out.
  // In any case reports are not sent if the user opted out.
  StartCrashController();

  // Always ignore SIGPIPE.  We check the return value of write().
  CHECK_NE(SIG_ERR, signal(SIGPIPE, SIG_IGN));

  // Register Chrome path providers.
  RegisterPathProviders();

  return RunUIApplicationMain(argc, argv);
}

#if !BUILDFLAG(USE_CHROME_MAIN_MODULE)
int main(int argc, char* argv[]) {
  // exit, don't return from main, to avoid the apparent removal of main from
  // stack backtraces under tail call optimization.
  exit(ChromeMain(argc, argv));
}
#endif  // !BUILDFLAG(USE_CHROME_MAIN_MODULE)
