// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_SECURE_FILE_READER_H_
#define REMOTING_HOST_LINUX_SECURE_FILE_READER_H_

#include <stddef.h>
#include <sys/types.h>

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/types/expected.h"
#include "remoting/base/loggable.h"

namespace remoting {

// Reads the file at `path`, which privileged processes rely on (e.g. a config
// or state file that only root may write), after checking that it and its
// parent directory are secure:
// - Neither the file nor its parent directory is a symlink. The parent
//   directory is opened with O_NOFOLLOW, and the file is opened relative to
//   it, so the checked directory is the one that the file is read from. Only
//   the immediate parent directory is checked; its ancestors may be symlinks
//   and aren't checked, so `path` should be in a trusted hierarchy (e.g. /etc
//   or /run).
// - Both are owned by `expected_owner_uid` and aren't writable by group or
//   others.
// - The file is a regular file of at most `max_size_bytes`.
// Returns std::nullopt if the parent directory or the file doesn't exist.
// Returns an error if any of the checks fail (including when the parent
// directory is insecure but the file doesn't exist), or if the file can't be
// read. Performs blocking file I/O.
base::expected<std::optional<std::string>, Loggable> ReadSecureFile(
    const base::FilePath& path,
    uid_t expected_owner_uid,
    size_t max_size_bytes);

// Checks that `path` is a directory that isn't a symlink, is owned by
// `expected_owner_uid`, and isn't writable by group or others, i.e. the same
// checks that ReadSecureFile() applies to the parent directory. Useful before
// writing a file into the directory. Only `path` itself is checked; its
// ancestors are trusted. Returns an error if the directory doesn't exist or
// any of the checks fail. Performs blocking file I/O.
base::expected<void, Loggable> CheckSecureDirectory(const base::FilePath& path,
                                                    uid_t expected_owner_uid);

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_SECURE_FILE_READER_H_
