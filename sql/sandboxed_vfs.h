// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_SANDBOXED_VFS_H_
#define SQL_SANDBOXED_VFS_H_

#include <memory>
#include <optional>

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/time/time.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql {

// SQLite VFS file implementation that works in a sandboxed process.
//
// Instances are thread-friendly.
class COMPONENT_EXPORT(SQL) SandboxedVfs {
 public:
  // Describes access rights for a path, used by Delegate::GetPathAccess below.
  struct PathAccessInfo {
    bool can_read = false;
    bool can_write = false;
  };

  // Environment-specific SandboxedVfs implementation details.
  //
  // This abstracts a handful of operations that don't typically work in a
  // sandbox environment given a typical naive implementation. Instances must be
  // thread-safe.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Opens a file.
    //
    // `file_path` is the parsed version of a path passed by SQLite to Open().
    // `sqlite_requested_flags` is a bitwise combination SQLite flags used when
    // opening files. Returns the opened File on success, or an invalid File on
    // failure.
    virtual base::File OpenFile(const base::FilePath& file_path,
                                int sqlite_requested_flags) = 0;

    // Deletes a file.
    //
    // `file_path` is the parsed version of a path passed by SQLite to Delete().
    // If `sync_dir` is true, the implementation should attempt to flush to disk
    // the changes to the file's directory, to ensure that the deletion is
    // reflected after a power failure. Returns an SQLite error code indicating
    // the status of the operation.
    virtual int DeleteFile(const base::FilePath& file_path, bool sync_dir) = 0;

    // Queries path access information for `file_path`. Returns null if the
    // given path does not exist.
    virtual std::optional<PathAccessInfo> GetPathAccess(
        const base::FilePath& file_path) = 0;
  };

  // We don't allow SandboxedVfs instances to be destroyed. Once created, they
  // are permanently registered in the calling process.
  ~SandboxedVfs() = delete;

  // Constructs a new instance of ths object using `delegate` to support various
  // operations from within the sandbox. The VFS is registered with SQLite under
  // `name` and if `make_default` is true then the VFS is also set as the global
  // default for new database instances within the calling process.
  //
  // Note that `name` must be globally unique to the calling process.
  static void Register(const char* name,
                       std::unique_ptr<Delegate> delegate,
                       bool make_default);

  Delegate* delegate() const { return delegate_.get(); }

  // sqlite3_vfs implementation.
  int Open(const char* full_path,
           sqlite3_file& result_file,
           int requested_flags,
           int* granted_flags);
  int Delete(const char* full_path, int sync_dir);
  int Access(const char* full_path, int flags, int& result);
  int FullPathname(const char* file_path, int result_size, char* result);
  int Randomness(int result_size, char* result);
  int Sleep(int microseconds);
  int GetLastError(int message_size, char* message) const;
  int CurrentTimeInt64(sqlite3_int64* result_ms);

  // Used by SandboxedVfsFile.
  void SetLastError(base::File::Error error) { this->last_error_ = error; }

 private:
  SandboxedVfs(const char* name,
               std::unique_ptr<Delegate> delegate,
               bool make_default);

  sqlite3_vfs sandboxed_vfs_;
  const base::Time sqlite_epoch_;
  const std::unique_ptr<Delegate> delegate_;
  base::File::Error last_error_;
};

}  // namespace sql

#endif  // SQL_SANDBOXED_VFS_H_
