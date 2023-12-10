// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/quota_settings.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/system/sys_info.h"
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
const double kDefaultPerStorageKeyRatio = 0.75;
const double kIncognitoQuotaRatioLowerBound = 0.15;
const double kIncognitoQuotaRatioUpperBound = 0.2;

// Skews |value| by +/- |percent|.
int64_t RandomizeByPercent(int64_t value, int percent) {
  double random_percent = (base::RandDouble() - 0.5) * percent * 2;
  return value + (value * (random_percent / 100.0));
}

QuotaSettings CalculateIncognitoDynamicSettings(
    uint64_t physical_memory_amount) {
  // The incognito pool size is a fraction of the amount of system memory.
  double incognito_pool_size_ratio =
      kIncognitoQuotaRatioLowerBound +
      (base::RandDouble() *
       (kIncognitoQuotaRatioUpperBound - kIncognitoQuotaRatioLowerBound));

  QuotaSettings settings;
  settings.pool_size =
      static_cast<int64_t>(physical_memory_amount * incognito_pool_size_ratio);
  settings.per_storage_key_quota = settings.pool_size / 3;
  settings.session_only_per_storage_key_quota = settings.per_storage_key_quota;
  settings.refresh_interval = base::TimeDelta::Max();
  return settings;
}

std::optional<QuotaSettings> CalculateNominalDynamicSettings(
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
  const double kTemporaryPoolSizeRatio = features::kPoolSizeRatio.Get();

  // The fixed size in bytes the browser is willing to use for temporary
  // storage. If both the ratio and the absolute size are set, the lower value
  // will be honored.
  const int64_t kTemporaryPoolSizeFixed =
      static_cast<int64_t>(features::kPoolSizeBytes.Get());

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
  const int64_t kShouldRemainAvailableFixed =
      static_cast<int64_t>(features::kShouldRemainAvailableBytes.Get());
  const double kShouldRemainAvailableRatio =
      features::kShouldRemainAvailableRatio.Get();

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
  const int64_t kMustRemainAvailableFixed =
      static_cast<int64_t>(features::kMustRemainAvailableBytes.Get());
  const double kMustRemainAvailableRatio =
      features::kMustRemainAvailableRatio.Get();

  // The fraction of the temporary pool that can be utilized by a single
  // StorageKey.
  const double kPerStorageKeyTemporaryRatio = kDefaultPerStorageKeyRatio;

  // SessionOnly (or ephemeral) origins are allotted a fraction of what
  // normal origins are provided, and the amount is capped to a hard limit.
  const double kSessionOnlyStorageKeyQuotaRatio = 0.1;  // 10%
  const int64_t kMaxSessionOnlyStorageKeyQuota = 300 * kMBytes;

  QuotaSettings settings;

  int64_t total = device_info_helper->AmountOfTotalDiskSpace(partition_path);
  if (total == -1) {
    LOG(ERROR) << "Unable to compute QuotaSettings.";
    return std::nullopt;
  }

  // Pool size calculated by ratio.
  int64_t pool_size_by_ratio = total * kTemporaryPoolSizeRatio;

  int64_t pool_size =
      kTemporaryPoolSizeFixed > 0
          ? std::min(kTemporaryPoolSizeFixed, pool_size_by_ratio)
          : pool_size_by_ratio;

  settings.pool_size = pool_size;
  settings.should_remain_available =
      std::min(kShouldRemainAvailableFixed,
               static_cast<int64_t>(total * kShouldRemainAvailableRatio));
  settings.must_remain_available =
      std::min(kMustRemainAvailableFixed,
               static_cast<int64_t>(total * kMustRemainAvailableRatio));
  settings.per_storage_key_quota = pool_size * kPerStorageKeyTemporaryRatio;
  settings.session_only_per_storage_key_quota = std::min(
      RandomizeByPercent(kMaxSessionOnlyStorageKeyQuota, kRandomizedPercentage),
      static_cast<int64_t>(settings.per_storage_key_quota *
                           kSessionOnlyStorageKeyQuotaRatio));
  settings.refresh_interval = base::Seconds(60);
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

double GetIncognitoQuotaRatioLowerBound_ForTesting() {
  return kIncognitoQuotaRatioLowerBound;
}
double GetIncognitoQuotaRatioUpperBound_ForTesting() {
  return kIncognitoQuotaRatioUpperBound;
}

}  // namespace storage
