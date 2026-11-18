// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/file_security_util.h"

#include <windows.h>

#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/types/expected.h"
#include "base/win/access_control_list.h"
#include "base/win/access_token.h"
#include "base/win/scoped_handle.h"
#include "base/win/security_descriptor.h"
#include "base/win/sid.h"

namespace remoting {

namespace {

constexpr wchar_t kTrustedInstallerSidString[] =
    L"S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464";

// `SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION` ensures that if an open
// ever lands on a named pipe, the server only gets an identification-level
// token for this (privileged) caller and cannot impersonate it.
// `SECURITY_SQOS_PRESENT` shares its value with `FILE_FLAG_OPEN_NO_RECALL`,
// which only prevents recalling offline (remote storage) file data and does not
// affect the metadata, security descriptor, and delete operations done here.
constexpr DWORD kOpenNoFollowFlags =
    FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS |
    SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION;

// Owner, group, and protected DACL, as written by
// `WriteNonPropagatingSecurityDescriptor()`.
constexpr SECURITY_INFORMATION kNonPropagatingSdInfo =
    OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION |
    DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION;

constexpr std::wstring_view kExtendedPrefix = L"\\\\?\\";
constexpr std::wstring_view kExtendedUncPrefix = L"\\\\?\\UNC\\";
constexpr std::wstring_view kUncPrefix = L"\\\\";

// Enables `SeBackupPrivilege` and `SeRestorePrivilege` on a thread-scoped
// impersonation copy of the process token (`ImpersonateSelf`) for the lifetime
// of this object, and reverts to the process token on destruction. The process
// token is never modified, so other threads never observe these privileges.
// With both privileges enabled, `CreateFileW` with `FILE_FLAG_BACKUP_SEMANTICS`
// is granted the requested rights (plus the `SYNCHRONIZE |
// FILE_READ_ATTRIBUTES` that `CreateFileW` always adds) even if the DACL
// explicitly denies `BA`. Elevated administrators and `LocalSystem` both hold
// these privileges.
class ScopedBackupRestorePrivileges {
 public:
  ScopedBackupRestorePrivileges() {
    if (base::win::AccessToken::FromCurrentThread().has_value()) {
      LOG(ERROR) << "Thread is already impersonating; not enabling "
                 << "backup/restore privileges";
      return;
    }
    // `FromCurrentThread()` returns nullopt for any `OpenThreadToken` failure.
    // Only `ERROR_NO_TOKEN` means the thread is not impersonating; otherwise
    // `ImpersonateSelf()` would replace the caller's impersonation and
    // `Revert()` would then drop it.
    const DWORD thread_token_error = ::GetLastError();
    if (thread_token_error != ERROR_NO_TOKEN) {
      LOG(ERROR) << "Failed to query the thread token; not enabling "
                 << "backup/restore privileges: "
                 << logging::SystemErrorCodeToString(thread_token_error);
      return;
    }
    if (!::ImpersonateSelf(SecurityImpersonation)) {
      PLOG(ERROR) << "ImpersonateSelf failed";
      return;
    }
    impersonating_ = true;

    std::optional<base::win::AccessToken> token =
        base::win::AccessToken::FromCurrentThread(
            /*open_as_self=*/true, TOKEN_ADJUST_PRIVILEGES);
    if (!token.has_value()) {
      PLOG(ERROR) << "Failed to open the impersonation token";
      Revert();
      return;
    }
    // `SetPrivilege()` can fail without a Win32 error (e.g. when the privilege
    // is not held), so this does not use `PLOG`.
    if (!token->SetPrivilege(SE_BACKUP_NAME, /*enable=*/true).has_value() ||
        !token->SetPrivilege(SE_RESTORE_NAME, /*enable=*/true).has_value()) {
      LOG(ERROR) << "Failed to enable backup/restore privileges";
      Revert();
      return;
    }
    enabled_ = true;
  }

  ScopedBackupRestorePrivileges(const ScopedBackupRestorePrivileges&) = delete;
  ScopedBackupRestorePrivileges& operator=(
      const ScopedBackupRestorePrivileges&) = delete;

  ~ScopedBackupRestorePrivileges() { Revert(); }

  bool enabled() const { return enabled_; }

 private:
  void Revert() {
    if (impersonating_) {
      // Failing to revert would leave this thread running with backup/restore
      // privileges enabled.
      PCHECK(::RevertToSelf());
      impersonating_ = false;
    }
  }

