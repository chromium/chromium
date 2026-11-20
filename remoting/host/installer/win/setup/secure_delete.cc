// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/installer/win/setup/secure_delete.h"

#include <windows.h>

#include <optional>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/types/expected.h"
#include "base/win/scoped_handle.h"
#include "base/win/security_descriptor.h"
#include "remoting/host/win/file_security_util.h"

namespace remoting::installer {

namespace {

// Bounds call-stack depth (and open directory handles) during recursive
// deletion. Legitimate host configuration trees are flat, so 32 levels is well
// above expected use while preventing stack exhaustion on a deeply nested
// untrusted tree.
constexpr int kMaxDeleteRecursionDepth = 32;

constexpr DWORD kShareAll =
    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;

bool IsNotFoundError(DWORD err) {
  return err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND;
}

bool IsDispositionFlagUnsupported(DWORD err) {
  return err == ERROR_INVALID_PARAMETER || err == ERROR_NOT_SUPPORTED ||
         err == ERROR_INVALID_FUNCTION;
}

bool ClearReadOnlyAttribute(HANDLE handle,
                            DWORD current_attrs,
                            const base::FilePath& path) {
  if ((current_attrs & FILE_ATTRIBUTE_READONLY) == 0) {
    return true;
  }
  FILE_BASIC_INFO basic_info = {};
  basic_info.FileAttributes = current_attrs & ~DWORD{FILE_ATTRIBUTE_READONLY};
  if (basic_info.FileAttributes == 0) {
    basic_info.FileAttributes = FILE_ATTRIBUTE_NORMAL;
  }
  if (!::SetFileInformationByHandle(handle, FileBasicInfo, &basic_info,
                                    sizeof(basic_info))) {
    PLOG(ERROR) << "Failed to clear read-only attribute on " << path.value();
    return false;
  }
  return true;
}

// Marks `handle` for deletion and closes it, then verifies `path` is gone.
bool UnlinkHandleAndVerifyRemoved(base::win::ScopedHandle handle,
                                  DWORD current_attrs,
                                  const base::FilePath& path) {
  // Request POSIX unlink semantics so the directory entry is unlinked
  // immediately when `handle` closes even if another process holds a read-only
  // handle (or `FilePathReparsePoint` holds a junction handle in tests), rather
  // than lingering in `DELETE_PENDING` where `CreateFileW(CREATE_NEW)` or
  // parent `RemoveDirectory` would fail. `IGNORE_READONLY_ATTRIBUTE` (Windows
  // 10 1809+) avoids modifying the attributes of the (possibly hard-linked)
  // file object.
  FILE_DISPOSITION_INFO_EX disp_ex = {
      .Flags = FILE_DISPOSITION_FLAG_DELETE |
               FILE_DISPOSITION_FLAG_POSIX_SEMANTICS |
               FILE_DISPOSITION_FLAG_IGNORE_READONLY_ATTRIBUTE};
  if (!::SetFileInformationByHandle(handle.Get(), FileDispositionInfoEx,
                                    &disp_ex, sizeof(disp_ex))) {
    DWORD err = ::GetLastError();
    if (!IsDispositionFlagUnsupported(err)) {
      LOG(ERROR) << "SetFileInformationByHandle(FileDispositionInfoEx) failed "
                 << "for " << path.value() << ": "
                 << logging::SystemErrorCodeToString(err);
      return false;
    }

    // Older Windows versions (without `IGNORE_READONLY_ATTRIBUTE` support):
    // the read-only attribute must be cleared before the entry can be marked
    // for deletion. For a hard link this also clears `FILE_ATTRIBUTE_READONLY`
    // on the shared file object; restore it if marking for deletion fails.
    const bool cleared_readonly =
        (current_attrs & FILE_ATTRIBUTE_READONLY) != 0;
    if (cleared_readonly &&
        !ClearReadOnlyAttribute(handle.Get(), current_attrs, path)) {
      return false;
    }
    auto restore_readonly = [&]() {
      if (cleared_readonly) {
        FILE_BASIC_INFO basic_info = {};
        basic_info.FileAttributes = current_attrs;
        ::SetFileInformationByHandle(handle.Get(), FileBasicInfo, &basic_info,
                                     sizeof(basic_info));
      }
    };
    disp_ex.Flags =
        FILE_DISPOSITION_FLAG_DELETE | FILE_DISPOSITION_FLAG_POSIX_SEMANTICS;
    if (!::SetFileInformationByHandle(handle.Get(), FileDispositionInfoEx,
                                      &disp_ex, sizeof(disp_ex))) {
      err = ::GetLastError();
      if (!IsDispositionFlagUnsupported(err)) {
        LOG(ERROR) << "SetFileInformationByHandle(FileDispositionInfoEx) "
                   << "failed for " << path.value() << ": "
                   << logging::SystemErrorCodeToString(err);
        restore_readonly();
        return false;
      }
      FILE_DISPOSITION_INFO disp = {.DeleteFile = TRUE};
      if (!::SetFileInformationByHandle(handle.Get(), FileDispositionInfo,
                                        &disp, sizeof(disp))) {
        PLOG(ERROR) << "SetFileInformationByHandle(FileDispositionInfo) "
                    << "failed for " << path.value();
        restore_readonly();
        return false;
      }
    }
  }

  handle.Close();

  if (::GetFileAttributesW(path.value().c_str()) != INVALID_FILE_ATTRIBUTES) {
    LOG(ERROR) << "Untrusted entry still present after deletion: "
               << path.value();
    return false;
  }
  DWORD post_err = ::GetLastError();
  if (!IsNotFoundError(post_err)) {
    LOG(ERROR) << "Untrusted entry remained in pending or inaccessible state "
               << "after deletion (" << post_err << "): " << path.value();
    return false;
  }
  return true;
}

bool DeleteUntrustedEntrySafelyImpl(
    const base::FilePath& path,
    const base::FilePath& expected_canonical_path,
    const base::win::SecurityDescriptor& admin_only_sd,
    int depth) {
  if (depth > kMaxDeleteRecursionDepth) {
    LOG(ERROR) << "Exceeded maximum recursion depth while deleting untrusted "
               << "entry: " << path.value();
    return false;
  }

  constexpr DWORD kDeleteAccess = DELETE | FILE_READ_ATTRIBUTES |
                                  FILE_WRITE_ATTRIBUTES | WRITE_DAC |
                                  WRITE_OWNER;

  // Open without `FILE_SHARE_WRITE` or `FILE_SHARE_DELETE` so no other process
  // holds an open data-write handle or delete handle (capable of cancelling
  // `DeletePending`) while we inspect and delete `path`. Share-mode checks do
  // not block handles opened with only `FILE_WRITE_ATTRIBUTES` (which is enough
  // for `FSCTL_SET_REPARSE_POINT`); the invariant preventing out-of-tree
  // deletion is the per-entry `IsExpectedCanonicalPath()` check on each child
  // handle, so if a directory is converted to a junction after this point, its
  // children fail that check and deletion fails closed.
  base::expected<base::win::ScopedHandle, DWORD> opened = OpenEntryNoFollow(
      path, kDeleteAccess, FILE_SHARE_READ, PrivilegedRetry::kBackupRestore);
  bool opened_with_share_all = false;
  if (!opened.has_value() && opened.error() == ERROR_SHARING_VIOLATION) {
    // Only permit `FILE_SHARE_WRITE | FILE_SHARE_DELETE` when unlinking a
    // reparse point itself: an attacker holding a junction open (or
    // `base::test::FilePathReparsePoint` in tests) cannot block its removal,
    // and unlinking a link only removes the link itself (a cancelled delete is
    // caught by the post-delete check in `UnlinkHandleAndVerifyRemoved()`). The
    // reparse attribute is verified below on this same handle, and regular
    // files and non-reparse directories fail closed so an attacker holding a
    // data-write or delete handle cannot modify contents or cancel deletion.
    opened = OpenEntryNoFollow(
        path, DELETE | FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES, kShareAll,
        PrivilegedRetry::kBackupRestore);
    opened_with_share_all = opened.has_value();
  }
  if (!opened.has_value()) {
    if (IsNotFoundError(opened.error())) {
      return true;
    }
    // Intentionally fail closed if an untrusted entry cannot be opened for
    // deletion (for example, an unprivileged process holds a planted
    // `host.json` open without `FILE_SHARE_DELETE` or holds a write/delete
    // handle to an untrusted file or subdirectory). Neither `DeleteFile` nor
    // `MoveFileExW` can unlink or rename an entry whose open handle omits
    // `FILE_SHARE_DELETE`, and proceeding while an attacker-controlled file
    // remains at a config path read by the `LocalSystem` service is unsafe.
    LOG(ERROR) << "Failed to open untrusted configuration entry for deletion: "
               << path.value() << ": "
               << logging::SystemErrorCodeToString(opened.error());
    return false;
  }
  base::win::ScopedHandle handle = std::move(opened).value();

  BY_HANDLE_FILE_INFORMATION info = {};
  if (!::GetFileInformationByHandle(handle.Get(), &info)) {
    PLOG(ERROR) << "GetFileInformationByHandle failed for " << path.value();
    return false;
  }

  // `handle` was opened with `FILE_FLAG_OPEN_REPARSE_POINT`, so for reparse
  // points this resolves to the link's own path. Verifying every entry
  // (including reparse points) before modifying it ensures a redirected parent
  // component cannot point the deletion outside the expected tree.
  if (!IsExpectedCanonicalPath(handle.Get(), expected_canonical_path)) {
    return false;
  }

  const bool is_reparse_point =
      (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
  if (opened_with_share_all && !is_reparse_point) {
    LOG(ERROR) << "Untrusted entry is held open by another process with write "
               << "or delete access: " << path.value();
    return false;
  }

  if (!is_reparse_point &&
      (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
    // Lock the subdirectory DACL to `SY`/`BA` only (protected, non-propagating)
    // to block new unprivileged opens from creating or renaming entries during
    // traversal, while holding `handle` open without `FILE_SHARE_WRITE` or
    // `FILE_SHARE_DELETE`. A handle opened before the DACL write could still
    // create children or (with `FILE_WRITE_ATTRIBUTES`) convert an empty
    // directory into a junction; the real backstops are per-child
    // `IsExpectedCanonicalPath()` verification before any modification and the
    // final unlink failing closed if the directory is not empty.
    if (!WriteNonPropagatingSecurityDescriptor(handle.Get(), admin_only_sd)) {
      LOG(ERROR) << "Failed to lock untrusted subdirectory DACL: "
                 << path.value();
      return false;
    }

    // `path` is in extended-length form, so enumerated child paths preserve
    // names with trailing dots/spaces and are not limited by `MAX_PATH`.
    std::vector<base::FilePath> children;
    {
      base::FileEnumerator enumerator(
          path, /*recursive=*/false,
          base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES,
          /*pattern=*/base::FilePath::StringType(),
          base::FileEnumerator::FolderSearchPolicy::MATCH_ONLY,
          base::FileEnumerator::ErrorPolicy::STOP_ENUMERATION);
      for (base::FilePath child = enumerator.Next(); !child.empty();
           child = enumerator.Next()) {
        children.push_back(std::move(child));
      }
      if (enumerator.GetError() != base::File::FILE_OK) {
        LOG(ERROR) << "Failed to enumerate untrusted subdirectory "
                   << path.value() << ": " << enumerator.GetError();
        return false;
      }
    }

    for (const base::FilePath& child : children) {
      if (!DeleteUntrustedEntrySafelyImpl(
              child, expected_canonical_path.Append(child.BaseName()),
              admin_only_sd, depth + 1)) {
        return false;
      }
    }
  }

  // Files (including hard links to files outside the tree) and reparse points
  // are unlinked without modifying their security descriptor, so a planted
  // hard link only loses this name (on older Windows versions without
  // `FILE_DISPOSITION_FLAG_IGNORE_READONLY_ATTRIBUTE`, the fallback path may
  // also clear `FILE_ATTRIBUTE_READONLY` on the shared file object).
  return UnlinkHandleAndVerifyRemoved(std::move(handle), info.dwFileAttributes,
                                      path);
}

}  // namespace

bool DeleteUntrustedEntrySafely(
    const base::FilePath& path,
    const base::FilePath& expected_canonical_path,
    const base::win::SecurityDescriptor& admin_only_sd) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  const std::optional<base::FilePath> long_path = ToExtendedLengthPath(path);
  if (!long_path.has_value()) {
    LOG(ERROR) << "Untrusted entry path cannot be converted to extended-length "
               << "form: " << path.value();
    return false;
  }
  return DeleteUntrustedEntrySafelyImpl(*long_path, expected_canonical_path,
                                        admin_only_sd, /*depth=*/0);
}

}  // namespace remoting::installer
