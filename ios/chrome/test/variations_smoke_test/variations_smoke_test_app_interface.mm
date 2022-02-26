// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/variations_smoke_test/variations_smoke_test_app_interface.h"

#include <sys/sysctl.h>

#include "base/process/process.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "components/variations/pref_names.h"
#include "ios/chrome/browser/application_context.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using variations::prefs::kVariationsCompressedSeed;
using variations::prefs::kVariationsLastFetchTime;
using variations::prefs::kVariationsSeedSignature;

namespace {

// Returns current process start time from kernel.
base::Time GetProcessStartTime() {
  struct kinfo_proc info;
  size_t length = sizeof(struct kinfo_proc);
  int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, (int)getpid()};
  const int kr = sysctl(mib, std::size(mib), &info, &length, nullptr, 0);
  DCHECK_EQ(KERN_SUCCESS, kr);
  return base::Time::FromTimeVal(info.kp_proc.p_starttime);
}

}  // namespace

@implementation VariationsSmokeTestAppInterface

+ (BOOL)variationsSeedInLocalStatePrefs {
  PrefService* localState = GetApplicationContext()->GetLocalState();
  const std::string& compressedSeed =
      localState->GetString(kVariationsCompressedSeed);
  const std::string& seedSignature =
      localState->GetString(kVariationsSeedSignature);
  return !compressedSeed.empty() && !seedSignature.empty();
}

+ (BOOL)variationsSeedFetchedInCurrentLaunch {
  // If the pref value doesn't exist, the returned time will be 0 microseconds
  // from Windows epoch.
  base::Time lastFetchTime = GetApplicationContext()->GetLocalState()->GetTime(
      kVariationsLastFetchTime);
  return GetProcessStartTime() < lastFetchTime;
}

+ (void)localStatePrefsCommitPendingWrite {
  GetApplicationContext()->GetLocalState()->CommitPendingWrite();
}

@end
