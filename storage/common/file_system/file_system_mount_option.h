// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_COMMON_FILE_SYSTEM_FILE_SYSTEM_MOUNT_OPTION_H_
#define STORAGE_COMMON_FILE_SYSTEM_FILE_SYSTEM_MOUNT_OPTION_H_

namespace storage {

// Option for specifying if flush or disk sync operation is wanted after
// writing.
enum class FlushPolicy {
  // No flushing is required after a writing operation is completed.
  FLUSH_ON_COMPLETION,

  // Flushing is required in order to commit written data. Note, that syncing
  // is only invoked via FileStreamWriter::Flush() and via base::File::Flush()
  // for native files. Hence, syncing will not be performed for copying within
  // non-native file systems as well as for non-native copies performed with
  // snapshots.
  NO_FLUSH_ON_COMPLETION
};

// Conveys options for a mounted file systems.
class FileSystemMountOption {
 public:
  // Constructs with the default options.
  FileSystemMountOption()
      : flush_policy_(FlushPolicy::NO_FLUSH_ON_COMPLETION) {}

  // Constructs with the specified component.
  explicit FileSystemMountOption(FlushPolicy flush_policy)
      : flush_policy_(flush_policy) {}

  FlushPolicy flush_policy() const { return flush_policy_; }

 private:
  FlushPolicy flush_policy_;
};

}  // namespace storage

#endif  // STORAGE_COMMON_FILE_SYSTEM_FILE_SYSTEM_MOUNT_OPTION_H_
