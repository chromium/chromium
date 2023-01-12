// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/file_system_features.h"

namespace storage::features {

// Enables persistent Filesystem API in incognito mode.
BASE_FEATURE(kEnablePersistentFilesystemInIncognito,
             "EnablePersistentFilesystemInIncognito",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Creates FileSystemContexts in incognito mode. This is used to run web tests
// in incognito mode to ensure feature parity for FileSystemAccessAccessHandles.
BASE_FEATURE(kIncognitoFileSystemContextForTesting,
             "IncognitoFileSystemContextForTesting",
             base::FEATURE_DISABLED_BY_DEFAULT);

// TODO(https://crbug.com/1396116): Remove this eventually.
// When enabled, FileSystemURL comparators will treat opaque origins as a null
// state. See https://crbug.com/1396116.
BASE_FEATURE(kFileSystemURLComparatorsTreatOpaqueOriginAsNoOrigin,
             "FileSystemURLComparatorsTreatOpaqueOriginAsNoOrigin",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace storage::features
