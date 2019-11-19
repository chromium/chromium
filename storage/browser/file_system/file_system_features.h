// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_FEATURES_H_
#define STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace storage {

namespace features {

COMPONENT_EXPORT(STORAGE_BROWSER)
extern const base::Feature kEnablePersistentFilesystemInIncognito;

}  // namespace features

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_FEATURES_H_