// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
#import "ios/chrome/app/tests_hook.h"
// clang-format on

#import <os/log.h>
#import <os/signpost.h>

#import "ios/public/provider/chrome/browser/primes/primes_api.h"

namespace tests_hook {

void SignalAppLaunched() {
  // The app launched signal is only used by startup tests, which unlike EG
  // tests do not have a tear down method which stops logging, so stop logging
  // here to flush logs
  ios::provider::PrimesStopLogging();

  os_log_t hke_os_log = os_log_create("com.google.hawkeye.ios",
                                      OS_LOG_CATEGORY_POINTS_OF_INTEREST);
  os_signpost_id_t os_signpost = os_signpost_id_generate(hke_os_log);
  os_signpost_event_emit(hke_os_log, os_signpost, "APP_LAUNCHED");
  // For startup tests instrumented with xctrace we need to log the signal using
  // os_log
  os_log(hke_os_log, "APP_LAUNCHED");

  // For regular startup tests we rely on printf to signal that the app has
  // started and can be terminated
  printf("APP_LAUNCHED\n");
}

}  // namespace tests_hook
