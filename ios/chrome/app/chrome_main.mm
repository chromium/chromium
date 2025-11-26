// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "base/allocator/partition_alloc_support.h"
#import "base/apple/bundle_locations.h"
#import "base/at_exit.h"
#import "base/debug/crash_logging.h"
#import "base/feature_list.h"
#import "base/memory/page_size.h"
#import "base/memory/safety_checks.h"
#import "base/strings/sys_string_conversions.h"
#import "build/blink_buildflags.h"
#import "components/component_updater/component_updater_paths.h"
#import "components/crash/core/app/crashpad.h"
#import "components/crash/core/common/crash_key.h"
#import "ios/chrome/app/ios_force_build_chrome_framework_buildflags.h"
#import "ios/chrome/app/startup/ios_chrome_main.h"
#import "ios/chrome/app/startup/ios_enable_sandbox_dump_buildflags.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/crash_report/model/crash_helper.h"
#import "ios/chrome/browser/shared/model/paths/paths.h"
#import "ios/public/provider/chrome/browser/primes/primes_api.h"

#if BUILDFLAG(IOS_ENABLE_SANDBOX_DUMP)
#import "ios/chrome/app/startup/sandbox_dump.h"  // nogncheck
#endif  // BUILDFLAG(IOS_ENABLE_SANDBOX_DUMP)

extern "C" {
#if BUILDFLAG(USE_BLINK)
// This function must be marked with NO_STACK_PROTECTOR or it may crash on
// return, see the --change-stack-guard-on-fork command line flag.
NO_STACK_PROTECTOR
#endif
__attribute__((visibility("default"))) int ChromeMain(int argc, char* argv[]);
}

namespace {

// Kill switch to disable adding memory ranges to crash data when heap
// corruption or double free is detected by PA-E.
BASE_FEATURE(kIOSCorruptionDetectedMemoryRangesKillSwitch,
             base::FEATURE_DISABLED_BY_DEFAULT);

// The number of times a PA-E double free or corruption has been detected.
int g_double_free_or_corruption_detected_count = 0;

NSString* const kUIApplicationDelegateInfoKey = @"UIApplicationDelegate";

void StartCrashController() {
// TODO(crbug.com/399131917): Disable CrashHelper for blink for now. The
// chromium binary is loaded via a framework with blink and that appears to
// cause the task_set_exception_ports/task_swap_exception_port to fail. It may
// be that setting the exception ports from a framework library isn't allowed.
#if !BUILDFLAG(USE_BLINK)
  @autoreleasepool {
    crash_helper::Start();
  }
#endif
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

#if BUILDFLAG(IOS_ENABLE_SANDBOX_DUMP)
  // Dumps the sandbox if needed. This must be called as soon as possible,
  // before actions are done on the sandbox.
  // This is a blocking call.
  DumpSandboxIfRequested();
#endif  // BUILDFLAG(IOS_ENABLE_SANDBOX_DUMP)

  // SAFETY: according to the C++ standard, main() `argv` contains at least
  // `argc` elements.
  tests_hook::WipeProfileIfRequested(
      UNSAFE_BUFFERS(base::span(argv, static_cast<size_t>(argc))));

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

  crashpad::SimpleAddressRangeBag ios_dump_extra_ranges;
  crash_reporter::SetIntermediateDumpExtraMemoryRanges(&ios_dump_extra_ranges);

  crashpad::SimpleAddressRangeBag ios_extra_ranges;
  crash_reporter::SetExtraMemoryRanges(&ios_extra_ranges);

  auto CorruptionDetectedFn = [](uintptr_t address) {
    static crash_reporter::CrashKeyString<16>
        double_free_or_corruption_detected_count_key(
            "double_free_or_corruption_detected_count");
    g_double_free_or_corruption_detected_count++;
    double_free_or_corruption_detected_count_key.Set(
        base::NumberToString(g_double_free_or_corruption_detected_count));

    // If the kill switch is enabled, skip adding memory ranges.
    if (base::FeatureList::IsEnabled(
            kIOSCorruptionDetectedMemoryRangesKillSwitch)) {
      return;
    }

    static size_t pagesize = base::GetPageSize();
    uintptr_t page_size_u = static_cast<uintptr_t>(pagesize);
    uintptr_t offset_mask = page_size_u - 1;
    uintptr_t page_base_addr = address & ~offset_mask;
    crashpad::SimpleAddressRangeBag* ranges =
        crash_reporter::ExtraMemoryRanges();

    // Crashpad's extra memory ranges bag prohibits writing the first
    // page (i.e., address zero).
    if (page_base_addr < pagesize * 2) {
      return;
    }

    // Clear ranges, don't keep around a previously suppressed range.
    crashpad::SimpleAddressRangeBag::Iterator iterator(*ranges);
    while (const crashpad::SimpleAddressRangeBag::Entry* entry =
               iterator.Next()) {
      ranges->Remove(reinterpret_cast<void*>(entry->base), entry->size);
    }

    // Keep around the current, previous and next page.
    ranges->Insert(reinterpret_cast<void*>(page_base_addr), pagesize);
    ranges->Insert(reinterpret_cast<void*>(page_base_addr - pagesize),
                   pagesize);
    ranges->Insert(reinterpret_cast<void*>(page_base_addr + pagesize),
                   pagesize);
  };

  base::SetDoubleFreeOrCorruptionDetectedFn(CorruptionDetectedFn);

  // Always ignore SIGPIPE.  We check the return value of write().
  CHECK_NE(SIG_ERR, signal(SIGPIPE, SIG_IGN));

  // Register Chrome path providers.
  RegisterPathProviders();

#if PA_BUILDFLAG(USE_PARTITION_ALLOC) && !BUILDFLAG(USE_BLINK)
  // ContentMainRunnerImpl::Initialize calls this when USE_BLINK is true.
  base::allocator::PartitionAllocSupport::Get()->ReconfigureEarlyish("");
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC) && !BUILDFLAG(USE_BLINK)

#if BUILDFLAG(IOS_FORCE_BUILD_CHROME_FRAMEWORK)
  // Overrides the framework bundle when building as a
  // framework. This allows code to load resources from the correct
  // location, rather than from the main bundle.
  //
  // This call would be correct when building without the framework,
  // but since it is a no-op in that case, it is omitted to avoid
  // increasing startup latency.
  base::apple::SetOverrideFrameworkBundle(
      [NSBundle bundleForClass:NSClassFromString(@"MainController")]);
#endif

  return RunUIApplicationMain(argc, argv);
}
