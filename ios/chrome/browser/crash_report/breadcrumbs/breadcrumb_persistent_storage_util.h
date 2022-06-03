// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_PERSISTENT_STORAGE_UTIL_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_PERSISTENT_STORAGE_UTIL_H_

#include "base/files/file_path.h"

namespace breadcrumb_persistent_storage_util {

// DEPRECATED: for migration only. Use
// breadcrumbs::GetBreadcrumbPersistentStorageTempFilePath() instead.
// TODO(crbug.com/1187988): Remove along with migration code.
base::FilePath GetOldBreadcrumbPersistentStorageFilePath(
    base::FilePath storage_dir);

// DEPRECATED: for migration only. Use
// breadcrumbs::GetBreadcrumbPersistentStorageTempFilePath() instead.
// TODO(crbug.com/1187988): Remove along with migration code.
base::FilePath GetOldBreadcrumbPersistentStorageTempFilePath(
    base::FilePath storage_dir);

}  // namespace breadcrumb_persistent_storage_util

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_PERSISTENT_STORAGE_UTIL_H_
