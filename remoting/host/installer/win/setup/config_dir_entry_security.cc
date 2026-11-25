// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/installer/win/setup/config_dir_entry_security.h"

#include <windows.h>

#include <cstddef>
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
#include "remoting/host/installer/win/setup/installer_constants.h"
#include "remoting/host/installer/win/setup/secure_delete.h"
#include "remoting/host/win/file_security_util.h"
#include "remoting/host/win/security_descriptor.h"

namespace remoting::installer {

namespace {

constexpr DWORD kShareAll =
    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;

// Non-admin rights permitted on `host_unprivileged.json`, and the maximum
// non-admin rights allowed for an otherwise-trusted config file (e.g.
// `host.json`) to be tightened in place rather than deleted. Any right outside
// this mask (data write/append, delete, or metadata/ACL writes such as
// `WRITE_DAC`, `WRITE_OWNER`, `FILE_WRITE_ATTRIBUTES`, or `FILE_WRITE_EA`)
// means file contents are untrusted or stale metadata handles may exist.
constexpr ACCESS_MASK kReadOnlyNonAdminMask =
    FILE_GENERIC_READ | FILE_GENERIC_EXECUTE | SYNCHRONIZE;

bool SameFileObject(const BY_HANDLE_FILE_INFORMATION& a,
                    const BY_HANDLE_FILE_INFORMATION& b) {
  return a.dwVolumeSerialNumber == b.dwVolumeSerialNumber &&
         a.nFileIndexHigh == b.nFileIndexHigh &&
         a.nFileIndexLow == b.nFileIndexLow;
}

// Reopens the read-permissive retained file at `path` with `FILE_READ_DATA`
// and `FILE_SHARE_READ` so `IoCheckShareAccess` rejects any pre-existing
// data-write, append, or delete handle while still allowing concurrent readers
// (such as `ConfigFileWatcher`), verifies that the reopened handle still refers
// to the same non-reparse, single-link file object at `expected_canonical`, and
// writes `target_sd` through that same handle without propagating ACEs.
// Note: `IoCheckShareAccess` does not check metadata-only handles (`WRITE_DAC`,
// `WRITE_OWNER`, `FILE_WRITE_ATTRIBUTES`, `FILE_WRITE_EA`); files whose prior
// DACL granted non-admins any such rights are deleted by the caller rather than
// tightened here.
bool TightenVerifiedFileDacl(const base::FilePath& path,
                             const base::FilePath& expected_canonical,
                             const BY_HANDLE_FILE_INFORMATION& expected_info,
                             const base::win::SecurityDescriptor& target_sd) {
  constexpr DWORD kDesiredAccess =
      FILE_READ_DATA | FILE_READ_ATTRIBUTES | WRITE_DAC | WRITE_OWNER;
  base::expected<base::win::ScopedHandle, DWORD> opened = OpenEntryNoFollow(
      path, kDesiredAccess, FILE_SHARE_READ, PrivilegedRetry::kBackupRestore);
  if (!opened.has_value()) {
    LOG(ERROR) << "Failed to open retained configuration file without "
               << "write/delete sharing: " << path.value() << ": "
               << logging::SystemErrorCodeToString(opened.error());
    return false;
  }
  base::win::ScopedHandle handle = std::move(opened).value();

  BY_HANDLE_FILE_INFORMATION info = {};
  if (!::GetFileInformationByHandle(handle.Get(), &info)) {
    PLOG(ERROR) << "GetFileInformationByHandle failed for " << path.value();
    return false;
  }
  if ((info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 ||
      (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ||
      info.nNumberOfLinks > 1 || !SameFileObject(info, expected_info) ||
      !IsExpectedCanonicalPath(handle.Get(), expected_canonical)) {
    LOG(ERROR) << "Retained configuration file failed handle verification: "
               << path.value();
    return false;
  }

  if (!WriteNonPropagatingSecurityDescriptor(handle.Get(), target_sd)) {
    LOG(ERROR) << "Failed to tighten security descriptor on " << path.value();
    return false;
  }
  return true;
}

}  // namespace

std::optional<size_t> PurgeAndHardenConfigDirEntries(
    const base::FilePath& config_dir,
    const base::FilePath& canonical_config_dir) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  std::optional<base::win::SecurityDescriptor> admin_only_sd =
      base::win::SecurityDescriptor::FromSddl(kAdminOnlyProtectedSddl);
  if (!admin_only_sd.has_value()) {
    PLOG(ERROR) << "Failed to convert admin-only security descriptor string";
    return std::nullopt;
  }

  // Enumerate via the extended-length path so entry names with trailing
  // dots/spaces are preserved when the entries are reopened below.
  const std::optional<base::FilePath> long_config_dir =
      ToExtendedLengthPath(config_dir);
  if (!long_config_dir.has_value()) {
    LOG(ERROR) << "Configuration directory path cannot be converted to "
               << "extended-length form: " << config_dir.value();
    return std::nullopt;
  }
  std::vector<base::FilePath> entries;
  {
    base::FileEnumerator enumerator(
        *long_config_dir, /*recursive=*/false,
        base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES,
        /*pattern=*/base::FilePath::StringType(),
        base::FileEnumerator::FolderSearchPolicy::MATCH_ONLY,
        base::FileEnumerator::ErrorPolicy::STOP_ENUMERATION);
    for (base::FilePath entry = enumerator.Next(); !entry.empty();
         entry = enumerator.Next()) {
      entries.push_back(std::move(entry));
    }
    if (enumerator.GetError() != base::File::FILE_OK) {
      LOG(ERROR) << "Failed to enumerate configuration directory "
                 << config_dir.value() << ": " << enumerator.GetError();
      return std::nullopt;
    }
  }

  size_t retained_entries = 0;
  for (const base::FilePath& entry : entries) {
    base::FilePath expected_canonical =
        canonical_config_dir.Append(entry.BaseName());
    base::expected<base::win::ScopedHandle, DWORD> opened_entry =
        OpenEntryNoFollow(entry, READ_CONTROL | FILE_READ_ATTRIBUTES, kShareAll,
                          PrivilegedRetry::kDisabled);
    if (!opened_entry.has_value()) {
      if (opened_entry.error() == ERROR_FILE_NOT_FOUND) {
        continue;
      }
      if (opened_entry.error() == ERROR_ACCESS_DENIED) {
        // Legitimate `SY`/`BA` config files always grant `BA` `READ_CONTROL`;
        // an entry denying `READ_CONTROL` to an elevated installer is
        // untrusted.
        LOG(WARNING) << "Removing access-denied entry in configuration "
                     << "directory: " << entry.value();
        if (!DeleteUntrustedEntrySafely(entry, expected_canonical,
                                        *admin_only_sd)) {
          return std::nullopt;
        }
        continue;
      }
      LOG(ERROR) << "Failed to inspect entry in configuration directory: "
                 << entry.value() << ": "
                 << logging::SystemErrorCodeToString(opened_entry.error());
      return std::nullopt;
    }
    base::win::ScopedHandle entry_handle = std::move(opened_entry).value();

    BY_HANDLE_FILE_INFORMATION info = {};
    if (!::GetFileInformationByHandle(entry_handle.Get(), &info)) {
      PLOG(ERROR) << "GetFileInformationByHandle failed for " << entry.value();
      return std::nullopt;
    }

    if ((info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
      entry_handle.Close();
      LOG(WARNING) << "Removing reparse point in configuration directory: "
                   << entry.value();
      if (!DeleteUntrustedEntrySafely(entry, expected_canonical,
                                      *admin_only_sd)) {
        return std::nullopt;
      }
      continue;
    }

    // Fail closed rather than deleting if canonical path verification fails on
    // a non-reparse child so a normalization anomaly cannot destroy a
    // legitimate configuration file.
    if (!IsExpectedCanonicalPath(entry_handle.Get(), expected_canonical)) {
      return std::nullopt;
    }

    if ((info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
      // Legitimate Windows host configuration directories are flat (`host.json`
      // and `host_unprivileged.json`). Removing subdirectories unconditionally
      // also avoids treating a subdirectory's owner/DACL as a trust signal
      // after an earlier `DeleteUntrustedEntrySafely()` run failed partway
      // through after locking the subdirectory to `kAdminOnlyProtectedSddl`.
      entry_handle.Close();
      LOG(WARNING) << "Removing unexpected subdirectory in configuration "
                   << "directory: " << entry.value();
      if (!DeleteUntrustedEntrySafely(entry, expected_canonical,
                                      *admin_only_sd)) {
        return std::nullopt;
      }
      continue;
    }

    if (info.nNumberOfLinks > 1) {
      // Legitimate configuration files are written via atomic rename of a newly
      // created temporary file and are never multiply linked. Unlink this name
      // unconditionally rather than retaining a hard link or rewriting a
      // security descriptor shared with another path.
      entry_handle.Close();
      LOG(WARNING) << "Removing multiply-linked entry in configuration "
                   << "directory: " << entry.value();
      if (!DeleteUntrustedEntrySafely(entry, expected_canonical,
                                      *admin_only_sd)) {
        return std::nullopt;
      }
      continue;
    }

    const bool is_unprivileged_config_file =
        base::FilePath::CompareEqualIgnoreCase(entry.BaseName().value(),
                                               kUnprivilegedHostConfigFileName);
    const ACCESS_MASK allowed_non_admin_mask =
        is_unprivileged_config_file ? kReadOnlyNonAdminMask : 0;

    std::optional<SecurityDescriptorInspection> inspection =
        InspectFileSecurityDescriptor(
            entry_handle.Get(),
            {.allowed_non_admin_mask = allowed_non_admin_mask});
    if (!inspection.has_value()) {
      return std::nullopt;
    }

    if (!inspection->trusted_owner) {
      entry_handle.Close();
      LOG(WARNING) << "Removing untrusted entry in configuration directory: "
                   << entry.value();
      if (!DeleteUntrustedEntrySafely(entry, expected_canonical,
                                      *admin_only_sd)) {
        return std::nullopt;
      }
      continue;
    }

    if (inspection->safe_dacl) {
      // Owner and DACL already satisfy policy. Skip reopening with
      // `FILE_READ_DATA` / `FILE_SHARE_READ`: `host_unprivileged.json` grants
      // Authenticated Users read access by design, so probing share mode here
      // would allow any local user holding an exclusive read handle
      // (`dwShareMode = 0`) to trigger `ERROR_SHARING_VIOLATION` and block
      // setup.
      ++retained_entries;
      continue;
    }

    // `!inspection->safe_dacl`: for `host_unprivileged.json` (which already
    // permitted `kReadOnlyNonAdminMask`), or for any other file that fails a
    // read-only inspection, non-admins had write, delete, or permission-editing
    // rights (or the DACL had a deny ACE, was NULL/empty, or lacked `SY`/`BA`
    // read access). Delete the file rather than tightening in place, because
    // its contents are untrusted and DACL tightening cannot revoke stale
    // metadata handles (`WRITE_DAC`, `WRITE_OWNER`, `FILE_WRITE_ATTRIBUTES`).
    bool tighten_in_place = false;
    if (!is_unprivileged_config_file) {
      std::optional<SecurityDescriptorInspection> read_only_inspection =
          InspectFileSecurityDescriptor(
              entry_handle.Get(),
              {.allowed_non_admin_mask = kReadOnlyNonAdminMask});
      if (!read_only_inspection.has_value()) {
        return std::nullopt;
      }
      tighten_in_place = read_only_inspection->safe_dacl;
    }

    if (!tighten_in_place) {
      entry_handle.Close();
      LOG(WARNING) << "Removing entry with unsafe or write-permissive DACL in "
                      "configuration directory: "
                   << entry.value();
      if (!DeleteUntrustedEntrySafely(entry, expected_canonical,
                                      *admin_only_sd)) {
        return std::nullopt;
      }
      continue;
    }

    // Non-admins had at most read/execute access (e.g. from legacy MSI
    // inheritance), so the file's contents remain trusted. Keep `entry_handle`
    // (which holds only metadata access and does not participate in share-mode
    // checks) open across the reopen so the file index in `info` cannot be
    // recycled while `TightenVerifiedFileDacl()` verifies object identity.
    LOG(WARNING) << "Tightening read-permissive DACL on configuration entry: "
                 << entry.value();
    if (!TightenVerifiedFileDacl(entry, expected_canonical, info,
                                 *admin_only_sd)) {
      return std::nullopt;
    }
    ++retained_entries;
  }
  return retained_entries;
}

}  // namespace remoting::installer
