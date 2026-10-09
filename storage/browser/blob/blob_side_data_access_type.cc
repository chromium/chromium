// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_side_data_access_type.h"

#include "base/metrics/histogram_functions.h"

namespace storage {

void RecordBlobSideDataAccess(BlobSideDataAccessType type) {
  base::UmaHistogramEnumeration("Storage.Blob.SideDataAccessType", type);
}

}  // namespace storage
