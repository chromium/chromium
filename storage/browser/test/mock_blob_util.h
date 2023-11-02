// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_TEST_MOCK_BLOB_UTIL_H_
#define STORAGE_BROWSER_TEST_MOCK_BLOB_UTIL_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"

namespace storage {

class BlobDataHandle;
class BlobStorageContext;

class ScopedTextBlob {
 public:
  // Registers a blob with the given |id| that contains |data|.
  ScopedTextBlob(BlobStorageContext* context,
                 const std::string& blob_id,
                 const std::string& data);

  ScopedTextBlob(const ScopedTextBlob&) = delete;
  ScopedTextBlob& operator=(const ScopedTextBlob&) = delete;

  ~ScopedTextBlob();

  // Returns a BlobDataHandle referring to the scoped blob.
  std::unique_ptr<BlobDataHandle> GetBlobDataHandle();

 private:
  const std::string blob_id_;
  raw_ptr<BlobStorageContext> context_;
  std::unique_ptr<BlobDataHandle> handle_;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_TEST_MOCK_BLOB_UTIL_H_
