// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/variations_smoke_test/variations_smoke_test_app_interface.h"

#import <sys/sysctl.h>

#import "base/process/process.h"
#import "base/time/time.h"
#import "components/prefs/pref_service.h"
#import "components/variations/pref_names.h"
#import "components/variations/service/variations_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"

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

+ (BOOL)isVariationsSeedStored {
  variations::SeedReaderWriter* seedReaderWriter =
      GetApplicationContext()
          ->GetVariationsService()
          ->GetSeedStoreForTesting()
          ->GetSeedReaderWriterForTesting();
  variations::StoredSeed storedSeed = seedReaderWriter->GetSeedData();
  return !storedSeed.data.empty() && !storedSeed.signature.empty() &&
         !seedReaderWriter->HasPendingWrite();
}

+ (BOOL)variationsSeedFetchedInCurrentLaunch {
  // If there's no fetch time, the returned time will be std::nullopt.
  base::Time lastFetchTime = GetApplicationContext()
                                 ->GetVariationsService()
                                 ->GetSeedStoreForTesting()
                                 ->GetSeedReaderWriterForTesting()
                                 ->GetSeedData()
                                 .client_fetch_time;
  // If there's no fetch time, the returned time will be 0 microseconds
  // from Windows epoch.
  return GetProcessStartTime() < lastFetchTime;
}

+ (void)localStatePrefsCommitPendingWrite {
  GetApplicationContext()->GetLocalState()->CommitPendingWrite();
}

@end
