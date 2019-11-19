// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_VIEW_BLOB_INTERNALS_JOB_H_
#define STORAGE_BROWSER_BLOB_VIEW_BLOB_INTERNALS_JOB_H_

#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace storage {

class BlobEntry;
class BlobStorageContext;

// A job subclass that implements a protocol to inspect the internal
// state of blob registry.
class COMPONENT_EXPORT(STORAGE_BROWSER) ViewBlobInternalsJob {
 public:
  static std::string GenerateHTML(BlobStorageContext* blob_storage_context);

 private:
  static void GenerateHTMLForBlobData(const BlobEntry& blob_data,
                                      const std::string& content_type,
                                      const std::string& content_disposition,
                                      size_t refcount,
                                      std::string* out);

  DISALLOW_COPY_AND_ASSIGN(ViewBlobInternalsJob);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_BLOB_VIEW_BLOB_INTERNALS_JOB_H_
