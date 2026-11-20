// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INSTALLER_WIN_SETUP_SECURE_DELETE_H_
#define REMOTING_HOST_INSTALLER_WIN_SETUP_SECURE_DELETE_H_

#include "base/files/file_path.h"

namespace base::win {
class SecurityDescriptor;
}  // namespace base::win

namespace remoting::installer {

// Deletes an untrusted file, reparse point, or directory subtree at `path`
// without following junctions/symlinks or exposing TOCTOU directory-swap
// windows. Returns true if `path` does not exist. All checks and modifications
// for an entry are done on a single handle opened via `OpenEntryNoFollow()`
// (with `PrivilegedRetry::kBackupRestore`), verified against its expected
// canonical path before any modification. A share-all retry on
// `ERROR_SHARING_VIOLATION` is only accepted for reparse points; regular files
// and non-reparse directories held open with conflicting access fail closed.
// Non-reparse subdirectories are held open with `FILE_SHARE_READ` and locked to
// `admin_only_sd` across child enumeration; files (including hard links) are
// unlinked without modifying their security descriptor. Entries are unlinked
// via POSIX disposition (`FileDispositionInfoEx`), using extended-length paths.
//
// Fails closed if `path` cannot be expressed in extended-length form, if an
// entry cannot be safely unlinked, or if directory recursion exceeds the
// internal depth limit (32 levels; a deeper nested directory chain causes setup
// to fail closed, similar to a file held open without `FILE_SHARE_DELETE`).
// Traversal stops at the first failing child, leaving remaining siblings
// untouched. Security descriptors applied to visited directories are not rolled
// back on failure, so a failed call may leave a partially deleted subtree whose
// visited directories have the owner and DACL of `admin_only_sd`; callers must
// not treat a directory's owner/DACL as a trust signal after a failed call.
//
// See the caller preconditions in `remoting/host/win/file_security_util.h`:
// ancestors of `path` must be pinned by the caller, and this must run on a
// sequence that allows blocking.
bool DeleteUntrustedEntrySafely(
    const base::FilePath& path,
    const base::FilePath& expected_canonical_path,
    const base::win::SecurityDescriptor& admin_only_sd);

}  // namespace remoting::installer

#endif  // REMOTING_HOST_INSTALLER_WIN_SETUP_SECURE_DELETE_H_