  bool impersonating_ = false;
  bool enabled_ = false;
};

HANDLE CreateFileNoFollow(const base::FilePath& path,
                          DWORD desired_access,
                          DWORD share_mode) {
  return ::CreateFileW(path.value().c_str(), desired_access, share_mode,
                       /*lpSecurityAttributes=*/nullptr, OPEN_EXISTING,
                       kOpenNoFollowFlags, /*hTemplateFile=*/nullptr);
}

// Returns true if any `\`-separated component of `path` is `.` or `..`.
bool HasDotComponent(std::wstring_view path) {
  size_t start = 0;
  while (true) {
    const size_t end = path.find(L'\\', start);
    const std::wstring_view component = end == std::wstring_view::npos
                                            ? path.substr(start)
                                            : path.substr(start, end - start);
    if (component == L"." || component == L"..") {
      return true;
    }
    if (end == std::wstring_view::npos) {
      return false;
    }
    start = end + 1;
  }
}

// Returns true if `rest` (an extended-length path with the `\\?\` prefix
// removed) starts with `X:\`, `UNC\server\`, or `Volume{GUID}\`.
bool IsAllowedExtendedLengthRoot(std::wstring_view rest) {
  if (rest.size() >= 3 && base::IsAsciiAlpha(rest[0]) && rest[1] == L':' &&
      rest[2] == L'\\') {
    return true;
  }

  constexpr std::wstring_view kUnc = L"UNC\\";
  if (base::StartsWith(rest, kUnc, base::CompareCase::INSENSITIVE_ASCII)) {
    // Require a non-empty server name followed by a separator.
    const size_t separator = rest.find(L'\\', kUnc.size());
    return separator != std::wstring_view::npos && separator > kUnc.size();
  }

  // `Volume{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}\`.
  constexpr std::wstring_view kVolume = L"Volume{";
  constexpr size_t kGuidLength = 36;
  if (!base::StartsWith(rest, kVolume, base::CompareCase::INSENSITIVE_ASCII) ||
      rest.size() < kVolume.size() + kGuidLength + 2) {
    return false;
  }
  const std::wstring_view guid = rest.substr(kVolume.size(), kGuidLength);
  for (size_t i = 0; i < guid.size(); ++i) {
    const bool is_dash_position = i == 8 || i == 13 || i == 18 || i == 23;
    if (is_dash_position ? guid[i] != L'-' : !base::IsHexDigit(guid[i])) {
      return false;
    }
  }
  return rest[kVolume.size() + kGuidLength] == L'}' &&
         rest[kVolume.size() + kGuidLength + 1] == L'\\';
}

}  // namespace

std::optional<base::FilePath> ToExtendedLengthPath(const base::FilePath& path) {
  const base::FilePath normalized = path.NormalizePathSeparators();
  const std::wstring& value = normalized.value();
  // This also rejects `\\.\` device paths, which have a `.` component. Other
  // components made of dots or spaces (e.g. `...`) are literal names in
  // extended-length form, so they are allowed.
  if (HasDotComponent(value)) {
    return std::nullopt;
  }
  if (value.starts_with(kExtendedPrefix)) {
    if (!IsAllowedExtendedLengthRoot(
            std::wstring_view(value).substr(kExtendedPrefix.size()))) {
      return std::nullopt;
    }
    return normalized;
  }
  if (!normalized.IsAbsolute()) {
    return std::nullopt;
  }
  base::FilePath extended =
      value.starts_with(kUncPrefix)
          ? base::FilePath(std::wstring(kExtendedUncPrefix) +
                           value.substr(kUncPrefix.size()))
          : base::FilePath(std::wstring(kExtendedPrefix) + value);
  // Apply the same root validation as for already-extended input (e.g. reject
  // a UNC path with an empty server name).
  if (!IsAllowedExtendedLengthRoot(
          std::wstring_view(extended.value()).substr(kExtendedPrefix.size()))) {
    return std::nullopt;
  }
  return extended;
}

std::optional<base::FilePath> GetCanonicalPath(HANDLE handle) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  for (DWORD volume_flag : {static_cast<DWORD>(VOLUME_NAME_DOS),
                            static_cast<DWORD>(VOLUME_NAME_GUID)}) {
    const DWORD flags = FILE_NAME_NORMALIZED | volume_flag;
    std::vector<wchar_t> buffer(MAX_PATH);
    DWORD result = ::GetFinalPathNameByHandleW(
        handle, buffer.data(), static_cast<DWORD>(buffer.size()), flags);
    if (result >= buffer.size()) {
      // `result` is the required size, including the terminating null.
      buffer.resize(result);
      result = ::GetFinalPathNameByHandleW(
          handle, buffer.data(), static_cast<DWORD>(buffer.size()), flags);
    }
    if (result == 0) {
      const DWORD err = ::GetLastError();
      // `VOLUME_NAME_DOS` fails with `ERROR_PATH_NOT_FOUND` for volumes without
      // a drive letter or mount point; fall back to `VOLUME_NAME_GUID`.
      if (volume_flag == VOLUME_NAME_DOS && err == ERROR_PATH_NOT_FOUND) {
        continue;
      }
      LOG(ERROR) << "GetFinalPathNameByHandleW failed: "
                 << logging::SystemErrorCodeToString(err);
      return std::nullopt;
    }
    if (result >= buffer.size()) {
      // The path grew between the two calls. No Win32 error is set here.
      LOG(ERROR) << "GetFinalPathNameByHandleW reported a too-small buffer "
                 << "after resizing";
      return std::nullopt;
    }

    std::wstring final_path(buffer.data(), result);
    if (volume_flag == VOLUME_NAME_DOS) {
      if (final_path.starts_with(kExtendedUncPrefix)) {
        final_path = std::wstring(kUncPrefix) +
                     final_path.substr(kExtendedUncPrefix.size());
      } else if (final_path.starts_with(kExtendedPrefix)) {
        final_path = final_path.substr(kExtendedPrefix.size());
      }
    }
    return base::FilePath(std::move(final_path));
  }
  // Not reached: the `VOLUME_NAME_GUID` iteration always returns.
  return std::nullopt;
}

