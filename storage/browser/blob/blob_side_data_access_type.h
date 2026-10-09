// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_SIDE_DATA_ACCESS_TYPE_H_
#define STORAGE_BROWSER_BLOB_BLOB_SIDE_DATA_ACCESS_TYPE_H_

#include "base/component_export.h"

namespace storage {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(BlobSideDataAccessType)
enum class BlobSideDataAccessType {
  kBlobURLWithoutSideData = 0,
  kBlobURLWithSideData = 1,
  kCacheStorageSideDataRead = 2,
  kMaxValue = kCacheStorageSideDataRead,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/storage/enums.xml:BlobSideDataAccessType)

COMPONENT_EXPORT(STORAGE_BROWSER)
void RecordBlobSideDataAccess(BlobSideDataAccessType type);

}  // namespace storage

#endif  // STORAGE_BROWSER_BLOB_BLOB_SIDE_DATA_ACCESS_TYPE_H_
