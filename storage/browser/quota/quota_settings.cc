// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/quota_settings.h"

#include <algorithm>

#include "base/rand_util.h"
#include "base/sys_info.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "storage/browser/quota/quota_macros.h"

namespace storage {

namespace {

// Skews |value| by +/- |percent|.
int64_t RandomizeByPercent(int64_t value, int percent) {
  double random_percent = (base::RandDouble() - 0.5) * percent * 2;
  return value + (value * (random_percent / 100.0));
}

base::Optional<storage::QuotaSettings> CalculateNominalDynamicSettings(
    const base::FilePath& partition_path,
    bool is_incognito) {
  base::AssertBlockingAllowedDeprecated();
  const int64_t kMBytes = 1024 * 1024;
  const int kRandomizedPercentage = 10;

  if (is_incognito) {
    // The incognito pool size is a fraction of the amount of system memory,
    // and the amount is capped to a hard limit.
    const double kIncognitoPoolSizeRatio = 0.1;  // 10%
    const int64_t kMaxIncognitoPoolSize = 300 * kMBytes;

    storage::QuotaSettings settings;
    settings.pool_size = std::min(
        RandomizeByPercent(kMaxIncognitoPoolSize, kRandomizedPercentage),
        static_cast<int64_t>(base::SysInfo::AmountOfPhysicalMemory() *
                             kIncognitoPoolSizeRatio));
    settings.per_host_quota = settings.pool_size / 3;
    settings.session_only_per_host_quota = settings.per_host_quota;
    settings.refresh_interval = base::TimeDelta::Max();
    return settings;
  }

// The fraction of the device's storage the browser is willing to
// use for temporary storage.
#if defined(OS_CHROMEOS)
  // Chrome OS is given a larger fraction, as web content is the considered
  // the primary use of the platform. Chrome OS itself maintains free space by
  // starting to evict data (old profiles) when less than 1GB remains,
  // stopping eviction once 2GB is free.
  // Prior to M66 this was 1/3, same as other platforms.
  const double kTemporaryPoolSizeRatio = 2.0 / 3.0;  // 66%
#else
  const double kTemporaryPoolSizeRatio = 1.0 / 3.0;  // 33%
#endif

  // The amount of the device's storage the browser attempts to
  // keep free. If there is less than this amount of storage free
  // on the device, Chrome will grant 0 quota to origins.
  //
  // Prior to M66, this was 10% of total storage instead of a fixed value on
  // all devices. Now the minimum of a fixed value (2GB) and 10% is used to
  // limit the reserve on devices with plenty of storage, but scale down for
  // devices with extremely limited storage.
  // *   1TB storage -- min(100GB,2GB) = 2GB
  // * 500GB storage -- min(50GB,2GB) = 2GB
  // *  64GB storage -- min(6GB,2GB) = 2GB
  // *  16GB storage -- min(1.6GB,2GB) = 1.6GB
  // *   8GB storage -- min(800MB,2GB) = 800MB
  const int64_t kShouldRemainAvailableFixed = 2048 * kMBytes;  // 2GB
  const double kShouldRemainAvailableRatio = 0.1;              // 10%

  // The amount of the device's storage the browser attempts to
  // keep free at all costs. Data will be aggressively evicted.
  //
  // Prior to M66, this was 1% of total storage instead of a fixed value on
  // all devices. Now the minimum of a fixed value (1GB) and 1% is used to
  // limit the reserve on devices with plenty of storage, but scale down for
  // devices with extremely limited storage.
  // *   1TB storage -- min(10GB,1GB) = 1GB
  // * 500GB storage -- min(5GB,1GB) = 1GB
  // *  64GB storage -- min(640MB,1GB) = 640MB
  // *  16GB storage -- min(160MB,1GB) = 160MB
  // *   8GB storage -- min(80MB,1GB) = 80MB
  const int64_t kMustRemainAvailableFixed = 1024 * kMBytes;  // 1GB
  const double kMustRemainAvailableRatio = 0.01;             // 1%

  // Determines the portion of the temp pool that can be
  // utilized by a single host (ie. 5 for 20%).
  const int kPerHostTemporaryPortion = 5;

  // SessionOnly (or ephemeral) origins are allotted a fraction of what
  // normal origins are provided, and the amount is capped to a hard limit.
  const double kSessionOnlyHostQuotaRatio = 0.1;  // 10%
  const int64_t kMaxSessionOnlyHostQuota = 300 * kMBytes;

  storage::QuotaSettings settings;

  int64_t total = base::SysInfo::AmountOfTotalDiskSpace(partition_path);
  if (total == -1) {
    LOG(ERROR) << "Unable to compute QuotaSettings.";
    return base::nullopt;
  }

  int64_t pool_size = total * kTemporaryPoolSizeRatio;

  settings.pool_size = pool_size;
  settings.should_remain_available =
      std::min(kShouldRemainAvailableFixed,
               static_cast<int64_t>(total * kShouldRemainAvailableRatio));
  settings.must_remain_available =
      std::min(kMustRemainAvailableFixed,
               static_cast<int64_t>(total * kMustRemainAvailableRatio));
  settings.per_host_quota = pool_size / kPerHostTemporaryPortion;
  settings.session_only_per_host_quota = std::min(
      RandomizeByPercent(kMaxSessionOnlyHostQuota, kRandomizedPercentage),
      static_cast<int64_t>(settings.per_host_quota *
                           kSessionOnlyHostQuotaRatio));
  settings.refresh_interval = base::TimeDelta::FromSeconds(60);
  return settings;
}

}  // namespace

void GetNominalDynamicSettings(const base::FilePath& partition_path,
                               bool is_incognito,
                               OptionalQuotaSettingsCallback callback) {
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&CalculateNominalDynamicSettings, partition_path,
                     is_incognito),
      std::move(callback));
}

}  // namespace storage