bool IsExpectedCanonicalPath(HANDLE handle,
                             const base::FilePath& expected_canonical) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  std::optional<base::FilePath> actual_canonical = GetCanonicalPath(handle);
  if (!actual_canonical.has_value()) {
    return false;
  }
  // The comparison is case-insensitive and does not account for directories
  // with per-directory case sensitivity enabled (`FILE_CS_FLAG_CASE_SENSITIVE_
  // DIR`). This can only cause a false match between two names differing by
  // case inside such a directory, which requires write access to the parent;
  // callers verify admin-owned parents, so that is not a concern here.
  if (base::FilePath::CompareEqualIgnoreCase(actual_canonical->value(),
                                             expected_canonical.value())) {
    return true;
  }

  // Expand any 8.3 short component in `expected_canonical` (without resolving
  // junctions or symlinks) in case a caller passed a short name below the
  // canonical root.
  const base::FilePath expanded = base::MakeLongFilePath(expected_canonical);
  if (!expanded.empty() && base::FilePath::CompareEqualIgnoreCase(
                               actual_canonical->value(), expanded.value())) {
    return true;
  }

  LOG(ERROR) << "Canonical path mismatch. Expected: "
             << expected_canonical.value()
             << ", Actual: " << actual_canonical->value();
  return false;
}

