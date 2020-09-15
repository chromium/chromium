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

namespace storage {

// A helper class to perform async file operations in a synchronous way.
class AsyncFileTestHelper {
 public:
  using FileEntryList = FileSystemOperation::FileEntryList;
  using CopyProgressCallback = FileSystemOperation::CopyProgressCallback;

  static const int64_t kDontCheckSize;

  // Performs Copy from |src| to |dest| and returns the status code.
  static base::File::Error Copy(FileSystemContext* context,
                                const FileSystemURL& src,
                                const FileSystemURL& dest);

  // Same as Copy, but this supports |progress_callback|.
  static base::File::Error CopyWithProgress(
      FileSystemContext* context,
      const FileSystemURL& src,
      const FileSystemURL& dest,
      const CopyProgressCallback& progress_callback);

  // Performs Move from |src| to |dest| and returns the status code.
  static base::File::Error Move(FileSystemContext* context,
                                const FileSystemURL& src,
                                const FileSystemURL& dest);

  // Removes the given |url|.
  static base::File::Error Remove(FileSystemContext* context,
                                  const FileSystemURL& url,
                                  bool recursive);

  // Performs ReadDirectory on |url|.
  static base::File::Error ReadDirectory(FileSystemContext* context,
                                         const FileSystemURL& url,
                                         FileEntryList* entries);

  // Creates a directory at |url|.
  static base::File::Error CreateDirectory(FileSystemContext* context,
                                           const FileSystemURL& url);

  // Creates a file at |url|.
  static base::File::Error CreateFile(FileSystemContext* context,
                                      const FileSystemURL& url);

  // Creates a file at |url| and fills with |buf|.
  static base::File::Error CreateFileWithData(FileSystemContext* context,
                                              const FileSystemURL& url,
                                              const char* buf,
                                              int buf_size);

  // Truncates the file |url| to |size|.
  static base::File::Error TruncateFile(FileSystemContext* context,
                                        const FileSystemURL& url,
                                        size_t size);

  // Retrieves File::Info for |url| and populates |file_info|.
  static base::File::Error GetMetadata(FileSystemContext* context,
                                       const FileSystemURL& url,
                                       base::File::Info* file_info);

  // Retrieves FilePath for |url| and populates |platform_path|.
  static base::File::Error GetPlatformPath(FileSystemContext* context,
                                           const FileSystemURL& url,
                                           base::FilePath* platform_path);

  // Returns true if a file exists at |url| with |size|. If |size| is
  // kDontCheckSize it doesn't check the file size (but just check its
  // existence).
  static bool FileExists(FileSystemContext* context,
                         const FileSystemURL& url,
                         int64_t size);

  // Returns true if a directory exists at |url|.
  static bool DirectoryExists(FileSystemContext* context,
                              const FileSystemURL& url);

  // Returns usage and quota. It's valid to pass nullptr to |usage| and/or
  // |quota|.
  static blink::mojom::QuotaStatusCode GetUsageAndQuota(
      QuotaManager* quota_manager,
      const url::Origin& origin,
      FileSystemType type,
      int64_t* usage,
      int64_t* quota);

  // Modifies timestamps of a file or directory at |url| with
  // |last_access_time| and |last_modified_time|. The function DOES NOT
  // create a file unlike 'touch' command on Linux.
  static base::File::Error TouchFile(FileSystemContext* context,
                                     const FileSystemURL& url,
                                     const base::Time& last_access_time,
                                     const base::Time& last_modified_time);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_TEST_ASYNC_FILE_TEST_HELPER_H_
