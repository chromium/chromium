// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_TEST_ASYNC_FILE_TEST_HELPER_H_
#define STORAGE_BROWSER_TEST_ASYNC_FILE_TEST_HELPER_H_

#include <stddef.h>
#include <stdint.h>

#include "storage/browser/file_system/file_system_operation.h"
#include "storage/common/file_system/file_system_types.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace storage {
class FileSystemContext;
class FileSystemURL;
class QuotaManager;
}

namespace url {
class Origin;
}

namespace content {

// A helper class to perform async file operations in a synchronous way.
class AsyncFileTestHelper {
 public:
  using FileEntryList = storage::FileSystemOperation::FileEntryList;
  using CopyProgressCallback =
      storage::FileSystemOperation::CopyProgressCallback;

  static const int64_t kDontCheckSize;

  // Performs Copy from |src| to |dest| and returns the status code.
  static base::File::Error Copy(storage::FileSystemContext* context,
                                const storage::FileSystemURL& src,
                                const storage::FileSystemURL& dest);

  // Same as Copy, but this supports |progress_callback|.
  static base::File::Error CopyWithProgress(
      storage::FileSystemContext* context,
      const storage::FileSystemURL& src,
      const storage::FileSystemURL& dest,
      const CopyProgressCallback& progress_callback);

  // Performs Move from |src| to |dest| and returns the status code.
  static base::File::Error Move(storage::FileSystemContext* context,
                                const storage::FileSystemURL& src,
                                const storage::FileSystemURL& dest);

  // Removes the given |url|.
  static base::File::Error Remove(storage::FileSystemContext* context,
                                  const storage::FileSystemURL& url,
                                  bool recursive);

  // Performs ReadDirectory on |url|.
  static base::File::Error ReadDirectory(storage::FileSystemContext* context,
                                         const storage::FileSystemURL& url,
                                         FileEntryList* entries);

  // Creates a directory at |url|.
  static base::File::Error CreateDirectory(storage::FileSystemContext* context,
                                           const storage::FileSystemURL& url);

  // Creates a file at |url|.
  static base::File::Error CreateFile(storage::FileSystemContext* context,
                                      const storage::FileSystemURL& url);

  // Creates a file at |url| and fills with |buf|.
  static base::File::Error CreateFileWithData(
      storage::FileSystemContext* context,
      const storage::FileSystemURL& url,
      const char* buf,
      int buf_size);

  // Truncates the file |url| to |size|.
  static base::File::Error TruncateFile(storage::FileSystemContext* context,
                                        const storage::FileSystemURL& url,
                                        size_t size);

  // Retrieves File::Info for |url| and populates |file_info|.
  static base::File::Error GetMetadata(storage::FileSystemContext* context,
                                       const storage::FileSystemURL& url,
                                       base::File::Info* file_info);

  // Retrieves FilePath for |url| and populates |platform_path|.
  static base::File::Error GetPlatformPath(storage::FileSystemContext* context,
                                           const storage::FileSystemURL& url,
                                           base::FilePath* platform_path);

  // Returns true if a file exists at |url| with |size|. If |size| is
  // kDontCheckSize it doesn't check the file size (but just check its
  // existence).
  static bool FileExists(storage::FileSystemContext* context,
                         const storage::FileSystemURL& url,
                         int64_t size);

  // Returns true if a directory exists at |url|.
  static bool DirectoryExists(storage::FileSystemContext* context,
                              const storage::FileSystemURL& url);

  // Returns usage and quota. It's valid to pass nullptr to |usage| and/or
  // |quota|.
  static blink::mojom::QuotaStatusCode GetUsageAndQuota(
      storage::QuotaManager* quota_manager,
      const url::Origin& origin,
      storage::FileSystemType type,
      int64_t* usage,
      int64_t* quota);
};

}  // namespace content

#endif  // STORAGE_BROWSER_TEST_ASYNC_FILE_TEST_HELPER_H_
