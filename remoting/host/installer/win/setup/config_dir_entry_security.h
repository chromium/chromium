// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INSTALLER_WIN_SETUP_CONFIG_DIR_ENTRY_SECURITY_H_
#define REMOTING_HOST_INSTALLER_WIN_SETUP_CONFIG_DIR_ENTRY_SECURITY_H_

#include <cstddef>
#include <optional>

#include "base/files/file_path.h"

namespace remoting::installer {

// Inspects, purges, and hardens the direct child entries of `config_dir`.
//
// `canonical_config_dir` must be the caller-verified canonical path of
// `config_dir` in `GetCanonicalPath()` form (built from a trusted root such as
// `%ProgramData%`), used to verify that opened children have not been
// redirected.
//
// Direct children that are unexpected or untrusted (reparse points,
// subdirectories, access-denied entries, multiply hard-linked files, files not
// owned by `LocalSystem` or `Administrators`, and files whose DACL grants
// non-admins write, delete, or permission-editing access or otherwise fails
// safety checks) are removed. Admin/system-owned files whose DACL only grants
// non-admins read/execute access (other than `host_unprivileged.json`, which
// remains readable by unprivileged users) are tightened in place to
// admin/system-only while rejecting concurrent data-write, append, or delete
// handles. Already-safe files are left untouched.
//
// Returns the number of retained files in `config_dir` on success, or
// `std::nullopt` (failing closed) on failure.
//
// Caller preconditions:
// - `config_dir` and any untrusted ancestors must already be verified against
//   `canonical_config_dir` and pinned open without `FILE_SHARE_DELETE` so the
//   directory chain cannot be renamed or swapped mid-purge.
// - `config_dir`'s DACL must already prevent unprivileged principals from
//   creating, renaming, or replacing entries.
// - Performs blocking file I/O and must run on a sequence that allows blocking.
std::optional<size_t> PurgeAndHardenConfigDirEntries(
    const base::FilePath& config_dir,
    const base::FilePath& canonical_config_dir);

}  // namespace remoting::installer

#endif  // REMOTING_HOST_INSTALLER_WIN_SETUP_CONFIG_DIR_ENTRY_SECURITY_H_
