// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_SQLITE_SANDBOXED_VFS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_SQLITE_SANDBOXED_VFS_H_

#include <tuple>

#include "base/files/file.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

#include "third_party/sqlite/sqlite3.h"

namespace blink {

class Platform;

// SQLite VFS implementation that works in the Chrome renderer sandbox.
//
// Instances are thread-friendly, and expected to be used on Blink's database
// thread.
class SandboxedVfs {
  USING_FAST_MALLOC(SandboxedVfs);

 public:
  // Factory method for the singleton instance.
  static SandboxedVfs& GetInstance();

  ~SandboxedVfs() = delete;

  // Opens a database file.
  //
  // Returns a SQLite status and a SQLite connection. If the status is not
  // SQLITE_OK, the returned connection is null.
  std::tuple<int, sqlite3*> OpenDatabase(const String& filename);

  // sqlite3_vfs implementation.
  int Open(const char* full_path,
           sqlite3_file* result_file,
           int requested_flags,
           int* granted_flags);
  int Delete(const char* full_path, int sync_dir);
  int Access(const char* full_path, int flags, int* result);
  int FullPathname(const char* file_path, int result_size, char* result);
  int Randomness(int result_size, char* result);
  int Sleep(int microseconds);
  int GetLastError(int message_size, char* message) const;
  int CurrentTimeInt64(sqlite3_int64* result_ms);

  // Used by SandboxedVfsFile.
  Platform* GetPlatform() { return platform_; }
  void SetLastError(base::File::Error error) { this->last_error_ = error; }

 private:
  // Use GetInstance() instead of constructing this directly.
  SandboxedVfs();

  // Registers the VFS with SQLite. Failures are silently ignored.
  void RegisterVfs();

  sqlite3_vfs sandboxed_vfs_;
  const base::Time sqlite_epoch_;
  Platform* const platform_;
  base::File::Error last_error_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_SQLITE_SANDBOXED_VFS_H_
