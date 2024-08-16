// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webdatabase/sqlite/sandboxed_vfs_delegate.h"

#include <tuple>

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/modules/webdatabase/web_database_host.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace blink {

namespace {

// Converts a SQLite full file path to a Blink string.
//
// The argument is guaranteed to be the result of a FullPathname() call, with
// an optional suffix. The suffix always starts with "-".
String StringFromFullPath(const base::FilePath& file_path) {
  return String::FromUTF8(file_path.AsUTF8Unsafe());
}

}  // namespace

SandboxedVfsDelegate::SandboxedVfsDelegate() = default;

SandboxedVfsDelegate::~SandboxedVfsDelegate() = default;

base::File SandboxedVfsDelegate::OpenFile(const base::FilePath& file_path,
                                          int sqlite_requested_flags) {
  DCHECK(!file_path.empty())
      << "WebSQL does not support creating temporary file names";
  DCHECK_EQ(0, sqlite_requested_flags & SQLITE_OPEN_DELETEONCLOSE)
      << "SQLITE_OPEN_DELETEONCLOSE should not be used by WebSQL";
  DCHECK_EQ(0, sqlite_requested_flags & SQLITE_OPEN_EXCLUSIVE)
      << "SQLITE_OPEN_EXCLUSIVE should not be used by WebSQL";

  String file_name = StringFromFullPath(file_path);
  return WebDatabaseHost::GetInstance().OpenFile(file_name,
                                                 sqlite_requested_flags);
}

int SandboxedVfsDelegate::DeleteFile(const base::FilePath& file_path,
                                     bool sync_dir) {
  return WebDatabaseHost::GetInstance().DeleteFile(
      StringFromFullPath(file_path), sync_dir);
}

std::optional<sql::SandboxedVfs::PathAccessInfo>
SandboxedVfsDelegate::GetPathAccess(const base::FilePath& file_path) {
  int32_t attributes = WebDatabaseHost::GetInstance().GetFileAttributes(
      StringFromFullPath(file_path));

  // TODO(pwnall): Make the mojo interface portable across OSes, instead of
  //               messing around with OS-dependent constants here.

#if BUILDFLAG(IS_WIN)
  const bool file_exists =
      static_cast<DWORD>(attributes) != INVALID_FILE_ATTRIBUTES;
#else
  const bool file_exists = attributes >= 0;
#endif  // BUILDFLAG(IS_WIN)

  if (!file_exists)
    return std::nullopt;

  sql::SandboxedVfs::PathAccessInfo access;
#if BUILDFLAG(IS_WIN)
  access.can_read = true;
  access.can_write = (attributes & FILE_ATTRIBUTE_READONLY) == 0;
#else
  access.can_read = (attributes & R_OK) != 0;
  access.can_write = (attributes & W_OK) != 0;
#endif  // BUILDFLAG(IS_WIN)
  return access;
}

}  // namespace blink
