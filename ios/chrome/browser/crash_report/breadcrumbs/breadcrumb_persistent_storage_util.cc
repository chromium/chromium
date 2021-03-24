// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_persistent_storage_util.h"

#include "base/path_service.h"

namespace breadcrumb_persistent_storage_util {

const base::FilePath::CharType kOldBreadcrumbsFile[] =
    FILE_PATH_LITERAL("iOS Breadcrumbs");

const base::FilePath::CharType kOldBreadcrumbsTempFile[] =
    FILE_PATH_LITERAL("iOS Breadcrumbs.temp");

base::FilePath GetOldBreadcrumbPersistentStorageFilePath(
    base::FilePath storage_dir) {
  return storage_dir.Append(kOldBreadcrumbsFile);
}

base::FilePath GetOldBreadcrumbPersistentStorageTempFilePath(
    base::FilePath storage_dir) {
  return storage_dir.Append(kOldBreadcrumbsTempFile);
}

}  // namespace breadcrumb_persistent_storage_util
