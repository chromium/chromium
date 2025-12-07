// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"

namespace content::features {

// TODO(crbug.com/40896420): Remove this flag eventually.
// TODO(b/354661640): Temporarily disable this flag while investigating CrOS
// file saving issue.
//
// When enabled, GetFile() and GetEntries() on a directory handle performs
// the blocklist check on child file handles.
BASE_FEATURE(kFileSystemAccessDirectoryIterationBlocklistCheck,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, sites are limited in how much underlying operating resources
// they can access through the `FileSystemObserver` API. This limit is called
// the quota limit. Without this enabled, sites will be limited by the system
// limit.
BASE_FEATURE(kFileSystemAccessObserverQuotaLimit,
             base::FEATURE_ENABLED_BY_DEFAULT);

// On Linux, the quota limit is found by:
// 1. Rounding down the system limit (read from
//    /proc/sys/fs/inotify/max_user_watches) to the nearest
//    `kFileSystemObserverQuotaLimitLinuxBucketSize`.
// 2. Taking that max of that result and
//    `kFileSystemObserverQuotaLimitLinuxMin`.
// 3. And setting quota limit to `kFileSystemObserverQuotaLimitLinuxPercent`% of
//    that result.
BASE_FEATURE_PARAM(size_t,
                   kFileSystemObserverQuotaLimitLinuxBucketSize,
                   &kFileSystemAccessObserverQuotaLimit,
                   "file_system_observer_quota_limit_linux_bucket_size",
                   100000);
BASE_FEATURE_PARAM(size_t,
                   kFileSystemObserverQuotaLimitLinuxMin,
                   &kFileSystemAccessObserverQuotaLimit,
                   "file_system_observer_quota_limit_linux_min",
                   8192);
BASE_FEATURE_PARAM(double,
                   kFileSystemObserverQuotaLimitLinuxPercent,
                   &kFileSystemAccessObserverQuotaLimit,
                   "file_system_observer_quota_limit_linux_percent",
                   0.8);

// On Mac, the quota limit is `kFileSystemObserverQuotaLimitMacPercent`% of the
// system limit (512) which is constant across all devices.
BASE_FEATURE_PARAM(double,
                   kFileSystemObserverQuotaLimitMacPercent,
                   &kFileSystemAccessObserverQuotaLimit,
                   "file_system_observer_quota_limit_mac_percent",
                   0.2  // About 100 FSEventStreamCreate calls.
);

// On Windows, the quota limit is a constant memory size.
BASE_FEATURE_PARAM(size_t,
                   kFileSystemObserverQuotaLimitWindows,
                   &kFileSystemAccessObserverQuotaLimit,
                   "file_system_observer_quota_limit_windows",
                   2 << 28  // 1/2GiB
);

}  // namespace content::features
