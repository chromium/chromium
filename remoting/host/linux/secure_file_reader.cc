// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/secure_file_reader.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/types/expected.h"
#include "remoting/base/loggable.h"

namespace remoting {

namespace {

Loggable ErrnoLoggable(const base::Location& from_here,
                       std::string_view operation,
                       const base::FilePath& path,
                       int error) {
  if (error == ELOOP) {
    return Loggable(from_here, base::StrCat({"'", path.value(),
                                             "' is a symbolic link, which is "
                                             "not allowed."}));
  }
  return Loggable(
      from_here,
      base::StrCat({"Failed to ", operation, " '", path.value(),
                    "': ", logging::SystemErrorCodeToString(error)}));
}

// Checks that `stat_buf`, describing `path`, is owned by `expected_owner_uid`
// and isn't writable by group or others.
base::expected<void, Loggable> CheckOwnerAndMode(const struct stat& stat_buf,
                                                 const base::FilePath& path,
                                                 uid_t expected_owner_uid) {
  if (stat_buf.st_uid != expected_owner_uid) {
    return base::unexpected(Loggable(
        FROM_HERE,
        base::StrCat({"'", path.value(), "' has owner UID ",
                      base::NumberToString(stat_buf.st_uid), " (expected ",
                      base::NumberToString(expected_owner_uid), ")."})));
  }
  if ((stat_buf.st_mode & (S_IWGRP | S_IWOTH)) != 0) {
    return base::unexpected(Loggable(
        FROM_HERE,
        base::StrCat(
            {"'", path.value(), "' is writable by group or others (mode: ",
             base::StringPrintf("0%o", stat_buf.st_mode & 07777), ")."})));
  }
  return base::ok();
}

}  // namespace

base::expected<std::optional<std::string>, Loggable> ReadSecureFile(
    const base::FilePath& path,
    uid_t expected_owner_uid,
    size_t max_size_bytes) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  base::FilePath directory = path.DirName();
  base::ScopedFD directory_fd(
      HANDLE_EINTR(open(directory.value().c_str(),
                        O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC)));
  if (!directory_fd.is_valid()) {
    int error = errno;
    if (error == ENOENT) {
      return base::ok(std::nullopt);
    }
    // With O_DIRECTORY, a symlink fails with ENOTDIR rather than ELOOP.
    struct stat link_stat;
    if (error == ENOTDIR && lstat(directory.value().c_str(), &link_stat) == 0 &&
        S_ISLNK(link_stat.st_mode)) {
      error = ELOOP;
    }
    return base::unexpected(
        ErrnoLoggable(FROM_HERE, "open directory", directory, error));
  }
  struct stat directory_stat;
  if (fstat(directory_fd.get(), &directory_stat) != 0) {
    return base::unexpected(ErrnoLoggable(FROM_HERE, "stat", directory, errno));
  }
  auto directory_check =
      CheckOwnerAndMode(directory_stat, directory, expected_owner_uid);
  if (!directory_check.has_value()) {
    return base::unexpected(std::move(directory_check).error());
  }

  // O_NONBLOCK prevents blocking if the path is a FIFO, which is then rejected
  // by the S_ISREG check below. It has no effect on reading regular files.
  base::ScopedFD file_fd(
      HANDLE_EINTR(openat(directory_fd.get(), path.BaseName().value().c_str(),
                          O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC)));
  if (!file_fd.is_valid()) {
    int error = errno;
    if (error == ENOENT) {
      return base::ok(std::nullopt);
    }
    return base::unexpected(ErrnoLoggable(FROM_HERE, "open", path, error));
  }
  struct stat file_stat;
  if (fstat(file_fd.get(), &file_stat) != 0) {
    return base::unexpected(ErrnoLoggable(FROM_HERE, "stat", path, errno));
  }
  if (!S_ISREG(file_stat.st_mode)) {
    return base::unexpected(Loggable(
        FROM_HERE,
        base::StrCat({"'", path.value(), "' is not a regular file."})));
  }
  auto file_check = CheckOwnerAndMode(file_stat, path, expected_owner_uid);
  if (!file_check.has_value()) {
    return base::unexpected(std::move(file_check).error());
  }
  if (file_stat.st_size > static_cast<off_t>(max_size_bytes)) {
    return base::unexpected(Loggable(
        FROM_HERE,
        base::StrCat({"'", path.value(), "' exceeds the size limit (",
                      base::NumberToString(file_stat.st_size), " > ",
                      base::NumberToString(max_size_bytes), " bytes)."})));
  }

  base::ScopedFILE file(fdopen(file_fd.get(), "r"));
  if (!file) {
    return base::unexpected(ErrnoLoggable(FROM_HERE, "fdopen", path, errno));
  }
  // `file` now owns the file descriptor.
  std::ignore = file_fd.release();

  // The size limit is enforced again here, in case the file has grown since it
  // was checked above.
  std::string content;
  if (!base::ReadStreamToStringWithMaxSize(file.get(), max_size_bytes,
                                           &content)) {
    return base::unexpected(Loggable(
        FROM_HERE, base::StrCat({"Failed to read '", path.value(), "'."})));
  }
  return base::ok(std::move(content));
}

}  // namespace remoting
