// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_WRITE_BLOB_TO_FILE_H_
#define STORAGE_BROWSER_BLOB_WRITE_BLOB_TO_FILE_H_

#include <optional>

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "components/services/storage/public/mojom/blob_storage_context.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_entry.h"

namespace storage {

// Writes the blob at |blob_handle| to the file at |file_path|. If a file
// already exists, then it is overwritten. If |flush_on_write| is true, then the
// Flush will be called on the file before it is closed. If |last_modified| is
// populated, then the file's last modified & last accessed time will be set to
// |last_modified|.
// If successful, |callback| is called with the resulting file size. If not,
// then a net error code is used ( < 0).
void WriteBlobToFile(
    std::unique_ptr<BlobDataHandle> blob_handle,
    const base::FilePath& file_path,
    bool flush_on_write,
    std::optional<base::Time> last_modified,
    mojom::BlobStorageContext::WriteBlobToFileCallback callback);

}  // namespace storage

#endif  // STORAGE_BROWSER_BLOB_WRITE_BLOB_TO_FILE_H_