// Note on trusted owners for config files (`allow_trusted_installer == false`):
// All Windows CRD config writers (the free function `WriteConfigSafe()` in
// `daemon_controller_delegate_win.cc`, used by both the elevated COM
// controller and `remoting_start_host.exe`) pass an explicit `O:BA` security
// descriptor to `CreateFileW`, so NTFS sets Owner to `BA` regardless of the
// machine's "Default owner for objects created by members of the Administrators
// group" policy. Individual user SIDs that happen to belong to `BA` are
// intentionally NOT treated as trusted owners because under UAC split-token,
// unelevated Medium-IL processes in an admin user's session create files owned
// by that user SID.
std::optional<SecurityDescriptorInspection> InspectFileSecurityDescriptor(
    HANDLE handle,
    const SecurityDescriptorPolicy& policy) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  std::optional<base::win::SecurityDescriptor> sd =
      base::win::SecurityDescriptor::FromHandle(
          handle, base::win::SecurityObjectType::kFile,
          OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION);
  if (!sd.has_value()) {
    PLOG(ERROR) << "Failed to read security descriptor";
    return std::nullopt;
  }
  if (!sd->owner().has_value()) {
    LOG(ERROR) << "Security descriptor has no owner";
    return std::nullopt;
  }

  const base::win::Sid local_system(base::win::WellKnownSid::kLocalSystem);
  const base::win::Sid administrators(
      base::win::WellKnownSid::kBuiltinAdministrators);
  const base::win::Sid creator_owner(base::win::WellKnownSid::kCreatorOwner);
  const std::optional<base::win::Sid> trusted_installer =
      policy.allow_trusted_installer
          ? base::win::Sid::FromSddlString(kTrustedInstallerSidString)
          : std::nullopt;
  auto is_trusted_principal = [&](PSID sid) {
    return local_system.Equal(sid) || administrators.Equal(sid) ||
           (trusted_installer.has_value() && trusted_installer->Equal(sid));
  };

  SecurityDescriptorInspection result;
  result.trusted_owner = is_trusted_principal(sd->owner()->GetPSID());

  if (!sd->dacl().has_value() || sd->dacl()->is_null()) {
    // Missing or NULL DACL (grants Everyone full access).
    return result;
  }
  // `AccessControlList` has no ACE iteration API, so walk the raw ACL.
  PACL dacl = sd->dacl()->get();

  ACL_SIZE_INFORMATION acl_info = {};
  if (!::GetAclInformation(dacl, &acl_info, sizeof(acl_info),
                           AclSizeInformation) ||
      acl_info.AceCount == 0) {
    return result;
  }

  constexpr GENERIC_MAPPING kFileGenericMapping = {
      FILE_GENERIC_READ, FILE_GENERIC_WRITE, FILE_GENERIC_EXECUTE,
      FILE_ALL_ACCESS};

  bool has_system_allow = false;
  bool has_admin_allow = false;
  for (DWORD i = 0; i < acl_info.AceCount; ++i) {
    void* raw_ace = nullptr;
    if (!::GetAce(dacl, i, &raw_ace)) {
      PLOG(ERROR) << "GetAce failed";
      return std::nullopt;
    }
    const auto* header = static_cast<const ACE_HEADER*>(raw_ace);
    // None of the security descriptors written by CRD contain deny ACEs, so
    // any deny ACE (or other non-allow ACE type) is unexpected. Treating it as
    // unsafe ensures a planted deny ACE (which could lock out the installer or
    // a host process) does not survive inspection.
    if (header->AceType != ACCESS_ALLOWED_ACE_TYPE) {
      return result;
    }

    const auto* allow_ace = static_cast<const ACCESS_ALLOWED_ACE*>(raw_ace);
    PSID sid =
        const_cast<SID*>(reinterpret_cast<const SID*>(&allow_ace->SidStart));
    ACCESS_MASK mask = allow_ace->Mask;
    GENERIC_MAPPING mapping = kFileGenericMapping;
    ::MapGenericMask(&mask, &mapping);

    if ((header->AceFlags & INHERIT_ONLY_ACE) == 0) {
      if (local_system.Equal(sid) &&
          (mask & FILE_GENERIC_READ) == FILE_GENERIC_READ) {
        has_system_allow = true;
      }
      if (administrators.Equal(sid) &&
          (mask & FILE_GENERIC_READ) == FILE_GENERIC_READ) {
        has_admin_allow = true;
      }
    }

    if (is_trusted_principal(sid)) {
      continue;
    }
    if (policy.allow_inherit_only_creator_owner && creator_owner.Equal(sid) &&
        (header->AceFlags & INHERIT_ONLY_ACE) != 0) {
      continue;
    }
    if ((mask & ~policy.allowed_non_admin_mask) != 0) {
      return result;
    }
  }

  result.safe_dacl = has_system_allow && has_admin_allow;
  return result;
}

