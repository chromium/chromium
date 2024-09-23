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
  NSArray* arguments = [[NSProcessInfo processInfo] arguments];
  main_params.argc = [arguments count];
  base::FixedArray<const char*> argv(main_params.argc);
  std::vector<std::string> argv_store;

  // Avoid using std::vector::push_back (or any other method that could cause
  // the vector to grow) as this will cause the std::string to be copied or
  // moved (depends on the C++ implementation) which may invalidates the pointer
  // returned by std::string::c_str(). Even if the strings are moved, this may
  // cause garbage if std::string uses optimisation for small strings (by
  // returning pointer to the object internals in that case).
  argv_store.resize([arguments count]);
  for (NSUInteger i = 0; i < [arguments count]; i++) {
    argv_store[i] = base::SysNSStringToUTF8([arguments objectAtIndex:i]);
    argv[i] = argv_store[i].c_str();
  }
  main_params.argv = argv.data();

  // Chrome registers an AtExitManager in main in order to initialize the crash
  // handler early, so prevent a second registration by WebMainRunner.
  main_params.register_exit_manager = false;
  web_main_ = std::make_unique<web::WebMain>(std::move(main_params));
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
