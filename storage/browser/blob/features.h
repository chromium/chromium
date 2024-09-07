// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_FEATURES_H_
#define STORAGE_BROWSER_BLOB_FEATURES_H_

#include "base/component_export.h"
#include "base/features.h"

namespace features {

// Please keep features in alphabetical order.
// Enables blob URL fetches to fail when cross-partition.
COMPONENT_EXPORT(STORAGE_BROWSER)
BASE_DECLARE_FEATURE(kBlockCrossPartitionBlobUrlFetching);

// Please keep features in alphabetical order.

}  // namespace features

#endif  // STORAGE_BROWSER_BLOB_FEATURES_H_
