// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/startup/ios_chrome_main.h"

#import <UIKit/UIKit.h>

#import <vector>

#import "base/check.h"
#import "base/process/process.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "base/types/fixed_array.h"
#import "ios/web/public/init/web_main.h"

namespace {
// Upper bound for a valid pre-main duration. Because both endpoints are
// wall-clock times (base::Time), a forward clock adjustment (e.g. NTP sync)
// between fork() and main() could inflate a delta.  So deltas above 30s are
// discarded because they're more likely to represent a clock adjustment
// than actual startup time, especially considering the iOS launch watchdog
// is expected to terminate foreground launches that take >20s.  The watchdog
// timer starts at fork(), so it will include pre-main duration
constexpr base::TimeDelta kMaxPreMainDuration = base::Seconds(30);

base::TimeTicks* g_start_time = nullptr;
base::TimeDelta* g_pre_main_duration = nullptr;
}  // namespace

IOSChromeMain::IOSChromeMain() {
  web::WebMainParams main_params(&main_delegate_);
  NSArray<NSString*>* arguments = [[NSProcessInfo processInfo] arguments];

  main_params.args.reserve([arguments count]);
  for (NSString* argument in arguments) {
    main_params.args.push_back(base::SysNSStringToUTF8(argument));
  }

  // Chrome registers an AtExitManager in main in order to initialize the crash
  // handler early, so prevent a second registration by WebMainRunner.
  main_params.register_exit_manager = false;
  web_main_ = std::make_unique<web::WebMain>(std::move(main_params));
  web_main_->Startup();
}

IOSChromeMain::~IOSChromeMain() {}

// static
void IOSChromeMain::InitStartTime() {
  DCHECK(!g_start_time);
  g_start_time = new base::TimeTicks(base::TimeTicks::Now());

  const base::Time creation_time = base::Process::Current().CreationTime();
  if (!creation_time.is_null()) {
    const base::TimeDelta delta = base::Time::Now() - creation_time;
    if (delta.is_positive() && delta <= kMaxPreMainDuration) {
      g_pre_main_duration = new base::TimeDelta(delta);
    }
  }
}

// static
base::TimeTicks IOSChromeMain::StartTime() {
  CHECK(g_start_time);
  return *g_start_time;
}

// static
base::TimeDelta IOSChromeMain::PreMainDuration() {
  if (g_pre_main_duration) {
    return *g_pre_main_duration;
  }
  return base::TimeDelta();
}
