// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/breadcrumbs/core/breadcrumb_persistent_storage_util.h"

#include "base/files/scoped_temp_dir.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture to test BreadcrumbPersistentStorageUtil.
typedef PlatformTest BreadcrumbPersistentStorageUtilTest;

// Tests that the breadcrumb storage file path is different from the temp file
// path.
TEST_F(BreadcrumbPersistentStorageUtilTest, UniqueTempStorage) {
  base::ScopedTempDir scoped_temp_directory;
  EXPECT_TRUE(scoped_temp_directory.CreateUniqueTempDir());

  base::FilePath directory = scoped_temp_directory.GetPath();
  EXPECT_NE(breadcrumbs::GetBreadcrumbPersistentStorageFilePath(directory),
            breadcrumbs::GetBreadcrumbPersistentStorageTempFilePath(directory));
}
