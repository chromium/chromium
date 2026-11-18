// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_FILE_SECURITY_UTIL_H_
#define REMOTING_HOST_WIN_FILE_SECURITY_UTIL_H_

// Hardened Windows filesystem access helpers for privileged host components:
// opening entries without following a reparse point at the final path
// component, canonical path verification, security descriptor inspection, and
// verified non-propagating security descriptor writes.
//
// Caller preconditions:
// - These helpers verify that the object they open matches an expected
//   canonical path (which rejects junctions or symlinks anywhere in the
//   resolved path at open time), and then act on that same handle. They do not
//   pin ancestors. Callers that need the parent chain to stay stable after
//   verification must hold handles to every ancestor that omit
//   `FILE_SHARE_DELETE` for the duration of the operation.
// - Every function except `ToExtendedLengthPath()` (which only manipulates
//   strings) performs blocking file I/O, asserts this via
//   `base::ScopedBlockingCall`, and must be called on a sequence that allows
//   blocking.
// - Unless documented otherwise, failures are logged and reported to the
//   caller only as `false` / `std::nullopt`, without distinguishing the cause.

#include <windows.h>

#include <optional>

#include "base/files/file_path.h"
#include "base/types/expected.h"
#include "base/win/scoped_handle.h"

namespace base::win {
class SecurityDescriptor;
}  // namespace base::win

