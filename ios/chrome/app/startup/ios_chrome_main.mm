// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/startup/ios_chrome_main.h"

#import <UIKit/UIKit.h>

#import <vector>

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "base/types/fixed_array.h"
#import "ios/web/public/init/web_main.h"

namespace {
base::TimeTicks* g_start_time;
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
}

// static
const base::TimeTicks& IOSChromeMain::StartTime() {
  CHECK(g_start_time);
  return *g_start_time;
}
