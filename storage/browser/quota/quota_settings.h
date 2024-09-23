// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_QUOTA_SETTINGS_H_
#define STORAGE_BROWSER_QUOTA_QUOTA_SETTINGS_H_

#include <stdint.h>

#include <optional>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "storage/browser/quota/quota_device_info_helper.h"

namespace storage {

// Settings the storage lib embedder must provide to the QuotaManager.
struct QuotaSettings {
  QuotaSettings() = default;
  QuotaSettings(int64_t pool_size,
                int64_t per_storage_key_quota,
                int64_t should_remain_available,
                int64_t must_remain_available)
      : pool_size(pool_size),
        per_storage_key_quota(per_storage_key_quota),
        session_only_per_storage_key_quota(per_storage_key_quota),
        should_remain_available(should_remain_available),
        must_remain_available(must_remain_available) {}

  // The target size in bytes of the shared pool of disk space the quota
  // system allows for use by websites using HTML5 storage apis, for
  // example an embedder may use 50% of the total volume size.
  int64_t pool_size = 0;

  // The amount in bytes of the pool an individual site may consume. The
  // value must be less than or equal to the pool_size.
  int64_t per_storage_key_quota = 0;

  // The amount allotted to origins that are considered session only
  // according to the SpecialStoragePolicy provided by the embedder.
  int64_t session_only_per_storage_key_quota = 0;

  // The amount of space that should remain available on the storage
  // volume. As the volume approaches this limit, the quota system gets
  // more aggressive about evicting data.
  int64_t should_remain_available = 0;

  // The amount of space that must remain available on the storage
  // volume. As the volume approaches this limit, the quota system gets
  // very aggressive about disallowing new data.
  int64_t must_remain_available = 0;

  // The quota system queries the embedder for the QuotaSettings,
  // but will rate limit the frequency of the queries to no more than once
  // per refresh interval.
  base::TimeDelta refresh_interval = base::TimeDelta::Max();
};

// Function type used to return the settings in response to a
// GetQuotaSettingsFunc invocation. If the embedder cannot
// produce a settings values, std::nullopt can be returned.
using OptionalQuotaSettingsCallback =
    base::OnceCallback<void(std::optional<QuotaSettings>)>;

// Function type used to query the embedder about the quota manager settings.
// This function is invoked on the UI thread.
using GetQuotaSettingsFunc =
    base::RepeatingCallback<void(OptionalQuotaSettingsCallback callback)>;

// Posts a background task to calculate and report quota settings to the
// |callback| function based on the size of the volume containing the storage
// partition and a guestimate of the size required for the OS. The refresh
// interval is 60 seconds to accommodate changes to the size of the volume.
// Except, in the case of incognito, the pool size and quota values are based
// on the amount of physical memory and the refresh interval is maxed out.
COMPONENT_EXPORT(STORAGE_BROWSER)
void GetNominalDynamicSettings(const base::FilePath& partition_path,
                               bool is_incognito,
                               QuotaDeviceInfoHelper* deviceInfoHelper,
                               OptionalQuotaSettingsCallback callback);

COMPONENT_EXPORT(STORAGE_BROWSER)

// Returns settings that provide given `per_storage_key_quota` and a total
// poolsize of five times that.
inline QuotaSettings GetHardCodedSettings(int64_t per_storage_key_quota) {
  return QuotaSettings(per_storage_key_quota * 5, per_storage_key_quota,
                       per_storage_key_quota, per_storage_key_quota);
}

COMPONENT_EXPORT(STORAGE_BROWSER)
double GetIncognitoQuotaRatioLowerBound_ForTesting();
COMPONENT_EXPORT(STORAGE_BROWSER)
double GetIncognitoQuotaRatioUpperBound_ForTesting();

// Returns object that can fetch actual total disk space; instance lives
// as long as the process is a live.
COMPONENT_EXPORT(STORAGE_BROWSER)
QuotaDeviceInfoHelper* GetDefaultDeviceInfoHelper();
}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_QUOTA_SETTINGS_H_