namespace remoting {

// Converts an absolute, normalized `path` to its `\\?\` (or `\\?\UNC\`)
// extended-length form so Win32 path normalization does not strip trailing
// dots/spaces from names and `MAX_PATH` does not apply. Returns `std::nullopt`
// (fail closed) for relative paths, `\\.\` device paths, UNC paths without a
// server name, and paths with `.` or `..` components, since those cannot be
// safely expressed in extended-length form. Other components made only of dots
// or spaces (e.g. `...`) are literal names in extended-length form and are
// allowed. A `path` that is already in extended-length form is accepted (and
// returned with its separators normalized) only if it is a `\\?\X:\`,
// `\\?\UNC\server\`, or `\\?\Volume{GUID}\` path without `.` or `..`
// components; other `\\?\` paths (e.g. `\\?\pipe\`, `\\?\PhysicalDrive0`, or
// `\\?\GLOBALROOT\`) return `std::nullopt`.
std::optional<base::FilePath> ToExtendedLengthPath(const base::FilePath& path);

// Returns the normalized canonical path of `handle` via
// `GetFinalPathNameByHandleW`. Uses `VOLUME_NAME_DOS` (with the `\\?\` prefix
// removed) when the volume has a drive letter or mount point, and falls back
// to `VOLUME_NAME_GUID`, which keeps the `\\?\Volume{...}\` prefix. Callers
// comparing against an expected path for a volume without a drive letter must
// use the same `\\?\Volume{...}\` form. Returns `std::nullopt` (and logs) on
// failure.
std::optional<base::FilePath> GetCanonicalPath(HANDLE handle);

// Returns true if `handle`'s normalized canonical path matches
// `expected_canonical` (expanding 8.3 short names in `expected_canonical`
// without resolving junctions or symlinks).
bool IsExpectedCanonicalPath(HANDLE handle,
                             const base::FilePath& expected_canonical);

// Policy for `InspectFileSecurityDescriptor()`.
struct SecurityDescriptorPolicy {
  // Treat `TrustedInstaller` as a trusted principal (in addition to
  // `LocalSystem` and `Administrators`).
  bool allow_trusted_installer = false;
  // Ignore inherit-only allow ACEs for `CREATOR OWNER`.
  bool allow_inherit_only_creator_owner = false;
  // Rights that non-trusted principals may be granted.
  ACCESS_MASK allowed_non_admin_mask = 0;
};

struct SecurityDescriptorInspection {
  // True when Owner is `LocalSystem`, `Administrators`, or (if allowed)
  // `TrustedInstaller`.
  bool trusted_owner = false;
  // True when the DACL is present/non-empty, contains only access-allowed ACEs
  // (any deny ACE or other ACE type makes it unsafe), grants both
  // `LocalSystem` and `Administrators` read access, and grants non-trusted
  // principals no rights outside `allowed_non_admin_mask`.
  bool safe_dacl = false;
};

// Inspects `handle`'s Owner and DACL according to `policy`. `handle` must have
// been opened with `READ_CONTROL`. Returns `std::nullopt` (and logs) if the
// security descriptor cannot be read or has no owner.
std::optional<SecurityDescriptorInspection> InspectFileSecurityDescriptor(
    HANDLE handle,
    const SecurityDescriptorPolicy& policy);

// Controls whether `OpenEntryNoFollow()` may retry with elevated privileges.
enum class PrivilegedRetry {
  // Only the caller's normal access rights are used.
  kDisabled,
  // If the open fails with `ERROR_ACCESS_DENIED`, retry once with
  // `SeBackupPrivilege` and `SeRestorePrivilege` enabled on a thread-scoped
  // impersonation token (the process token is never modified). Combined with
  // `FILE_FLAG_BACKUP_SEMANTICS`, this bypasses the DACL for every requested
  // right, including data and delete access, so only use it for remediation of
  // entries that may deny Administrators. The retry is skipped (and the
  // original `ERROR_ACCESS_DENIED` returned) if the calling thread is already
  // impersonating.
  kBackupRestore,
};

// Opens `path` (in extended-length form) with `FILE_FLAG_OPEN_REPARSE_POINT |
// FILE_FLAG_BACKUP_SEMANTICS`, so a reparse point at the final path component
// is opened itself rather than followed. Reparse points in intermediate path
// components are still followed, so callers must verify the returned handle
// (e.g. with `IsExpectedCanonicalPath()`, which rejects such redirection, and
// its attributes) before acting on it. The open uses `SECURITY_IDENTIFICATION`
// quality of service, and handles that do not refer to a disk file or
// directory are rejected. Returns the handle, or the Win32 error on failure
// (`ERROR_INVALID_NAME` if `path` cannot be converted to extended-length form,
// `ERROR_BAD_FILE_TYPE` if the opened object is not a disk file or directory).
base::expected<base::win::ScopedHandle, DWORD> OpenEntryNoFollow(
    const base::FilePath& path,
    DWORD desired_access,
    DWORD share_mode,
    PrivilegedRetry privileged_retry);

// Writes the owner, group, and DACL of `sd` to `handle` (which must have
// `WRITE_DAC | WRITE_OWNER` access) via `SetKernelObjectSecurity`, without
// propagating inheritable ACEs to existing children. The DACL is always
// written as protected (`SE_DACL_PROTECTED`), regardless of whether `sd` sets
// it, so ACEs are never inherited from the parent. Fails closed (returns false
// without writing) unless `sd` has an owner, a group, and a non-NULL DACL.
//
// The write uses the calling thread's token without enabling
// `SeRestorePrivilege`, so the owner in `sd` must be one the token may assign:
// the token user, or a group in the token with `SE_GROUP_OWNER`. For example,
// an elevated administrator may pass `BA`, and `LocalSystem` may pass `SY` or
// `BA`. Other owners fail with `ERROR_INVALID_OWNER`.
bool WriteNonPropagatingSecurityDescriptor(
    HANDLE handle,
    const base::win::SecurityDescriptor& sd);

// Opens `path` via `OpenEntryNoFollow()` (with
// `PrivilegedRetry::kBackupRestore`) and `share_mode`, verifies on that same
// handle that it is not a reparse point, is not a multiply hard-linked file,
// and matches `expected_canonical_path`, and then applies `sd` via
// `WriteNonPropagatingSecurityDescriptor()` (with the same owner
// restrictions). Returns false (and logs the cause) if any step fails.
bool ApplyNonPropagatingSdToVerifiedPath(
    const base::FilePath& path,
    const base::FilePath& expected_canonical_path,
    DWORD share_mode,
    const base::win::SecurityDescriptor& sd);

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_FILE_SECURITY_UTIL_H_
