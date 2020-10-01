// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/quota_settings.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/rand_util.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "storage/browser/quota/quota_device_info_helper.h"
#include "storage/browser/quota/quota_features.h"
#include "storage/browser/quota/quota_macros.h"

namespace storage {

namespace {

const int64_t kMBytes = 1024 * 1024;
const int kRandomizedPercentage = 10;
const double kDefaultPerHostRatio = 0.75;
const double kDefaultPoolSizeRatio = 0.8;

// Skews |value| by +/- |percent|.
int64_t RandomizeByPercent(int64_t value, int percent) {
  double random_percent = (base::RandDouble() - 0.5) * percent * 2;
  return value + (value * (random_percent / 100.0));
}

QuotaSettings CalculateIncognitoDynamicSettings(
    int64_t physical_memory_amount) {
  // The incognito pool size is a fraction of the amount of system memory,
  // and the amount is capped to a hard limit.
  double incognito_pool_size_ratio = 0.1;  // 10%
  int64_t max_incognito_pool_size = 300 * kMBytes;
  if (base::FeatureList::IsEnabled(features::kIncognitoDynamicQuota)) {
    const double lower_bound = features::kIncognitoQuotaRatioLowerBound.Get();
    const double upper_bound = features::kIncognitoQuotaRatioUpperBound.Get();
    incognito_pool_size_ratio =
        lower_bound + (base::RandDouble() * (upper_bound - lower_bound));
    max_incognito_pool_size = std::numeric_limits<int64_t>::max();
  } else {
    max_incognito_pool_size =
        RandomizeByPercent(max_incognito_pool_size, kRandomizedPercentage);
  }

  QuotaSettings settings;
  settings.pool_size = std::min(
      max_incognito_pool_size,
      static_cast<int64_t>(physical_memory_amount * incognito_pool_size_ratio));
  settings.per_host_quota = settings.pool_size / 3;
  settings.session_only_per_host_quota = settings.per_host_quota;
  settings.refresh_interval = base::TimeDelta::Max();
  return settings;
}

base::Optional<QuotaSettings> CalculateNominalDynamicSettings(
    const base::FilePath& partition_path,
    bool is_incognito,
    QuotaDeviceInfoHelper* device_info_helper) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (is_incognito) {
    return CalculateIncognitoDynamicSettings(
        device_info_helper->AmountOfPhysicalMemory());
  }

  // The fraction of the device's storage the browser is willing to use for
  // temporary storage.
  const double kTemporaryPoolSizeRatio = kDefaultPoolSizeRatio;

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

  // The fraction of the temporary pool that can be utilized by a single host.
  const double kPerHostTemporaryRatio = kDefaultPerHostRatio;

  // SessionOnly (or ephemeral) origins are allotted a fraction of what
  // normal origins are provided, and the amount is capped to a hard limit.
  const double kSessionOnlyHostQuotaRatio = 0.1;  // 10%
  const int64_t kMaxSessionOnlyHostQuota = 300 * kMBytes;

  QuotaSettings settings;

  int64_t total = device_info_helper->AmountOfTotalDiskSpace(partition_path);
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
  settings.per_host_quota = pool_size * kPerHostTemporaryRatio;
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
                               QuotaDeviceInfoHelper* device_info_helper,
                               OptionalQuotaSettingsCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&CalculateNominalDynamicSettings, partition_path,
                     is_incognito, base::Unretained(device_info_helper)),
      std::move(callback));
}

QuotaDeviceInfoHelper* GetDefaultDeviceInfoHelper() {
  static base::NoDestructor<QuotaDeviceInfoHelper> singleton;
  return singleton.get();
}

}  // namespace storage
