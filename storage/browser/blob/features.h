// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_FEATURES_H_
#define STORAGE_BROWSER_BLOB_FEATURES_H_

#include "base/component_export.h"
#include "base/features.h"

namespace features {

// Please keep features in alphabetical order.
// Enables stateful memory pressure handling in BlobMemoryController, where the
// in-memory blob quota scales with the memory limit. Only has an effect if
// base::kStatefulMemoryPressure is also enabled. When disabled,
// BlobMemoryController registers as a stateless memory consumer and uses
// one-shot paging to disk on memory pressure.
COMPONENT_EXPORT(STORAGE_BROWSER)
BASE_DECLARE_FEATURE(kBlobStatefulMemoryPressure);

// Enables Fetch-compliant Range header validation for blob: URL fetches.
// Invalid or unsupported Range headers fail with a network error instead of
// falling back to serving the full blob.
COMPONENT_EXPORT(STORAGE_BROWSER)
BASE_DECLARE_FEATURE(kBlobURLFetchRangeHeaderValidation);

// Enables blob URL fetches to fail when cross-partition.
COMPONENT_EXPORT(STORAGE_BROWSER)
BASE_DECLARE_FEATURE(kBlockCrossPartitionBlobUrlFetching);

// Please keep features in alphabetical order.

}  // namespace features

#endif  // STORAGE_BROWSER_BLOB_FEATURES_H_
