// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/file_system_features.h"

namespace storage {

namespace features {

// Enables persistent Filesystem API in incognito mode.
const base::Feature kEnablePersistentFilesystemInIncognito{
    "EnablePersistentFilesystemInIncognito", base::FEATURE_ENABLED_BY_DEFAULT};
}  // namespace features

}  // namespace storage