base::expected<base::win::ScopedHandle, DWORD> OpenEntryNoFollow(
    const base::FilePath& path,
    DWORD desired_access,
    DWORD share_mode,
    PrivilegedRetry privileged_retry) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  const std::optional<base::FilePath> long_path = ToExtendedLengthPath(path);
  if (!long_path.has_value()) {
    LOG(ERROR) << "Path cannot be converted to extended-length form: "
               << path.value();
    return base::unexpected(static_cast<DWORD>(ERROR_INVALID_NAME));
  }

  base::win::ScopedHandle handle(
      CreateFileNoFollow(*long_path, desired_access, share_mode));
  DWORD err = handle.is_valid() ? ERROR_SUCCESS : ::GetLastError();
  if (!handle.is_valid() && err == ERROR_ACCESS_DENIED &&
      privileged_retry == PrivilegedRetry::kBackupRestore) {
    ScopedBackupRestorePrivileges privileges;
    if (privileges.enabled()) {
      handle.Set(CreateFileNoFollow(*long_path, desired_access, share_mode));
      err = handle.is_valid() ? ERROR_SUCCESS : ::GetLastError();
    } else {
      LOG(ERROR) << "Access denied and backup/restore privileges are "
                 << "unavailable for " << path.value();
    }
  }
  if (!handle.is_valid()) {
    return base::unexpected(err);
  }

  if (::GetFileType(handle.Get()) != FILE_TYPE_DISK) {
    LOG(ERROR) << "Refusing to use a handle that is not a disk file or "
               << "directory: " << path.value();
    return base::unexpected(static_cast<DWORD>(ERROR_BAD_FILE_TYPE));
  }
  return handle;
}

bool WriteNonPropagatingSecurityDescriptor(
    HANDLE handle,
    const base::win::SecurityDescriptor& sd) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  // Fail closed: `kNonPropagatingSdInfo` always writes the owner, group, and
  // DACL. A missing DACL (no `SE_DACL_PRESENT`) or a NULL DACL would grant
  // Everyone full access, and a missing owner or group would make the write
  // fail.
  if (!sd.owner().has_value() || !sd.group().has_value() ||
      !sd.dacl().has_value() || sd.dacl()->is_null()) {
    LOG(ERROR) << "Refusing to write a security descriptor without an owner, "
               << "group, and non-NULL DACL";
    return false;
  }
  base::win::SecurityDescriptor protected_sd = sd.Clone();
  protected_sd.set_dacl_protected(true);
  SECURITY_DESCRIPTOR absolute_sd = protected_sd.ToAbsolute();

  // This intentionally does not use
  // `base::win::SecurityDescriptor::WriteToHandle()`: with
  // `SecurityObjectType::kFile` it calls `SetSecurityInfo(SE_FILE_OBJECT)`,
  // which walks existing (potentially attacker-controlled) children to
  // propagate inheritable ACEs. `SetKernelObjectSecurity` writes the security
  // descriptor to this object only.
  if (!::SetKernelObjectSecurity(handle, kNonPropagatingSdInfo, &absolute_sd)) {
    PLOG(ERROR) << "SetKernelObjectSecurity failed";
    return false;
  }
  return true;
}

bool ApplyNonPropagatingSdToVerifiedPath(
    const base::FilePath& path,
    const base::FilePath& expected_canonical_path,
    DWORD share_mode,
    const base::win::SecurityDescriptor& sd) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  constexpr DWORD kAccess = WRITE_DAC | WRITE_OWNER | FILE_READ_ATTRIBUTES;

  base::expected<base::win::ScopedHandle, DWORD> handle = OpenEntryNoFollow(
      path, kAccess, share_mode, PrivilegedRetry::kBackupRestore);
  if (!handle.has_value()) {
    LOG(ERROR) << "Failed to open entry to apply security descriptor: "
               << path.value() << ": "
               << logging::SystemErrorCodeToString(handle.error());
    return false;
  }

  BY_HANDLE_FILE_INFORMATION info = {};
  if (!::GetFileInformationByHandle(handle->Get(), &info)) {
    PLOG(ERROR) << "GetFileInformationByHandle failed for " << path.value();
    return false;
  }
  if ((info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
    LOG(ERROR) << "Refusing to apply security descriptor to reparse point: "
               << path.value();
    return false;
  }
  // `GetFinalPathNameByHandleW` reports the name used to open a hard link, so
  // reject multiply-linked files explicitly: applying `sd` would rewrite the
  // security descriptor of a file object shared with a path outside the tree.
  if ((info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 &&
      info.nNumberOfLinks > 1) {
    LOG(ERROR) << "Refusing to apply security descriptor to hard-linked file: "
               << path.value();
    return false;
  }
  if (!IsExpectedCanonicalPath(handle->Get(), expected_canonical_path)) {
    return false;
  }

  if (!WriteNonPropagatingSecurityDescriptor(handle->Get(), sd)) {
    LOG(ERROR) << "Failed to apply security descriptor to " << path.value();
    return false;
  }
  return true;
}

}  // namespace remoting
