// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_SANDBOX_DIRECTORY_DATABASE_H_
#define STORAGE_BROWSER_FILE_SYSTEM_SANDBOX_DIRECTORY_DATABASE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/time/time.h"

namespace base {
class Location;
}

namespace leveldb {
class DB;
class Env;
class Status;
class WriteBatch;
}  // namespace leveldb

namespace storage {

// This class WILL NOT protect you against producing directory loops, giving an
// empty directory a backing data file, giving two files the same backing file,
// or pointing to a nonexistent backing file.  It does no file IO other than
// that involved with talking to its underlying database.  It does not create or
// in any way touch real files; it only creates path entries in its database.

// TODO(ericu): Safe mode, which does more checks such as the above on debug
// builds.
// TODO(ericu): Add a method that will give a unique filename for a data file.
class COMPONENT_EXPORT(STORAGE_BROWSER) SandboxDirectoryDatabase {
 public:
  using FileId = int64_t;

  struct COMPONENT_EXPORT(STORAGE_BROWSER) FileInfo {
    FileInfo();
    ~FileInfo();

    bool is_directory() const { return data_path.empty(); }

    FileId parent_id;
    base::FilePath data_path;
    base::FilePath::StringType name;
    // This modification time is valid only for directories, not files, as
    // FileWriter will get the files out of sync.
    // For files, look at the modification time of the underlying data_path.
    base::Time modification_time;
  };

  SandboxDirectoryDatabase(const base::FilePath& filesystem_data_directory,
                           leveldb::Env* env_override);
  ~SandboxDirectoryDatabase();

  bool GetChildWithName(FileId parent_id,
                        const base::FilePath::StringType& name,
                        FileId* child_id);
  bool GetFileWithPath(const base::FilePath& path, FileId* file_id);
  // ListChildren will succeed, returning 0 children, if parent_id doesn't
  // exist.
  bool ListChildren(FileId parent_id, std::vector<FileId>* children);
  bool GetFileInfo(FileId file_id, FileInfo* info);
  base::File::Error AddFileInfo(const FileInfo& info, FileId* file_id);
  bool RemoveFileInfo(FileId file_id);
  // This does a full update of the FileInfo, and is what you'd use for moves
  // and renames.  If you just want to update the modification_time, use
  // UpdateModificationTime.
  bool UpdateFileInfo(FileId file_id, const FileInfo& info);
  bool UpdateModificationTime(FileId file_id,
                              const base::Time& modification_time);
  // This is used for an overwriting move of a file [not a directory] on top of
  // another file [also not a directory]; we need to alter two files' info in a
  // single transaction to avoid weird backing file references in the event of a
  // partial failure.
  bool OverwritingMoveFile(FileId src_file_id, FileId dest_file_id);

  // This produces the series 0, 1, 2..., starting at 0 when the underlying
  // filesystem is first created, and maintaining state across
  // creation/destruction of SandboxDirectoryDatabase objects.
  bool GetNextInteger(int64_t* next);

  bool IsDirectory(FileId file_id);

  // Returns true if the database looks consistent with local filesystem.
  bool IsFileSystemConsistent();

  // Completely deletes contents of the database.
  bool DestroyDatabase();

 private:
  enum RecoveryOption {
    DELETE_ON_CORRUPTION,
    REPAIR_ON_CORRUPTION,
    FAIL_ON_CORRUPTION,
  };

  friend class ObfuscatedFileUtil;
  friend class SandboxDirectoryDatabaseTest;

  bool Init(RecoveryOption recovery_option);
  bool RepairDatabase(const std::string& db_path);
  void ReportInitStatus(const leveldb::Status& status);
  bool StoreDefaultValues();
  bool GetLastFileId(FileId* file_id);
  bool AddFileInfoHelper(const FileInfo& info,
                         FileId file_id,
                         leveldb::WriteBatch* batch);
  bool RemoveFileInfoHelper(FileId file_id, leveldb::WriteBatch* batch);
  // Close the database. Before this, all iterators associated with the database
  // must be deleted.
  void HandleError(const base::Location& from_here,
                   const leveldb::Status& status);

  const base::FilePath filesystem_data_directory_;
  leveldb::Env* env_override_;
  std::unique_ptr<leveldb::DB> db_;
  base::Time last_reported_time_;
  DISALLOW_COPY_AND_ASSIGN(SandboxDirectoryDatabase);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_SANDBOX_DIRECTORY_DATABASE_H_
